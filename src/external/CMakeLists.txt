# This setting tells the subdirectory CMake logic to
# build using the integrated logic, rather than the
# stand-along build logic used for the primary product
# builds.
set(BRLCAD_BUILDTEST_EXTERNALS 1)

add_subdirectory(Creo)
add_subdirectory(Cubit)
add_subdirectory(Unigraphics)

if(WIN32 AND BUILD_SHARED_LIBS)
  # Without exports MSVC omits the import libraries declared as Ninja outputs,
  # leaving the build-test DLL targets perpetually out of date.
  foreach(external_target creo-brl g-sat ug-g)
    if(TARGET ${external_target})
      set_property(TARGET ${external_target} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
  endforeach()
endif()

cmakefiles(README)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
