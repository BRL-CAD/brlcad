
BRLCAD_ADDEXEC(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST)

add_test(NAME bigdb_1gb COMMAND bigdb 1)
add_test(NAME bigdb_3gb COMMAND bigdb 3)
add_test(NAME bigdb_5gb COMMAND bigdb 5)
