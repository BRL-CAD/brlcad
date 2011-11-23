# -fast provokes a stack corruption in the shadow computations because
# of strict aliasing getting enabled.  we _require_
# -fno-strict-aliasing until someone changes how lists are managed.
# -fast-math results in non-IEEE floating point math among a handful
# of other optimizations that cause substantial error in ray tracing
# and tessellation (and probably more).

IF(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
	CHECK_C_FLAG_GATHER(O3 OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fstrength-reduce OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fexpensive-optimizations OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(finline-functions OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER("finline-limit=10000" OPTIMIZE_FLAGS)
	IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD_ENABLE_DEBUG AND NOT BRLCAD_ENABLE_PROFILING)
		CHECK_C_FLAG_GATHER(fomit-frame-pointer OPTIMIZE_FLAGS)
	ELSE(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD_ENABLE_DEBUG AND NOT BRLCAD_ENABLE_PROFILING)
		CHECK_C_FLAG_GATHER(fno-omit-frame-pointer OPTIMIZE_FLAGS)
	ENDIF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD_ENABLE_DEBUG AND NOT BRLCAD_ENABLE_PROFILING)
	ADD_NEW_FLAG(C OPTIMIZE_FLAGS)
	ADD_NEW_FLAG(CXX OPTIMIZE_FLAGS)
ENDIF(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
MARK_AS_ADVANCED(OPTIMIZE_FLAGS)
#need to strip out non-debug-compat flags after the fact based on build type, or do something else
#that will restore them if build type changes

# verbose warning flags
IF(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)
	# also of interest:
	# -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
	# -Wformat=2 (after bu_fopen_uniq() is obsolete)
	CHECK_C_FLAG_GATHER(pedantic WARNING_FLAGS)
	# The Wall warnings are too verbose with Visual C++
	IF(NOT MSVC)
		CHECK_C_FLAG_GATHER(Wall WARNING_FLAGS)
	ELSE(NOT MSVC)
		CHECK_C_FLAG_GATHER(W4 WARNING_FLAGS)
	ENDIF(NOT MSVC)
	CHECK_C_FLAG_GATHER(Wextra WARNING_FLAGS)
	CHECK_C_FLAG_GATHER(Wundef WARNING_FLAGS)
	CHECK_C_FLAG_GATHER(Wfloat-equal WARNING_FLAGS)
	CHECK_C_FLAG_GATHER(Wshadow WARNING_FLAGS)
	CHECK_C_FLAG_GATHER(Winline WARNING_FLAGS)
	# Need this for tcl.h
	CHECK_C_FLAG_GATHER(Wno-long-long WARNING_FLAGS) 
	ADD_NEW_FLAG(C WARNING_FLAGS)
	ADD_NEW_FLAG(CXX WARNING_FLAGS)
ENDIF(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)
MARK_AS_ADVANCED(WARNING_FLAGS)

IF(BRLCAD_ENABLE_STRICT)
	CHECK_C_FLAG_GATHER(Werror STRICT_FLAGS)
	ADD_NEW_FLAG(C STRICT_FLAGS)
	ADD_NEW_FLAG(CXX STRICT_FLAGS)
ENDIF(BRLCAD_ENABLE_STRICT)
MARK_AS_ADVANCED(STRICT_FLAGS)

SET(CMAKE_C_FLAGS_${BUILD_TYPE} "${CMAKE_C_FLAGS_${BUILD_TYPE}}" CACHE STRING "Make sure c flags make it into the cache" FORCE)
SET(CMAKE_CXX_FLAGS_${BUILD_TYPE} "${CMAKE_CXX_FLAGS_${BUILD_TYPE}}" CACHE STRING "Make sure c++ flags make it into the cache" FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE}} CACHE STRING "Make sure shared linker flags make it into the cache" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE}} CACHE STRING "Make sure exe linker flags make it into the cache" FORCE)
