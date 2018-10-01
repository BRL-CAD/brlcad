# build DLL
add_subdirectory(Dll)

if(CPP_DLL_DEFINES)
  add_library(bs SHARED bootstrap.cpp)
endif(CPP_DLL_DEFINES)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
