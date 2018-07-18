# To minimize the number of build targets and binaries that are created, we
# combine some of the unit tests into a single program.

set(bu_test_srcs
  b64.c
  badmagic.c
  basename.c
  bitv.c
  booleanize.c
  color.c
  datetime.c
  dirname.c
  encode.c
  escape.c
  file_mime.c
  heap.c
  humanize_number.c
  vls_incr.c
  vls_simplify.c
  opt.c
  parallel.c
  path_component.c
  ptbl.c
  redblack.c
  semaphore.c
  sort.c
  str.c
  str_isprint.c
  uuid.c
  vls.c
  vls_vprintf.c
  )

# Generate and assemble the necessary per-test-type source code
set(BU_TEST_SRC_INCLUDES)
set(BU_TEST_SRC_SWITCHES)
set(BU_TEST_LIST)
foreach(srcfile ${bu_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BU_TEST_SRC_INCLUDES "${BU_TEST_SRC_INCLUDES}\n#include \"libbu/tests/${srcfile}\"")
  set(BU_TEST_SRC_SWITCHES "${BU_TEST_SRC_SWITCHES}\n	if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(BU_TEST_LIST "${BU_TEST_LIST}\\n	${NAME_ROOT}")
endforeach(srcfile ${bu_test_srcs})

# Generate bu_test.c file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bu_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bu_test.c" @ONLY)
BRLCAD_ADDEXEC(bu_test "${CMAKE_CURRENT_BINARY_DIR}/bu_test.c" libbu TEST)

# Housekeeping for included source files
CMAKEFILES(${bu_test_srcs})
VALIDATE_STYLE("${bu_test_srcs}" bu_test)

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
add_test(NAME bu_basename_null                                   COMMAND bu_test basename) # tests NULL
add_test(NAME bu_basename_empty                                  COMMAND bu_test basename "")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_file                  COMMAND bu_test basename "/usr/dir/file")
add_test(NAME bu_basename_sl_usr_sl_dir_sl_                      COMMAND bu_test basename "/usr/dir/")
add_test(NAME bu_basename_sl_usr_bsl__sl_dir                     COMMAND bu_test basename "/usr\\\\/dir")
add_test(NAME bu_basename_sl_usr_sl__p_                          COMMAND bu_test basename "/usr/.")
add_test(NAME bu_basename_sl_usr_sl_                             COMMAND bu_test basename "/usr/")
add_test(NAME bu_basename_sl_usr                                 COMMAND bu_test basename "/usr")
add_test(NAME bu_basename_usr                                    COMMAND bu_test basename "usr")
add_test(NAME bu_basename_sl_usr_sl_some_sp_long_sl_file         COMMAND bu_test basename "/usr/some long/file")
add_test(NAME bu_basename_sl_usr_sl_some_sp_file                 COMMAND bu_test basename "/usr/some file")
add_test(NAME bu_basename_C_c__sl_usr_sl_some_bsl__sp_drivepath  COMMAND bu_test basename "C:/usr/some\\\\ drivepath")
add_test(NAME bu_basename_sl_a_sp_test_sp_file                   COMMAND bu_test basename "/a test file")
add_test(NAME bu_basename_another_sp_file                        COMMAND bu_test basename "another file")
add_test(NAME bu_basename_C_c__bsl__Temp                         COMMAND bu_test basename "C:\\\\Temp")
add_test(NAME bu_basename_C_c__sl_Temp                           COMMAND bu_test basename "C:/Temp")
add_test(NAME bu_basename_sl_                                    COMMAND bu_test basename "/")
add_test(NAME bu_basename_sl__sl__sl__sl__sl_                    COMMAND bu_test basename "/////")
add_test(NAME bu_basename_p_                                     COMMAND bu_test basename ".")
add_test(NAME bu_basename_p__p_                                  COMMAND bu_test basename "..")
add_test(NAME bu_basename_p__p__p_                               COMMAND bu_test basename "...")
add_test(NAME bu_basename_sp__sp__sp_                            COMMAND bu_test basename "   ")


##########################################
#      bu_file_path_component testing    #
##########################################
add_test(NAME bu_path_component_null                               COMMAND bu_test path_component) # tests NULL
add_test(NAME bu_path_component_empty                              COMMAND bu_test path_component 0) # tests empty

# NOTE: these test numbers correspond with the bu_path_component_t enum values in include/bu/path.h
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_ext          COMMAND bu_test path_component "/usr/dir/file.ext" "ext" 4)
add_test(NAME bu_path_component_c_sl_usr_sl_dir_sl_file_ext        COMMAND bu_test path_component "C:/usr/dir/file.ext" "ext" 4)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_base         COMMAND bu_test path_component "/usr/dir/file.ext" "file.ext" 2)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_base_core    COMMAND bu_test path_component "/usr/dir/file.ext" "file" 3)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_dir          COMMAND bu_test path_component "/usr/dir/file.ext" "/usr/dir" 0)
add_test(NAME bu_path_component_sl_usr_sl_dir_sl_file_dir_core     COMMAND bu_test path_component "/usr/dir/file.ext" "/usr/dir/file" 1)
add_test(NAME bu_path_component_c_sl_usr_sl_dir_sl_file_dir_core   COMMAND bu_test path_component "C:/usr/dir/file.ext" "C:/usr/dir/file" 1)


