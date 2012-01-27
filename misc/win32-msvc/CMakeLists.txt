# build DLL
add_subdirectory(Dll)

if(CPP_DLL_DEFINES)
  add_library(bs SHARED bootstrap.cpp)
endif(CPP_DLL_DEFINES)
