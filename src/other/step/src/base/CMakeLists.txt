
set(SCL_BASE_SOURCES
    scl_memmgr.cc
)

include_directories(
	${SCL_SOURCE_DIR}/include
)

option(SCL_MEMMGR_ENABLE_CHECKS "Enable scl_memmgr's memory leak detection" OFF)

if(MSVC OR BORLAND)
add_definitions( -DSCL_BASE_DLL_EXPORTS )
endif()

if (${SCL_MEMMGR_ENABLE_CHECKS})
	add_definitions( -DSCL_MEMMGR_ENABLE_CHECKS )
endif()

SCL_ADDLIB(base "${SCL_BASE_SOURCES}" "")