################################
#      bu_file_mime testing    #
################################

# NOTE: these test numbers correspond with the bu_mime_t enum values in include/bu/mime.h
add_test(NAME bu_file_mime_dxf   COMMAND bu_test file_mime dxf 5 1) # tests dxf extension in model context
add_test(NAME bu_file_mime_stp   COMMAND bu_test file_mime stp 5 7) # tests stp extension in model context
add_test(NAME bu_file_mime_png   COMMAND bu_test file_mime png 3 13) # tests png extension in image context

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
# expected returns are false:0, strongly true:1, or anything else:>1
add_test(NAME bu_booleanize_nullptr      COMMAND bu_test booleanize) # tests NULL
add_test(NAME bu_booleanize_nullstr      COMMAND bu_test booleanize "(null)" 0)
add_test(NAME bu_booleanize_empty        COMMAND bu_test booleanize "" 0)
add_test(NAME bu_booleanize_space        COMMAND bu_test booleanize " " 0)
add_test(NAME bu_booleanize_n            COMMAND bu_test booleanize "n" 0)
add_test(NAME bu_booleanize_nabcd        COMMAND bu_test booleanize "nabcd" 2)
add_test(NAME bu_booleanize_N            COMMAND bu_test booleanize "N" 0)
add_test(NAME bu_booleanize_Nabcd        COMMAND bu_test booleanize "Nabcd" 2)
add_test(NAME bu_booleanize__sp__sp_abcd COMMAND bu_test booleanize "  abcd" 2)
add_test(NAME bu_booleanize_y            COMMAND bu_test booleanize "y" 1)
add_test(NAME bu_booleanize_yabcd        COMMAND bu_test booleanize "yabcd" 2)
add_test(NAME bu_booleanize_Y            COMMAND bu_test booleanize "Y" 1)
add_test(NAME bu_booleanize_Yabcd        COMMAND bu_test booleanize "Yabcd" 2)
add_test(NAME bu_booleanize_abcd         COMMAND bu_test booleanize "abcd" 2)
add_test(NAME bu_booleanize_neg1         COMMAND bu_test booleanize "-1" 2)
add_test(NAME bu_booleanize_1            COMMAND bu_test booleanize "1" 1)
add_test(NAME bu_booleanize_2            COMMAND bu_test booleanize "2" 2)
add_test(NAME bu_booleanize_0            COMMAND bu_test booleanize "0" 0)
add_test(NAME bu_booleanize_neg0         COMMAND bu_test booleanize "-0" 0)
add_test(NAME bu_booleanize_0000         COMMAND bu_test booleanize "0000" 0)
add_test(NAME bu_booleanize_0001         COMMAND bu_test booleanize "0001" 1)

#
#  ************ ctype.c tests *************
#

##################################
#      bu_str_isprint testing    #
##################################
# Note - because of the special characters needed in these tests, the strings
# don't work well when fed in from the command line.
add_test(NAME bu_str_isprint_basic                COMMAND bu_test str_isprint 1)
add_test(NAME bu_str_isprint_eol                  COMMAND bu_test str_isprint 2)
add_test(NAME bu_str_isprint_double_slash         COMMAND bu_test str_isprint 3)
add_test(NAME bu_str_isprint_horiz_tab            COMMAND bu_test str_isprint 4)
add_test(NAME bu_str_isprint_non-alphanumeric     COMMAND bu_test str_isprint 5)
add_test(NAME bu_str_isprint_eol_non-alphanumeric COMMAND bu_test str_isprint 6)
add_test(NAME bu_str_isprint_carriage_return      COMMAND bu_test str_isprint 7)

#
#  ************ dirname.c tests *************
#

