LIBGCV_ADD_PLUGIN(asc "asc.cpp;asc_v4.cpp;asc_v5.cpp" "librt;libbu")

CMAKEFILES(
  asc.cpp
  asc_v4.cpp
  asc_v5.cpp
  )
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

