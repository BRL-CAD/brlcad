# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(TKTREECTRL)

SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake")
find_package(TCL)
find_package(X11)

#GTK theme awareness do we want it?  Would it play well with the rest of Tk?

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/generic
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${X11_INCLUDE_DIR}
  )

# Because TkTreeCtrl is Yet Another Tcl/Tk package that needs
# the interal headers to build, we must include a local
# copy of the candidate headers and use them - an installed
# Tcl/Tk has no obligation to provide them.
#include_directories(
#  ${CMAKE_CURRENT_SOURCE_DIR}/include/tk${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}
#  )

include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES(sys/param.h HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILES(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILES(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILES(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILES(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES(limits.h HAVE_LIMITS_H)
# TODO - check for intptr type

set(TKTREECTRL_PKGNAME treectrl)
set(TKTREECTRL_PKGVERSION "2.4")
set(TKTREECTRL_PATCHLEVEL "2.4.1")

# Windows specific flags
if(WIN32)
  ENABLE_LANGUAGE(RC)
  set(TKTREECTRL_CFLAGS "${TKTREECTRL_CFLAGS} -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL")
  set(TKTREECTRL_CFLAGS "${TKTREECTRL_CFLAGS} -Dinline=__inline -DBUILD_treectrl")
endif(WIN32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TKTREECTRL_CFLAGS}")

set(TKTREECTRL_SRCS
  generic/qebind.c 
  generic/tkTreeColumn.c 
  generic/tkTreeCtrl.c 
  generic/tkTreeDisplay.c
  generic/tkTreeDrag.c 
  generic/tkTreeElem.c 
  generic/tkTreeHeader.c 
  generic/tkTreeItem.c
  generic/tkTreeMarquee.c 
  generic/tkTreeNotify.c
  generic/tkTreeStyle.c 
  generic/tkTreeTheme.c
  generic/tkTreeUtils.c
  )
set(GUI_LIBRARIES "")
if("${TK_SYSTEM_GRAPHICS}" STREQUAL "win32")
  set(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} win/tkWinTree.c treectrl.rc)
  set(GUI_LIBRARIES gdi32.lib user32.lib)
endif("${TK_SYSTEM_GRAPHICS}" STREQUAL "win32")
if("${TK_SYSTEM_GRAPHICS}" STREQUAL "x11")
  set(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} unix/tkUnixTree.c)
  set(GUI_LIBRARIES ${X11_LIBRARIES})
endif("${TK_SYSTEM_GRAPHICS}" STREQUAL "x11")
if("${TK_SYSTEM_GRAPHICS}" STREQUAL "aqua")
  set(TKTREECTRL_SRCS ${TKTREECTRL_SRCS} macosx/tkMacOSXTTree.c)
  set(TKTREECTRL_CFLAGS "${TKTREECTRL_CFLAGS} -DMAC_TK_COCOA -std=gnu99 -x objective-c -fobjc-gc")
  find_library(COCOA_FRAMEWORK Cocoa)
  mark_as_advanced(COCOA_FRAMEWORK)
  find_library(CARBON_FRAMEWORK Carbon)
  mark_as_advanced(CARBON_FRAMEWORK)
  set(GUI_LIBRARIES ${COCOA_FRAMEWORK} ${CARBON_FRAMEWORK})
endif("${TK_SYSTEM_GRAPHICS}" STREQUAL "aqua")

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_library(treectrl SHARED ${TKTREECTRL_SRCS})
target_link_libraries(treectrl ${TCL_LIBRARY} ${TCL_TK_LIBRARY} ${GUI_LIBRARIES})
set_target_properties(treectrl PROPERTIES VERSION ${TKTREECTRL_PKGVERSION})
install(TARGETS treectrl 
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})

# Create the pkgIndex.tcl file.
TCL_PKGINDEX(treectrl ${TKTREECTRL_PKGNAME} ${TKTREECTRL_PATCHLEVEL})

SET(TKTREECTRL_MISC
  library/treectrl.tcl
  library/filelist-bindings.tcl
  )
if(CMAKE_CONFIGURATION_TYPES)
   foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
     string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
     file(COPY ${TKTREECTRL_MISC} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/${TKTREECTRL_PKGNAME}${TKTREECTRL_PATCHLEVEL})
   endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
else(CMAKE_CONFIGURATION_TYPES)
   file(COPY ${TKTREECTRL_MISC} DESTINATION ${CMAKE_BINARY_DIR}/lib/${TKTREECTRL_PKGNAME}${TKTREECTRL_PATCHLEVEL})
endif(CMAKE_CONFIGURATION_TYPES)

install(FILES ${TKTREECTRL_MISC} DESTINATION lib/${TKTREECTRL_PKGNAME}${TKTREECTRL_PATCHLEVEL})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

