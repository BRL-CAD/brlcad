if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
  add_subdirectory(win32-msvc)
endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")

add_subdirectory(pkgconfig)
add_subdirectory(perl)
add_subdirectory(attributes)
add_subdirectory(opencl-raytracer-tests)

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
  configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/CMakeTmp/brlcad-config @ONLY)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    file(COPY ${CMAKE_BINARY_DIR}/CMakeTmp/brlcad-config
      DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}
      FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
      GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/brlcad-config)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  file(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/brlcad-config)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/bin/brlcad-config DESTINATION ${BIN_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)

BRLCAD_ADDDATA(fortran_example.f sample_applications)

set(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
  )
BRLCAD_ADDDATA(density_DATAFILES data)

add_subdirectory(nirt)

BRLCAD_ADDDATA(profont/ProFont.ttf fonts)

BRLCAD_ADDDATA(nsis/brlcad.ico icons)
BRLCAD_ADDDATA(nsis/archer.ico icons)


add_subdirectory(doxygen)
add_subdirectory(CMake)
add_subdirectory(debian)
add_subdirectory(osl_shaders)

set(misc_ignore_files
  Bullet_Box_Chain_Demo.cpp
  CMakeLists.txt
  Description.txt
  Makefile.defs
  archlinux/PKGBUILD
  archlinux/brlcad.install
  astyle.opt
  auto-man-page/BIN_OPT_ARG_ANALYSIS.txt
  auto-man-page/README.auto-man-page-handling
  auto-man-page/auto-man-progs.txt
  batch-indent-region.el
  brlcad-config.in
  brlcad.spec.in
  check/adj_air.sh
  check/exp_air.sh
  check/overlaps.sh
  check/rtcheck.sh
  check/volume.sh
  check/gap.sh
  clang-static-analyzer-run.sh
  coverity_model.txt
  flawfinder
  hawaii_islands.sh
  jove.emacs
  macosx/README
  macosx/Resources/License.rtf
  macosx/Resources/ReadMe.rtfd/.typeAttributes.dict
  macosx/Resources/ReadMe.rtfd/TXT.rtf
  macosx/Resources/ReadMe.rtfd/brlcad_logo_tiny.png
  macosx/Resources/Welcome.rtfd/.typeAttributes.dict
  macosx/Resources/Welcome.rtfd/TXT.rtf
  macosx/Resources/Welcome.rtfd/brlcad_logo_tiny.png
  macosx/background.png
  macosx/openUp.c
  mime.types
  mime_cad.types
  nsis/archer.ico
  nsis/brlcad.ico
  nsis/header.bmp
  nsis/side.bmp
  nsis/uninstall.ico
  profont/LICENSE
  profont/profont-IIx-charmap.png
  profont/profont_IIx_readme.pdf
  profont/readme.txt
  uncpowerplant2g.sh
  win32-msvc/CMakeLists.txt
  win32-msvc/Dll/BrlcadCore.def
  win32-msvc/Dll/BrlcadCore.rc
  win32-msvc/Dll/CMakeLists.txt
  win32-msvc/Dll/TclDummies.c
  win32-msvc/Dll/brlcad.def
  win32-msvc/Dll/brlcad.rc
  win32-msvc/bootstrap.cpp
  wix/License.rtf
  wix/brlcad_banner.bmp
  wix/brlcad_dialog.bmp
  wix/brlcad_product.ico
  )
CMAKEFILES(${misc_ignore_files})
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