##############################
#      bu_dirname testing    #
##############################
add_test(NAME bu_dirname__usr_dir_file COMMAND bu_test dirname "/usr/dir/file")
add_test(NAME bu_dirname__usr_dir_ COMMAND bu_test dirname "/usr/dir/")
add_test(NAME bu_dirname__usr__bsl__dir COMMAND bu_test dirname "/usr\\\\/dir")
add_test(NAME bu_dirname__usr_period COMMAND bu_test dirname "/usr/.")
add_test(NAME bu_dirname__usr_ COMMAND bu_test dirname "/usr/")
add_test(NAME bu_dirname__usr COMMAND bu_test dirname "/usr")
add_test(NAME bu_dirname_usr COMMAND bu_test dirname "usr")
add_test(NAME bu_dirname__usr_some_sp_long_file COMMAND bu_test dirname "/usr/some long/file")
add_test(NAME bu_dirname__usr_some_sp_file COMMAND bu_test dirname "/usr/some file")
add_test(NAME bu_dirname__usr_some__bsl__sp_drivepath COMMAND bu_test dirname "C:/usr/some\\\\ drivepath")
add_test(NAME bu_dirname__a_sp_file COMMAND bu_test dirname "/a test file")
add_test(NAME bu_dirname_another_file COMMAND bu_test dirname "another file")
add_test(NAME bu_dirname_C_colon__bsl_Temp COMMAND bu_test dirname "C:\\\\Temp")
add_test(NAME bu_dirname_C_colon__Temp COMMAND bu_test dirname "C:/Temp")
add_test(NAME bu_dirname__ COMMAND bu_test dirname "/")
add_test(NAME bu_dirname______ COMMAND bu_test dirname "/////")
add_test(NAME bu_dirname__period_ COMMAND bu_test dirname ".")
add_test(NAME bu_dirname__period__period_ COMMAND bu_test dirname "..")
add_test(NAME bu_dirname__period__period__period_ COMMAND bu_test dirname "...")
add_test(NAME bu_dirname__sp__sp__sp_ COMMAND bu_test dirname "   ")
add_test(NAME bu_dirname_empty COMMAND bu_test dirname "")
add_test(NAME bu_dirname_null COMMAND bu_test dirname)

#
#  ************ escape.c tests *************
#
# Note - because of the quoting needed in these tests, it is simpler
# to leave the input strings in the C code.  Individual tests are triggered
# by number

###################################
#      bu_str_unescape testing    #
###################################
foreach(num RANGE 1 23)
  add_test(NAME "bu_str_unescape_${num}" COMMAND bu_test escape 1 "${num}")
endforeach()

#################################
#      bu_str_escape testing    #
#################################
foreach(num RANGE 1 31)
  add_test(NAME "bu_str_escape_${num}" COMMAND bu_test escape 2 "${num}")
endforeach()

##############################################
#      escape/unescape round trip testing    #
##############################################
foreach(num RANGE 1 4)
  add_test(NAME "bu_escape_roundtrip_${num}" COMMAND bu_test escape 3 "${num}")
endforeach()

###
# bu_heap memory allocation testing
###
add_test(NAME bu_heap_1 COMMAND bu_test heap)

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
foreach(num RANGE 1 23)
  add_test(NAME "bu_encode_${num}" COMMAND bu_test encode "${num}")
endforeach()

#
#  ************ b64.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_b64_encode and bu_b64_decode.
add_test(NAME "bu_b64_test" COMMAND bu_test b64)


#
#  ************ rb_*.c tests *************
#
add_test(NAME bu_redblack_test COMMAND bu_test redblack)

#
#  ************ bu_semaphore.c tests *************
#
add_test(NAME bu_semaphore_test COMMAND bu_test semaphore)
add_test(NAME bu_semaphore_P1 COMMAND bu_test semaphore -P1)
add_test(NAME bu_semaphore_P2 COMMAND bu_test semaphore -P2)
add_test(NAME bu_semaphore_P3 COMMAND bu_test semaphore -P3)

#
#  ************ bu_parallel.c tests *************
#
add_test(NAME bu_parallel_test COMMAND bu_test parallel)

#
#  ************ vls_vprintf.c tests *************
#
foreach(num RANGE 1 67)
  add_test(NAME "bu_vls_vprintf_${num}" COMMAND bu_test vls_vprintf "${num}")
endforeach()

#
#  ************ vls.c tests *************
#
include(tests_vls.cmake)


