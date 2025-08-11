set(LIBPKG_SOURCES pkg.c vers.c)

# Note - libpkg_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(PKG_LIBS
  ${libpkg_deps}
  ${WINSOCK_LIB}
  )

brlcad_addlib(libpkg "${LIBPKG_SOURCES}" "${PKG_LIBS}" "" "")
set_target_properties(libpkg PROPERTIES VERSION 20.0.1 SOVERSION 20)

brlcad_adddata(tpkg.c sample_applications)
if (TARGET libbu)
  brlcad_addexec(tpkg tpkg.c "libbu;libpkg" TEST)
endif()

add_subdirectory(example)
add_subdirectory(example_qt)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
