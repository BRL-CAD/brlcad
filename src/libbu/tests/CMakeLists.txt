BRLCAD_ADDEXEC(tester_bu_sscanf bu_sscanf.c libbu NO_INSTALL)
BRLCAD_ADDEXEC(tester_bu_gethostname bu_gethostname.c libbu NO_INSTALL)

#
#  ************ basename.c tests *************
#

###############################
#      bu_basename testing    #
###############################
BRLCAD_ADDEXEC(tester_bu_basename    bu_basename.c    libbu NO_INSTALL)
add_test(bu_basename_null                                   tester_bu_basename) # tests NULL
add_test(bu_basename_empty                                  tester_bu_basename "")
add_test(bu_basename_sl_usr_sl_dir_sl_file                  tester_bu_basename "/usr/dir/file")
add_test(bu_basename_sl_usr_sl_dir_sl_                      tester_bu_basename "/usr/dir/")
add_test(bu_basename_sl_usr_bsl__sl_dir                     tester_bu_basename "/usr\\\\/dir")
add_test(bu_basename_sl_usr_sl__p_                          tester_bu_basename "/usr/.")
add_test(bu_basename_sl_usr_sl_                             tester_bu_basename "/usr/")
add_test(bu_basename_sl_usr                                 tester_bu_basename "/usr")
add_test(bu_basename_usr                                    tester_bu_basename "usr")
add_test(bu_basename_sl_usr_sl_some_sp_long_sl_file         tester_bu_basename "/usr/some long/file")
add_test(bu_basename_sl_usr_sl_some_sp_file                 tester_bu_basename "/usr/some file")
add_test(bu_basename_C_c__sl_usr_sl_some_bsl__sp_drivepath  tester_bu_basename "C:/usr/some\\\\ drivepath")
add_test(bu_basename_sl_a_sp_test_sp_file                   tester_bu_basename "/a test file")
add_test(bu_basename_another_sp_file                        tester_bu_basename "another file")
add_test(bu_basename_C_c__bsl__Temp                         tester_bu_basename "C:\\\\Temp")
add_test(bu_basename_C_c__sl_Temp                           tester_bu_basename "C:/Temp")
add_test(bu_basename_sl_                                    tester_bu_basename "/")
add_test(bu_basename_sl__sl__sl__sl__sl_                    tester_bu_basename "/////")
add_test(bu_basename_p_                                     tester_bu_basename ".")
add_test(bu_basename_p__p_                                  tester_bu_basename "..")
add_test(bu_basename_p__p__p_                               tester_bu_basename "...")
add_test(bu_basename_sp__sp__sp_                            tester_bu_basename "   ")

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
add_test(bu_booleanize_null         tester_bu_booleanize) # tests NULL
add_test(bu_booleanize_empty        tester_bu_booleanize "")
add_test(bu_booleanize_n            tester_bu_booleanize "n")
add_test(bu_booleanize_nabcd        tester_bu_booleanize "nabcd")
add_test(bu_booleanize_N            tester_bu_booleanize "N")
add_test(bu_booleanize_Nabcd        tester_bu_booleanize "Nabcd")
add_test(bu_booleanize__sp__sp_abcd tester_bu_booleanize "  abcd")
add_test(bu_booleanize_0            tester_bu_booleanize "0")
add_test(bu_booleanize_0000         tester_bu_booleanize "0000")
add_test(bu_booleanize__lp_null_rp_ tester_bu_booleanize "(null)")
add_test(bu_booleanize_y            tester_bu_booleanize "y")
add_test(bu_booleanize_yabcd        tester_bu_booleanize "yabcd")
add_test(bu_booleanize_Y            tester_bu_booleanize "Y")
add_test(bu_booleanize_Yabcd        tester_bu_booleanize "Yabcd")
add_test(bu_booleanize_1            tester_bu_booleanize "1")
add_test(bu_booleanize_0001         tester_bu_booleanize "0001")
add_test(bu_booleanize_abcd         tester_bu_booleanize "abcd")

#
#  ************ ctype.c tests *************
#

