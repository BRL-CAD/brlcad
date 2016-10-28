#
#  ************ bu_sscanf tests *************
#

BRLCAD_ADDEXEC(bu_sscanf sscanf.c libbu TEST)
BRLCAD_ADDEXEC(bu_gethostname gethostname.c libbu TEST)

#
#  ************ basename.c tests *************
#

###############################
#      bu_basename testing    #
###############################
BRLCAD_ADDEXEC(bu_basename basename.c libbu TEST)
add_test(NAME bu_basename_null                                   COMMAND bu_basename) # tests NULL
add_test(NAME bu_basename_empty                                  COMMAND bu_basename "")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_file                  COMMAND bu_basename "/usr/dir/file")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_                      COMMAND bu_basename "/usr/dir/")
add_test(NAME bu_basename_sl_usr_bsl__sl_dir                     COMMAND bu_basename "/usr\\\\/dir")
add_test(NAME bu_basename_sl_usr_sl__p_                          COMMAND bu_basename "/usr/.")
add_test(NAME bu_basename_sl_usr_sl_                             COMMAND bu_basename "/usr/")
add_test(NAME bu_basename_sl_usr                                 COMMAND bu_basename "/usr")
add_test(NAME bu_basename_usr                                    COMMAND bu_basename "usr")
add_test(NAME bu_basename_sl_usr_sl_some_sp_long_sl_file         COMMAND bu_basename "/usr/some long/file")
add_test(NAME bu_basename_sl_usr_sl_some_sp_file                 COMMAND bu_basename "/usr/some file")
add_test(NAME bu_basename_C_c__sl_usr_sl_some_bsl__sp_drivepath  COMMAND bu_basename "C:/usr/some\\\\ drivepath")
add_test(NAME bu_basename_sl_a_sp_test_sp_file                   COMMAND bu_basename "/a test file")
add_test(NAME bu_basename_another_sp_file                        COMMAND bu_basename "another file")
add_test(NAME bu_basename_C_c__bsl__Temp                         COMMAND bu_basename "C:\\\\Temp")
add_test(NAME bu_basename_C_c__sl_Temp                           COMMAND bu_basename "C:/Temp")
add_test(NAME bu_basename_sl_                                    COMMAND bu_basename "/")
add_test(NAME bu_basename_sl__sl__sl__sl__sl_                    COMMAND bu_basename "/////")
add_test(NAME bu_basename_p_                                     COMMAND bu_basename ".")
add_test(NAME bu_basename_p__p_                                  COMMAND bu_basename "..")
add_test(NAME bu_basename_p__p__p_                               COMMAND bu_basename "...")
add_test(NAME bu_basename_sp__sp__sp_                            COMMAND bu_basename "   ")


##########################################
#      bu_file_path_component testing    #
##########################################
BRLCAD_ADDEXEC(bu_path_component path_component.c libbu TEST)
add_test(NAME bu_path_component_null                               COMMAND bu_path_component) # tests NULL
add_test(NAME bu_path_component_empty                              COMMAND bu_path_component 0) # tests empty

# NOTE: these test numbers correspond with the bu_path_component_t enum values in include/bu/path.h
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_ext          COMMAND bu_path_component "/usr/dir/file.ext" "ext" 4)
add_test(NAME bu_path_component_c_sl_usr_sl_dir_sl_file_ext        COMMAND bu_path_component "C:/usr/dir/file.ext" "ext" 4)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_base         COMMAND bu_path_component "/usr/dir/file.ext" "file.ext" 2)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_base_core    COMMAND bu_path_component "/usr/dir/file.ext" "file" 3)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_dir          COMMAND bu_path_component "/usr/dir/file.ext" "/usr/dir" 0)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_dir_core     COMMAND bu_path_component "/usr/dir/file.ext" "/usr/dir/file" 1)
add_test(NAME bu_path_component_c_sl_usr_sl_dir_sl_file_dir_core   COMMAND bu_path_component "C:/usr/dir/file.ext" "C:/usr/dir/file" 1)


################################
#      bu_file_mime testing    #
################################
BRLCAD_ADDEXEC(bu_file_mime file_mime.c libbu TEST)