#
#  ************ sort.c test *************
#
add_test(NAME bu_sort_unsigned_int_1 COMMAND bu_test sort 1)
add_test(NAME bu_sort_unsigned_int_2 COMMAND bu_test sort 2)
add_test(NAME bu_sort_fastf_t_1 COMMAND bu_test sort 3)
add_test(NAME bu_sort_fastf_t_2 COMMAND bu_test sort 4)
add_test(NAME bu_sort_string_1 COMMAND bu_test sort 5)
add_test(NAME bu_sort_string_2 COMMAND bu_test sort 6)
add_test(NAME bu_sort_dist_to_int_1 COMMAND bu_test sort 7)
add_test(NAME bu_sort_dist_to_int_2 COMMAND bu_test sort 8)
add_test(NAME bu_sort_fastf_empty COMMAND bu_test sort 9)
add_test(NAME bu_sort_fastf_inf COMMAND bu_test sort 10)

#
#  ************ color.c tests *************

# For COMMAND bu_test color, the input format is as follows:
#
# COMMAND bu_test color <function number> <args>

# For function #1 (bu_rgb_to_hsv) the <args> format is as follows:
#
# rgb expected_hsv
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_rgb_to_hsv_1 COMMAND bu_test color 1 192,78,214 290.294,0.636,0.839)
add_test(NAME bu_rgb_to_hsv_2 COMMAND bu_test color 1 0,0,0 0.0,0.0,0.0)
add_test(NAME bu_rgb_to_hsv_3 COMMAND bu_test color 1 255,255,255 0.0,0.0,1.0)
add_test(NAME bu_rgb_to_hsv_4 COMMAND bu_test color 1 255,0,0 0.0,1.0,1.0)
add_test(NAME bu_rgb_to_hsv_5 COMMAND bu_test color 1 0,255,0 120.0,1.0,1.0)
add_test(NAME bu_rgb_to_hsv_6 COMMAND bu_test color 1 0,0,255 240.0,1.0,1.0)
add_test(NAME bu_rgb_to_hsv_7 COMMAND bu_test color 1 128,128,128 0.0,0.0,0.501)

# For function #2 (bu_hsv_to_rgb) the <args> format is as follows:
#
# hsv expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(NAME bu_hsv_to_rgb_1 COMMAND bu_test color 2 290.294,0.636,0.839 192,78,214)
add_test(NAME bu_hsv_to_rgb_2 COMMAND bu_test color 2 0.0,0.0,0.0 0,0,0)
add_test(NAME bu_hsv_to_rgb_3 COMMAND bu_test color 2 0.0,0.0,1.0 255,255,255)
add_test(NAME bu_hsv_to_rgb_4 COMMAND bu_test color 2 0.0,1.0,1.0 255,0,0)
add_test(NAME bu_hsv_to_rgb_5 COMMAND bu_test color 2 120.0,1.0,1.0 0,255,0)
add_test(NAME bu_hsv_to_rgb_6 COMMAND bu_test color 2 240.0,1.0,1.0 0,0,255)
add_test(NAME bu_hsv_to_rgb_7 COMMAND bu_test color 2 0.0,0.0,0.501 128,128,128)

# For function #3 (bu_str_to_rgb) the <args> format is as follows:
#
# string expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers.

add_test(NAME bu_str_to_rgb_1 COMMAND bu_test color 3 "#C04ED6" 192,78,214)
add_test(NAME bu_str_to_rgb_2 COMMAND bu_test color 3 "192/78/214" 192,78,214)
add_test(NAME bu_str_to_rgb_3 COMMAND bu_test color 3 "192 78 214" 192,78,214)

# For function #4 (bu_color_to_rgb_floats) the <args> format is as follows:
#
# rgb_color
#
# Where rgb colors are represented by comma-separated triples of
# integers or floats.

add_test(NAME bu_color_to_rgb_floats_1 COMMAND bu_test color 4 192,78,214)

#
# ************ str.c tests ************
#

# For COMMAND bu_test str, the input format is as follows:
#
# COMMAND bu_test str <function number> <args>

# For function #1 (bu_strlcatm) the <args> format is as follows:
#
# string1 string2 result_size expected_ret expected_result
#
# Where string1 and string2 are the strings to concatenate,
# result_size is the size for the buffer that the result will be
# stored in, expected_ret is the expected return value of the
# function, and expected_result is the expected resultant string in
# dst.

add_test(NAME bu_strlcatm_valid_1 COMMAND bu_test str 1 "abc" "def" 7 6 "abcdef")
add_test(NAME bu_strlcatm_valid_2 COMMAND bu_test str 1 "abc" "def" 8 6 "abcdef")
add_test(NAME bu_strlcatm_bad_size_1 COMMAND bu_test str 1 "abc" "def" 0 0 "")
add_test(NAME bu_strlcatm_bad_size_2 COMMAND bu_test str 1 "abc" "def" 5 4 "abcd")

