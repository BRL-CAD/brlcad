BRLCAD_ADDEXEC(tester_bu_sscanf bu_sscanf.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(tester_bu_gethostname bu_gethostname.c libbu NO_INSTALL)

#
#  ************ basename.c tests *************
#

###############################
#      bu_basename testing    #
###############################
BRLCAD_ADDEXEC(tester_bu_basename    bu_basename.c    libbu NO_INSTALL)
add_test(NAME bu_basename_null                                   COMMAND tester_bu_basename) # tests NULL
add_test(NAME bu_basename_empty                                  COMMAND tester_bu_basename "")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_file                  COMMAND tester_bu_basename "/usr/dir/file")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_                      COMMAND tester_bu_basename "/usr/dir/")
add_test(NAME bu_basename_sl_usr_bsl__sl_dir                     COMMAND tester_bu_basename "/usr\\\\/dir")
add_test(NAME bu_basename_sl_usr_sl__p_                          COMMAND tester_bu_basename "/usr/.")
add_test(NAME bu_basename_sl_usr_sl_                             COMMAND tester_bu_basename "/usr/")
add_test(NAME bu_basename_sl_usr                                 COMMAND tester_bu_basename "/usr")
add_test(NAME bu_basename_usr                                    COMMAND tester_bu_basename "usr")
add_test(NAME bu_basename_sl_usr_sl_some_sp_long_sl_file         COMMAND tester_bu_basename "/usr/some long/file")
add_test(NAME bu_basename_sl_usr_sl_some_sp_file                 COMMAND tester_bu_basename "/usr/some file")
add_test(NAME bu_basename_C_c__sl_usr_sl_some_bsl__sp_drivepath  COMMAND tester_bu_basename "C:/usr/some\\\\ drivepath")
add_test(NAME bu_basename_sl_a_sp_test_sp_file                   COMMAND tester_bu_basename "/a test file")
add_test(NAME bu_basename_another_sp_file                        COMMAND tester_bu_basename "another file")
add_test(NAME bu_basename_C_c__bsl__Temp                         COMMAND tester_bu_basename "C:\\\\Temp")
add_test(NAME bu_basename_C_c__sl_Temp                           COMMAND tester_bu_basename "C:/Temp")
add_test(NAME bu_basename_sl_                                    COMMAND tester_bu_basename "/")
add_test(NAME bu_basename_sl__sl__sl__sl__sl_                    COMMAND tester_bu_basename "/////")
add_test(NAME bu_basename_p_                                     COMMAND tester_bu_basename ".")
add_test(NAME bu_basename_p__p_                                  COMMAND tester_bu_basename "..")
add_test(NAME bu_basename_p__p__p_                               COMMAND tester_bu_basename "...")
add_test(NAME bu_basename_sp__sp__sp_                            COMMAND tester_bu_basename "   ")

#
#  ************ bitv.c tests *************
#
include(bitv-tests.cmake)

#
#  ************ booleanize.c tests *************
#

#################################
#      bu_booleanize testing    #
#################################
BRLCAD_ADDEXEC(tester_bu_booleanize bu_booleanize.c libbu NO_INSTALL)
add_test(NAME bu_booleanize_null         COMMAND tester_bu_booleanize) # tests NULL
add_test(NAME bu_booleanize_empty        COMMAND tester_bu_booleanize "")
add_test(NAME bu_booleanize_n            COMMAND tester_bu_booleanize "n")
add_test(NAME bu_booleanize_nabcd        COMMAND tester_bu_booleanize "nabcd")
add_test(NAME bu_booleanize_N            COMMAND tester_bu_booleanize "N")
add_test(NAME bu_booleanize_Nabcd        COMMAND tester_bu_booleanize "Nabcd")
add_test(NAME bu_booleanize__sp__sp_abcd COMMAND tester_bu_booleanize "  abcd")
add_test(NAME bu_booleanize_0            COMMAND tester_bu_booleanize "0")
add_test(NAME bu_booleanize_0000         COMMAND tester_bu_booleanize "0000")
add_test(NAME bu_booleanize__lp_null_rp_ COMMAND tester_bu_booleanize "(null)")
add_test(NAME bu_booleanize_y            COMMAND tester_bu_booleanize "y")
add_test(NAME bu_booleanize_yabcd        COMMAND tester_bu_booleanize "yabcd")
add_test(NAME bu_booleanize_Y            COMMAND tester_bu_booleanize "Y")
add_test(NAME bu_booleanize_Yabcd        COMMAND tester_bu_booleanize "Yabcd")
add_test(NAME bu_booleanize_1            COMMAND tester_bu_booleanize "1")
add_test(NAME bu_booleanize_0001         COMMAND tester_bu_booleanize "0001")
add_test(NAME bu_booleanize_abcd         COMMAND tester_bu_booleanize "abcd")

