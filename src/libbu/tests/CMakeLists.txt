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

if(ENABLE_BITV_TESTS)
#
#  ************ bitv.c tests *************
#

###############################
#      bu_bitv_and testing    #
###############################
BRLCAD_ADDEXEC(tester_bu_bitv_and    bu_bitv_and.c    libbu NO_INSTALL)
add_test(bu_bitv_and_test1 tester_bu_bitv_and "ffffffff" "00000000" "00000000")
add_test(bu_bitv_and_test2 tester_bu_bitv_and "ab00" "1200" "0200")

###############################
#      bu_bitv_or testing     #
###############################
BRLCAD_ADDEXEC(tester_bu_bitv_or     bu_bitv_or.c     libbu NO_INSTALL)
add_test(bu_bitv_or_test1 tester_bu_bitv_or "ffffffff" "00000000" "ffffffff")
add_test(bu_bitv_or_test2 tester_bu_bitv_or "ab00" "1200" "bb00")

##################################
#      bu_bitv_shift testing     #
##################################
BRLCAD_ADDEXEC(tester_bu_bitv_shift  bu_bitv_shift.c  libbu NO_INSTALL)
add_test(bu_bitv_shift tester_bu_bitv_shift)

##################################
#      bu_bitv_to_hex testing    #
##################################
BRLCAD_ADDEXEC(tester_bu_bitv_to_hex bu_bitv_to_hex.c libbu NO_INSTALL)
add_test(bu_bitv_to_hex_test1 tester_bu_bitv_to_hex "0123" "33323130" 32)
add_test(bu_bitv_to_hex_test2 tester_bu_bitv_to_hex "12" "3231" 16)

################################
#      bu_bitv_vls testing     #
################################
BRLCAD_ADDEXEC(tester_bu_bitv_vls    bu_bitv_vls.c    libbu NO_INSTALL)
add_test(bu_bitv_vls_test1 tester_bu_bitv_vls "00000000" "() ")
add_test(bu_bitv_vls_test2 tester_bu_bitv_vls "f0f0f0f0" "(4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31) ")

##################################
#      bu_hex_to_bitv testing    #
##################################
BRLCAD_ADDEXEC(tester_bu_hex_to_bitv bu_hex_to_bitv.c libbu NO_INSTALL)
add_test(bu_hex_to_bitv_test1 tester_bu_hex_to_bitv "33323130" "0123" 0)
add_test(bu_hex_to_bitv_test2 tester_bu_hex_to_bitv "30" "0" 0)
add_test(bu_hex_to_bitv_error tester_bu_hex_to_bitv "303" "" 1)

else(ENABLE_BITV_TESTS)
CMAKEFILES(bu_bitv_and.c bu_bitv_or.c bu_bitv_shift.c bu_bitv_to_hex.c bu_bitv_vls.c bu_hex_to_bitv.c)
endif(ENABLE_BITV_TESTS)

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
add_test(bu_str_unescape_1   tester_bu_escape 1 1)   # NULL
add_test(bu_str_unescape_2   tester_bu_escape 1 2)   # ""
add_test(bu_str_unescape_3   tester_bu_escape 1 3)   # " "
add_test(bu_str_unescape_4   tester_bu_escape 1 4)   # "hello"
add_test(bu_str_unescape_5   tester_bu_escape 1 5)   # "\""
add_test(bu_str_unescape_6   tester_bu_escape 1 6)   # "\'"
add_test(bu_str_unescape_7   tester_bu_escape 1 7)   # "\\"
add_test(bu_str_unescape_8   tester_bu_escape 1 8)   # "\\\""
add_test(bu_str_unescape_9   tester_bu_escape 1 9)   # "\\\\"
add_test(bu_str_unescape_10  tester_bu_escape 1 10)  # "\"hello\""
add_test(bu_str_unescape_11  tester_bu_escape 1 11)  # "\'hello\'"
add_test(bu_str_unescape_12  tester_bu_escape 1 12)  # "\\hello"
add_test(bu_str_unescape_13  tester_bu_escape 1 13)  # "\\hello\""
add_test(bu_str_unescape_14  tester_bu_escape 1 14)  # "hello\\\\"
add_test(bu_str_unescape_15  tester_bu_escape 1 15)  # "\"hello\'\""
add_test(bu_str_unescape_16  tester_bu_escape 1 16)  # "\"hello\'"
add_test(bu_str_unescape_17  tester_bu_escape 1 17)  # "\'hello\'"
add_test(bu_str_unescape_18  tester_bu_escape 1 18)  # "\'hello\""
add_test(bu_str_unescape_19  tester_bu_escape 1 19)  # "\"\"hello\""
add_test(bu_str_unescape_20  tester_bu_escape 1 20)  # "\'\'hello\'\'"
add_test(bu_str_unescape_21  tester_bu_escape 1 21)  # "\'\"hello\"\'"
add_test(bu_str_unescape_22  tester_bu_escape 1 22)  # "\"\"hello\"\""
add_test(bu_str_unescape_23  tester_bu_escape 1 23)  # "\\\"\\\"\\\"hello\\"

