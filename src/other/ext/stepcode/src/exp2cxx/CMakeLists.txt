include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(exp2cxx_SOURCES
  main.c
  classes_misc.c
  classes_wrapper.cc
  classes.c
  selects.c
  multpass.c
  collect.cc
  complexlist.cc
  entlist.cc
  multlist.cc
  orlist.cc
  entnode.cc
  expressbuild.cc
  non-ors.cc
  match-ors.cc
  trynext.cc
  write.cc
  print.cc
  sc_getopt.cpp
  sc_mkdir.c
  )

add_executable(exp2cxx ${exp2cxx_SOURCES})
target_link_libraries(exp2cxx express)
target_compile_definitions(exp2cxx PRIVATE STEPCODE_EXPRESS_DLL_IMPORTS)
install(TARGETS exp2cxx
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

