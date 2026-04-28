# Single comprehensive edit test: all phase content consolidated into test_edit.cpp.
# Covers: infrastructure (section 0), parser/selection/buffer (section 1),
# translate/rotate/scale/checkpoint/mat (section 2),
# DbiState null-safety (section A), axis-mode rotate / scale -c (section C).
brlcad_addexec(ged_test_edit
  test_edit.cpp
  "libged;libwdb"
  TEST
)
if(TARGET ged_test_edit)
  add_dependencies(ged_test_edit ged_plugins)
  if(TARGET ged-edit)
    add_dependencies(ged_test_edit ged-edit)
  endif()
endif()
brlcad_add_test(NAME ged_test_edit COMMAND ged_test_edit)

cmakefiles(
  CMakeLists.txt
  test_edit.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
