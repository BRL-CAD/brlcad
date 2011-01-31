INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)

MACRO(CHECK_C_FLAG flag)
	STRING(TOUPPER ${flag} UPPER_FLAG)
	STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
	STRING(REGEX REPLACE "=" "_" UPPER_FLAG ${UPPER_FLAG})
	IF(${ARGC} LESS 2)
		CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
		IF(${UPPER_FLAG}_COMPILER_FLAG)
			SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -${flag}")
		ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
	ELSE(${ARGC} LESS 2)
		IF(NOT ${ARGV1})
			CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
			IF(${UPPER_FLAG}_COMPILER_FLAG)
				MESSAGE("Found ${ARGV1} - setting to -${flag}")
				SET(${ARGV1} "-${flag}" CACHE STRING "${ARGV1}" FORCE)
			ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
		ENDIF(NOT ${ARGV1})
	ENDIF(${ARGC} LESS 2)
	IF(${UPPER_FLAG}_COMPILER_FLAG)
		SET(${UPPER_FLAG}_COMPILER_FLAG "-${flag}")
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

MACRO(CHECK_CXX_FLAG flag)
	STRING(TOUPPER ${flag} UPPER_FLAG)
	STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
	STRING(REGEX REPLACE "=" "_" UPPER_FLAG ${UPPER_FLAG})
	IF(${ARGC} LESS 2)
		CHECK_CXX_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
		IF(${UPPER_FLAG}_COMPILER_FLAG)
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -${flag}")
		ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
	ELSE(${ARGC} LESS 2)
		IF(NOT ${ARGV1})
			CHECK_CXX_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
			IF(${UPPER_FLAG}_COMPILER_FLAG)
				MESSAGE("Found ${ARGV1} - setting to -${flag}")
				SET(${ARGV1} "-${flag}" CACHE STRING "${ARGV1}" FORCE)
			ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
		ENDIF(NOT ${ARGV1})
	ENDIF(${ARGC} LESS 2)
	IF(${UPPER_FLAG}_COMPILER_FLAG)
		SET(${UPPER_FLAG}_COMPILER_FLAG "-${flag}")
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()


MACRO(CHECK_C_FLAG_GATHER flag FLAGS)
	STRING(TOUPPER ${flag} UPPER_FLAG)
	STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
	CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
	IF(${UPPER_FLAG}_COMPILER_FLAG)
		SET(${FLAGS} "${${FLAGS}} -${flag}")
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

SET(CMAKE_C_FLAGS "")
SET(CMAKE_CXX_FLAGS "")
# try to use -pipe to speed up the compiles
CHECK_C_FLAG(pipe)
CHECK_CXX_FLAG(pipe)

# check for -fno-strict-aliasing
# XXX - THIS FLAG IS REQUIRED if any level of optimization is
# enabled with GCC as we do use aliasing and type-punning.
CHECK_C_FLAG(fno-strict-aliasing)
CHECK_CXX_FLAG(fno-strict-aliasing)

# check for -fno-common (libtcl needs it on darwin, do we need
# this anymore with ExternalProject_ADD calling the configure?)
CHECK_C_FLAG(fno-common)
CHECK_CXX_FLAG(fno-common)

# check for -fexceptions
# this is needed to resolve __Unwind_Resume when compiling and
# linking against openNURBS in librt for some binaries, for
# example rttherm (i.e. any -static binaries)
CHECK_C_FLAG(fexceptions)
CHECK_CXX_FLAG(fexceptions)

# check for -ftemplate-depth-NN this is needed in libpc and
# other code using boost where the template instantiation depth
# needs to be increased from the default ANSI minimum of 17.
CHECK_CXX_FLAG(ftemplate-depth-50)

# dynamic SSE optimizations for NURBS processing
#
# XXX disable the SSE flags for now as they can cause illegal instructions.
#     the test needs to also be tied to run-time functionality since gcc
#     may still output SSE instructions (e.g., for cross-compiling).
# CHECK_C_FLAG(msse)
# CHECK_C_FLAG(msse2)

# 64bit compilation flags
IF(BRLCAD-ENABLE_64BIT)
	IF(NOT 64BIT_FLAG AND NOT N64BIT_FLAG)
		MESSAGE("Checking for 64-bit support:")
	ENDIF(NOT 64BIT_FLAG AND NOT N64BIT_FLAG)
	CHECK_C_FLAG("arch x86_64" 64BIT_FLAG)
	CHECK_C_FLAG(64 64BIT_FLAG)
	CHECK_C_FLAG("mabi=64" 64BIT_FLAG)
	CHECK_C_FLAG(m64 64BIT_FLAG)
	CHECK_C_FLAG(q64 64BIT_FLAG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${64BIT_FLAG}")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${64BIT_FLAG}")
	IF(NOT 64BIT_FLAG)
		SET(N64BIT_FLAG 1)
	ENDIF(NOT 64BIT_FLAG)
	MARK_AS_ADVANCED(N64BIT_FLAG)
	MARK_AS_ADVANCED(64BIT_FLAG)
