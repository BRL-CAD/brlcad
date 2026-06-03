brlcad_addexec(mcpcad_test_parse parse.c "libmcpcad;libbu" TEST)
brlcad_add_test(NAME mcpcad_parse COMMAND mcpcad_test_parse)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
