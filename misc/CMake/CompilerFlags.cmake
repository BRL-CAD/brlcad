INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)

# To reduce verbosity in this file, determine up front which
# build configuration type (if any) we are using and stash
# the variables we want to assign flags to into a common
# variable that will be used for all routines.
MACRO(ADD_NEW_FLAG FLAG_TYPE NEW_FLAG)
	STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
	IF(BUILD_TYPE)
		SET(CMAKE_${FLAG_TYPE}_FLAGS_${BUILD_TYPE} "${CMAKE_${FLAG_TYPE}_FLAGS_${BUILD_TYPE}} ${${NEW_FLAG}}")
	ELSE(BUILD_TYPE)
		SET(CMAKE_${FLAG_TYPE}_FLAGS "${CMAKE_${FLAG_TYPE}_FLAGS} ${${NEW_FLAG}}")
	ENDIF(BUILD_TYPE)
	FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
		STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
		SET(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE} "${CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE}} ${NEW_FLAG}")
	ENDFOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
ENDMACRO(ADD_NEW_FLAG)

STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
IF(BUILD_TYPE)
	SET(C_FLAGS CMAKE_C_FLAGS_${BUILD_TYPE})
	SET(CXX_FLAGS CMAKE_CXX_FLAGS_${BUILD_TYPE})
	SET(LD_FLAGS CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE})
	SET(EXE_FLAGS CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE})
ELSE(BUILD_TYPE)
	SET(C_FLAGS CMAKE_C_FLAGS)
	SET(CXX_FLAGS CMAKE_CXX_FLAGS)
	SET(LD_FLAGS CMAKE_SHARED_LINKER_FLAGS)
	SET(EXE_FLAGS CMAKE_EXE_LINKER_FLAGS)
ENDIF(BUILD_TYPE)

MACRO(CHECK_C_FLAG flag)
	STRING(TOUPPER ${flag} UPPER_FLAG)
	STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
	STRING(REGEX REPLACE "=" "_" UPPER_FLAG ${UPPER_FLAG})
	IF(${ARGC} LESS 2)
		CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
		IF(${UPPER_FLAG}_COMPILER_FLAG)
			SET(NEW_FLAG "-${flag}")
			ADD_NEW_FLAG(C NEW_FLAG)
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
			SET(NEW_FLAG "-${flag}")
			ADD_NEW_FLAG(CXX NEW_FLAG)
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
		IF(${FLAGS})
			SET(${FLAGS} "${${FLAGS}} -${flag}")
		ELSE(${FLAGS})
			SET(${FLAGS} "-${flag}")
		ENDIF(${FLAGS})
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

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

# Check for gnu c99 support
CHECK_C_FLAG("std=gnu99")

# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care. 
CHECK_C_FLAG(Qunused-arguments)
CHECK_CXX_FLAG(Qunused-arguments)

# 64bit compilation flags
IF(BRLCAD-ENABLE_64BIT)
	CHECK_C_FLAG(m64 64BIT_FLAG)
	CHECK_C_FLAG("arch x86_64" 64BIT_FLAG)
	CHECK_C_FLAG(64 64BIT_FLAG)
	CHECK_C_FLAG("mabi=64" 64BIT_FLAG)
	CHECK_C_FLAG(q64 64BIT_FLAG)
	ADD_NEW_FLAG(C 64BIT_FLAG)
	ADD_NEW_FLAG(CXX 64BIT_FLAG)
	ADD_NEW_FLAG(SHARED_LINKER 64BIT_FLAG)
	ADD_NEW_FLAG(EXE_LINKER 64BIT_FLAG)
ENDIF(BRLCAD-ENABLE_64BIT)

