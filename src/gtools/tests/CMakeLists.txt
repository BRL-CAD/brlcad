
BRLCAD_ADDEXEC(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST)

add_test(NAME slow-bigdb_1gb COMMAND bigdb 1)
add_test(NAME slow-bigdb_5gb COMMAND bigdb 5)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