# NOTE: these test numbers correspond with the bu_mime_t enum values in include/bu/mime.h
add_test(NAME bu_file_mime_dxf   COMMAND bu_file_mime dxf 5 1) # tests dxf extension in model context
add_test(NAME bu_file_mime_stp   COMMAND bu_file_mime stp 5 7) # tests stp extension in model context
add_test(NAME bu_file_mime_png   COMMAND bu_file_mime png 3 13) # tests png extension in image context

#
#  ************ bitv.c tests *************
#
include(tests_bitv.cmake)

#
#  ************ booleanize.c tests *************
#

#################################
#      bu_booleanize testing    #
#################################
BRLCAD_ADDEXEC(bu_booleanize booleanize.c libbu TEST)
# expected returns are false:0, strongly true:1, or anything else:>1
add_test(NAME bu_booleanize_nullptr      COMMAND bu_booleanize) # tests NULL
add_test(NAME bu_booleanize_nullstr      COMMAND bu_booleanize "(null)" 0)
add_test(NAME bu_booleanize_empty        COMMAND bu_booleanize "" 0)
add_test(NAME bu_booleanize_space        COMMAND bu_booleanize " " 0)
add_test(NAME bu_booleanize_n            COMMAND bu_booleanize "n" 0)
add_test(NAME bu_booleanize_nabcd        COMMAND bu_booleanize "nabcd" 2)
add_test(NAME bu_booleanize_N            COMMAND bu_booleanize "N" 0)
add_test(NAME bu_booleanize_Nabcd        COMMAND bu_booleanize "Nabcd" 2)
add_test(NAME bu_booleanize__sp__sp_abcd COMMAND bu_booleanize "  abcd" 2)
add_test(NAME bu_booleanize_y            COMMAND bu_booleanize "y" 1)
add_test(NAME bu_booleanize_yabcd        COMMAND bu_booleanize "yabcd" 2)
add_test(NAME bu_booleanize_Y            COMMAND bu_booleanize "Y" 1)
add_test(NAME bu_booleanize_Yabcd        COMMAND bu_booleanize "Yabcd" 2)
add_test(NAME bu_booleanize_abcd         COMMAND bu_booleanize "abcd" 2)
add_test(NAME bu_booleanize_neg1         COMMAND bu_booleanize "-1" 2)
add_test(NAME bu_booleanize_1            COMMAND bu_booleanize "1" 1)
add_test(NAME bu_booleanize_2            COMMAND bu_booleanize "2" 2)
add_test(NAME bu_booleanize_0            COMMAND bu_booleanize "0" 0)
add_test(NAME bu_booleanize_neg0         COMMAND bu_booleanize "-0" 0)
add_test(NAME bu_booleanize_0000         COMMAND bu_booleanize "0000" 0)
add_test(NAME bu_booleanize_0001         COMMAND bu_booleanize "0001" 1)

#
#  ************ ctype.c tests *************
#

##################################
#      bu_str_isprint testing    #
##################################
# Note - because of the special characters needed in these tests, the strings
# don't work well when fed in from the command line.
BRLCAD_ADDEXEC(bu_str_isprint str_isprint.c libbu TEST)
add_test(NAME bu_str_isprint_basic                COMMAND bu_str_isprint 1)
add_test(NAME bu_str_isprint_eol                  COMMAND bu_str_isprint 2)
add_test(NAME bu_str_isprint_double_slash         COMMAND bu_str_isprint 3)
add_test(NAME bu_str_isprint_horiz_tab            COMMAND bu_str_isprint 4)
add_test(NAME bu_str_isprint_non-alphanumeric     COMMAND bu_str_isprint 5)
add_test(NAME bu_str_isprint_eol_non-alphanumeric COMMAND bu_str_isprint 6)
add_test(NAME bu_str_isprint_carriage_return      COMMAND bu_str_isprint 7)

#
#  ************ dirname.c tests *************
#

