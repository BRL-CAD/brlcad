
# For tester_bu_vls, the input format is as follows:
#
# tester_bu_vls <function number> <args>
BRLCAD_ADDEXEC(tester_bu_vls bu_vls.c libbu NO_INSTALL)

# For function #1 (bu_vls_init) there are no arguments
add_test(bu_vls_init tester_bu_vls 1)

# For function #2 (bu_vls_vlsinit) there are no arguments
add_test(bu_vls_vlsinit tester_bu_vls 2)

# For function #3 (bu_vls_strcpy/bu_vls_addr) the <args> format is as follows:
#
# string_to_test
#
# where string_to_test is a string to be used in testing bu_vls_strcpy/bu_vls_addr.

add_test(bu_vls_access_1 tester_bu_vls 3 "Test 1 2 3")

# For function #4 (bu_vls_strncpy) the <args> format is as follwos:
#
# string_orig string_new n expected_result
#
# where string_new will be put into the vls via strncpy() after
# string_new is put in with strcpy(); n is the n given to strncpy()

add_test(bu_vls_strncpy_1 tester_bu_vls 4 "Test 1" "Test 1 2" 4 "Test")
add_test(bu_vls_strncpy_2 tester_bu_vls 4 "Test 1" "Test 1 2" 8 "Test 1 2")

# For function #5 (bu_vls_strdup) the <args> format is as follwos:
#
# string_to_test
#
# where string_to_test is a string to be used in testing bu_vls_strdup.

add_test(bu_vls_strdup_1 tester_bu_vls 5 "Test 1 2 3")

# For function #6 (bu_vls_strlen) the <args> format is as follows:
#
# string_to_test
#
# where string_to_test is a string whose length will be checked

add_test(bu_vls_strlen_1 tester_bu_vls 6 "Test 1 2 3")

# For function #7 (bu_vls_trunc) the <args> format is as follows:
#
# string_to_test trunc_len expected_result
#
# where string_to_test is the string which will be truncated, and
# trunc_len is the length that will be used for truncation.

add_test(bu_vls_trunc_1 tester_bu_vls 7 "Test 1 2 3" 6 "Test 1")
add_test(bu_vls_trunc_2 tester_bu_vls 7 "Test 1 2 3" -2 "Test 1 2")

# For function #9 (bu_vls_nibble) the <args> format is as follows:
#
# string_to_test nibble_len expected_result
#
# where string_to_test is the string which will be nibbled, and
# nibble_len is the length that will be nibbled.

add_test(bu_vls_nibble_1 tester_bu_vls 9 "Test 1 2 3" 4 " 1 2 3")

# For function #10 (bu_vls_strcat) the <args> format is as follows:
#
# string1 string2 expected_result
#
# where string1 and string2 will be concatenated.

add_test(bu_vls_strcat_1 tester_bu_vls 10 "Test 1" " 2 3" "Test 1 2 3")

# For function #11 (bu_vls_strncat) the <args> format is as follows:
#
# string1 string2 n expected_result
#
# where string1 and string2 will be concatenated.

add_test(bu_vls_strncat_1 tester_bu_vls 11 "Test 1" " 2 3"  4 "Test 1 2 3" )
add_test(bu_vls_strncat_2 tester_bu_vls 11 "Test 1" " 2 3"  2 "Test 1 2"   )
add_test(bu_vls_strncat_3 tester_bu_vls 11 "Test 1" " 2 3" -4 "Test 1 2 3" )
add_test(bu_vls_strncat_4 tester_bu_vls 11 "Test 1" " 2 3"  0 "Test 1"     )
add_test(bu_vls_strncat_5 tester_bu_vls 11 "Test 1" " 2 3"  5 "Test 1 2 3" )

# For function #12 (bu_vls_vlscat) the <args> format is as follows:
#
# string1 string2 expected_result
#
# where string1 and string2 will be concatenated via bu_vls_vlscat.

add_test(bu_vls_vlscat_1 tester_bu_vls 12 "Test 1" " 2 3" "Test 1 2 3")

# For function #13 (bu_vls_strcmp) the <args> format is as follows:
#
# string1 string2 expected_result
#
# where string1 and string2 will be compared via bu_vls_strcmp.

