#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

// --- Internal Structs w/ Capacity ---
// These are used internally to build the lists dynamically.

typedef struct {
    Property* properties;
    int count;
    int capacity;
} InternalPropertyList;

typedef struct {
    Material* materials;
    int count;
    int capacity;
} InternalMaterialList;


// --- Error Formatting Helper ---

// Sets the error message string, freeing any old one.
static void set_error(char** error_ptr, const char* format, ...) {
    if (error_ptr == NULL) return;
    free(*error_ptr);

    va_list args;
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    *error_ptr = (char*)malloc(size + 1);
    if (*error_ptr == NULL) { // out of memory while trying to report an error
        return; 
    }

    va_start(args, format);
    vsnprintf(*error_ptr, size + 1, format, args);
    va_end(args);
}

// --- Memory Management Helpers ---

//Safe string duplication. Aborts on failure.
static char* safe_strdup(const char* s, char** error_ptr) {
    char* new_str = strdup(s);
    if (new_str == NULL) {
        set_error(error_ptr, "Memory allocation failed (strdup)");
    }
    return new_str;
}

// Adds a property to an internal list, resizing if needed.
// Takes ownership of key and value pointers.
static bool add_property(InternalPropertyList* list, char* key, char* value, char** error_ptr) {
    if (list->count >= list->capacity) {
        int new_cap = (list->capacity == 0) ? 8 : list->capacity * 2;
        Property* new_props = (Property*)realloc(list->properties, new_cap * sizeof(Property));
        if (new_props == NULL) {
            set_error(error_ptr, "Memory allocation failed (realloc properties)");
            free(key);
            free(value);
            return false;
        }
        list->properties = new_props;
        list->capacity = new_cap;
    }
    list->properties[list->count].key = key;
    list->properties[list->count].value = value;
    list->count++;
    return true;
}

// Adds a material to the internal list, resizing if needed.
// Takes ownership of the material's internal pointers (name, properties).
static bool add_material(InternalMaterialList* list, Material mat, char** error_ptr) {
    if (list->count >= list->capacity) {
        int new_cap = (list->capacity == 0) ? 4 : list->capacity * 2;
        Material* new_mats = (Material*)realloc(list->materials, new_cap * sizeof(Material));
        if (new_mats == NULL) {
            set_error(error_ptr, "Memory allocation failed (realloc materials)");
            return false;
        }
        list->materials = new_mats;
        list->capacity = new_cap;
    }
    list->materials[list->count] = mat; // Struct copy
    list->count++;
    return true;
}

// Frees the heap-allocated contents of a single material.
static void free_material_contents(Material* mat) {
    if (mat == NULL) return;
    free(mat->name);
    for (int i = 0; i < mat->prop_count; i++) {
        free(mat->properties[i].key);
        free(mat->properties[i].value);
    }
    free(mat->properties);
    mat->name = NULL;
    mat->properties = NULL;
    mat->prop_count = 0;
}

// --- Lexer (Tokenizer) ---

// A simple 1-character lookahead buffer for the parser.
static int g_lookahead = -1;

// Gets the next character from the stream, skipping comments.
static int next_char(FILE* input) {
    if (g_lookahead != -1) {
        int c = g_lookahead;
        g_lookahead = -1;
        return c;
    }
    
    int c = fgetc(input);
    if (c == '#') {
        while ((c = fgetc(input)) != EOF && c != '\n');
        return (c == EOF) ? EOF : '\n';
    }
    return c;
}

// Puts a character back into the stream (1-char buffer).
static void unget_char(int c) {
    g_lookahead = c;
}

// Reads a multi-line quoted value from the stream.
static bool get_quoted_value(FILE* input, char* buffer, int max_len, char** error_ptr) {
    int i = 0;
    int c;
    while ((c = fgetc(input)) != EOF) { // Use fgetc to avoid comment skipping
        if (c == '"') {
            buffer[i] = '\0';
            return true; // Found closing quote
        }
        if (i >= max_len - 1) {
            set_error(error_ptr, "Parsing error: Quoted value exceeds buffer limit (%d bytes)", max_len);
            return false;
        }
        buffer[i++] = (char)c;
    }
    
    set_error(error_ptr, "Parsing error: Unterminated quoted value (reached EOF)");
    return false;
}

// Gets the next token from the stream.
// A token is one of: '{', '}', '=', a quoted string, or a word.
// Returns true on success, false on EOF or error.
static bool get_token(FILE* input, char* buffer, int max_len, char** error_ptr) {
    int c;
    
    while ((c = next_char(input)) != EOF && isspace(c));
    
    if (c == EOF) return false;

    if (c == '{' || c == '}' || c == '=') {
        buffer[0] = (char)c;
        buffer[1] = '\0';
        return true;
    }

    if (c == '"') {
        return get_quoted_value(input, buffer, max_len, error_ptr);
    }

    int i = 0;
    while (c != EOF && !isspace(c) && c != '{' && c != '}' && c != '=') {
        if (i >= max_len - 1) {
            set_error(error_ptr, "Parsing error: Token exceeds buffer limit (%d bytes)", max_len);
            return false;
        }
        buffer[i++] = (char)c;
        c = next_char(input);
    }
    
    if (c != EOF) {
        unget_char(c);
    }
    
    buffer[i] = '\0';
    return true;
}

