# Tcl bindings
SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/tcl.cmake)

# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
add_definitions(-DPACKAGE_NAME="sqlite")
add_definitions(-DPACKAGE_TARNAME="sqlite")
add_definitions(-DPACKAGE_VERSION="3.7.8")
add_definitions(-DPACKAGE_BUGREPORT="")
add_definitions(-DSTDC_HEADERS=1)

TCL_CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
TCL_CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
TCL_CHECK_INCLUDE_FILE(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
  add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_SYS_FSTATFS_H)
TCL_CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
TCL_CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
TCL_CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
TCL_CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
TCL_CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
TCL_CHECK_INCLUDE_FILE(values.h HAVE_VALUES_H)
IF(NOT HAVE_VALUES_H)
  add_definitions(-DNO_VALUES_H=1)
ENDIF(NOT HAVE_VALUES_H)
TCL_CHECK_INCLUDE_FILE(limits.h HAVE_LIMITS_H)
TCL_CHECK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)

add_definitions(-DUSE_THREAD_ALLOC=1)
add_definitions(-D_THREAD_SAFE=1)
add_definitions(-DTCL_THREADS=1)
add_definitions(-DSQLITE_THREADSAFE=1)
add_definitions(-DSQLITE_ENABLE_FTS3=1)
include_directories(${TCL_INCLUDE_DIRS})
add_library(libtclsqlite3 SHARED tclsqlite3.c)
target_link_libraries(libtclsqlite3 ${TCL_STUB_LIBRARY})
set_target_properties(libtclsqlite3 PROPERTIES PREFIX "")
set_property(TARGET libtclsqlite3 APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
install(TARGETS libtclsqlite3
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})

TCL_PKGINDEX(libtclsqlite3 sqlite3 "3.7.8")
