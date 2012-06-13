set(LIBEXPPP_SOURCES
    exppp.c
)

SET(EXPPP_SOURCES
    ../express/fedex.c
    exppp-main.c
)

include_directories(
    ${SCL_SOURCE_DIR}/include
    ${SCL_SOURCE_DIR}/include/exppp
    ${SCL_SOURCE_DIR}/src/base
)

if(MSVC OR BORLAND)
add_definitions( -DSCL_EXPPP_DLL_EXPORTS )
add_definitions( -DSCL_EXPRESS_DLL_IMPORTS )
add_definitions( -DSCL_BASE_DLL_IMPORTS )
endif()

if(BORLAND)
add_definitions( -D__STDC__ )
endif()

SCL_ADDLIB(libexppp "${LIBEXPPP_SOURCES}" "express;base")
set_target_properties(libexppp PROPERTIES PREFIX "")

SCL_ADDEXEC(exppp "${EXPPP_SOURCES}" "libexppp;express")
