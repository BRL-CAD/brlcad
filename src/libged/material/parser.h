#ifndef MATPROP_PARSER_H
#define MATPROP_PARSER_H

#include <stdio.h> // For FILE*

/**
 * @struct Property
 * @brief Holds a single key-value property pair as C strings.
 */
typedef struct {
    char* key;
    char* value;
} Property;

/**
 * @struct Material
 * @brief Holds the data for a single material definition.
 *
 * This structure stores the material's name and a dynamic array of its properties.
 */
typedef struct {
    char* name;
    Property* properties;
    int prop_count;
} Material;

/**
 * @struct ParseResult
 * @brief Holds the result of a parsing operation.
 *
 * If parsing is successful, 'error_message' will be NULL, and 'materials'
 * will contain the array of parsed materials.
 * If parsing fails, 'error_message' will contain a heap-allocated error
 * string, and 'materials' will be NULL.
 *
 * The caller is ALWAYS responsible for calling free_parse_result() on
 * the returned struct to prevent memory leaks.
 */
typedef struct {
    Material* materials;
    int mat_count;
    char* error_message; // NULL on success
} ParseResult;

/**
 * @brief Parses a stream containing material property definitions.
 *
 * This function reads from a C input stream (like from fopen() or stdin)
 * and parses material definitions.
 *
 * @param input An input stream to read from.
 * @return A ParseResult struct. The caller MUST call free_parse_result()
 * on this struct when finished.
 */
ParseResult parse_matprop(FILE* input);

/**
 * @brief Frees all memory associated with a ParseResult.
 *
 * This frees the error message (if any), all materials, all property
 * keys and values, and the materials array itself.
 *
 * @param result A pointer to the ParseResult to be freed.
 */
void free_parse_result(ParseResult* result);

/**
 * @brief A helper function to print the material's data for debugging.
 * (Replaces the C++ member function).
 */
void print_material(const Material* mat);

#endif // MATPROP_PARSER_H