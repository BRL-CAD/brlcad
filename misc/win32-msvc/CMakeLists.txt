# build DLL
add_subdirectory(Dll)

if(MSVC)
  add_library(bs SHARED bootstrap.cpp)
endif(MSVC)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