#
#  ************ ctype.c tests *************
#

##################################
#      bu_str_isprint testing    #
##################################
# Note - because of the special characters needed in these tests, the strings
# don't work well when fed in from the command line.
BRLCAD_ADDEXEC(tester_bu_str_isprint bu_str_isprint.c libbu NO_INSTALL)
add_test(NAME bu_str_isprint_basic                COMMAND tester_bu_str_isprint 1)
add_test(NAME bu_str_isprint_eol                  COMMAND tester_bu_str_isprint 2)
add_test(NAME bu_str_isprint_double_slash         COMMAND tester_bu_str_isprint 3)
add_test(NAME bu_str_isprint_horiz_tab            COMMAND tester_bu_str_isprint 4)
add_test(NAME bu_str_isprint_non-alphanumeric     COMMAND tester_bu_str_isprint 5)
add_test(NAME bu_str_isprint_eol_non-alphanumeric COMMAND tester_bu_str_isprint 6)
add_test(NAME bu_str_isprint_carriage_return      COMMAND tester_bu_str_isprint 7)

#
#  ************ dirname.c tests *************
#

##############################
#      bu_dirname testing    #
##############################
BRLCAD_ADDEXEC(tester_bu_dirname bu_dirname.c libbu NO_INSTALL)
add_test(NAME bu_dirname__usr_dir_file COMMAND tester_bu_dirname "/usr/dir/file")
add_test(NAME bu_dirname__usr_dir_ COMMAND tester_bu_dirname "/usr/dir/")
add_test(NAME bu_dirname__usr__bsl__dir COMMAND tester_bu_dirname "/usr\\\\/dir")
add_test(NAME bu_dirname__usr_period COMMAND tester_bu_dirname "/usr/.")
add_test(NAME bu_dirname__usr_ COMMAND tester_bu_dirname "/usr/")
add_test(NAME bu_dirname__usr COMMAND tester_bu_dirname "/usr")
add_test(NAME bu_dirname_usr COMMAND tester_bu_dirname "usr")
add_test(NAME bu_dirname__usr_some_sp_long_file COMMAND tester_bu_dirname "/usr/some long/file")
add_test(NAME bu_dirname__usr_some_sp_file COMMAND tester_bu_dirname "/usr/some file")
add_test(NAME bu_dirname__usr_some__bsl__sp_drivepath COMMAND tester_bu_dirname "C:/usr/some\\\\ drivepath")
add_test(NAME bu_dirname__a_sp_test_file COMMAND tester_bu_dirname "/a test file")
add_test(NAME bu_dirname_another_file COMMAND tester_bu_dirname "another file")
add_test(NAME bu_dirname_C_colon__bsl_Temp COMMAND tester_bu_dirname "C:\\\\Temp")
add_test(NAME bu_dirname_C_colon__Temp COMMAND tester_bu_dirname "C:/Temp")
add_test(NAME bu_dirname__ COMMAND tester_bu_dirname "/")
add_test(NAME bu_dirname______ COMMAND tester_bu_dirname "/////")
add_test(NAME bu_dirname__period_ COMMAND tester_bu_dirname ".")
add_test(NAME bu_dirname__period__period_ COMMAND tester_bu_dirname "..")
add_test(NAME bu_dirname__period__period__period_ COMMAND tester_bu_dirname "...")
add_test(NAME bu_dirname__sp__sp__sp_ COMMAND tester_bu_dirname "   ")
add_test(NAME bu_dirname_empty COMMAND tester_bu_dirname "")
add_test(NAME bu_dirname_null COMMAND tester_bu_dirname)

#
#  ************ escape.c tests *************
#
# Note - because of the quoting needed in these tests, it is simpler
# to leave the input strings in the C code.  Individual tests are triggered
# by number
BRLCAD_ADDEXEC(tester_bu_escape bu_escape.c libbu NO_INSTALL)