##############################
#      bu_dirname testing    #
##############################
BRLCAD_ADDEXEC(bu_dirname dirname.c libbu TEST)
add_test(NAME bu_dirname__usr_dir_file COMMAND bu_dirname "/usr/dir/file")
add_test(NAME bu_dirname__usr_dir_ COMMAND bu_dirname "/usr/dir/")
add_test(NAME bu_dirname__usr__bsl__dir COMMAND bu_dirname "/usr\\\\/dir")
add_test(NAME bu_dirname__usr_period COMMAND bu_dirname "/usr/.")
add_test(NAME bu_dirname__usr_ COMMAND bu_dirname "/usr/")
add_test(NAME bu_dirname__usr COMMAND bu_dirname "/usr")
add_test(NAME bu_dirname_usr COMMAND bu_dirname "usr")
add_test(NAME bu_dirname__usr_some_sp_long_file COMMAND bu_dirname "/usr/some long/file")
add_test(NAME bu_dirname__usr_some_sp_file COMMAND bu_dirname "/usr/some file")
add_test(NAME bu_dirname__usr_some__bsl__sp_drivepath COMMAND bu_dirname "C:/usr/some\\\\ drivepath")
add_test(NAME bu_dirname__a_sp_file COMMAND bu_dirname "/a test file")
add_test(NAME bu_dirname_another_file COMMAND bu_dirname "another file")
add_test(NAME bu_dirname_C_colon__bsl_Temp COMMAND bu_dirname "C:\\\\Temp")
add_test(NAME bu_dirname_C_colon__Temp COMMAND bu_dirname "C:/Temp")
add_test(NAME bu_dirname__ COMMAND bu_dirname "/")
add_test(NAME bu_dirname______ COMMAND bu_dirname "/////")
add_test(NAME bu_dirname__period_ COMMAND bu_dirname ".")
add_test(NAME bu_dirname__period__period_ COMMAND bu_dirname "..")
add_test(NAME bu_dirname__period__period__period_ COMMAND bu_dirname "...")
add_test(NAME bu_dirname__sp__sp__sp_ COMMAND bu_dirname "   ")
add_test(NAME bu_dirname_empty COMMAND bu_dirname "")
add_test(NAME bu_dirname_null COMMAND bu_dirname)

#
#  ************ escape.c tests *************
#
# Note - because of the quoting needed in these tests, it is simpler
# to leave the input strings in the C code.  Individual tests are triggered
# by number
BRLCAD_ADDEXEC(bu_escape escape.c libbu TEST)

###################################
#      bu_str_unescape testing    #
###################################
foreach(num RANGE 1 23)
  add_test(NAME "bu_str_unescape_${num}" COMMAND bu_escape 1 "${num}")
endforeach()

#################################
#      bu_str_escape testing    #
#################################
foreach(num RANGE 1 31)
  add_test(NAME "bu_str_escape_${num}" COMMAND bu_escape 2 "${num}")
endforeach()

##############################################
#      escape/unescape round trip testing    #
##############################################
foreach(num RANGE 1 4)
  add_test(NAME "bu_escape_roundtrip_${num}" COMMAND bu_escape 3 "${num}")
endforeach()

###
# bu_heap memory allocation testing
###
BRLCAD_ADDEXEC(bu_heap heap.c libbu TEST)
add_test(NAME bu_heap_1 COMMAND bu_heap)

#
#  ************ progname.c tests *************
#
# Note - some of the test conditions for bu_progname require previous
# steps to be performed in the same executable context.  Hence, in this
# case, multiple test cases are rolled into a single program execution.

###############################
#      bu_progname testing    #
###############################
BRLCAD_ADDEXEC(bu_progname progname.c libbu TEST)
add_test(NAME bu_progname_tests COMMAND bu_progname)

#
#  ************ encode.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_vls_encode and bu_vls_decode.
# Like many string functions using escaped characters the input strings are
# left in C to minimize pass-through errors.
BRLCAD_ADDEXEC(bu_encode encode.c libbu TEST)
foreach(num RANGE 1 23)
  add_test(NAME "bu_encode_${num}" COMMAND bu_encode "${num}")
endforeach()

#
#  ************ b64.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_b64_encode and bu_b64_decode.
BRLCAD_ADDEXEC(bu_b64 b64.c libbu TEST)
add_test(NAME "bu_b64_test" COMMAND bu_b64)


#
#  ************ rb_*.c tests *************
#
BRLCAD_ADDEXEC(bu_redblack redblack.c libbu TEST)
add_test(NAME bu_redblack_test COMMAND bu_redblack)

