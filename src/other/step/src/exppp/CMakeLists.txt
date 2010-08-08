set(LIBEXPPP_SOURCES
    exppp.c
)

SET(EXPPP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/express/fedex.c
    exppp-main.c
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/exppp
)
MESSAGE("currentsrcdir: ${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions(
    -DHAVE_CONFIG_H
)

add_library(exppplib ${LIBEXPPP_SOURCES})
target_link_libraries(exppplib express)

add_executable(exppp ${EXPPP_SOURCES})
target_link_libraries(exppp exppplib express)