###################################
#      bu_str_unescape testing    #
###################################
foreach(test_num RANGE 1 23)
  add_test(NAME "bu_str_unescape_${test_num}" COMMAND tester_bu_escape 1 "${test_num}")
endforeach()

#################################
#      bu_str_escape testing    #
#################################
foreach(test_num RANGE 1 31)
  add_test(NAME "bu_str_escape_${test_num}" COMMAND tester_bu_escape 2 "${test_num}")
endforeach()

##############################################
#      escape/unescape round trip testing    #
##############################################
foreach(test_num RANGE 1 4)
  add_test(NAME "bu_escape_roundtrip_${test_num}" COMMAND tester_bu_escape 3 "${test_num}")
endforeach()

###
# bu_heap memory allocation testing
###
BRLCAD_ADDEXEC(tester_bu_heap bu_heap.c libbu NO_INSTALL)
add_test(NAME bu_heap_test_1 COMMAND tester_bu_heap)

#
#  ************ progname.c tests *************
#
# Note - some of the test conditions for bu_progname require previous
# steps to be performed in the same executable context.  Hence, in this
# case, multiple test cases are rolled into a single program execution.

###############################
#      bu_progname testing    #
###############################
BRLCAD_ADDEXEC(tester_bu_progname bu_progname.c libbu NO_INSTALL)
add_test(NAME bu_progname_tests COMMAND tester_bu_progname)

#
#  ************ encode.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_vls_encode and bu_vls_decode.
# Like many string functions using escaped characters the input strings are
# left in C to minimize pass-through errors.
BRLCAD_ADDEXEC(tester_bu_encode bu_encode.c libbu NO_INSTALL)
foreach(test_num RANGE 1 23)
  add_test(NAME "bu_encode_test_${test_num}" COMMAND tester_bu_encode "${test_num}")
endforeach()

#
#  ************ b64.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_b64_encode and bu_b64_decode.
BRLCAD_ADDEXEC(tester_bu_b64 bu_b64.c libbu NO_INSTALL)
add_test(NAME "bu_b64_test" COMMAND tester_bu_b64)


#
#  ************ rb_*.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_redblack bu_redblack.c libbu NO_INSTALL)
add_test(NAME bu_redblack_test COMMAND tester_bu_redblack)

#
#  ************ timer.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_timer bu_timer.c libbu NO_INSTALL)
add_test(NAME bu_gettime_test COMMAND tester_bu_timer)

#
#  ************ bu_semaphore.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_semaphore bu_semaphore.c "libbu;${WINMM_LIBRARY}" NO_INSTALL)
add_test(NAME bu_semaphore_test COMMAND tester_bu_semaphore)
add_test(NAME bu_semaphore_test_P1 COMMAND tester_bu_semaphore -P1)
add_test(NAME bu_semaphore_test_P2 COMMAND tester_bu_semaphore -P2)
add_test(NAME bu_semaphore_test_P3 COMMAND tester_bu_semaphore -P3)

#
#  ************ bu_parallel.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_parallel bu_parallel.c "libbu;${WINMM_LIBRARY}" NO_INSTALL)
add_test(NAME bu_parallel_test COMMAND tester_bu_parallel)

#
#  ************ vls_vprintf.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_vls_vprintf bu_vls_vprintf.c libbu NO_INSTALL)
foreach(test_num RANGE 1 67)
  add_test(NAME "bu_vls_vprintf_test_${test_num}" COMMAND tester_bu_vls_vprintf "${test_num}")
endforeach()

#
#  ************ vls.c tests *************
#
include(vls-tests.cmake)


