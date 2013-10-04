if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
  add_subdirectory(win32-msvc)
endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")

add_subdirectory(pkgconfig)
add_subdirectory(perl)

if(NOT CMAKE_CONFIGURATION_TYPES)
  configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/misc/brlcad-config @ONLY)
  file(COPY ${CMAKE_BINARY_DIR}/misc/brlcad-config
    DESTINATION ${CMAKE_BINARY_DIR}/${BIN_DIR}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  DISTCLEAN(${CMAKE_BINARY_DIR}/${BIN_DIR}/brlcad-config)
  file(REMOVE ${CMAKE_BINARY_DIR}/misc/brlcad-config)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${BIN_DIR})
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config @ONLY)
    file(COPY ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config
      DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}
      FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
      GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/brlcad-config)
    file(REMOVE ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  install(PROGRAMS ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/bin/brlcad-config DESTINATION ${BIN_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)

BRLCAD_ADDDATA(fortran_example.f sample_applications)

set(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
  )
BRLCAD_ADDDATA(density_DATAFILES data)

BRLCAD_ADDDATA(nsis/brlcad.ico icons)
BRLCAD_ADDDATA(nsis/archer.ico icons)

if(DOXYGEN_FOUND)
  configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/Doxyfile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
    )
  add_custom_target(dox     DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)
  add_custom_target(doxygen DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html)

  configure_file(DoxyfileLibs.in ${CMAKE_BINARY_DIR}/CMakeTmp/DoxyfileLibs @ONLY)
  configure_file(DoxyfileLibs-defaults.in ${CMAKE_BINARY_DIR}/CMakeTmp/DoxyfileLibs-defaults @ONLY)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output_libs/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/DoxyfileLibs
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
    )
  add_custom_target(libdox DEPENDS ${CMAKE_BINARY_DIR}/doc/doxygen_output_libs/html/index.html)

endif(DOXYGEN_FOUND)

set(misc_ignore_files
  CMake
  Description.txt
  Doxyfile
  Doxyfile.in
  DoxyfileLibs.in
  Makefile.defs
  archlinux
  astyle.opt
  auto-man-page
  batch-indent-region.el
  brlcad-config.in
  brlcad.spec.in
  debian
  doxygen_structure
  doxygen_structure_libs.txt
  flawfinder
  jove.emacs
  macosx
  nsis
  win32-msvc
  )
CMAKEFILES(${misc_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
