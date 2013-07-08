
set(LIBSTEPUTILS_SRCS
    Str.cc 
    dirobj.cc 
    gennode.cc 
    gennodelist.cc 
    gennodearray.cc 
    sc_hash.cc
    errordesc.cc 
)

SET(LIBSTEPUTILS_PRIVATE_HDRS
    dirobj.h
    errordesc.h
    gennodearray.h
    gennode.h
    gennodelist.h
    sc_hash.h
    Str.h
)

include_directories(
    ${SC_SOURCE_DIR}/src/base
    ${SC_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

SC_ADDLIB(steputils "${LIBSTEPUTILS_SRCS}" "base")

IF(MINGW OR MSVC OR BORLAND)
    TARGET_LINK_LIBRARIES(steputils shlwapi.lib)
ENDIF()
