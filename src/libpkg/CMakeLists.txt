set(LIBPKG_SOURCES pkg.c vers.c)

# Note - libpkg_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libpkg "${LIBPKG_SOURCES}" "${libpkg_deps}" "" "")
set_target_properties(libpkg PROPERTIES VERSION 20.0.1 SOVERSION 20)

brlcad_adddata(tpkg.c sample_applications)
brlcad_addexec(tpkg tpkg.c "libbu;libpkg" TEST)

add_subdirectory(example)
add_subdirectory(example_qt)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
