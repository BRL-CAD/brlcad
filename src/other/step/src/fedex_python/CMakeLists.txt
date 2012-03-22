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
    ${SCL_SOURCE_DIR}/src/fedex_plus/xgetopt.cc
)
add_definitions( -DSCL_BASE_DLL_IMPORTS )
add_definitions( -DSCL_EXPRESS_DLL_IMPORTS )
add_definitions( -DSCL_EXPPP_DLL_IMPORTS )
ENDIF(MSVC)

set(fedex_python_SOURCES
    src/classes_misc_python.c
    ${SCL_SOURCE_DIR}/src/express/fedex.c
    src/fedex_main_python.c 
    src/classes_wrapper_python.cc
    src/classes_python.c 
    src/selects_python.c 
    src/multpass_python.c 
    ${SCL_SOURCE_DIR}/src/fedex_plus/collect.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/complexlist.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/entlist.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/multlist.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/orlist.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/entnode.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/expressbuild.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/non-ors.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/match-ors.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/trynext.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/write.cc 
    ${SCL_SOURCE_DIR}/src/fedex_plus/print.cc
)
SCL_ADDEXEC(fedex_python "${fedex_python_SOURCES} ${fedex_plus_MSVC_SOURCES}" "libexppp express base")

add_dependencies( fedex_python version_string )
endif(SCL_PYTHON_GENERATOR)