#
#  ************ sort.c test *************
#
BRLCAD_ADDEXEC(tester_bu_sort bu_sort.c libbu NO_INSTALL)
add_test(NAME bu_sort_test_unsigned_int_1 COMMAND tester_bu_sort 1)
add_test(NAME bu_sort_test_unsigned_int_2 COMMAND tester_bu_sort 2)
add_test(NAME bu_sort_test_fastf_t_1 COMMAND tester_bu_sort 3)
add_test(NAME bu_sort_test_fastf_t_2 COMMAND tester_bu_sort 4)
add_test(NAME bu_sort_test_string_1 COMMAND tester_bu_sort 5)
add_test(NAME bu_sort_test_string_2 COMMAND tester_bu_sort 6)
add_test(NAME bu_sort_test_dist_to_int_1 COMMAND tester_bu_sort 7)
add_test(NAME bu_sort_test_dist_to_int_2 COMMAND tester_bu_sort 8)
add_test(NAME bu_sort_test_fastf_empty COMMAND tester_bu_sort 9)
add_test(NAME bu_sort_test_fastf_inf COMMAND tester_bu_sort 10)

#
#  ************ color.c tests *************

# For COMMAND tester_bu_color, the input format is as follows:
#
# COMMAND tester_bu_color <function number> <args>
BRLCAD_ADDEXEC(tester_bu_color bu_color.c libbu NO_INSTALL)

# For function #1 (bu_rgb_to_hsv) the <args> format is as follows:
#
# rgb expected_hsv
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_rgb_to_hsv_1 COMMAND tester_bu_color 1 192,78,214 290.294,0.636,0.839)

# For function #2 (bu_hsv_to_rgb) the <args> format is as follows:
#
# hsv expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_hsv_to_rgb_1 COMMAND tester_bu_color 2 290.294,0.636,0.839 192,78,214)

# For function #3 (bu_str_to_rgb) the <args> format is as follows:
#
# string expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers.

add_test(NAME bu_str_to_rgb_1 COMMAND tester_bu_color 3 "#C04ED6" 192,78,214)
add_test(NAME bu_str_to_rgb_2 COMMAND tester_bu_color 3 "192/78/214" 192,78,214)
add_test(NAME bu_str_to_rgb_3 COMMAND tester_bu_color 3 "192 78 214" 192,78,214)

# For function #4 (bu_color_to_rgb_floats) the <args> format is as follows:
#
# rgb_color
#
# Where rgb colors are represented by comma-separated triples of
# integers or floats.

add_test(NAME bu_color_to_rgb_floats_1 COMMAND tester_bu_color 4 192,78,214)
#
# ************ str.c tests ************
#

# For COMMAND tester_bu_str, the input format is as follows:
#
# COMMAND tester_bu_str <function number> <args>
BRLCAD_ADDEXEC(tester_bu_str bu_str.c libbu NO_INSTALL)

# For function #1 (bu_strlcatm) the <args> format is as follows:
#
# string1 string2 result_size expected_ret expected_result
#
# Where string1 and string2 are the strings to concatenate,
# result_size is the size for the buffer that the result will be
# stored in, expected_ret is the expected return value of the
# function, and expected_result is the expected resultant string in
# dst.

add_test(NAME bu_strlcatm_valid_1 COMMAND tester_bu_str 1 "abc" "def" 7 6 "abcdef")
add_test(NAME bu_strlcatm_valid_2 COMMAND tester_bu_str 1 "abc" "def" 8 6 "abcdef")
add_test(NAME bu_strlcatm_bad_size_1 COMMAND tester_bu_str 1 "abc" "def" 0 0 "")
add_test(NAME bu_strlcatm_bad_size_2 COMMAND tester_bu_str 1 "abc" "def" 5 4 "abcd")

# For function #2 (bu_strlcpym) the <args> format is as follows:
#
# string result_size expected_ret expected_result
#
# Where string is the string to copy, result_size is the size of the
# result buffer, expected_ret is the expected return value of the
# function, and expected_result is the expected contents of the copied
# string.

add_test(NAME bu_strlcpym_valid_1 COMMAND tester_bu_str 2 "abcdef" 7 6 "abcdef")
add_test(NAME bu_strlcpym_valid_2 COMMAND tester_bu_str 2 "abcdef" 8 6 "abcdef")
add_test(NAME bu_strlcpym_bad_size_1 COMMAND tester_bu_str 2 "abcdef" 0 0 "")
add_test(NAME bu_strlcpym_bad_size_2 COMMAND tester_bu_str 2 "abcdef" 5 4 "abcd")

# For function #3 (bu_strdupm) the <args> format is as follows:
#
# string
#
# Where string is the string to duplicate.

add_test(NAME bu_strdupm_1 COMMAND tester_bu_str 3 "abcdef")

