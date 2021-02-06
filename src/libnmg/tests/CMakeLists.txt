# nmg_mk testing
BRLCAD_ADDEXEC(nmg_mk nmg_mk.c "libnmg" TEST)
BRLCAD_ADD_TEST(NAME nmg_mk COMMAND nmg_mk)

# nmg_copy testing
BRLCAD_ADDEXEC(nmg_copy nmg_copy.c "libnmg" TEST)
BRLCAD_ADD_TEST(NAME nmg_copy COMMAND nmg_copy)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
