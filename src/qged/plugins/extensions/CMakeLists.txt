add_subdirectory(host_status_command)
add_subdirectory(host_status_dialog)
add_subdirectory(host_status_panel)

cmakefiles(
  CMakeLists.txt
  common/HostStatusCommon.cpp
  common/HostStatusCommon.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
