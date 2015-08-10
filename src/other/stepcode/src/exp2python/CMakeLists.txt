if(SC_PYTHON_GENERATOR)

include_directories(
    ${SC_SOURCE_DIR}/include
    ${SC_SOURCE_DIR}/include/exppp
    ${SC_SOURCE_DIR}/include/express
    ${SC_SOURCE_DIR}/src/base
    ${SC_SOURCE_DIR}/src/exp2cxx
)

add_definitions( -DHAVE_CONFIG_H )

set(exp2python_SOURCES
    src/classes_misc_python.c
    ../express/fedex.c
    src/fedex_main_python.c 
    src/classes_wrapper_python.cc
    src/classes_python.c 
    src/selects_python.c 
    src/multpass_python.c 
    ../exp2cxx/collect.cc 
    ../exp2cxx/complexlist.cc 
    ../exp2cxx/entlist.cc 
    ../exp2cxx/multlist.cc 
    ../exp2cxx/orlist.cc 
    ../exp2cxx/entnode.cc 
    ../exp2cxx/expressbuild.cc 
    ../exp2cxx/non-ors.cc 
    ../exp2cxx/match-ors.cc 
    ../exp2cxx/trynext.cc 
    ../exp2cxx/write.cc 
    ../exp2cxx/print.cc
)
SC_ADDEXEC(exp2python "${exp2python_SOURCES}" "libexppp;express;base")

add_dependencies( exp2python version_string )
endif(SC_PYTHON_GENERATOR)
