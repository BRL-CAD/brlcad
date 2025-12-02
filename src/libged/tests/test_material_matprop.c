/* T E S T _ M A T E R I A L _ M A T P R O P . C
 * BRL-CAD
 *
 * Copyright (c) 2021-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty o
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bu.h"
#include "../material/parser.h"

/* Helper: Create a temporary FILE* stream from a string. */
static FILE*
create_test_stream(const char* content)
{
    FILE* f = tmpfile();
    if (f == NULL) {
        perror("Failed to create temp file for test");
        exit(1);
    }
    fputs(content, f);
    rewind(f);
    return f;
}

/* Helper: Find and get a property value. Returns NULL if not found. */
static const char*
get_prop_value(const Material* mat, const char* key)
{
    int i;
    for (i = 0; i < mat->prop_count; i++) {
        if (BU_STR_EQUIV(mat->properties[i].key, key)) {
            return mat->properties[i].value;
        }
    }
    return NULL;
}

/* * COPY OF THE FUNCTION TO BE TESTED 
 * We include this here so we can unit test the logic in isolation.
 */
static int
is_in_overwrite_list(const char *name, char **overwrite_list)
{
    int i;
    if (!overwrite_list) return 0;

    for (i = 0; overwrite_list[i] != NULL; i++) {
        if (BU_STR_EQUIV(name, overwrite_list[i])) {
            return 1;
        }
    }
    return 0;
}

/*
 * Test 1: Verify basic material and property parsing
 */