// --- Validation ---

// Validates a property key. (Replaces regex)
// Rules: Starts with [a-zA-Z_], followed by [a-zA-Z0-9_]*
static bool is_valid_key(const char* key) {
    if (key == NULL || key[0] == '\0') return false;
    
    if (!isalpha(key[0]) && key[0] != '_') return false;
    
    for (int i = 1; key[i] != '\0'; i++) {
        if (!isalnum(key[i]) && key[i] != '_') return false;
    }
    return true;
}

// Checks if a property key already exists. O(n) scan.
static bool property_exists(const InternalPropertyList* list, const char* key) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->properties[i].key, key) == 0) {
            return true;
        }
    }
    return false;
}

// --- Main Parser ---

ParseResult parse_matprop(FILE* input) {
    InternalMaterialList mat_list = {0}; // Our dynamic array of materials
    ParseResult result = {0};            // The final result
    
    char token_buf[1024];  // Buffer for general tokens
    char value_buf[8192];  // Larger buffer for (multiline) values
    
    g_lookahead = -1; 
    
    Material current_material = {0};
    InternalPropertyList current_props = {0};
    char* key = NULL;
    
    // The main loop reads the *first token* of a new entry,
    // which should be the material name.
    while (get_token(input, token_buf, 1024, &result.error_message)) {
        
        current_material.name = safe_strdup(token_buf, &result.error_message);
        if (result.error_message) goto cleanup;

        if (!get_token(input, token_buf, 1024, &result.error_message) || strcmp(token_buf, "{") != 0) {
            set_error(&result.error_message, "Parsing error: Expected '{' after material name '%s'", current_material.name);
            goto cleanup;
        }
        
        bool found_closing_brace = false;

        while (get_token(input, token_buf, 1024, &result.error_message)) {

            if (strcmp(token_buf, "}") == 0) {
                found_closing_brace = true;
                break;
            }

            if (!is_valid_key(token_buf)) {
                set_error(&result.error_message, "Parsing error: Invalid property key '%s' in material '%s'", token_buf, current_material.name);
                goto cleanup;
            }
            if (property_exists(&current_props, token_buf)) {
                 set_error(&result.error_message, "Parsing error: Duplicate property '%s' in material '%s'", token_buf, current_material.name);
                 goto cleanup;
            }
            key = safe_strdup(token_buf, &result.error_message);
            if (result.error_message) goto cleanup;

            if (!get_token(input, token_buf, 1024, &result.error_message) || strcmp(token_buf, "=") != 0) {
                set_error(&result.error_message, "Parsing error: Expected '=' after key '%s' in material '%s'", key, current_material.name);
                goto cleanup;
            }
            
            if (!get_token(input, value_buf, 8192, &result.error_message)) {
                set_error(&result.error_message, "Parsing error: Expected a value after '%s =' in material '%s'", key, current_material.name);
                goto cleanup;
            }
            
            char* value = safe_strdup(value_buf, &result.error_message);
            if (result.error_message) goto cleanup;

            if (!add_property(&current_props, key, value, &result.error_message)) {
                goto cleanup;
            }
            key = NULL; // Ownership transferred
        }

        if (!found_closing_brace) {
            set_error(&result.error_message, "Parsing error: Missing closing '}' for material '%s'", current_material.name);
            goto cleanup;
        }

        if (current_props.count == 0) {
            set_error(&result.error_message, "Parsing error: Material '%s' has no properties defined.", current_material.name);
            goto cleanup;
        }
        
        current_material.properties = current_props.properties;
        current_material.prop_count = current_props.count;
        
        if (!add_material(&mat_list, current_material, &result.error_message)) {
            goto cleanup;
        }
        
        current_material = (Material){0};
        current_props = (InternalPropertyList){0};
    }

cleanup:
    // This 'goto' target handles cleanup on error.
    
    // Free any partially-built material
    free(key); // Free key if we were in the middle of parsing a property
    free(current_material.name);
    for (int i = 0; i < current_props.count; i++) {
        free(current_props.properties[i].key);
        free(current_props.properties[i].value);
    }
    free(current_props.properties);

    if (result.error_message) {
        // Error occurred: free everything we've built so far
        for (int i = 0; i < mat_list.count; i++) {
            free_material_contents(&mat_list.materials[i]);
        }
        free(mat_list.materials);
        result.materials = NULL;
        result.mat_count = 0;
    } else {
        // Success: assign the final list to the result
        result.materials = mat_list.materials;
        result.mat_count = mat_list.count;
    }

    return result;
}


// --- Public API Functions ---

void free_parse_result(ParseResult* result) {
    if (result == NULL) return;
    
    free(result->error_message);
    for (int i = 0; i < result->mat_count; i++) {
        free_material_contents(&result->materials[i]);
    }
    free(result->materials);
    
    *result = (ParseResult){0};
}

void print_material(const Material* mat) {
    if (mat == NULL) return;
    
    printf("Material: %s\n", mat->name);
    printf("{\n");
    for (int i = 0; i < mat->prop_count; i++) {
        printf("  %s = %s\n", mat->properties[i].key, mat->properties[i].value);
    }
    printf("}\n");
}