##################################
#      bu_str_isprint testing    #
##################################
# Note - because of the special characters needed in these tests, the strings
# don't work well when fed in from the command line.
BRLCAD_ADDEXEC(tester_bu_str_isprint bu_str_isprint.c libbu NO_INSTALL)
add_test(bu_str_isprint_basic                tester_bu_str_isprint 1)
add_test(bu_str_isprint_eol                  tester_bu_str_isprint 2)
add_test(bu_str_isprint_double_slash         tester_bu_str_isprint 3)
add_test(bu_str_isprint_horiz_tab            tester_bu_str_isprint 4)
add_test(bu_str_isprint_non-alphanumeric     tester_bu_str_isprint 5)
add_test(bu_str_isprint_eol_non-alphanumeric tester_bu_str_isprint 6)
add_test(bu_str_isprint_carriage_return      tester_bu_str_isprint 7)

#
#  ************ dirname.c tests *************
#

##############################
#      bu_dirname testing    #
##############################
BRLCAD_ADDEXEC(tester_bu_dirname bu_dirname.c libbu NO_INSTALL)
add_test(bu_dirname__usr_dir_file tester_bu_dirname "/usr/dir/file")
add_test(bu_dirname__usr_dir_ tester_bu_dirname "/usr/dir/")
add_test(bu_dirname__usr__bsl__dir tester_bu_dirname "/usr\\\\/dir")
add_test(bu_dirname__usr_period tester_bu_dirname "/usr/.")
add_test(bu_dirname__usr_ tester_bu_dirname "/usr/")
add_test(bu_dirname__usr tester_bu_dirname "/usr")
add_test(bu_dirname_usr tester_bu_dirname "usr")
add_test(bu_dirname__usr_some_sp_long_file tester_bu_dirname "/usr/some long/file")
add_test(bu_dirname__usr_some_sp_file tester_bu_dirname "/usr/some file")
add_test(bu_dirname__usr_some__bsl__sp_drivepath tester_bu_dirname "C:/usr/some\\\\ drivepath")
add_test(bu_dirname__a_sp_test_file tester_bu_dirname "/a test file")
add_test(bu_dirname_another_file tester_bu_dirname "another file")
add_test(bu_dirname_C_colon__bsl_Temp tester_bu_dirname "C:\\\\Temp")
add_test(bu_dirname_C_colon__Temp tester_bu_dirname "C:/Temp")
add_test(bu_dirname__ tester_bu_dirname "/")
add_test(bu_dirname______ tester_bu_dirname "/////")
add_test(bu_dirname__period_ tester_bu_dirname ".")
add_test(bu_dirname__period__period_ tester_bu_dirname "..")
add_test(bu_dirname__period__period__period_ tester_bu_dirname "...")
add_test(bu_dirname__sp__sp__sp_ tester_bu_dirname "   ")
add_test(bu_dirname_empty tester_bu_dirname "")
add_test(bu_dirname_null tester_bu_dirname)

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
  add_test("bu_str_unescape_${test_num}" tester_bu_escape 1 "${test_num}")
endforeach()

#################################
#      bu_str_escape testing    #
#################################
foreach(test_num RANGE 1 31)
  add_test("bu_str_escape_${test_num}" tester_bu_escape 2 "${test_num}")
endforeach()

##############################################
#      escape/unescape round trip testing    #
##############################################
foreach(test_num RANGE 1 4)
  add_test("bu_escape_roundtrip_${test_num}" tester_bu_escape 3 "${test_num}")
endforeach()

###
# bu_heap memory allocation testing
###
BRLCAD_ADDEXEC(tester_bu_heap bu_heap.c libbu NO_INSTALL)
add_test(bu_heap_test_1 tester_bu_heap)

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
add_test(bu_progname_tests tester_bu_progname)

#
#  ************ quote.c tests *************
#
# The testing of encoding and de-coding is tested at once in a "round trip"
# approach - the tests below all test both bu_vls_encode and bu_vls_decode.
# Like many string functions using escaped characters the input strings are
# left in C to minimize pass-through errors.
BRLCAD_ADDEXEC(tester_bu_quote bu_quote.c libbu NO_INSTALL)
foreach(test_num RANGE 1 23)
  add_test("bu_quote_test_${test_num}" tester_bu_quote "${test_num}")
endforeach()

#
#  ************ rb_*.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_redblack bu_redblack.c libbu NO_INSTALL)
add_test(bu_redblack_test tester_bu_redblack)

