# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(TKTREECTRL)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

find_package(TCL)
find_package(X11)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/generic
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/include
	${TCL_INCLUDE_DIRS}
	${TK_INCLUDE_PATH}
	)

# Because TkTreeCtrl is Yet Another Tcl/Tk package that needs
# the interal headers to build, we must include a local
# copy of the candidate headers and use them - an installed
# Tcl/Tk has no obligation to provide them.
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include/tcltk/tk${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}
	)


INCLUDE(CheckFunctions)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stddef.h HAVE_STDDEF_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(limits.h HAVE_LIMITS_H)
CHECK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)

SET(TKTREECTRL_PACKAGE_NAME treectrl)
SET(TKTREECTRL_PACKAGE_PATCHLEVEL "2.3.2")
SET(TKTREECTRL_PKGVERSION "2.3.2")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/tktreectrl_config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/tktreectrl_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/config.h)


IF(WIN32)
	SET(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} win/tkWinTree.c)
ENDIF(WIN32)

# This needs to key off of Aqua vs X11 windowing system, not APPLE 
IF(APPLE)
	SET(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} macosx/tkMacOSXTree.c)
ENDIF(APPLE)

# This needs to key off of Aqua vs X11 windowing system, not APPLE
IF(NOT WIN32 AND NOT APPLE)
	SET(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} unix/tkUnixTree.c)
ENDIF(NOT WIN32 AND NOT APPLE)

SET(TKTREECTRL_SRCS
	${TKTREECTRL_SRCS}
	generic/qebind.c
	generic/tkTreeColumn.c
	generic/tkTreeCtrl.c
	generic/tkTreeDisplay.c
	generic/tkTreeDrag.c
	generic/tkTreeElem.c
	generic/tkTreeItem.c
	generic/tkTreeMarquee.c
	generic/tkTreeNotify.c
	generic/tkTreeStyle.c
	generic/tkTreeTheme.c
	generic/tkTreeUtils.c
)

add_library(TkTreeCtrl SHARED ${TKTREECTRL_SRCS})
IF(WIN32)
	target_link_libraries(TkTreeCtrl ${TCL_STUB_LIBRARIES} ${X11_LIBRARIES})
ELSE(WIN32)
	target_link_libraries(TkTreeCtrl ${TCL_LIBRARY} ${TCL_TK_LIBRARY} ${TCL_STUB_LIBRARIES} ${X11_LIBRARIES})
ENDIF(WIN32)
SET_TARGET_PROPERTIES(TkTreeCtrl PROPERTIES VERSION ${TKTREECTRL_PKGVERSION})
install(TARGETS TkTreeCtrl DESTINATION ${LIB_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/treectrl.tcl
	${CMAKE_BINARY_DIR}/lib/treectrl${TKTREECTRL_PACKAGE_PATCHLEVEL}/treectrl.tcl COPYONLY)
INSTALL(FILES library/treectrl.tcl DESTINATION
	lib/treectrl${TKTREECTRL_PACKAGE_PATCHLEVEL})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/filelist-bindings.tcl
	${CMAKE_BINARY_DIR}/lib/treectrl${TKTREECTRL_PACKAGE_PATCHLEVEL}/filelist-bindings.tcl COPYONLY)
INSTALL(FILES library/filelist-bindings.tcl DESTINATION
	lib/treectrl${TKTREECTRL_PACKAGE_PATCHLEVEL})

# Create the pkgIndex.tcl file.
GET_TARGET_PROPERTY(TKTREECTRL_LIBLOCATION TkTreeCtrl LOCATION_${CMAKE_BUILD_TYPE})
GET_FILENAME_COMPONENT(TKTREECTRL_LIBNAME ${TKTREECTRL_LIBLOCATION} NAME)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TKTREECTRL_PACKAGE_NAME} ${TKTREECTRL_PKGVERSION} [list load [file join $dir .. .. ${LIB_DIR} ${TKTREECTRL_LIBNAME}]]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TKTREECTRL_PACKAGE_NAME}${TKTREECTRL_PKGVERSION})

FILE(WRITE ${CMAKE_BINARY_DIR}/lib/${TKTREECTRL_PACKAGE_NAME}${TKTREECTRL_PKGVERSION}/pkgIndex.tcl "package ifneeded ${TKTREECTRL_PACKAGE_NAME} ${TKTREECTRL_PKGVERSION} [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${TKTREECTRL_LIBNAME}] ${TKTREECTRL_PACKAGE_NAME}]")

