# This setting tells the subdirectory CMake logic to
# build using the integrated logic, rather than the
# stand-along build logic used for the primary product
# builds.
set(BRLCAD_BUILDTEST_EXTERNALS 1)

add_subdirectory(Creo)
add_subdirectory(Cubit)
add_subdirectory(Unigraphics)

CMAKEFILES(README)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

