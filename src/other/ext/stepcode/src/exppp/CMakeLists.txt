set(LIBEXPPP_SOURCES
    exppp.c
)

SET(EXPPP_SOURCES
    ../express/fedex.c
    exppp-main.c
)

include_directories(
    ${SC_SOURCE_DIR}/include
    ${SC_SOURCE_DIR}/include/exppp
    ${SC_SOURCE_DIR}/src/base
)

if(BORLAND)
    add_definitions( -D__STDC__ )
endif()

SC_ADDLIB(libexppp "${LIBEXPPP_SOURCES}" "express;base")
set_target_properties(libexppp PROPERTIES PREFIX "")

SC_ADDEXEC(exppp "${EXPPP_SOURCES}" "libexppp;express;base" ${SC_EXEC_NOINSTALL})