int
test_basic_parsing()
{
    printf("Testing Basic Parsing...");
    const char* content = 
        "# A test material\n"
        "MyMaterial\n"
        "{\n"
        "  Density           = 1.23\n"
        "  Yield_Strength    = 20.0E6\n"
        "}\n"
        "\n"
        "AnotherMaterial { Density = 5.8 }"; // Test compact form
    
    FILE* f = create_test_stream(content);
    ParseResult result = parse_matprop(f);
    int ret = 0;

    if (result.error_message != NULL) {
        printf("\nFAILED: Expected success but got error: %s\n", result.error_message);
        ret = 1;
        goto cleanup;
    }

    if (result.mat_count != 2) {
        printf("\nFAILED: Expected 2 materials, got %d\n", result.mat_count);
        ret = 1;
        goto cleanup;
    }

    // Check Material 1
    if (!BU_STR_EQUIV(result.materials[0].name, "MyMaterial")) {
        printf("\nFAILED: Expected MyMaterial, got %s\n", result.materials[0].name);
        ret = 1;
    }
    const char* val = get_prop_value(&result.materials[0], "Density");
    if (!val || !BU_STR_EQUIV(val, "1.23")) {
        printf("\nFAILED: Incorrect Density for MyMaterial\n");
        ret = 1;
    }

    // Check Material 2
    if (!BU_STR_EQUIV(result.materials[1].name, "AnotherMaterial")) {
        printf("\nFAILED: Expected AnotherMaterial, got %s\n", result.materials[1].name);
        ret = 1;
    }

cleanup:
    free_parse_result(&result);
    fclose(f);
    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Test 2: Verify multiline quoted strings (e.g. descriptions)
 */
int
test_multiline_parsing()
{
    printf("Testing Multiline Parsing...");
    const char* content = 
        "Material1 {\n"
        "  Description = \"Line 1\n"
        "Line 2\n"
        "Line 3\"\n"
        "}\n";
    
    FILE* f = create_test_stream(content);
    ParseResult result = parse_matprop(f);
    int ret = 0;

    if (result.error_message != NULL) {
        printf("\nFAILED: Unexpected error: %s\n", result.error_message);
        ret = 1;
        goto cleanup;
    }

    const char* val = get_prop_value(&result.materials[0], "Description");
    const char* expected = "Line 1\nLine 2\nLine 3";
    
    if (val == NULL || !BU_STR_EQUIV(val, expected)) {
        printf("\nFAILED: Multiline string did not match expected output.\n");
        printf("Got:\n'%s'\n", val ? val : "NULL");
        ret = 1;
    }

cleanup:
    free_parse_result(&result);
    fclose(f);
    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Test 3: Verify the parser correctly fails on bad input
 */
int
test_malformed_input()
{
    printf("Testing Malformed Input...");
    int ret = 0;

    // Case 1: Missing brace
    FILE* f1 = create_test_stream("Material1 Density = 1.0");
    ParseResult r1 = parse_matprop(f1);
    if (r1.error_message == NULL) {
        printf("\nFAILED: Parser accepted missing brace input.\n");
        ret = 1;
    }
    free_parse_result(&r1);
    fclose(f1);

    // Case 2: Missing closing brace
    FILE* f2 = create_test_stream("Material1 { Density = 1.0"); 
    ParseResult r2 = parse_matprop(f2);
    if (r2.error_message == NULL) {
        printf("\nFAILED: Parser accepted missing closing brace.\n");
        ret = 1;
    }
    free_parse_result(&r2);
    fclose(f2);

    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Test 4: Verify full content parsing (mimics the large test in parser_test.c)
 */
int
test_full_content()
{
    printf("Testing Full File Content...");
    const char* content = 
        "# This is a comment line.\n"
        "\n"
        "Playdough\n"
        "{\n"
        "  foo_Material     = playdough\n"
        "  Density           = 1.2345\n"
        "  Yield_Strength    = 20.0E6\n"
        "}\n"
        "\n"
        "Air\n"
        "        {\n"
        "  Density         = 0.001\n"
        "  }\n";

    FILE* f = create_test_stream(content);
    ParseResult result = parse_matprop(f);
    int ret = 0;

    if (result.mat_count != 2) {
        printf("\nFAILED: Expected 2 materials, got %d\n", result.mat_count);
        ret = 1;
        goto cleanup;
    }

    if (!BU_STR_EQUIV(result.materials[0].name, "Playdough")) {
        printf("\nFAILED: First material name mismatch.\n");
        ret = 1;
    }

    const char* density = get_prop_value(&result.materials[1], "Density");
    if (!BU_STR_EQUIV(density, "0.001")) {
        printf("\nFAILED: Air Density mismatch.\n");
        ret = 1;
    }

cleanup:
    free_parse_result(&result);
    fclose(f);
    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Test 5: Verify is_in_overwrite_list
 */
int
test_overwrite_helper()
{
    printf("Testing Overwrite List Helper...");
    int ret = 0;
    
    // Create a mock argv list. Must be NULL terminated.
    // Casting to char* to satisfy signature, though string literals are const.
    char *mock_list[] = { "Aluminum", "Steel", "Titanium", NULL };

    // Test 1: Item is in list
    if (is_in_overwrite_list("Aluminum", mock_list) != 1) {
        printf("\nFAILED: Did not find 'Aluminum' in list.\n");
        ret = 1;
    }

    // Test 2: Item is NOT in list
    if (is_in_overwrite_list("Wood", mock_list) != 0) {
        printf("\nFAILED: Found 'Wood' in list, but it should not be there.\n");
        ret = 1;
    }

    // Test 3: NULL list
    if (is_in_overwrite_list("Steel", NULL) != 0) {
        printf("\nFAILED: NULL list should return 0.\n");
        ret = 1;
    }

    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Test 6: Verify the Logic Flow (Simulate import_matprop_file logic)
 */
int
test_overwrite_logic_flow()
{
    printf("Testing Overwrite Decision Logic...");
    int ret = 0;

    char *mock_list[] = { "Aluminum", NULL };
    char *current_mat_name = "Aluminum";
    
    // Scenario 1: Force ALL is ON. List irrelevant.
    // Expected: should_overwrite = 1
    int force_all = 1;
    int overwrite_argc = 0; 
    
    int should_overwrite = force_all;
    if (!should_overwrite && overwrite_argc > 0) {
        should_overwrite = is_in_overwrite_list(current_mat_name, mock_list);
    }

    if (should_overwrite != 1) {
        printf("\nFAILED: Force=1 should result in overwrite.\n");
        ret = 1;
    }

    // Scenario 2: Force is OFF. List provided. Name matches.
    // Expected: should_overwrite = 1
    force_all = 0;
    overwrite_argc = 1; // List has items
    
    should_overwrite = force_all;
    if (!should_overwrite && overwrite_argc > 0) {
        should_overwrite = is_in_overwrite_list(current_mat_name, mock_list);
    }
    
    if (should_overwrite != 1) {
        printf("\nFAILED: Force=0, In List=Yes should result in overwrite.\n");
        ret = 1;
    }

    // Scenario 3: Force is OFF. List provided. Name mismatch.
    // Expected: should_overwrite = 0
    current_mat_name = "Wood"; // Not in mock_list
    force_all = 0;
    overwrite_argc = 1;

    should_overwrite = force_all;
    if (!should_overwrite && overwrite_argc > 0) {
        should_overwrite = is_in_overwrite_list(current_mat_name, mock_list);
    }

    if (should_overwrite != 0) {
        printf("\nFAILED: Force=0, In List=No should result in SKIP.\n");
        ret = 1;
    }

    // Scenario 4: Force is OFF. No list provided.
    // Expected: should_overwrite = 0
    force_all = 0;
    overwrite_argc = 0; // Empty list logic
    
    should_overwrite = force_all;
    if (!should_overwrite && overwrite_argc > 0) {
        should_overwrite = is_in_overwrite_list(current_mat_name, mock_list);
    }

    if (should_overwrite != 0) {
        printf("\nFAILED: Force=0, No List should result in SKIP.\n");
        ret = 1;
    }

    if (ret == 0) printf(" PASSED\n");
    return ret;
}

/*
 * Main entry point
 */
int
main(int ac, char* av[])
{
    int failure_count = 0;

    // Silence unused parameter warnings if any
    (void)ac; 
    (void)av;

    printf("--- Running Matprop Parser Unit Tests ---\n");

    failure_count += test_basic_parsing();
    failure_count += test_multiline_parsing();
    failure_count += test_malformed_input();
    failure_count += test_full_content();
    failure_count += test_overwrite_helper();
    failure_count += test_overwrite_logic_flow();

    if (failure_count > 0) {
        printf("\n%d TESTS FAILED\n", failure_count);
        return 1;
    }

    printf("\nAll parser unit tests passed.\n");
    return 0;
}
