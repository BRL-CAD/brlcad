brlcad_addexec(test_bottess test_bottess.c libgcv NO_INSTALL)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(HIDE_INTERNAL_SYMBOLS)
brlcad_add_test(NAME bottess_test COMMAND test_bottess)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
