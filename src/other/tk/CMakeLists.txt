#                     C M A K E L I S T S . T X T
# SCL
#
# Copyright (c) 2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***                     Tk CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(TK)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(TK_MAJOR_VERSION 8)
SET(TK_MINOR_VERSION 5)
SET(TK_PATCH_LEVEL 8)

SET(TK_VERSION "${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}.${TK_PATCH_LEVEL}")

#-----------------------------------------------------------------------------
# Output directories.
IF(NOT LIBRARY_OUTPUT_PATH)
  SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT LIBRARY_OUTPUT_PATH)
IF(NOT EXECUTABLE_OUTPUT_PATH)
  SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT EXECUTABLE_OUTPUT_PATH)

#-----------------------------------------------------------------------------
# Configure install locations. 

IF(NOT CMAKE_INSTALL_PREFIX)
	IF(WIN32)
		SET(CMAKE_INSTALL_PREFIX "/usr/local")
	ELSE(WIN32) 
		SET(CMAKE_INSTALL_PREFIX "C:/Tcl")
	ENDIF(WIN32) 
ENDIF(NOT CMAKE_INSTALL_PREFIX)


#-----------------------------------------------------------------------------
# Check if the compiler supports pipe - if so, use it
INCLUDE(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-pipe PIPE_COMPILER_FLAG)
IF(PIPE_COMPILER_FLAG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
ENDIF(PIPE_COMPILER_FLAG)

#-----------------------------------------------------------------------------
# Set CMake module path
SET(CMAKE_MODULE_PATH "${TK_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

#-----------------------------------------------------------------------------
# Unlike the autotools based builds, CMake will always use tkConfig.h - define
# this at the toplevel so tclPort.h includes it.

add_definitions(
	-DHAVE_TK_CONFIG_H
	-DTCL_NO_DEPRECATED 
	-DUSE_TCL_STUBS
	-DHAVE_UNISTD_H=1
)

# We will need a tkConfig.h.in file to hold all the #cmakedefine statements,
# which will in turn be used to generate a tkConfig.h file.  To succeed, the 
# file must first be present, which we take care of here:

SET(CONFIG_H_FILE "${CMAKE_BINARY_DIR}/include/tkConfig.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** Tk define statements from CMake ****/\n")
FILE(APPEND ${CONFIG_H_FILE} "#ifndef _TKCONFIG\n")
FILE(APPEND ${CONFIG_H_FILE} "#define _TKCONFIG\n")

INCLUDE(${TK_SOURCE_DIR}/CMake/TK_ConfigFile.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"tk\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"tk\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_VERSION \"${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"tk ${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TK_SHLIB_EXT \"${CMAKE_SHARED_LIBRARY_SUFFIX}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_LIBDIR \"${CMAKE_INSTALL_PREFIX}/lib\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_BINDIR \"${CMAKE_INSTALL_PREFIX}/bin\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_SCRDIR \"${CMAKE_INSTALL_PREFIX}/scripts\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_INCDIR \"${CMAKE_INSTALL_PREFIX}/include\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_DOCDIR \"${CMAKE_INSTALL_PREFIX}/share/man\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_LIBDIR \"${CMAKE_INSTALL_PREFIX}/lib\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_BINDIR \"${CMAKE_INSTALL_PREFIX}/bin\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_SCRDIR \"${CMAKE_INSTALL_PREFIX}/scripts\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_INCDIR \"${CMAKE_INSTALL_PREFIX}/include\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_DOCDIR \"${CMAKE_INSTALL_PREFIX}/share/man\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TK_LIBRARY \"${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tk${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TK_PACKAGE_PATH \"${CMAKE_INSTALL_PREFIX}/lib\"\n")

# Hardcode this for now, check m4 logic
FILE(APPEND ${CONFIG_H_FILE} "#define TK_CFGVAL_ENCODING \"iso8859-1\"\n")


IF(APPLE)
	FILE(APPEND ${CONFIG_H_FILE} "#define MAC_OSX_TCL 1\n")
ENDIF(APPLE)
#----------------------------------------------------------------------------
# Define a number of variables to be controlled by options

FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_PTHREAD_ATTR_SETSTACKSIZE 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_PTHREAD_GET_STACKSIZE_NP 1\n")

IF(APPLE)
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_COREFOUNDATION 1\n")
	ENDIF(COREFOUNDATION_FRAMEWORK)
	find_library(FONTCONFIG_LIBRARY fontconfig)
ENDIF(APPLE)

FIND_PACKAGE(Freetype)
FIND_PACKAGE(X11)

TK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
TK_INCLUDE_FILE(sys/statfs.h HAVE_SYS_STATFS_H)
IF(NOT HAVE_SYS_STATFS_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_FSTATFS 1\n")
ENDIF(NOT HAVE_SYS_STATFS_H)
TK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
TK_INCLUDE_FILE(string.h HAVE_STRING_H)
TK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
TK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
TK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
TK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
TK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
IF(NOT HAVE_UNISTD_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_UNISTD_H 1\n")
ENDIF(NOT HAVE_UNISTD_H)

TK_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
IF(NOT HAVE_DIRENT_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_DIRENT_H 1\n")
ENDIF(NOT HAVE_DIRENT_H)

# need to check usability
TK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
IF(NOT HAVE_FLOAT_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_FLOAT_H 1\n")
ENDIF(NOT HAVE_FLOAT_H)

# need to check usability
TK_INCLUDE_FILE(values.h HAVE_VALUES_H)
IF(NOT HAVE_VALUES_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_VALUES_H 1\n")
ENDIF(NOT HAVE_VALUES_H)


# need to check usability
TK_INCLUDE_FILE(limits.h HAVE_LIMITS_H)
IF(NOT HAVE_LIMITS_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_LIMITS_H 1\n")
ENDIF(NOT HAVE_LIMITS_H)

# need to check usability
TK_INCLUDE_FILE(sys/wait.h HAVE_SYS_WAIT_H)
IF(NOT HAVE_SYS_WAIT_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_SYS_WAIT_H 1\n")
ENDIF(NOT HAVE_SYS_WAIT_H)



TK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)

TK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
TK_FUNCTION_EXISTS(gmtime_r HAVE_GMTIME_R)
TK_FUNCTION_EXISTS(localtime_r HAVE_LOCALTIME_R)
TK_FUNCTION_EXISTS(mktime HAVE_MKTIME)

TK_INCLUDE_FILE(copyfile.h HAVE_COPYFILE_H)
TK_FUNCTION_EXISTS(copyfile HAVE_COPYFILE)

TK_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H)
TK_INCLUDE_FILE(sys/filio.h HAVE_SYS_FILIO_H)

TK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
TK_FUNCTION_EXISTS(opendir HAVE_OPENDIR)
TK_FUNCTION_EXISTS(strtol HAVE_STRTOL)
TK_FUNCTION_EXISTS(waitpid HAVE_WAITPID)
TK_FUNCTION_EXISTS(getaddrinfo HAVE_GETADDRINFO)
#TK_FUNCTION_EXISTS( HAVE_GETPWUID_R_5)
TK_FUNCTION_EXISTS(getpwuid HAVE_GETPWUID_R)
#TK_FUNCTION_EXISTS( HAVE_GETPWNAM_R_5)
TK_FUNCTION_EXISTS(getpwnam HAVE_GETPWNAM_R)
#TK_FUNCTION_EXISTS( HAVE_GETGRGID_R_5)
TK_FUNCTION_EXISTS(getgrgid HAVE_GETGRGID_R)
#TK_FUNCTION_EXISTS( HAVE_GETGRNAM_R_5)
TK_FUNCTION_EXISTS(getgrnam HAVE_GETGRNAM_R)
#TK_FUNCTION_EXISTS( HAVE_MTSAFE_GETHOSTBYNAME)
#TK_FUNCTION_EXISTS( HAVE_MTSAFE_GETHOSTBYADDR)
#TK_FUNCTION_EXISTS( HAVE_STRUCT_STAT_ST_BLOCKS)
#TK_FUNCTION_EXISTS( HAVE_STRUCT_STAT_ST_BLKSIZE)
TK_TYPE_SIZE(blkcnt_t HAVE_BLKCNT_T)
TK_TYPE_SIZE(intptr_t HAVE_INTPTR_T)
TK_TYPE_SIZE(uintptr_t HAVE_UINTPTR_T)
#TK_TYPE_SIZE( HAVE_SIGNED_CHAR)
#TK_TYPE_SIZE( HAVE_LANGINFO)
#TK_TYPE_SIZE( HAVE_CHFLAGS)
TK_FUNCTION_EXISTS(getattrlist HAVE_GETATTRLIST)
TK_INCLUDE_FILE(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
TK_FUNCTION_EXISTS(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
TK_FUNCTION_EXISTS(pthread_atfork HAVE_PTHREAD_ATFORK)
#TK_INCLUDE_FILE( USE_VFORK)
#TK_INCLUDE_FILE( TK_WIDE_INT_TYPE long\ long\n")
#TK_INCLUDE_FILE( TK_CFG_DEBUG)
#TK_INCLUDE_FILE( USE_TERMIOS)
#TK_INCLUDE_FILE( TIME_WITH_SYS_TIME)
#TK_INCLUDE_FILE( HAVE_TM_GMTOFF)
#TK_INCLUDE_FILE( HAVE_TIMEZONE_VAR)
#TK_INCLUDE_FILE( HAVE_STRUCT_STAT64)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine M_PREC 4\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TK_DEFAULT_ENCODING \"utf-8\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TK_LOAD_FROM_MEMORY 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TK_WIDE_CLICKS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_AVAILABILITYMACROS_H 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_WEAK_IMPORT 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine _DARWIN_C_SOURCE 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_FTS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TK_UNLOAD_DLLS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine USE_DTRACE 1\n")

# Now that all the tests are done, configure the tkConfig.h file:
FILE(APPEND ${CONFIG_H_FILE} "#endif /*_TKCONFIG*/\n")
configure_file(${CONFIG_H_FILE} ${TK_BINARY_DIR}/include/tkConfig.h)

SET(TK_GENERIC_SRCS
	generic/tk3d.c
	generic/tkArgv.c
	generic/tkAtom.c
	generic/tkBind.c
	generic/tkBitmap.c
	generic/tkButton.c
	generic/tkCanvArc.c
	generic/tkCanvBmap.c
	generic/tkCanvImg.c
	generic/tkCanvLine.c
	generic/tkCanvPoly.c
	generic/tkCanvPs.c
	generic/tkCanvText.c
	generic/tkCanvUtil.c
	generic/tkCanvWind.c
	generic/tkCanvas.c
	generic/tkClipboard.c
	generic/tkCmds.c
	generic/tkColor.c
	generic/tkConfig.c
	generic/tkConsole.c
	generic/tkCursor.c
	generic/tkEntry.c
	generic/tkError.c
	generic/tkEvent.c
	generic/tkFocus.c
	generic/tkFont.c
	generic/tkFrame.c
	generic/tkGC.c
	generic/tkGeometry.c
	generic/tkGet.c
	generic/tkGrab.c
	generic/tkGrid.c
	generic/tkImage.c
	generic/tkImgBmap.c
	generic/tkImgGIF.c
	generic/tkImgPPM.c
	generic/tkImgPhoto.c
	generic/tkListbox.c
	generic/tkMain.c
	generic/tkMenu.c
	generic/tkMenuDraw.c
	generic/tkMenubutton.c
	generic/tkMessage.c
	generic/tkObj.c
	generic/tkOldConfig.c
	generic/tkOption.c
	generic/tkPack.c
	generic/tkPanedWindow.c
	generic/tkPlace.c
	generic/tkRectOval.c
	generic/tkScale.c
	generic/tkScrollbar.c
	generic/tkSelect.c
	generic/tkStubInit.c
	generic/tkStubLib.c
	generic/tkStyle.c
	generic/tkText.c
	generic/tkTextBTree.c
	generic/tkTextDisp.c
	generic/tkTextImage.c
	generic/tkTextIndex.c
	generic/tkTextMark.c
	generic/tkTextTag.c
	generic/tkTextWind.c
	generic/tkTrig.c
	generic/tkUndo.c
	generic/tkUtil.c
	generic/tkVisual.c
	generic/tkWindow.c
)

SET(TTK_SRCS
	generic/ttk/ttkBlink.c
	generic/ttk/ttkButton.c
	generic/ttk/ttkCache.c
	generic/ttk/ttkClamTheme.c
	generic/ttk/ttkClassicTheme.c
	generic/ttk/ttkDefaultTheme.c
	generic/ttk/ttkElements.c
	generic/ttk/ttkEntry.c
	generic/ttk/ttkFrame.c
	generic/ttk/ttkImage.c
	generic/ttk/ttkInit.c
	generic/ttk/ttkLabel.c
	generic/ttk/ttkLayout.c
	generic/ttk/ttkManager.c
	generic/ttk/ttkNotebook.c
	generic/ttk/ttkPanedwindow.c
	generic/ttk/ttkProgress.c
	generic/ttk/ttkScale.c
	generic/ttk/ttkScrollbar.c
	generic/ttk/ttkScroll.c
	generic/ttk/ttkSeparator.c
	generic/ttk/ttkSquare.c
	generic/ttk/ttkState.c
	generic/ttk/ttkStubInit.c
	generic/ttk/ttkStubLib.c
	generic/ttk/ttkTagSet.c
	generic/ttk/ttkTheme.c
	generic/ttk/ttkTrace.c
	generic/ttk/ttkTrack.c
	generic/ttk/ttkTreeview.c
	generic/ttk/ttkWidget.c
)

SET(TK_STUB_SRCS
	generic/tkStubInit.c
	generic/tkStubLib.c
)

SET(TTK_STUB_SRCS
	generic/ttk/ttkStubInit.c
	generic/ttk/ttkStubLib.c
)

SET(TK_X11_SRCS
	unix/tkUnix.c
	unix/tkUnix3d.c
	unix/tkUnixButton.c
	unix/tkUnixColor.c
	unix/tkUnixConfig.c
	unix/tkUnixCursor.c
	unix/tkUnixDraw.c
	unix/tkUnixEmbed.c
	unix/tkUnixEvent.c
	unix/tkUnixFocus.c
	unix/tkUnixInit.c
	unix/tkUnixKey.c
	unix/tkUnixMenu.c
	unix/tkUnixMenubu.c
	unix/tkUnixRFont.c
	unix/tkUnixScale.c
	unix/tkUnixScrlbr.c
	unix/tkUnixSelect.c
	unix/tkUnixSend.c
	unix/tkUnixWm.c
	unix/tkUnixXId.c
)

SET(TK_AQUA_SRCS
	macosx/tkMacOSXBitmap.c
	macosx/tkMacOSXButton.c
	macosx/tkMacOSXClipboard.c
	macosx/tkMacOSXColor.c
	macosx/tkMacOSXConfig.c
	macosx/tkMacOSXCursor.c
	macosx/tkMacOSXDebug.c
	macosx/tkMacOSXDialog.c
	macosx/tkMacOSXDraw.c
	macosx/tkMacOSXEmbed.c
	macosx/tkMacOSXEntry.c
	macosx/tkMacOSXEvent.c
	macosx/tkMacOSXFont.c
	macosx/tkMacOSXHLEvents.c
	macosx/tkMacOSXInit.c
	macosx/tkMacOSXKeyboard.c
	macosx/tkMacOSXKeyEvent.c
	macosx/tkMacOSXMenu.c
	macosx/tkMacOSXMenubutton.c
	macosx/tkMacOSXMenus.c
	macosx/tkMacOSXMouseEvent.c
	macosx/tkMacOSXNotify.c
	macosx/tkMacOSXRegion.c
	macosx/tkMacOSXScrlbr.c
	macosx/tkMacOSXSend.c
	macosx/tkMacOSXSubwindows.c
	macosx/tkMacOSXTest.c
	macosx/tkMacOSXWindowEvent.c
	macosx/tkMacOSXWm.c
	macosx/tkMacOSXXStubs.c
	macosx/tkMacOSXCarbonEvents.c
	generic/tkFileFilter.c
	generic/tkMacWinMenu.c
	generic/tkPointer.c
	unix/tkUnix3d.c
	unix/tkUnixScale.c
	xlib/xcolors.c
	xlib/xdraw.c
	xlib/xgc.c
	xlib/ximage.c
	xlib/xutil.c
	generic/ttk/ttkMacOSXTheme.c
)

SET(TK_WIN_SRCS
)

SET(TK_SRCS ${TK_GENERIC_SRCS} ${TTK_SRCS})

IF(WIN32)
	SET(TK_SRCS ${TK_SRCS} ${TK_WIN_SRCS})
ELSE(WIN32)
	IF(ENABLE_AQUA)
		SET(TK_SRCS ${TK_SRCS} ${TK_AQUA_SRCS})
	ELSE(ENABLE_AQUA)
		SET(TK_SRCS ${TK_SRCS} ${TK_X11_SRCS})
	ENDIF(ENABLE_AQUA)
ENDIF(WIN32)

# When it comes to identification of the location of the Tcl
# sources, the convention is as follows:
#
# 1.  If TCL_INCLUDE_PATH is defined, use it
# 2.  Else, if TCL_PREFIX is defined build include paths from it
# 3.  If TCL_INCLUDE_PATH is not defined and TCL_BIN_PREFIX is,
#     add include directories based on the presumption of the
#     binary build dir (and hence tkConfig.h) being in a non-src
#     location.
#
# It's still possible, depending on how Tcl was built, to 
# have a situation where TCL_INCLUDE_PATH has to be augmented
# manually.  However, the logic below should cover the most common cases.

IF(NOT TCL_INCLUDE_PATH)
	IF(TCL_PREFIX)
		SET(TCL_INCLUDE_PATH ${TCL_PREFIX}/generic ${TCL_PREFIX}/libtommath)
		IF(WIN32)
			SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_PREFIX}/win)
		ELSE(WIN32)
			SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_PREFIX}/unix)
		ENDIF(WIN32)
	ENDIF(TCL_PREFIX)
	IF(TCL_BIN_PREFIX)
		SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_BIN_PREFIX}/include)
	ENDIF(TCL_BIN_PREFIX)
ENDIF(NOT TCL_INCLUDE_PATH)

SET(TK_INCLUDE_PATH ${TK_SOURCE_DIR}/generic ${TK_SOURCE_DIR}/bitmaps ${TK_BINARY_DIR}/include)
IF(WIN32)
	SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/win)
ELSE(WIN32)
	SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/unix)
	IF(APPLE)
		SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/macosx)
	ENDIF(APPLE)
ENDIF(WIN32)

include_directories(
   ${TCL_INCLUDE_PATH}	
   ${TK_INCLUDE_PATH}	
	${FREETYPE_INCLUDE_DIRS}
)

IF(WIN32)
	SET(WISH_SRCS win/tkAppInit.c)
ELSE(WIN32)
	SET(WISH_SRCS unix/tkAppInit.c)
ENDIF(WIN32)

add_library(tk ${TK_SRCS})
target_link_libraries(tk tclstub ${COREFOUNDATION_FRAMEWORK} ${X11_X11_LIB} ${X11_Xscreensaver_LIB} ${X11_Xext_LIB} ${X11_Xft_LIB} ${FREETYPE_LIBRARIES} ${FONTCONFIG_LIBRARY} ${X11_Xrender_LIB})
install(TARGETS tk DESTINATION lib)
SET_TARGET_PROPERTIES(tk PROPERTIES VERSION ${TK_VERSION} SOVERSION ${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})

add_library(tkstub STATIC ${TK_STUB_SRCS})
install(TARGETS tkstub DESTINATION lib)


add_executable(wish ${WISH_SRCS})
target_link_libraries(wish tcl tk ${COREFOUNDATION_FRAMEWORK} ${X11_X11_LIB} ${X11_Xscreensaver_LIB} ${X11_Xext_LIB} ${X11_Xft_LIB} ${FREETYPE_LIBRARIES} ${FONTCONFIG_LIBRARY} ${X11_Xrender_LIB})
install(TARGETS wish DESTINATION bin)

SET(TK_LIBRARY_FILES
	library/bgerror.tcl
	library/button.tcl
	library/choosedir.tcl
	library/clrpick.tcl
	library/comdlg.tcl
	library/console.tcl
	library/dialog.tcl
	library/entry.tcl
	library/focus.tcl
	library/listbox.tcl
	library/menu.tcl
	library/mkpsenc.tcl
	library/msgbox.tcl
	library/obsolete.tcl
	library/optMenu.tcl
	library/palette.tcl
	library/panedwindow.tcl
	library/safetk.tcl
	library/scale.tcl
	library/scrlbar.tcl
	library/spinbox.tcl
	library/tearoff.tcl
	library/text.tcl
	library/tk.tcl
	library/tkfbox.tcl
	library/unsupported.tcl
	library/xmfbox.tcl
	)
install(FILES ${TK_LIBRARY_FILES} DESTINATION lib/tk${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})
SET(TK_LIBRARY_IMAGE_FILES
	library/images/README
	library/images/logo.eps
	library/images/logo100.gif
	library/images/logo64.gif
	library/images/logoLarge.gif
	library/images/logoMed.gif
	library/images/pwrdLogo.eps
	library/images/pwrdLogo100.gif
	library/images/pwrdLogo150.gif
	library/images/pwrdLogo175.gif
	library/images/pwrdLogo200.gif
	library/images/pwrdLogo75.gif
	library/images/tai-ku.gif
	)
install(FILES ${TK_LIBRARY_IMAGE_FILES} DESTINATION lib/tk${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}/images)
SET(TK_LIBRARY_MSG_FILES
	library/msgs/cs.msg
	library/msgs/da.msg
	library/msgs/de.msg
	library/msgs/el.msg
	library/msgs/en.msg
	library/msgs/en_gb.msg
	library/msgs/eo.msg
	library/msgs/es.msg
	library/msgs/fr.msg
	library/msgs/hu.msg
	library/msgs/it.msg
	library/msgs/nl.msg
	library/msgs/pl.msg
	library/msgs/pt.msg
	library/msgs/ru.msg
	library/msgs/sv.msg
	)
install(FILES ${TK_LIBRARY_MSG_FILES} DESTINATION lib/tk${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}/msgs)
SET(TTK_LIBRARY_FILES
	library/ttk/altTheme.tcl
	library/ttk/aquaTheme.tcl
	library/ttk/button.tcl
	library/ttk/clamTheme.tcl
	library/ttk/classicTheme.tcl
	library/ttk/combobox.tcl
	library/ttk/cursors.tcl
	library/ttk/defaults.tcl
	library/ttk/entry.tcl
	library/ttk/fonts.tcl
	library/ttk/menubutton.tcl
	library/ttk/notebook.tcl
	library/ttk/panedwindow.tcl
	library/ttk/progress.tcl
	library/ttk/scale.tcl
	library/ttk/scrollbar.tcl
	library/ttk/sizegrip.tcl
	library/ttk/treeview.tcl
	library/ttk/ttk.tcl
	library/ttk/utils.tcl
	library/ttk/vistaTheme.tcl
	library/ttk/winTheme.tcl
	library/ttk/xpTheme.tcl
)
install(FILES ${TTK_LIBRARY_FILES} DESTINATION lib/tk${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}/ttk)
