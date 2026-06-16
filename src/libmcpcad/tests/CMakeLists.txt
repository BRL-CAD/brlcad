brlcad_addexec(mcpcad_test_parse parse.c "libmcpcad;libbu" TEST)
brlcad_add_test(NAME mcpcad_parse COMMAND mcpcad_test_parse)

brlcad_addexec(mcpcad_test_exec exec.c "libmcpcad;libged;libbu" TEST)
brlcad_add_test(NAME mcpcad_exec COMMAND mcpcad_test_exec)

brlcad_addexec(mcpcad_test_session session.c "libmcpcad;libged;libbu" TEST)
brlcad_add_test(NAME mcpcad_session COMMAND mcpcad_test_session)

# Manual/integration TCP harness - deliberately NOT brlcad_add_test'd
# (it blocks listening on a socket); run it by hand or drive it with
# an external client speaking the framed protocol (see mcpcad.h).
brlcad_addexec(mcpcad_test_server server.c "libmcpcad;libged;libbu" TEST)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