#
#  ************ timer.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_timer bu_timer.c libbu NO_INSTALL)
add_test(bu_gettime_test tester_bu_timer)

#
#  ************ bu_semaphore.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_semaphore bu_semaphore.c "libbu;${WINMM_LIBRARY}" NO_INSTALL)

# failure encountered, output:
# parallel-increment bu_semaphore test invalid: threads did not run in parallel
# disabled prior to release 7.22.2
# add_test(bu_semaphore_test tester_bu_semaphore)

#
#  ************ vls_vprintf.c tests *************
#
BRLCAD_ADDEXEC(tester_bu_vls_vprintf bu_vls_vprintf.c libbu NO_INSTALL)
foreach(test_num RANGE 1 67)
  add_test("bu_vls_vprintf_test_${test_num}" tester_bu_vls_vprintf "${test_num}")
endforeach()

#
#  ************ vls.c tests *************
#
include(vls-tests.cmake)


#
#  ************ sort.c test *************
#
BRLCAD_ADDEXEC(tester_bu_sort bu_sort.c libbu NO_INSTALL)
add_test(bu_sort_test_unsigned_int_1 tester_bu_sort 1)
add_test(bu_sort_test_unsigned_int_2 tester_bu_sort 2)
add_test(bu_sort_test_fastf_t_1 tester_bu_sort 3)
add_test(bu_sort_test_fastf_t_2 tester_bu_sort 4)
add_test(bu_sort_test_string_1 tester_bu_sort 5)
add_test(bu_sort_test_string_2 tester_bu_sort 6)
add_test(bu_sort_test_dist_to_int_1 tester_bu_sort 7)
add_test(bu_sort_test_dist_to_int_2 tester_bu_sort 8)
add_test(bu_sort_test_fastf_empty tester_bu_sort 9)
add_test(bu_sort_test_fastf_inf tester_bu_sort 10)

#
#  ************ color.c tests *************

# For tester_bu_color, the input format is as follows:
#
# tester_bu_color <function number> <args>
BRLCAD_ADDEXEC(tester_bu_color bu_color.c libbu NO_INSTALL)

# For function #1 (bu_rgb_to_hsv) the <args> format is as follows:
#
# rgb expected_hsv
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(bu_rgb_to_hsv_1 tester_bu_color 1 192,78,214 290,0.636,0.839)

# For function #2 (bu_hsv_to_rgb) the <args> format is as follows:
#
# hsv expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers, and hsv colors are represented by comma-separated triples
# of floats.

add_test(bu_hsv_to_rgb_1 tester_bu_color 2 290,0.636,0.839 192,78,214)

# For function #3 (bu_str_to_rgb) the <args> format is as follows:
#
# string expected_rgb
#
# Where rgb colors are represented by comma-separated triples of
# integers.

add_test(bu_str_to_rgb_1 tester_bu_color 3 "#C04ED6" 192,78,214)
add_test(bu_str_to_rgb_2 tester_bu_color 3 "192/78/214" 192,78,214)
add_test(bu_str_to_rgb_3 tester_bu_color 3 "192 78 214" 192,78,214)

# For function #4 (bu_color_to_rgb_floats) the <args> format is as follows:
#
# rgb_color
#
# Where rgb colors are represented by comma-separated triples of
# integers or floats.

add_test(bu_color_to_rgb_floats_1 tester_bu_color 4 192,78,214)

#
#  *********** badmagic.c tests ************
#

BRLCAD_ADDEXEC(tester_bu_badmagic bu_badmagic.c libbu NO_INSTALL)
add_test(bu_badmagic_diff_magic  tester_bu_badmagic 1)
add_test(bu_badmagic_normal      tester_bu_badmagic 2)
add_test(bu_badmagic_null_ptr    tester_bu_badmagic 3)
add_test(bu_badmagic_misaligned  tester_bu_badmagic 4)

#
#  *********** date-time.c tests ************
#

BRLCAD_ADDEXEC(tester_bu_date-time bu_date-time.c libbu NO_INSTALL)
foreach(test_num RANGE 1 10)
  add_test("bu_date-time${test_num}" tester_bu_date-time "${test_num}")
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
