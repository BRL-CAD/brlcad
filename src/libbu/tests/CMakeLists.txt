BRLCAD_ADDEXEC(tester_bu_basename    bu_basename.c    libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_bitv_and    bu_bitv_and.c    libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_bitv_or     bu_bitv_or.c     libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_bitv_to_hex bu_bitv_to_hex.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_bitv_vls    bu_bitv_vls.c    libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_hex_to_bitv bu_hex_to_bitv.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_bitv bu_bitv.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_booleanize bu_booleanize.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_ctype bu_ctype.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_dirname bu_dirname.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_escape bu_escape.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_progname bu_progname.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_quote bu_quote.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_sscanf bu_sscanf.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_timer bu_timer.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bu_vls_vprintf bu_vls_vprintf.c libbu NO_INSTALL LOCAL)

###############################
#      bu_basename testing    #
###############################
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

###############################
#      bu_bitv_and testing    #
###############################
add_test(bu_bitv_and_test1                                  tester_bu_bitv_and "ffffffff" "00000000" "00000000")
add_test(bu_bitv_and_test2                                  tester_bu_bitv_and "ab00" "1200" "0200")

###############################
#      bu_bitv_or testing     #
###############################
add_test(bu_bitv_or_test1                                   tester_bu_bitv_or "ffffffff" "00000000" "ffffffff")
add_test(bu_bitv_or_test2                                   tester_bu_bitv_or "ab00" "1200" "bb00")

##################################
#      bu_bitv_to_hex testing    #
##################################
add_test(bu_bitv_to_hex_test1                               tester_bu_bitv_to_hex "0123" "33323130" 32)
add_test(bu_bitv_to_hex_test2                               tester_bu_bitv_to_hex "12" "3231" 16)

################################
#      bu_bitv_vls testing     #
################################
add_test(bu_bitv_vls_test1                                  tester_bu_bitv_vls "00000000" "() ")
add_test(bu_bitv_vls_test2                                  tester_bu_bitv_vls "f0f0f0f0" "(4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31) ")

##################################
#      bu_hex_to_bitv testing    #
##################################
add_test(bu_hex_to_bitv_test1                               tester_bu_hex_to_bitv "33323130" "0123" 0)
add_test(bu_hex_to_bitv_test2                               tester_bu_hex_to_bitv "30" "0" 0)
add_test(bu_hex_to_bitv_error                               tester_bu_hex_to_bitv "303" "" 1)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