# Forced 32 bit compilation flags
IF(NOT BRLCAD-ENABLE_64BIT AND ${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
	CHECK_C_FLAG(m32 32BIT_FLAG)
	CHECK_C_FLAG("arch i686" 32BIT_FLAG)
	CHECK_C_FLAG(32 32BIT_FLAG)
	CHECK_C_FLAG("mabi=32" 32BIT_FLAG)
	CHECK_C_FLAG(q32 32BIT_FLAG)
	ADD_NEW_FLAG(C 32BIT_FLAG)
	ADD_NEW_FLAG(CXX 32BIT_FLAG)
	ADD_NEW_FLAG(SHARED_LINKER 32BIT_FLAG)
	ADD_NEW_FLAG(EXE_LINKER 32BIT_FLAG)
ENDIF(NOT BRLCAD-ENABLE_64BIT AND ${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")


IF(BRLCAD-ENABLE_PROFILING)
	CHECK_C_FLAG(pg PROFILE_FLAG)
	CHECK_C_FLAG(p PROFILE_FLAG)
	CHECK_C_FLAG(prof_gen PROFILE_FLAG)
	IF(NOT PROFILE_FLAG)
		MESSAGE("Warning - profiling requested, but don't know how to profile with this compiler - disabling.")
		SET(BRLCAD-ENABLE_PROFILING OFF)
	ELSE(NOT PROFILE_FLAG)
		ADD_NEW_FLAG(C PROFILE_FLAG)
		ADD_NEW_FLAG(CXX PROFILE_FLAG)
	ENDIF(NOT PROFILE_FLAG)
ENDIF(BRLCAD-ENABLE_PROFILING)

# Debugging flags
IF(BRLCAD-ENABLE_DEBUG_FLAGS)
	IF(APPLE)
		EXEC_PROGRAM(sw_vers ARGS -productVersion OUTPUT_VARIABLE MACOSX_VERSION)
		IF(${MACOSX_VERSION} VERSION_LESS "10.5")
			CHECK_C_FLAG_GATHER(ggdb3 DEBUG_FLAG)
		ELSE(${MACOSX_VERSION} VERSION_LESS "10.5")
			#CHECK_C_COMPILER_FLAG silently eats gstabs+
			SET(DEBUG_FLAG "${DEBUG_FLAG} -gstabs+")
		ENDIF(${MACOSX_VERSION} VERSION_LESS "10.5")
	ELSE(APPLE)
		CHECK_C_FLAG_GATHER(ggdb3 DEBUG_FLAG)
	ENDIF(APPLE)
	CHECK_C_FLAG(debug DEBUG_FLAG)
	# add -D_FORTIFY_SOURCE=2 to flags. provides compile-time
	# best-practice error checking on certain libc functions
	# (e.g., memcpy), and provides run-time checks on buffer
	# lengths and memory regions.
	CHECK_C_FLAG_GATHER("D_FORTIFY_SOURCE=2" DEBUG_FLAG)
	ADD_NEW_FLAG(C DEBUG_FLAG)
	ADD_NEW_FLAG(CXX DEBUG_FLAG)
	ADD_NEW_FLAG(SHARED_LINKER DEBUG_FLAG)
	ADD_NEW_FLAG(EXE_LINKER DEBUG_FLAG)
	MARK_AS_ADVANCED(DEBUG_FLAG)
ENDIF(BRLCAD-ENABLE_DEBUG_FLAGS)

# -fast provokes a stack corruption in the shadow computations because
# of strict aliasing getting enabled.  we _require_
# -fno-strict-aliasing until someone changes how lists are managed.
IF(BRLCAD-ENABLE_OPTIMIZED_BUILD)
	CHECK_C_FLAG_GATHER(O3 OPTIMIZE_FLAGS)
	#CHECK_C_FLAG_GATHER(ffast-math OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fstrength-reduce OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(fexpensive-optimizations OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER(finline-functions OPTIMIZE_FLAGS)
	CHECK_C_FLAG_GATHER("finline-limit=10000" OPTIMIZE_FLAGS)
	IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG AND NOT BRLCAD-ENABLE_PROFILING)
		CHECK_C_FLAG_GATHER(fomit-frame-pointer OPTIMIZE_FLAGS)
	ELSE(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG AND NOT BRLCAD-ENABLE_PROFILING)
		CHECK_C_FLAG_GATHER(fno-omit-frame-pointer OPTIMIZE_FLAGS)
	ENDIF(NOT ${CMAKE_BUILD_TYPE} MATCHES "^Debug$" AND NOT BRLCAD-ENABLE_DEBUG AND NOT BRLCAD-ENABLE_PROFILING)
	ADD_NEW_FLAG(C OPTIMIZE_FLAG)
	ADD_NEW_FLAG(CXX OPTIMIZE_FLAG)
ENDIF(BRLCAD-ENABLE_OPTIMIZED_BUILD)
MARK_AS_ADVANCED(OPTIMIZE_FLAGS)
#need to strip out non-debug-compat flags after the fact based on build type, or do something else
#that will restore them if build type changes

# verbose warning flags
IF(BRLCAD-ENABLE_COMPILER_WARNINGS OR BRLCAD-ENABLE_STRICT)
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
	SET(${C_FLAGS} "${${C_FLAGS}} ${WARNING_FLAGS}")
	SET(${CXX_FLAGS} "${${CXX_FLAGS}} ${WARNING_FLAGS}")
	ADD_NEW_FLAG(C WARNING_FLAG)
	ADD_NEW_FLAG(CXX WARNING_FLAG)
ENDIF(BRLCAD-ENABLE_COMPILER_WARNINGS OR BRLCAD-ENABLE_STRICT)
MARK_AS_ADVANCED(WARNING_FLAGS)

IF(BRLCAD-ENABLE_STRICT)
	CHECK_C_FLAG_GATHER(Werror STRICT_FLAGS) 
	SET(${C_FLAGS} "${${C_FLAGS}} ${STRICT_FLAGS}")
	SET(${CXX_FLAGS} "${${CXX_FLAGS}} ${STRICT_FLAGS}")
	ADD_NEW_FLAG(C STRICT_FLAG)
	ADD_NEW_FLAG(CXX STRICT_FLAG)
ENDIF(BRLCAD-ENABLE_STRICT)
MARK_AS_ADVANCED(STRICT_FLAGS)

