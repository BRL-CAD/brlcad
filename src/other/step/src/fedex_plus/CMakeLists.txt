set(FEDEX_COMMON_SRCS
    classes_misc.c
    ../express/fedex.c
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

SCL_ADDEXEC(fedex_plus "${fedex_plus_SOURCES}" "libexppp express")
