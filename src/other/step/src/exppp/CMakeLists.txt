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

add_library(exppplib ${LIBEXPPP_SOURCES})
target_link_libraries(exppplib express)

add_executable(exppp ${EXPPP_SOURCES})
target_link_libraries(exppp exppplib express)