# For function #4 (bu_strcmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcmp_equal_1 COMMAND tester_bu_str 4 "abcd" "abcd" 0)
add_test(NAME bu_strcmp_less_1 COMMAND tester_bu_str 4 "a" "b" -1)
add_test(NAME bu_strcmp_less_2 COMMAND tester_bu_str 4 "abCD" "abcd" -1)
add_test(NAME bu_strcmp_greater_1 COMMAND tester_bu_str 4 "b" "a" 1)
add_test(NAME bu_strcmp_greater_2 COMMAND tester_bu_str 4 "abcd" "abCD" 1)

# For function #5 (bu_strncmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncmp_equal_1 COMMAND tester_bu_str 5 "abcd" "abcd" 4 0)
add_test(NAME bu_strncmp_equal_2 COMMAND tester_bu_str 5 "abcd" "abcd" 3 0)
add_test(NAME bu_strncmp_equal_3 COMMAND tester_bu_str 5 "abcd" "abcd" 5 0)
add_test(NAME bu_strncmp_equal_4 COMMAND tester_bu_str 5 "abcd" "abc" 3 0)
add_test(NAME bu_strncmp_equal_5 COMMAND tester_bu_str 5 "abcd" "abce" 3 0)
add_test(NAME bu_strncmp_less_1 COMMAND tester_bu_str 5 "abcd" "abce" 4 -1)
add_test(NAME bu_strncmp_less_2 COMMAND tester_bu_str 5 "abCD" "abcd" 4 -1)
add_test(NAME bu_strncmp_greater_1 COMMAND tester_bu_str 5 "abcd" "abc" 4 1)
add_test(NAME bu_strncmp_greater_2 COMMAND tester_bu_str 5 "abcd" "abCD" 4 1)

# For function #6 (bu_strcasecmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcasecmp_equal_1 COMMAND tester_bu_str 6 "abcd" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_2 COMMAND tester_bu_str 6 "abCD" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_3 COMMAND tester_bu_str 6 "abcd" "abCD" 0)
add_test(NAME bu_strcasecmp_less_1 COMMAND tester_bu_str 4 "a" "b" -1)
add_test(NAME bu_strcasecmp_greater_1 COMMAND tester_bu_str 4 "b" "a" 1)

# For function #7 (bu_strncasecmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncasecmp_equal_1 COMMAND tester_bu_str 7 "abcd" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_2 COMMAND tester_bu_str 7 "abcd" "abcd" 3 0)
add_test(NAME bu_strncasecmp_equal_3 COMMAND tester_bu_str 7 "abcd" "abcd" 5 0)
add_test(NAME bu_strncasecmp_equal_4 COMMAND tester_bu_str 7 "abcd" "abc" 3 0)
add_test(NAME bu_strncasecmp_equal_5 COMMAND tester_bu_str 7 "abcd" "abce" 3 0)
add_test(NAME bu_strncasecmp_equal_6 COMMAND tester_bu_str 7 "abCD" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_7 COMMAND tester_bu_str 7 "abcd" "abCD" 4 0)
add_test(NAME bu_strncasecmp_less_1 COMMAND tester_bu_str 7 "abcd" "abce" 4 -1)
add_test(NAME bu_strncasecmp_greater_1 COMMAND tester_bu_str 7 "abcd" "abc" 4 1)

#
#  *********** badmagic.c tests ************
#

BRLCAD_ADDEXEC(tester_bu_badmagic bu_badmagic.c libbu NO_INSTALL)
add_test(NAME bu_badmagic_diff_magic  COMMAND tester_bu_badmagic 1)
add_test(NAME bu_badmagic_normal      COMMAND tester_bu_badmagic 2)
add_test(NAME bu_badmagic_null_ptr    COMMAND tester_bu_badmagic 3)
add_test(NAME bu_badmagic_misaligned  COMMAND tester_bu_badmagic 4)

#
#  *********** date-time.c tests ************
#

BRLCAD_ADDEXEC(tester_bu_date-time bu_date-time.c libbu NO_INSTALL)
foreach(test_num RANGE 1 10)
  add_test(NAME "bu_date-time${test_num}" COMMAND tester_bu_date-time "${test_num}")
endforeach()

CMAKEFILES(
  bitv-tests.cmake
  test_internals.h
  vls-tests.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
