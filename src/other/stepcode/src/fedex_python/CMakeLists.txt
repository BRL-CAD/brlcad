if(SC_PYTHON_GENERATOR)

include_directories(
    ${SC_SOURCE_DIR}/include
    ${SC_SOURCE_DIR}/include/exppp
    ${SC_SOURCE_DIR}/include/express
    ${SC_SOURCE_DIR}/src/base
    ${SC_SOURCE_DIR}/src/fedex_plus
)

add_definitions( -DHAVE_CONFIG_H )

set(fedex_python_SOURCES
    src/classes_misc_python.c
    ../express/fedex.c
    src/fedex_main_python.c 
    src/classes_wrapper_python.cc
    src/classes_python.c 
    src/selects_python.c 
    src/multpass_python.c 
    ../fedex_plus/collect.cc 
    ../fedex_plus/complexlist.cc 
    ../fedex_plus/entlist.cc 
    ../fedex_plus/multlist.cc 
    ../fedex_plus/orlist.cc 
    ../fedex_plus/entnode.cc 
    ../fedex_plus/expressbuild.cc 
    ../fedex_plus/non-ors.cc 
    ../fedex_plus/match-ors.cc 
    ../fedex_plus/trynext.cc 
    ../fedex_plus/write.cc 
    ../fedex_plus/print.cc
)
SC_ADDEXEC(fedex_python "${fedex_python_SOURCES}" "libexppp;express;base")

add_dependencies( fedex_python version_string )
endif(SC_PYTHON_GENERATOR)
