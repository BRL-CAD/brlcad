brlcad_addexec(test_bottess test_bottess.c libgcv NO_INSTALL)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(HIDE_INTERNAL_SYMBOLS)
brlcad_add_test(NAME bottess_test COMMAND test_bottess)

brlcad_addexec(test_gcv_context test_context.c libgcv NO_INSTALL)
brlcad_add_test(NAME gcv_context_test COMMAND test_gcv_context)

if(TARGET gcv-asc)
  brlcad_addexec(test_gcv_asc_legacy asc_legacy.c "libgcv;libwdb;librt" TEST)
  if(TARGET test_gcv_asc_legacy)
    add_dependencies(test_gcv_asc_legacy gcv-asc)
  endif()
  brlcad_add_test(NAME gcv_asc_legacy COMMAND test_gcv_asc_legacy)
  set_tests_properties(gcv_asc_legacy PROPERTIES TIMEOUT 10)
  brlcad_addexec(test_gcv_asc_script asc_script.c "libgcv;librt" TEST)
  if(TARGET test_gcv_asc_script)
    add_dependencies(test_gcv_asc_script gcv-asc)
  endif()
  brlcad_add_test(NAME gcv_asc_dispatch COMMAND test_gcv_asc_script dispatch)
  brlcad_add_test(NAME gcv_asc_parser COMMAND test_gcv_asc_script parser)
  brlcad_add_test(NAME gcv_asc_attributes COMMAND test_gcv_asc_script attributes)
endif()

cmakefiles(CMakeLists.txt test_context.c)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