# For function #2 (bu_strlcpym) the <args> format is as follows:
#
# string result_size expected_ret expected_result
#
# Where string is the string to copy, result_size is the size of the
# result buffer, expected_ret is the expected return value of the
# function, and expected_result is the expected contents of the copied
# string.

add_test(NAME bu_strlcpym_valid_1 COMMAND bu_test str 2 "abcdef" 7 6 "abcdef")
add_test(NAME bu_strlcpym_valid_2 COMMAND bu_test str 2 "abcdef" 8 6 "abcdef")
add_test(NAME bu_strlcpym_bad_size_1 COMMAND bu_test str 2 "abcdef" 0 0 "")
add_test(NAME bu_strlcpym_bad_size_2 COMMAND bu_test str 2 "abcdef" 5 4 "abcd")

# For function #3 (bu_strdupm) the <args> format is as follows:
#
# string
#
# Where string is the string to duplicate.

add_test(NAME bu_strdupm_1 COMMAND bu_test str 3 "abcdef")

# For function #4 (bu_strcmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcmp_equal_1 COMMAND bu_test str 4 "abcd" "abcd" 0)
add_test(NAME bu_strcmp_less_1 COMMAND bu_test str 4 "a" "b" -1)
add_test(NAME bu_strcmp_less_2 COMMAND bu_test str 4 "abCD" "abcd" -1)
add_test(NAME bu_strcmp_greater_1 COMMAND bu_test str 4 "b" "a" 1)
add_test(NAME bu_strcmp_greater_2 COMMAND bu_test str 4 "abcd" "abCD" 1)

# For function #5 (bu_strncmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncmp_equal_1 COMMAND bu_test str 5 "abcd" "abcd" 4 0)
add_test(NAME bu_strncmp_equal_2 COMMAND bu_test str 5 "abcd" "abcd" 3 0)
add_test(NAME bu_strncmp_equal_3 COMMAND bu_test str 5 "abcd" "abcd" 5 0)
add_test(NAME bu_strncmp_equal_4 COMMAND bu_test str 5 "abcd" "abc" 3 0)
add_test(NAME bu_strncmp_equal_5 COMMAND bu_test str 5 "abcd" "abce" 3 0)
add_test(NAME bu_strncmp_less_1 COMMAND bu_test str 5 "abcd" "abce" 4 -1)
add_test(NAME bu_strncmp_less_2 COMMAND bu_test str 5 "abCD" "abcd" 4 -1)
add_test(NAME bu_strncmp_greater_1 COMMAND bu_test str 5 "abcd" "abc" 4 1)
add_test(NAME bu_strncmp_greater_2 COMMAND bu_test str 5 "abcd" "abCD" 4 1)

# For function #6 (bu_strcasecmp) the <args> format is as follows:
#
# string1 string2 expected_ret
#
# Where string1 and string2 are the strings to compare, and
# expected_ret is a number of the same sign as the expected comparison
# result.

add_test(NAME bu_strcasecmp_equal_1 COMMAND bu_test str 6 "abcd" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_2 COMMAND bu_test str 6 "abCD" "abcd" 0)
add_test(NAME bu_strcasecmp_equal_3 COMMAND bu_test str 6 "abcd" "abCD" 0)
add_test(NAME bu_strcasecmp_less_1 COMMAND bu_test str 4 "a" "b" -1)
add_test(NAME bu_strcasecmp_greater_1 COMMAND bu_test str 4 "b" "a" 1)

# For function #7 (bu_strncasecmp) the <args> format is as follows:
#
# string1 string2 n expected_ret
#
# Where string1 and string2 are the strings to compare, n is the
# length to give strncmp, and expected_ret is a number of the same
# sign as the expected comparison result.

add_test(NAME bu_strncasecmp_equal_1 COMMAND bu_test str 7 "abcd" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_2 COMMAND bu_test str 7 "abcd" "abcd" 3 0)
add_test(NAME bu_strncasecmp_equal_3 COMMAND bu_test str 7 "abcd" "abcd" 5 0)
add_test(NAME bu_strncasecmp_equal_4 COMMAND bu_test str 7 "abcd" "abc" 3 0)
add_test(NAME bu_strncasecmp_equal_5 COMMAND bu_test str 7 "abcd" "abce" 3 0)
add_test(NAME bu_strncasecmp_equal_6 COMMAND bu_test str 7 "abCD" "abcd" 4 0)
add_test(NAME bu_strncasecmp_equal_7 COMMAND bu_test str 7 "abcd" "abCD" 4 0)
add_test(NAME bu_strncasecmp_less_1 COMMAND bu_test str 7 "abcd" "abce" 4 -1)
add_test(NAME bu_strncasecmp_greater_1 COMMAND bu_test str 7 "abcd" "abc" 4 1)

