if(SCL_PYTHON_GENERATOR)

include_directories(
    ${SCL_SOURCE_DIR}/include
    ${SCL_SOURCE_DIR}/include/exppp
    ${SCL_SOURCE_DIR}/include/express
    ${SCL_SOURCE_DIR}/src/base
    ${SCL_SOURCE_DIR}/src/fedex_plus
)

add_definitions( -DHAVE_CONFIG_H )

IF(MSVC)
set(fedex_plus_MSVC_SOURCES
    ../fedex_plus/xgetopt.cc
)
add_definitions( -DSCL_BASE_DLL_IMPORTS )
add_definitions( -DSCL_EXPRESS_DLL_IMPORTS )
add_definitions( -DSCL_EXPPP_DLL_IMPORTS )
ENDIF(MSVC)

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
SCL_ADDEXEC(fedex_python "${fedex_python_SOURCES};${fedex_plus_MSVC_SOURCES}" "libexppp;express;base")

add_dependencies( fedex_python version_string )
endif(SCL_PYTHON_GENERATOR)