ENDIF(BRLCAD-ENABLE_64BIT)

IF(BRLCAD-ENABLE_PROFILING)
	CHECK_C_FLAG(pg PROFILE_FLAG)
	CHECK_C_FLAG(p PROFILE_FLAG)
	CHECK_C_FLAG(prof_gen PROFILE_FLAG)
	IF(NOT PROFILE_FLAG)
		MESSAGE("Warning - profiling requested, but don't know how to profile with this compiler - disabling.")
		SET(BRLCAD-ENABLE_PROFILING OFF)
	ELSE(NOT PROFILE_FLAG)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PROFILE_FLAG}")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PROFILE_FLAG}")
	ENDIF(NOT PROFILE_FLAG)
ENDIF(BRLCAD-ENABLE_PROFILING)

# Debugging flags
IF(APPLE)
	EXEC_PROGRAM(sw_vers ARGS -productVersion OUTPUT_VARIABLE MACOSX_VERSION)
	IF(${MACOSX_VERSION} VERSION_LESS "10.5")
		CHECK_C_FLAG(ggdb3 DEBUG_FLAG)
	ELSE(${MACOSX_VERSION} VERSION_LESS "10.5")
		CHECK_C_FLAG(gstabs+ DEBUG_FLAG)
	ENDIF(${MACOSX_VERSION} VERSION_LESS "10.5")
ELSE(APPLE)
	CHECK_C_FLAG(ggdb3 DEBUG_FLAG)
ENDIF(APPLE)
CHECK_C_FLAG(g DEBUG_FLAG)
CHECK_C_FLAG(debug DEBUG_FLAG)
# add -D_FORTIFY_SOURCE=2 to flags. provides compile-time
# best-practice error checking on certain libc functions
# (e.g., memcpy), and provides run-time checks on buffer
# lengths and memory regions.
CHECK_C_FLAG_GATHER("D_FORTIFY_SOURCE=2" DEBUG_FLAG)
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAG}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAG}")
MARK_AS_ADVANCED(DEBUG_FLAG)

# -fast provokes a stack corruption in the shadow computations because
# of strict aliasing getting enabled.  we _require_
# -fno-strict-aliasing until someone changes how lists are managed.
IF(BRLCAD-ENABLE_OPTIMIZED_BUILD)
	CHECK_C_FLAG_GATHER(O3 OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(ffast-math OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fstrength-reduce OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fexpensive-optimizations OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(finline-functions OPTIMIZE_FLAGS)
	IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG)
		CHECK_C_FLAG_GATHER(fomit-frame-pointer OPTIMIZE_FLAGS)
	ELSE(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG)
		CHECK_C_FLAG_GATHER(fno-omit-frame-pointer OPTIMIZE_FLAGS)
	ENDIF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPTIMIZE_FLAGS}")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZE_FLAGS}")
ENDIF(BRLCAD-ENABLE_OPTIMIZED_BUILD)
MARK_AS_ADVANCED(OPTIMIZE_FLAGS)
#need to strip out non-debug-compat flags after the fact based on build type, or do something else
#that will restore them if build type changes

# verbose warning flags
IF(BRLCAD-ENABLE_COMPILER_WARNINGS)
	# also of interest:
	# -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
	# -Wformat=2 (after bu_fopen_uniq() is obsolete)
	CHECK_C_FLAG_GATHER(pedantic WARNING_FLAGS)
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
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS}")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")
ENDIF(BRLCAD-ENABLE_COMPILER_WARNINGS)
MARK_AS_ADVANCED(WARNING_FLAGS)

# Unlike other flags, the STRICT flags are managed on a per-library
# basis and do not need to be added to any CMAKE_*_FLAGS variable
IF(BRLCAD-ENABLE_STRICT)
	CHECK_C_FLAG_GATHER(pedantic STRICT_FLAGS)
	# The Wall warnings are too verbose with Visual C++
	IF(NOT MSVC)
		CHECK_C_FLAG_GATHER(Wall STRICT_FLAGS)
	ELSE(NOT MSVC)
		CHECK_C_FLAG_GATHER(W4 STRICT_FLAGS)
	ENDIF(NOT MSVC)
	CHECK_C_FLAG_GATHER(Wextra STRICT_FLAGS)
	CHECK_C_FLAG_GATHER(Wundef STRICT_FLAGS)
	CHECK_C_FLAG_GATHER(Wfloat-equal STRICT_FLAGS)
	CHECK_C_FLAG_GATHER(Wshadow STRICT_FLAGS)
	CHECK_C_FLAG_GATHER(Winline STRICT_FLAGS)
	CHECK_C_FLAG_GATHER(Werror STRICT_FLAGS) 
	# Need this for tcl.h
	CHECK_C_FLAG_GATHER(Wno-long-long STRICT_FLAGS) 
ENDIF(BRLCAD-ENABLE_STRICT)
MARK_AS_ADVANCED(STRICT_FLAGS)