#
#  *********** badmagic.c tests ************
#

add_test(NAME bu_badmagic_diff_magic  COMMAND bu_test badmagic 1)
add_test(NAME bu_badmagic_normal      COMMAND bu_test badmagic 2)
add_test(NAME bu_badmagic_null_ptr    COMMAND bu_test badmagic 3)
add_test(NAME bu_badmagic_misaligned  COMMAND bu_test badmagic 4)

#
#  *********** vls_incr.c tests ************
#

add_test(NAME bu_vls_incr_1  COMMAND bu_test vls_incr "test1.r"      0 "NULL"        1  "test2.r")
add_test(NAME bu_vls_incr_2  COMMAND bu_test vls_incr "test1.r"      0 "0:0:0:0"    10  "test11.r")
add_test(NAME bu_vls_incr_3  COMMAND bu_test vls_incr "test1_1.r"    0 "0:0:0:0"     2  "test1_3.r")
add_test(NAME bu_vls_incr_4  COMMAND bu_test vls_incr "test-1.r"     0 "0:0:0:0"     1  "test-2.r")
add_test(NAME bu_vls_incr_5  COMMAND bu_test vls_incr "test1.r"      0 "3:0:0:0"     1  "test002.r")
add_test(NAME bu_vls_incr_6  COMMAND bu_test vls_incr "test1.r"      0 "3:5:0:0"     1  "test005.r")
add_test(NAME bu_vls_incr_7  COMMAND bu_test vls_incr "test5.r"      0 "3:5:0:0"     1  "test006.r")
add_test(NAME bu_vls_incr_8  COMMAND bu_test vls_incr "test8.r"      0 "0:0:9:0"     2  "test0.r")
add_test(NAME bu_vls_incr_9  COMMAND bu_test vls_incr "test1.r"      0 "0:0:0:2"     2  "test5.r")
add_test(NAME bu_vls_incr_10 COMMAND bu_test vls_incr "test90.r"     0 "4:20:99:5"   2  "test0020.r")
add_test(NAME bu_vls_incr_11 COMMAND bu_test vls_incr "test_1_obj.r" 1 "0:0:0:0:-:-" 1  "test-2-obj.r")
add_test(NAME bu_vls_incr_12 COMMAND bu_test vls_incr "test.r"       1 "3:0:0:0:-"   5  "test.r-005")
add_test(NAME bu_vls_incr_13 COMMAND bu_test vls_incr "test1"        0 "NULL"        1  "test2")
add_test(NAME bu_vls_incr_14 COMMAND bu_test vls_incr "test20"       0 "NULL"      100  "test120")
add_test(NAME bu_vls_incr_15 COMMAND bu_test vls_incr "test"         0 "NULL"        1  "test1")
add_test(NAME bu_vls_incr_16 COMMAND bu_test vls_incr "test"         0 "0:0:0:0:-"   1  "test-1")

BRLCAD_ADDEXEC(bu_vls_incr_uniq vls_incr_uniq.cpp libbu TEST)
add_test(NAME bu_vls_incr_uniq_1  COMMAND bu_vls_incr_uniq  "test.r1" "test.r3")
add_test(NAME bu_vls_incr_uniq_2  COMMAND bu_vls_incr_uniq  "test.r2" "test.r3")

#
#  *********** vls_simlify.c tests ************
#

add_test(NAME bu_vls_simplify_1  COMMAND bu_test vls_simplify "^%test1@@%"    "__test1___")
add_test(NAME bu_vls_simplify_2  COMMAND bu_test vls_simplify "^%test1@@%"    "_test1_"     ""   "_")
add_test(NAME bu_vls_simplify_3  COMMAND bu_test vls_simplify "^%test1@@%"    "_test1@@_"   "@"  "_")
add_test(NAME bu_vls_simplify_4  COMMAND bu_test vls_simplify "^%test1.s@@%"  "_test1_s_"   ""   "_")
add_test(NAME bu_vls_simplify_5  COMMAND bu_test vls_simplify "^%test1.s@@%"  "_test1.s_"   "."  "_")
add_test(NAME bu_vls_simplify_6  COMMAND bu_test vls_simplify "^%test1.s@@%"  "test1.s"     "."  "_"  "_")
add_test(NAME bu_vls_simplify_7  COMMAND bu_test vls_simplify "^%test1.s@@%"  "%test1.s_%"  ".%" "_"  "_")
add_test(NAME bu_vls_simplify_8  COMMAND bu_test vls_simplify "^%test1.s@@%"  "test1.s"     ".%" "_"  "%_")
add_test(NAME bu_vls_simplify_9  COMMAND bu_test vls_simplify "^%test1.s@@%"  "test1.s@"    ".@" "_@" "_")
add_test(NAME bu_vls_simplify_10 COMMAND bu_test vls_simplify "^%test1.s@@%"  "test1.s"     ".@" "_@" "@_")
add_test(NAME bu_vls_simplify_11 COMMAND bu_test vls_simplify "^%test@1.s@@%" "test@1.s"    ".@" "_@" "@_")