#################################
#      bu_str_escape testing    #
#################################
add_test(bu_str_escape_1   tester_bu_escape 2 1)     # NULL, NULL
add_test(bu_str_escape_2   tester_bu_escape 2 2)     # NULL, ""
add_test(bu_str_escape_3   tester_bu_escape 2 3)     # "", NULL
add_test(bu_str_escape_4   tester_bu_escape 2 4)     # "", ""
add_test(bu_str_escape_5   tester_bu_escape 2 5)     # " ", ""
add_test(bu_str_escape_6   tester_bu_escape 2 6)     # "[ ]", " "
add_test(bu_str_escape_7   tester_bu_escape 2 7)     # "[  ]", " "
add_test(bu_str_escape_8   tester_bu_escape 2 8)     # "h e l l o", " "
add_test(bu_str_escape_9   tester_bu_escape 2 9)     # "h\\ ello", " "
add_test(bu_str_escape_10  tester_bu_escape 2 10)    # "[]", "\\"
add_test(bu_str_escape_11  tester_bu_escape 2 11)    # "\\", "\\"
add_test(bu_str_escape_12  tester_bu_escape 2 12)    # "\\\\", "\\"
add_test(bu_str_escape_13  tester_bu_escape 2 13)    # "\\a\\b", "\\"
add_test(bu_str_escape_14  tester_bu_escape 2 14)    # "abc", "a"
add_test(bu_str_escape_15  tester_bu_escape 2 15)    # "abc", "b"
add_test(bu_str_escape_16  tester_bu_escape 2 16)    # "abc", "c"
add_test(bu_str_escape_17  tester_bu_escape 2 17)    # "abc", "ab"
add_test(bu_str_escape_18  tester_bu_escape 2 18)    # "abc", "bc"
add_test(bu_str_escape_19  tester_bu_escape 2 19)    # "abc", "abc"
add_test(bu_str_escape_20  tester_bu_escape 2 20)    # "aaa", "bc"
add_test(bu_str_escape_21  tester_bu_escape 2 21)    # "aaa", "a"
add_test(bu_str_escape_22  tester_bu_escape 2 22)    # "aaa", "aaa"
add_test(bu_str_escape_23  tester_bu_escape 2 23)    # "abc", "^a"
add_test(bu_str_escape_24  tester_bu_escape 2 24)    # "abc", "^b"
add_test(bu_str_escape_25  tester_bu_escape 2 25)    # "abc", "^c"
add_test(bu_str_escape_26  tester_bu_escape 2 26)    # "abc", "^ab"
add_test(bu_str_escape_27  tester_bu_escape 2 27)    # "abc", "^bc"
add_test(bu_str_escape_28  tester_bu_escape 2 28)    # "abc", "^abc"
add_test(bu_str_escape_29  tester_bu_escape 2 29)    # "aaa", "^bc"
add_test(bu_str_escape_30  tester_bu_escape 2 30)    # "aaa", "^a"
add_test(bu_str_escape_31  tester_bu_escape 2 31)    # "aaa", "^aaa"

