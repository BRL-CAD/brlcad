
BRLCAD_ADDEXEC(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST_USESDATA)

BRLCAD_ADD_TEST(NAME slow-bigdb_1gb COMMAND bigdb 1)
BRLCAD_ADD_TEST(NAME slow-bigdb_5gb COMMAND bigdb 5)

set_property(TEST slow-bigdb_1gb PROPERTY SKIP_RETURN_CODE 123)
set_property(TEST slow-bigdb_5gb PROPERTY SKIP_RETURN_CODE 123)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