#
#  ************ bu_semaphore.c tests *************
#
BRLCAD_ADDEXEC(bu_semaphore semaphore.c "libbu;${WINMM_LIBRARY}" TEST)
add_test(NAME bu_semaphore_test COMMAND bu_semaphore)
add_test(NAME bu_semaphore_P1 COMMAND bu_semaphore -P1)
add_test(NAME bu_semaphore_P2 COMMAND bu_semaphore -P2)
add_test(NAME bu_semaphore_P3 COMMAND bu_semaphore -P3)

#
#  ************ bu_parallel.c tests *************
#
BRLCAD_ADDEXEC(bu_parallel parallel.c "libbu;${WINMM_LIBRARY}" TEST)
add_test(NAME bu_parallel_test COMMAND bu_parallel)

#
#  ************ vls_vprintf.c tests *************
#
BRLCAD_ADDEXEC(bu_vls_vprintf vls_vprintf.c libbu TEST)
foreach(num RANGE 1 67)
  add_test(NAME "bu_vls_vprintf_${num}" COMMAND bu_vls_vprintf "${num}")
endforeach()

#
#  ************ vls.c tests *************
#
include(tests_vls.cmake)


#
#  ************ sort.c test *************
#
BRLCAD_ADDEXEC(bu_sort sort.c libbu TEST)
add_test(NAME bu_sort_unsigned_int_1 COMMAND bu_sort 1)
add_test(NAME bu_sort_unsigned_int_2 COMMAND bu_sort 2)
add_test(NAME bu_sort_fastf_t_1 COMMAND bu_sort 3)
add_test(NAME bu_sort_fastf_t_2 COMMAND bu_sort 4)
add_test(NAME bu_sort_string_1 COMMAND bu_sort 5)
add_test(NAME bu_sort_string_2 COMMAND bu_sort 6)
add_test(NAME bu_sort_dist_to_int_1 COMMAND bu_sort 7)
add_test(NAME bu_sort_dist_to_int_2 COMMAND bu_sort 8)
add_test(NAME bu_sort_fastf_empty COMMAND bu_sort 9)
add_test(NAME bu_sort_fastf_inf COMMAND bu_sort 10)

#
#  ************ color.c tests *************

# For COMMAND bu_color, the input format is as follows:
#
# COMMAND bu_color <function number> <args>
BRLCAD_ADDEXEC(bu_color color.c libbu TEST)

# For function #1 (bu_rgb_to_hsv) the <args> format is as follows:
#
# rgb expected_hsv
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_rgb_to_hsv_1 COMMAND bu_color 1 192,78,214 290.294,0.636,0.839)

# For function #2 (bu_hsv_to_rgb) the <args> format is as follows:
#
# hsv expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_hsv_to_rgb_1 COMMAND bu_color 2 290.294,0.636,0.839 192,78,214)

# For function #3 (bu_str_to_rgb) the <args> format is as follows:
#
# string expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers.

add_test(NAME bu_str_to_rgb_1 COMMAND bu_color 3 "#C04ED6" 192,78,214)
add_test(NAME bu_str_to_rgb_2 COMMAND bu_color 3 "192/78/214" 192,78,214)
add_test(NAME bu_str_to_rgb_3 COMMAND bu_color 3 "192 78 214" 192,78,214)

# For function #4 (bu_color_to_rgb_floats) the <args> format is as follows:
#
# rgb_color
#
# Where rgb colors are represented by comma-separated triples of
# integers or floats.

add_test(NAME bu_color_to_rgb_floats_1 COMMAND bu_color 4 192,78,214)
#
# ************ str.c tests ************
#

# For COMMAND bu_str, the input format is as follows:
#
# COMMAND bu_str <function number> <args>
BRLCAD_ADDEXEC(bu_str str.c libbu TEST)

# For function #1 (bu_strlcatm) the <args> format is as follows:
#
# string1 string2 result_size expected_ret expected_result
#
# Where string1 and string2 are the strings to concatenate,
# result_size is the size for the buffer that the result will be
# stored in, expected_ret is the expected return value of the
# function, and expected_result is the expected resultant string in
# dst.