##############################################
#      escape/unescape round trip testing    #
##############################################
add_test(bu_escape_roundtrip_1  tester_bu_escape 3 1) # "abc", "b"
add_test(bu_escape_roundtrip_2  tester_bu_escape 3 2) # "abc\\cba", "b"
add_test(bu_escape_roundtrip_3  tester_bu_escape 3 3) # "abc\\\\cba", "b"
add_test(bu_escape_roundtrip_4  tester_bu_escape 3 4) # "abc\\\\\\c\\ba\\"

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
add_test(bu_quote_test_1  tester_bu_quote 1)   # NULL
add_test(bu_quote_test_2  tester_bu_quote 2)   # ""
add_test(bu_quote_test_3  tester_bu_quote 3)   # " "
add_test(bu_quote_test_4  tester_bu_quote 4)   # "hello"
add_test(bu_quote_test_5  tester_bu_quote 5)   # "\""
add_test(bu_quote_test_6  tester_bu_quote 6)   # "\'"
add_test(bu_quote_test_7  tester_bu_quote 7)   # "\\"
add_test(bu_quote_test_8  tester_bu_quote 8)   # "\\\""
add_test(bu_quote_test_9  tester_bu_quote 9)   # "\\\\"
add_test(bu_quote_test_10 tester_bu_quote 10)  # "\"hello\""
add_test(bu_quote_test_11 tester_bu_quote 11)  # "\'hello\'"
add_test(bu_quote_test_12 tester_bu_quote 12)  # "\\hello"
add_test(bu_quote_test_13 tester_bu_quote 13)  # "\\hello\""
add_test(bu_quote_test_14 tester_bu_quote 14)  # "hello\\\\"
add_test(bu_quote_test_15 tester_bu_quote 15)  # "\"hello\'\""
add_test(bu_quote_test_16 tester_bu_quote 16)  # "\"hello\'"
add_test(bu_quote_test_17 tester_bu_quote 17)  # "\'hello\'"
add_test(bu_quote_test_18 tester_bu_quote 18)  # "\'hello\""
add_test(bu_quote_test_19 tester_bu_quote 19)  # "\"\"hello\""
add_test(bu_quote_test_20 tester_bu_quote 20)  # "\'\'hello\'\'"
add_test(bu_quote_test_21 tester_bu_quote 21)  # "\'\"hello\"\'"
add_test(bu_quote_test_22 tester_bu_quote 22)  # "\"\"hello\"\""
add_test(bu_quote_test_23 tester_bu_quote 23)  # "\"\"\"hello\"\"\""

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
add_test(bu_vls_vprintf_test_1  tester_bu_vls_vprintf 1)
add_test(bu_vls_vprintf_test_2  tester_bu_vls_vprintf 2)
add_test(bu_vls_vprintf_test_3  tester_bu_vls_vprintf 3)
add_test(bu_vls_vprintf_test_4  tester_bu_vls_vprintf 4)
add_test(bu_vls_vprintf_test_5  tester_bu_vls_vprintf 5)
add_test(bu_vls_vprintf_test_6  tester_bu_vls_vprintf 6)
add_test(bu_vls_vprintf_test_7  tester_bu_vls_vprintf 7)
add_test(bu_vls_vprintf_test_8  tester_bu_vls_vprintf 8)
add_test(bu_vls_vprintf_test_9  tester_bu_vls_vprintf 9)
add_test(bu_vls_vprintf_test_10 tester_bu_vls_vprintf 10)
add_test(bu_vls_vprintf_test_11 tester_bu_vls_vprintf 11)
add_test(bu_vls_vprintf_test_12 tester_bu_vls_vprintf 12)
add_test(bu_vls_vprintf_test_13 tester_bu_vls_vprintf 13)
add_test(bu_vls_vprintf_test_14 tester_bu_vls_vprintf 14)
add_test(bu_vls_vprintf_test_15 tester_bu_vls_vprintf 15)
add_test(bu_vls_vprintf_test_16 tester_bu_vls_vprintf 16)
add_test(bu_vls_vprintf_test_17 tester_bu_vls_vprintf 17)
add_test(bu_vls_vprintf_test_18 tester_bu_vls_vprintf 18)
add_test(bu_vls_vprintf_test_19 tester_bu_vls_vprintf 19)
add_test(bu_vls_vprintf_test_20 tester_bu_vls_vprintf 20)
add_test(bu_vls_vprintf_test_21 tester_bu_vls_vprintf 21)
add_test(bu_vls_vprintf_test_22 tester_bu_vls_vprintf 22)
add_test(bu_vls_vprintf_test_23 tester_bu_vls_vprintf 23)
add_test(bu_vls_vprintf_test_24 tester_bu_vls_vprintf 24)
add_test(bu_vls_vprintf_test_25 tester_bu_vls_vprintf 25)
add_test(bu_vls_vprintf_test_26 tester_bu_vls_vprintf 26)
add_test(bu_vls_vprintf_test_27 tester_bu_vls_vprintf 27)
add_test(bu_vls_vprintf_test_28 tester_bu_vls_vprintf 28)
add_test(bu_vls_vprintf_test_29 tester_bu_vls_vprintf 29)
add_test(bu_vls_vprintf_test_30 tester_bu_vls_vprintf 30)
add_test(bu_vls_vprintf_test_31 tester_bu_vls_vprintf 31)
add_test(bu_vls_vprintf_test_32 tester_bu_vls_vprintf 32)
add_test(bu_vls_vprintf_test_33 tester_bu_vls_vprintf 33)
add_test(bu_vls_vprintf_test_34 tester_bu_vls_vprintf 34)
add_test(bu_vls_vprintf_test_35 tester_bu_vls_vprintf 35)
add_test(bu_vls_vprintf_test_36 tester_bu_vls_vprintf 36)
add_test(bu_vls_vprintf_test_37 tester_bu_vls_vprintf 37)
add_test(bu_vls_vprintf_test_38 tester_bu_vls_vprintf 38)
add_test(bu_vls_vprintf_test_39 tester_bu_vls_vprintf 39)
add_test(bu_vls_vprintf_test_40 tester_bu_vls_vprintf 40)
add_test(bu_vls_vprintf_test_41 tester_bu_vls_vprintf 41)
add_test(bu_vls_vprintf_test_42 tester_bu_vls_vprintf 42)
add_test(bu_vls_vprintf_test_43 tester_bu_vls_vprintf 43)
add_test(bu_vls_vprintf_test_44 tester_bu_vls_vprintf 44)
add_test(bu_vls_vprintf_test_45 tester_bu_vls_vprintf 45)
add_test(bu_vls_vprintf_test_46 tester_bu_vls_vprintf 46)
add_test(bu_vls_vprintf_test_47 tester_bu_vls_vprintf 47)
add_test(bu_vls_vprintf_test_48 tester_bu_vls_vprintf 48)
add_test(bu_vls_vprintf_test_49 tester_bu_vls_vprintf 49)
add_test(bu_vls_vprintf_test_50 tester_bu_vls_vprintf 50)
add_test(bu_vls_vprintf_test_51 tester_bu_vls_vprintf 51)
add_test(bu_vls_vprintf_test_52 tester_bu_vls_vprintf 52)
add_test(bu_vls_vprintf_test_53 tester_bu_vls_vprintf 53)
add_test(bu_vls_vprintf_test_54 tester_bu_vls_vprintf 54)
add_test(bu_vls_vprintf_test_55 tester_bu_vls_vprintf 55)
add_test(bu_vls_vprintf_test_56 tester_bu_vls_vprintf 56)
add_test(bu_vls_vprintf_test_57 tester_bu_vls_vprintf 57)
add_test(bu_vls_vprintf_test_58 tester_bu_vls_vprintf 58)
add_test(bu_vls_vprintf_test_59 tester_bu_vls_vprintf 59)
add_test(bu_vls_vprintf_test_60 tester_bu_vls_vprintf 60)
add_test(bu_vls_vprintf_test_61 tester_bu_vls_vprintf 61)
add_test(bu_vls_vprintf_test_62 tester_bu_vls_vprintf 62)
add_test(bu_vls_vprintf_test_63 tester_bu_vls_vprintf 63)
add_test(bu_vls_vprintf_test_64 tester_bu_vls_vprintf 64)
add_test(bu_vls_vprintf_test_65 tester_bu_vls_vprintf 65)

