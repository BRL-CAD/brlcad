
set(LIBSTEPUTILS_SRCS
    Str.cc
    dirobj.cc
    gennode.cc
    gennodelist.cc
    gennodearray.cc
    scl_hash.cc
    errordesc.cc
)

SET(LIBSTEPUTILS_PRIVATE_HDRS
    dirobj.h
    errordesc.h
    gennodearray.h
    gennode.h
    gennodelist.h
    scl_hash.h
    Str.h
)

include_directories(
    ${SCL_SOURCE_DIR}/src/base
    ${SCL_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (MSVC OR BORLAND)
add_definitions( -DSCL_BASE_DLL_IMPORTS )
add_definitions( -DSCL_UTILS_DLL_EXPORTS )
endif()

SCL_ADDLIB(steputils "${LIBSTEPUTILS_SRCS}" "base")