add_test(NAME bu_strlcatm_valid_1 COMMAND bu_str 1 "abc" "def" 7 6 "abcdef")
add_test(NAME bu_strlcatm_valid_2 COMMAND bu_str 1 "abc" "def" 8 6 "abcdef")
add_test(NAME bu_strlcatm_bad_size_1 COMMAND bu_str 1 "abc" "def" 0 0 "")
add_test(NAME bu_strlcatm_bad_size_2 COMMAND bu_str 1 "abc" "def" 5 4 "abcd")

# For function #2 (bu_strlcpym) the <args> format is as follows:
#
# string result_size expected_ret expected_result
#
# Where string is the string to copy, result_size is the size of the
# result buffer, expected_ret is the expected return value of the
# function, and expected_result is the expected contents of the copied
# string.

add_test(NAME bu_strlcpym_valid_1 COMMAND bu_str 2 "abcdef" 7 6 "abcdef")
add_test(NAME bu_strlcpym_valid_2 COMMAND bu_str 2 "abcdef" 8 6 "abcdef")
add_test(NAME bu_strlcpym_bad_size_1 COMMAND bu_str 2 "abcdef" 0 0 "")
add_test(NAME bu_strlcpym_bad_size_2 COMMAND bu_str 2 "abcdef" 5 4 "abcd")

# For function #3 (bu_strdupm) the <args> format is as follows:
#
# string
#
# Where string is the string to duplicate.

add_test(NAME bu_strdupm_1 COMMAND bu_str 3 "abcdef")

# For function #4 (bu_strcmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcmp_equal_1 COMMAND bu_str 4 "abcd" "abcd" 0)
add_test(NAME bu_strcmp_less_1 COMMAND bu_str 4 "a" "b" -1)
add_test(NAME bu_strcmp_less_2 COMMAND bu_str 4 "abCD" "abcd" -1)
add_test(NAME bu_strcmp_greater_1 COMMAND bu_str 4 "b" "a" 1)
add_test(NAME bu_strcmp_greater_2 COMMAND bu_str 4 "abcd" "abCD" 1)

# For function #5 (bu_strncmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncmp_equal_1 COMMAND bu_str 5 "abcd" "abcd" 4 0)
add_test(NAME bu_strncmp_equal_2 COMMAND bu_str 5 "abcd" "abcd" 3 0)
add_test(NAME bu_strncmp_equal_3 COMMAND bu_str 5 "abcd" "abcd" 5 0)
add_test(NAME bu_strncmp_equal_4 COMMAND bu_str 5 "abcd" "abc" 3 0)
add_test(NAME bu_strncmp_equal_5 COMMAND bu_str 5 "abcd" "abce" 3 0)
add_test(NAME bu_strncmp_less_1 COMMAND bu_str 5 "abcd" "abce" 4 -1)
add_test(NAME bu_strncmp_less_2 COMMAND bu_str 5 "abCD" "abcd" 4 -1)
add_test(NAME bu_strncmp_greater_1 COMMAND bu_str 5 "abcd" "abc" 4 1)
add_test(NAME bu_strncmp_greater_2 COMMAND bu_str 5 "abcd" "abCD" 4 1)

# For function #6 (bu_strcasecmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcasecmp_equal_1 COMMAND bu_str 6 "abcd" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_2 COMMAND bu_str 6 "abCD" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_3 COMMAND bu_str 6 "abcd" "abCD" 0)
add_test(NAME bu_strcasecmp_less_1 COMMAND bu_str 4 "a" "b" -1)
add_test(NAME bu_strcasecmp_greater_1 COMMAND bu_str 4 "b" "a" 1)

# For function #7 (bu_strncasecmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncasecmp_equal_1 COMMAND bu_str 7 "abcd" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_2 COMMAND bu_str 7 "abcd" "abcd" 3 0)
add_test(NAME bu_strncasecmp_equal_3 COMMAND bu_str 7 "abcd" "abcd" 5 0)
add_test(NAME bu_strncasecmp_equal_4 COMMAND bu_str 7 "abcd" "abc" 3 0)
add_test(NAME bu_strncasecmp_equal_5 COMMAND bu_str 7 "abcd" "abce" 3 0)
add_test(NAME bu_strncasecmp_equal_6 COMMAND bu_str 7 "abCD" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_7 COMMAND bu_str 7 "abcd" "abCD" 4 0)
add_test(NAME bu_strncasecmp_less_1 COMMAND bu_str 7 "abcd" "abce" 4 -1)
add_test(NAME bu_strncasecmp_greater_1 COMMAND bu_str 7 "abcd" "abc" 4 1)

