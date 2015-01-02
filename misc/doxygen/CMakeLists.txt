configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile @ONLY)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
  )
add_custom_target(dox     DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)
add_custom_target(doxygen DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)

configure_file(DoxyfileLibs-defaults.in ${CMAKE_BINARY_DIR}/CMakeTmp/DoxyfileLibs-defaults @ONLY)
set(DOX_LIBS
  libbu
  libbn
  libbrep
  libpc
  librt
  libfb
  )
foreach(libname ${DOX_LIBS})
  set(doxlibname ${libname})
  string(REPLACE "lib" "" sublibname ${libname})
  configure_file(DoxyfileLibs.in ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname} @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output_${libname}/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile_${libname}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
    )
  add_custom_target(dox-${libname} DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output_${libname}/html/index.html)
  CMAKEFILES(doxygen_structure_${libname}.h)
endforeach(libname ${DOX_LIBS})

set(doxygen_ignore_files
  Doxyfile.in
  doxygen_intro.h
  DoxyfileLibs.in
  DoxyfileLibs-defaults.in
  )
CMAKEFILES(${doxygen_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