#
#  ************ vls.c tests *************

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

# For function #8 (bu_vls_trunc2) the <args> format is as follows:
#
# string_to_test trunc_len expected_result
#
# where string_to_test is the string which will be truncated, and
# trunc_len is the length that will be used for truncation.

add_test(bu_vls_trunc2_1 tester_bu_vls 8 "Test 1 2 3" 6 "Test 1")
add_test(bu_vls_trunc2_2 tester_bu_vls 8 "Test 1 2 3" -2 "Test 1 2 3")

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

add_test(bu_vls_strncat_1 tester_bu_vls 11 "Test 1" " 2 3" 4 "Test 1 2 3")
add_test(bu_vls_strncat_2 tester_bu_vls 11 "Test 1" " 2 3" 2 "Test 1 2")
# This seems to be failing because a if() statement in strncat thinks
# that 4 is not greater than -4, so len is left at strlen(" 2 3")
add_test(bu_vls_strncat_3 tester_bu_vls 11 "Test 1" " 2 3" -4 "Test 1")

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
add_test(bu_date-time1  tester_bu_date-time 1)
add_test(bu_date-time2  tester_bu_date-time 2)
add_test(bu_date-time3  tester_bu_date-time 3)
add_test(bu_date-time4  tester_bu_date-time 4)
add_test(bu_date-time5  tester_bu_date-time 5)
add_test(bu_date-time6  tester_bu_date-time 6)
add_test(bu_date-time7  tester_bu_date-time 7)
add_test(bu_date-time8  tester_bu_date-time 8)
add_test(bu_date-time9  tester_bu_date-time 9)
add_test(bu_date-time10  tester_bu_date-time 10)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
