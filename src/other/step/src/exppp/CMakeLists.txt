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

SCL_ADDLIB(exppplib "${LIBEXPPP_SOURCES}" express)

SCL_ADDEXEC(exppp "${EXPPP_SOURCES}" "exppplib express")

