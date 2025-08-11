brlcad_addexec(rt_edit_test_tor tor.cpp "librt" TEST)

set(
  distcheck_files
  CMakeLists.txt
)

cmakefiles(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