#
#  *********** opt.c tests ************
#

add_test(NAME bu_opt_null  COMMAND bu_test opt 0 0 0)

# Verbosity (custom handler) testing
add_test(NAME bu_opt_1_v_00  COMMAND bu_test opt 1 v 0)
add_test(NAME bu_opt_1_v_01  COMMAND bu_test opt 1 v 1)
add_test(NAME bu_opt_1_v_02  COMMAND bu_test opt 1 v 2)
add_test(NAME bu_opt_1_v_03  COMMAND bu_test opt 1 v 3)
add_test(NAME bu_opt_1_v_04  COMMAND bu_test opt 1 v 4)
add_test(NAME bu_opt_1_v_05  COMMAND bu_test opt 1 v 5)
add_test(NAME bu_opt_1_v_06  COMMAND bu_test opt 1 v 6)
add_test(NAME bu_opt_1_v_07  COMMAND bu_test opt 1 v 7)
add_test(NAME bu_opt_1_v_08  COMMAND bu_test opt 1 v 8)
add_test(NAME bu_opt_1_v_09  COMMAND bu_test opt 1 v 9)
add_test(NAME bu_opt_1_v_10  COMMAND bu_test opt 1 v 10)
add_test(NAME bu_opt_1_v_11  COMMAND bu_test opt 1 v 11)
add_test(NAME bu_opt_1_v_12  COMMAND bu_test opt 1 v 12)
add_test(NAME bu_opt_1_v_13  COMMAND bu_test opt 1 v 13)
add_test(NAME bu_opt_1_v_14  COMMAND bu_test opt 1 v 14)

# Help option testing
add_test(NAME bu_opt_1_h_01  COMMAND bu_test opt 1 h 1)
add_test(NAME bu_opt_1_h_02  COMMAND bu_test opt 1 h 2)
add_test(NAME bu_opt_1_h_03  COMMAND bu_test opt 1 h 3)
add_test(NAME bu_opt_1_h_04  COMMAND bu_test opt 1 h 4)
add_test(NAME bu_opt_1_h_05  COMMAND bu_test opt 1 h 5)
add_test(NAME bu_opt_1_h_06  COMMAND bu_test opt 1 h 6)
add_test(NAME bu_opt_1_h_07  COMMAND bu_test opt 1 h 7)
add_test(NAME bu_opt_1_h_08  COMMAND bu_test opt 1 h 8)
add_test(NAME bu_opt_1_h_09  COMMAND bu_test opt 1 h 9)
add_test(NAME bu_opt_1_h_10  COMMAND bu_test opt 1 h 10)

# Boolean option testing
add_test(NAME bu_opt_1_b_01  COMMAND bu_test opt 1 b 1)
add_test(NAME bu_opt_1_b_02  COMMAND bu_test opt 1 b 2)
add_test(NAME bu_opt_1_b_03  COMMAND bu_test opt 1 b 3)
add_test(NAME bu_opt_1_b_04  COMMAND bu_test opt 1 b 4)

# String option testing
add_test(NAME bu_opt_1_s_01  COMMAND bu_test opt 1 s 1)
add_test(NAME bu_opt_1_s_02  COMMAND bu_test opt 1 s 2)

# Integer option testing
add_test(NAME bu_opt_1_i_01  COMMAND bu_test opt 1 i 1)
add_test(NAME bu_opt_1_i_02  COMMAND bu_test opt 1 i 2)
add_test(NAME bu_opt_1_i_03  COMMAND bu_test opt 1 i 3)
add_test(NAME bu_opt_1_i_04  COMMAND bu_test opt 1 i 4)

# Long int option testing
add_test(NAME bu_opt_1_l_01  COMMAND bu_test opt 1 l 1)
add_test(NAME bu_opt_1_l_02  COMMAND bu_test opt 1 l 2)