#
#  *********** badmagic.c tests ************
#

BRLCAD_ADDEXEC(bu_badmagic badmagic.c libbu TEST)
add_test(NAME bu_badmagic_diff_magic  COMMAND bu_badmagic 1)
add_test(NAME bu_badmagic_normal      COMMAND bu_badmagic 2)
add_test(NAME bu_badmagic_null_ptr    COMMAND bu_badmagic 3)
add_test(NAME bu_badmagic_misaligned  COMMAND bu_badmagic 4)

#
#  *********** opt.c tests ************
#

BRLCAD_ADDEXEC(bu_opt opt.c libbu TEST)
add_test(NAME bu_opt_null  COMMAND bu_opt 0 0 0)

# Verbosity (custom handler) testing
add_test(NAME bu_opt_1_v_00  COMMAND bu_opt 1 v 0)
add_test(NAME bu_opt_1_v_01  COMMAND bu_opt 1 v 1)
add_test(NAME bu_opt_1_v_02  COMMAND bu_opt 1 v 2)
add_test(NAME bu_opt_1_v_03  COMMAND bu_opt 1 v 3)
add_test(NAME bu_opt_1_v_04  COMMAND bu_opt 1 v 4)
add_test(NAME bu_opt_1_v_05  COMMAND bu_opt 1 v 5)
add_test(NAME bu_opt_1_v_06  COMMAND bu_opt 1 v 6)
add_test(NAME bu_opt_1_v_07  COMMAND bu_opt 1 v 7)
add_test(NAME bu_opt_1_v_08  COMMAND bu_opt 1 v 8)
add_test(NAME bu_opt_1_v_09  COMMAND bu_opt 1 v 9)
add_test(NAME bu_opt_1_v_10  COMMAND bu_opt 1 v 10)
add_test(NAME bu_opt_1_v_11  COMMAND bu_opt 1 v 11)
add_test(NAME bu_opt_1_v_12  COMMAND bu_opt 1 v 12)
add_test(NAME bu_opt_1_v_13  COMMAND bu_opt 1 v 13)
add_test(NAME bu_opt_1_v_14  COMMAND bu_opt 1 v 14)

# Help option testing
add_test(NAME bu_opt_1_h_01  COMMAND bu_opt 1 h 1)
add_test(NAME bu_opt_1_h_02  COMMAND bu_opt 1 h 2)
add_test(NAME bu_opt_1_h_03  COMMAND bu_opt 1 h 3)
add_test(NAME bu_opt_1_h_04  COMMAND bu_opt 1 h 4)
add_test(NAME bu_opt_1_h_05  COMMAND bu_opt 1 h 5)
add_test(NAME bu_opt_1_h_06  COMMAND bu_opt 1 h 6)
add_test(NAME bu_opt_1_h_07  COMMAND bu_opt 1 h 7)
add_test(NAME bu_opt_1_h_08  COMMAND bu_opt 1 h 8)
add_test(NAME bu_opt_1_h_09  COMMAND bu_opt 1 h 9)
add_test(NAME bu_opt_1_h_10  COMMAND bu_opt 1 h 10)

# Boolean option testing
add_test(NAME bu_opt_1_b_01  COMMAND bu_opt 1 b 1)
add_test(NAME bu_opt_1_b_02  COMMAND bu_opt 1 b 2)
add_test(NAME bu_opt_1_b_03  COMMAND bu_opt 1 b 3)
add_test(NAME bu_opt_1_b_04  COMMAND bu_opt 1 b 4)

# String option testing
add_test(NAME bu_opt_1_s_01  COMMAND bu_opt 1 s 1)
add_test(NAME bu_opt_1_s_02  COMMAND bu_opt 1 s 2)

# Integer option testing
add_test(NAME bu_opt_1_i_01  COMMAND bu_opt 1 i 1)
add_test(NAME bu_opt_1_i_02  COMMAND bu_opt 1 i 2)
add_test(NAME bu_opt_1_i_03  COMMAND bu_opt 1 i 3)
add_test(NAME bu_opt_1_i_04  COMMAND bu_opt 1 i 4)

