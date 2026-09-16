brlcad_addexec(test_bottess test_bottess.c libgcv NO_INSTALL)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(HIDE_INTERNAL_SYMBOLS)
brlcad_add_test(NAME bottess_test COMMAND test_bottess)

brlcad_addexec(test_gcv_context test_context.c libgcv NO_INSTALL)
brlcad_add_test(NAME gcv_context_test COMMAND test_gcv_context)

cmakefiles(CMakeLists.txt test_context.c)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