add_test(bu_vls_strcmp_equal_1   tester_bu_vls 13 "Test 1" "Test 1" 0)
add_test(bu_vls_strcmp_lesser_1  tester_bu_vls 13 "Test 1" "Test 2" -1)
add_test(bu_vls_strcmp_greater_1 tester_bu_vls 13 "Test 1" "Test 0" 1)

# For function #14 (bu_vls_strcmp) the <args> format is as follows:
#
# string1 string2 expected_result
#
# where string1 and string2 will be concatenated via bu_vls_vlscat.

add_test(bu_vls_strncmp_equal_1   tester_bu_vls 14 "Test 1" "Test 1" 6 0)
add_test(bu_vls_strncmp_lesser_1  tester_bu_vls 14 "Test 1" "Test 2" 6 -1)
add_test(bu_vls_strncmp_greater_1 tester_bu_vls 14 "Test 1" "Test 0" 6 1)
add_test(bu_vls_strncmp_equal_2   tester_bu_vls 14 "Test 1" "Test 1" 4 0)
add_test(bu_vls_strncmp_equal_3   tester_bu_vls 14 "Test 1" "Test 2" 4 0)
add_test(bu_vls_strncmp_equal_4   tester_bu_vls 14 "Test 1" "Test 0" 4 0)

# For function #15 (bu_vls_from_argv) the <args> format is as follows:
#
# strings expected_result
#
# where strings will be recreated via bu_vls_from_argv, and the
# recreation will be checked against expected_result

add_test(bu_vls_from_argv_1 tester_bu_vls 15 Test Test 2 3 4 "Test Test 2 3 4")

# For function #16 (bu_vls_trimspace) the <args> format is as follows:
#
# string expected_result
#
# where string will have spaces trimmed, and the result will be
# compared with expected_result

add_test(bu_vls_trimspace_1 tester_bu_vls 16 "   Testing1 2   " "Testing1 2")

# For function #17 (bu_vls_spaces) the <args> format is as follows:
#
# string num_spaces expected_result
#
# where string will have num_spaces appended, and the result will be
# checked against expected_result

add_test(bu_vls_spaces_1 tester_bu_vls 17 "Testing1 2" 3 "Testing1 2   ")

# For function #18 (bu_vls_detab) the <args> format is as follows:
#
# string expected_result
#
# where string will have tabs replaced, and the result will be checked
# against expected_result

add_test(bu_vls_detab_1 tester_bu_vls 18 "Testing	1 2	3" "Testing 1 2     3")

# For function #19 (bu_vls_prepend) the <args> format is as follows:
#
# string string_to_prepend expected_result
#
# where string_to_prepend will be prepended to string, and the result
# will be checked against expected_result

add_test(bu_vls_prepend_1 tester_bu_vls 19 "2 3" "Test 1 " "Test 1 2 3")

# For function #20 (bu_vls_substr) the <args> format is as follows:
#
#   source_string substring_begin_index substring_length expected_result_string
#
#  -1         0         1
#   098765432101234567890
#            "01234567890" <= 11 chars
add_test(bu_vls_substr_01 tester_bu_vls 20 "01234567890"  0  5  "01234")
add_test(bu_vls_substr_02 tester_bu_vls 20 "01234567890" -2  5  "")
add_test(bu_vls_substr_03 tester_bu_vls 20 "01234567890" -2 20  "")
add_test(bu_vls_substr_04 tester_bu_vls 20 "01234567890"  5 20  "567890")
add_test(bu_vls_substr_05 tester_bu_vls 20 "01234567890"  0 20  "01234567890")
add_test(bu_vls_substr_06 tester_bu_vls 20 "01234567890"  2  5  "23456")
add_test(bu_vls_substr_07 tester_bu_vls 20 "01234567890" 20  5  "")
add_test(bu_vls_substr_08 tester_bu_vls 20 "01234567890"  0 -1  "01234567890")
add_test(bu_vls_substr_09 tester_bu_vls 20 "01234567890"  2 -1  "234567890")
add_test(bu_vls_substr_10 tester_bu_vls 20 "01234567890"  0  0  "")
add_test(bu_vls_substr_11 tester_bu_vls 20 "01234567890"  0  2  "01")
add_test(bu_vls_substr_12 tester_bu_vls 20 "01234567890" 11 20  "")
