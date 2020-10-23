set(FEDEX_COMMON_SRCS
    classes_misc.c
    ../express/fedex.c
)

set(exp2cxx_SOURCES
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
    ${SC_SOURCE_DIR}/include
    ${SC_SOURCE_DIR}/include/exppp
    ${SC_SOURCE_DIR}/include/express
    ${SC_SOURCE_DIR}/src/base
)

SC_ADDEXEC(exp2cxx "${exp2cxx_SOURCES}" "libexppp;express;base" ${SC_EXEC_NOINSTALL})

if (NOT SC_IS_SUBBUILD)
	add_dependencies( exp2cxx version_string )
endif (NOT SC_IS_SUBBUILD)