# Long int option testing
add_test(NAME bu_opt_1_l_01  COMMAND bu_opt 1 l 1)
add_test(NAME bu_opt_1_l_02  COMMAND bu_opt 1 l 2)

# fastf_t option testing
add_test(NAME bu_opt_1_f_01  COMMAND bu_opt 1 f 1)
add_test(NAME bu_opt_1_f_02  COMMAND bu_opt 1 f 2)
add_test(NAME bu_opt_1_f_03  COMMAND bu_opt 1 f 3)

# Multiple flag option testing
add_test(NAME bu_opt_1_mf_01  COMMAND bu_opt 1 m 1)
add_test(NAME bu_opt_1_mf_02  COMMAND bu_opt 1 m 2)

# non-static var assignment option testing (and also complex
# custom arg processing testing)
add_test(NAME bu_opt_2_color_00  COMMAND bu_opt 2 C 0)
add_test(NAME bu_opt_2_color_01  COMMAND bu_opt 2 C 1)
add_test(NAME bu_opt_2_color_02  COMMAND bu_opt 2 C 2)
add_test(NAME bu_opt_2_color_03  COMMAND bu_opt 2 C 3)
add_test(NAME bu_opt_2_color_04  COMMAND bu_opt 2 C 4)
add_test(NAME bu_opt_2_color_05  COMMAND bu_opt 2 C 5)
add_test(NAME bu_opt_2_color_06  COMMAND bu_opt 2 C 6)
add_test(NAME bu_opt_2_color_07  COMMAND bu_opt 2 C 7)
add_test(NAME bu_opt_2_color_08  COMMAND bu_opt 2 C 8)
add_test(NAME bu_opt_2_color_09  COMMAND bu_opt 2 C 9)
add_test(NAME bu_opt_2_color_10  COMMAND bu_opt 2 C 10)
add_test(NAME bu_opt_2_color_11  COMMAND bu_opt 2 C 11)
add_test(NAME bu_opt_2_color_12  COMMAND bu_opt 2 C 12)
add_test(NAME bu_opt_2_color_13  COMMAND bu_opt 2 C 13)
add_test(NAME bu_opt_2_color_14  COMMAND bu_opt 2 C 14)
add_test(NAME bu_opt_2_color_15  COMMAND bu_opt 2 C 15)

# vector tests
add_test(NAME bu_opt_3_vect_00  COMMAND bu_opt 3 V 0)
add_test(NAME bu_opt_3_vect_01  COMMAND bu_opt 3 V 1)
add_test(NAME bu_opt_3_vect_02  COMMAND bu_opt 3 V 2)
add_test(NAME bu_opt_3_vect_03  COMMAND bu_opt 3 V 3)
add_test(NAME bu_opt_3_vect_04  COMMAND bu_opt 3 V 4)

#
#  *********** datetime.c tests ************
#

BRLCAD_ADDEXEC(bu_datetime datetime.c libbu TEST)
foreach(num RANGE 0 10)
  add_test(NAME "bu_datetime${num}" COMMAND bu_datetime "${num}")
endforeach()

#
#  *********** hash.c tests ************
#

BRLCAD_ADDEXEC(bu_hash hash.cpp libbu TEST)
add_test(NAME bu_hash_noop         COMMAND bu_hash 0)
add_test(NAME bu_hash_one_entry    COMMAND bu_hash 1)
add_test(NAME bu_hash_lorem_ipsum  COMMAND bu_hash 2)

#
#  *********** humanize_number.c tests ************
#

BRLCAD_ADDEXEC(bu_humanize_number humanize_number.c libbu TEST)
add_test(NAME bu_humanize_number_BSD COMMAND bu_humanize_number)

#
#  *********** uuid.c tests ************
#

BRLCAD_ADDEXEC(bu_uuid uuid.c libbu TEST)
add_test(NAME bu_uuid_encode COMMAND bu_uuid)

#add_test(NAME bu_uuid_create  COMMAND bu_uuid 0)
#add_test(NAME bu_uuid_compare COMMAND bu_uuid 1)
#add_test(NAME bu_uuid_encode  COMMAND bu_uuid 2)


CMAKEFILES(
  tests_bitv.cmake
  tests_vls.cmake
)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
