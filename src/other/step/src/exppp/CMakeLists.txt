set(LIBEXPPP_SOURCES
    exppp.c
)

SET(EXPPP_SOURCES
    ${SCL_SOURCE_DIR}/src/express/fedex.c
    exppp-main.c
)

include_directories(
    ${SCL_SOURCE_DIR}/include
    ${SCL_SOURCE_DIR}/include/exppp
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(libexppp "${LIBEXPPP_SOURCES}" express)
set_target_properties(libexppp PROPERTIES PREFIX "")

SCL_ADDEXEC(exppp "${EXPPP_SOURCES}" "libexppp express")
