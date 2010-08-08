set(FEDEX_COMMON_SRCS
    classes_misc.c
    ${SCL_SOURCE_DIR}/src/express/fedex.c
)

set(fedex_os_SOURCES
    ${FEDEX_COMMON_SRCS}
    fedex_os.c
)

set(fedex_idl_SOURCES
    ${FEDEX_COMMON_SRCS}
    fedex_idl.c
)

set(fedex_plus_SOURCES
    ${FEDEX_COMMON_SRCS} 
    fedex_main.c 
    classes_wrapper.cc 
    classes.c 
    selects.c 
    multpass.c 
    collect.cc 
    complexlist.cc 
    entlist.cc 
    multlist.cc 
    orlist.cc 
    entnode.cc 
    expressbuild.cc 
    non-ors.cc 
    match-ors.cc 
    trynext.cc 
    write.cc 
    print.cc
)

include_directories(
    ${SCL_SOURCE_DIR}/include
    ${SCL_SOURCE_DIR}/include/exppp
    ${SCL_SOURCE_DIR}/include/express
)

add_definitions(
    -DHAVE_CONFIG_H
)

add_executable(fedex_os ${fedex_os_SOURCES})
target_link_libraries(fedex_os exppplib express)

add_executable(fedex_idl ${fedex_idl_SOURCES})
target_link_libraries(fedex_idl exppplib express)

add_executable(fedex_plus ${fedex_plus_SOURCES})
target_link_libraries(fedex_plus exppplib express)

