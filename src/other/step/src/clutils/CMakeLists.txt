
set(LIBSTEPUTILS_SRCS
    Str.cc 
    dirobj.cc 
    gennode.cc 
    gennodelist.cc 
    gennodearray.cc 
    scl_hash.cc 
    errordesc.cc 
    scl_string.cc 
    scl_char_str_list.cc
)

SET(LIBSTEPUTILS_PRIVATE_HDRS
    dirobj.h 
    errordesc.h 
    gennodearray.h 
    gennode.h 
    gennodelist.h 
    scl_char_str_list.h 
    scldir.h 
    scl_hash.h 
    scl_string.h 
    stat.h 
    Str.h 
    str_list.h
)

include_directories(
    ${SCL_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(steputils "${LIBSTEPUTILS_SRCS}" "")