# fastf_t option testing
add_test(NAME bu_opt_1_f_01  COMMAND bu_test opt 1 f 1)
add_test(NAME bu_opt_1_f_02  COMMAND bu_test opt 1 f 2)
add_test(NAME bu_opt_1_f_03  COMMAND bu_test opt 1 f 3)

# Multiple flag option testing
add_test(NAME bu_opt_1_mf_01  COMMAND bu_test opt 1 m 1)
add_test(NAME bu_opt_1_mf_02  COMMAND bu_test opt 1 m 2)

# non-static var assignment option testing (and also complex
# custom arg processing testing)
add_test(NAME bu_opt_2_color_00  COMMAND bu_test opt 2 C 0)
add_test(NAME bu_opt_2_color_01  COMMAND bu_test opt 2 C 1)
add_test(NAME bu_opt_2_color_02  COMMAND bu_test opt 2 C 2)
add_test(NAME bu_opt_2_color_03  COMMAND bu_test opt 2 C 3)
add_test(NAME bu_opt_2_color_04  COMMAND bu_test opt 2 C 4)
add_test(NAME bu_opt_2_color_05  COMMAND bu_test opt 2 C 5)
add_test(NAME bu_opt_2_color_06  COMMAND bu_test opt 2 C 6)
add_test(NAME bu_opt_2_color_07  COMMAND bu_test opt 2 C 7)
add_test(NAME bu_opt_2_color_08  COMMAND bu_test opt 2 C 8)
add_test(NAME bu_opt_2_color_09  COMMAND bu_test opt 2 C 9)
add_test(NAME bu_opt_2_color_10  COMMAND bu_test opt 2 C 10)
add_test(NAME bu_opt_2_color_11  COMMAND bu_test opt 2 C 11)
add_test(NAME bu_opt_2_color_12  COMMAND bu_test opt 2 C 12)
add_test(NAME bu_opt_2_color_13  COMMAND bu_test opt 2 C 13)
add_test(NAME bu_opt_2_color_14  COMMAND bu_test opt 2 C 14)
add_test(NAME bu_opt_2_color_15  COMMAND bu_test opt 2 C 15)

# vector tests
add_test(NAME bu_opt_3_vect_00  COMMAND bu_test opt 3 V 0)
add_test(NAME bu_opt_3_vect_01  COMMAND bu_test opt 3 V 1)
add_test(NAME bu_opt_3_vect_02  COMMAND bu_test opt 3 V 2)
add_test(NAME bu_opt_3_vect_03  COMMAND bu_test opt 3 V 3)
add_test(NAME bu_opt_3_vect_04  COMMAND bu_test opt 3 V 4)

#
#  *********** datetime.c tests ************
#

foreach(num RANGE 0 10)
  add_test(NAME "bu_datetime${num}" COMMAND bu_test datetime "${num}")
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

add_test(NAME bu_humanize_number_BSD COMMAND bu_test humanize_number)

#
#  *********** uuid.c tests ************
#

add_test(NAME bu_uuid_encode COMMAND bu_test uuid)

#add_test(NAME bu_uuid_create  COMMAND bu_test uuid 0)
#add_test(NAME bu_uuid_compare COMMAND bu_test uuid 1)
#add_test(NAME bu_uuid_encode  COMMAND bu_test uuid 2)

#
#  *********** ptbl.c tests ************
#

add_test(NAME bu_ptbl_init_default COMMAND bu_test ptbl init)
add_test(NAME bu_ptbl_init_0 COMMAND bu_test ptbl init 0)
add_test(NAME bu_ptbl_init_10000 COMMAND bu_test ptbl init 10000)

add_test(NAME bu_ptbl_reset COMMAND bu_test ptbl reset)
add_test(NAME bu_ptbl_zero COMMAND bu_test ptbl zero)

add_test(NAME bu_ptbl_ins COMMAND bu_test ptbl ins)
add_test(NAME bu_ptbl_ins_uniq COMMAND bu_test ptbl ins uniq)
add_test(NAME bu_ptbl_locate COMMAND bu_test ptbl locate)

add_test(NAME bu_ptbl_rm_cons COMMAND bu_test ptbl rm cons)
add_test(NAME bu_ptbl_rm_mix COMMAND bu_test ptbl rm mix)

add_test(NAME bu_ptbl_cat COMMAND bu_test ptbl cat)
add_test(NAME bu_ptbl_cat_uniq COMMAND bu_test ptbl cat uniq)

add_test(NAME bu_ptbl_trunc COMMAND bu_test ptbl trunc)

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
