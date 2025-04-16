if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
  add_subdirectory(win32-msvc)
endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")

if(NOT BRLCAD_COMPONENTS)
  add_subdirectory(pkgconfig)
  add_subdirectory(perl)

  add_subdirectory(opencl-raytracer-tests)

  configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/misc/brlcad-config @ONLY)
  file(
    COPY ${CMAKE_BINARY_DIR}/misc/brlcad-config
    DESTINATION ${CMAKE_BINARY_DIR}/${BIN_DIR}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
  distclean(${CMAKE_BINARY_DIR}/${BIN_DIR}/brlcad-config)
  file(REMOVE ${CMAKE_BINARY_DIR}/misc/brlcad-config)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${BIN_DIR})

  brlcad_adddata(fortran_example.f sample_applications)
endif(NOT BRLCAD_COMPONENTS)

if(TARGET libanalyze)
  set(density_DATAFILES GQA_SAMPLE_DENSITIES NIST_DENSITIES)
  brlcad_adddata(density_DATAFILES data)
endif(TARGET libanalyze)

add_subdirectory(nirt)

brlcad_adddata(profont/ProFont.ttf fonts)

if(TARGET archer)
  brlcad_adddata(nsis/brlcad.ico icons)
  brlcad_adddata(nsis/archer.ico icons)
endif(TARGET archer)

add_subdirectory(CMake)

if(NOT BRLCAD_COMPONENTS)
  add_subdirectory(doxygen)
  add_subdirectory(debian)
  add_subdirectory(osl_shaders)
endif(NOT BRLCAD_COMPONENTS)

# add the bugs subdir as an external project so its bug "tests" are
# not added to the main BRL-CAD "test" target
if(TARGET mged)
  # add_subdirectory(bugs)
  include(ExternalProject)
  set(BUGS_DIR ${CMAKE_CURRENT_BINARY_DIR}/bugs)

  ExternalProject_Add(BrlcadBugs
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bugs
    BINARY_DIR ${BUGS_DIR}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUGS_DIR=${BUGS_DIR} -DCMAKE_INSTALL_PREFIX=${BUGS_DIR}
    INSTALL_COMMAND ""
  )

  # Add a custom target to run the bug tests manually
  add_custom_target(bugs
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${BUGS_DIR}
    DEPENDS BUGS
  )
  set_target_properties(bugs PROPERTIES FOLDER "BRL-CAD Tests")
endif(TARGET mged)


set(
  misc_ignore_files
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
  bugs/CMakeLists.txt
  check/adj_air.sh
  check/exp_air.sh
  check/overlaps.sh
  check/rtcheck.sh
  check/volume.sh
  check/gap.sh
  clang-static-analyzer-run.sh
  coverity_model.cpp
  docker/docker-compose.yaml
  docker/Makefile
  docker/user/docker-compose.yaml
  docker/user/Dockerfile
  docker/user/xinit
  docker/user/README.md
  docker/user/startup
  docker/README.md
  docker/dev/docker-compose.yaml
  docker/dev/Dockerfile
  docker/dev/README.md
  docker/dev/startup
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
cmakefiles(${misc_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
