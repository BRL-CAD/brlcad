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
# FIXME - need to finish porting enigma to Windows -
# tried adding postgresql crypt function but it doesn't
# seem to be working.
if(NOT WIN32)
  add_subdirectory(enigma)
endif(NOT WIN32)

if(NOT CMAKE_CONFIGURATION_TYPES)
  configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/bin/brlcad-config @ONLY)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${BIN_DIR})
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/bin/brlcad-config @ONLY)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  install(PROGRAMS ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/bin/brlcad-config DESTINATION ${BIN_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)

BRLCAD_ADDDATA(fortran_example.f sample_applications)
ADD_MAN_PAGES(1 brlcad-config.1)

set(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
  )
BRLCAD_ADDDATA(density_DATAFILES data)

BRLCAD_ADDDATA(nsis/brlcad.ico icons)
BRLCAD_ADDDATA(nsis/archer.ico icons)

find_package(Doxygen)
if(DOXYGEN_FOUND)
  if(BRLCAD_BUILD_DOXYGEN_DOCS)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/misc/Doxyfile
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
      )
    add_custom_target(dox DEPENDS ${CMAKE_BINARY_DIR}/misc/doxygen_output/html/index.html)
  endif(BRLCAD_BUILD_DOXYGEN_DOCS)
else(DOXYGEN_FOUND)
  if(BRLCAD_BUILD_DOXYGEN_DOCS)
    message("Doxygen build requested but doxygen not found, disabling")
  endif(BRLCAD_BUILD_DOXYGEN_DOCS)
endif(DOXYGEN_FOUND)

set(misc_ignore_files
  CMake
  Description.txt
  Doxyfile
  Doxyfile.in
  Makefile.defs
  archlinux
  astyle.opt
  batch-indent-region.el
  brlcad-config.1
  brlcad-config.in
  brlcad.spec.in
  debian
  doxygen_structure
  flawfinder
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
