brlcad_addexec(gsh "gsh.cpp" "libged;libdm;libbu")
target_include_directories(gsh BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libtermio)
add_dependencies(gsh dm_plugins)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
