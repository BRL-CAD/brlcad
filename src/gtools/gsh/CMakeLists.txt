brlcad_addexec(gsh2 "gsh2.cpp" "libged;libdm;libbu")
target_include_directories(gsh2 BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(gsh2 dm_plugins)

cmakefiles(
  CMakeLists.txt
  gsh2.cpp
  linenoise.hpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
