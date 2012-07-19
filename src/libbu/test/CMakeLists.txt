BRLCAD_ADDEXEC(tester_basename basename.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bitv bitv.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_booleanize booleanize.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_ctype ctype.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_dirname dirname.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_escape escape.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_progname progname.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_quote quote.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_sscanf sscanf.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_timer timer.c libbu NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_vls_vprintf vls_vprintf.c libbu NO_INSTALL LOCAL)

############################
#     basename testing     #
############################
add_test(bu_basename_empty                                      tester_basename) # tests NULL and ""
add_test(bu_basename_sl_usr_sl_dir_sl_file                      tester_basename "/usr/dir/file")
add_test(bu_basename_sl_usr_sl_dir_sl_                          tester_basename "/usr/dir/")
add_test(bu_basename_sl_usr_bsl__bsl__sl_dir                    tester_basename "/usr\\/dir")
add_test(bu_basename_sl_usr_sl__p_                              tester_basename "/usr/.")
add_test(bu_basename_sl_usr_sl_                                 tester_basename "/usr/")
add_test(bu_basename_sl_usr                                     tester_basename "/usr")
add_test(bu_basename_usr                                        tester_basename "usr")
add_test(bu_basename_sl_usr_sl_some_sp_long_sl_file             tester_basename "/usr/some long/file")
add_test(bu_basename_sl_usr_sl_some_sp_file                     tester_basename "/usr/some file")
add_test(bu_basename_C_c__sl_usr_sl_some_bsl__bsl__sp_drivepath tester_basename "C:/usr/some\\ drivepath")
add_test(bu_basename_sl_a_sp_test_sp_file                       tester_basename "/a test file")
add_test(bu_basename_another_sp_file                            tester_basename "another file")
add_test(bu_basename_C_c__bsl__bsl_Temp                         tester_basename "C:\\Temp")
add_test(bu_basename_C_c__sl_Temp                               tester_basename "C:/Temp")
add_test(bu_basename_sl_                                        tester_basename "/")
add_test(bu_basename_sl__sl__sl__sl__sl_                        tester_basename "/////")
add_test(bu_basename_p_                                         tester_basename ".")
add_test(bu_basename_p__p_                                      tester_basename "..")
add_test(bu_basename_p__p__p_                                   tester_basename "...")
add_test(bu_basename_sp__sp__sp_                                tester_basename "   ")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
