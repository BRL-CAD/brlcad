brlcad_addexec(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST_USESDATA)

brlcad_add_test(
  NAME brep_audit_surface_trees_all
  COMMAND
    brep-audit --surface-trees-all --surface-tree-depth=0
    ${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g
)

brlcad_add_test(NAME slow-bigdb_1gb COMMAND bigdb 1)
if(TARGET slow-bigdb_1gb)
  set_property(TEST slow-bigdb_1gb PROPERTY SKIP_RETURN_CODE 123)
endif(TARGET slow-bigdb_1gb)

brlcad_add_test(NAME slow-bigdb_5gb COMMAND bigdb 5)
if(TARGET slow-bigdb_5gb)
  set_property(TEST slow-bigdb_5gb PROPERTY SKIP_RETURN_CODE 123)
endif(TARGET slow-bigdb_5gb)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
