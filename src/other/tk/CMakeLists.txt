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
# Tcl/Tk's normal build system uses autotools macros, referred to as the
# TEA system.  An attempt to duplicate the required functionality from
# TEA is found in tcl.cmake
INCLUDE(${TK_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)
INCLUDE(${TK_SOURCE_DIR}/CMake/ac_std_funcs.cmake)
INCLUDE(${TK_SOURCE_DIR}/CMake/tcl.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_NAME=\\\"tk\\\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_TARNAME=\\\"tk\\\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_VERSION=\\\"${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}\\\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_STRING=\\\"tk\\ ${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}\\\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_BUGREPORT=\\\"\\\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DSTDC_HEADERS=1")

IF(APPLE)
	FILE(APPEND ${CONFIG_H_FILE} "#define MAC_OSX_TCL 1\n")
ENDIF(APPLE)
#----------------------------------------------------------------------------

SC_MISSING_POSIX_HEADERS()  
SC_ENABLE_THREADS()

SC_TCL_64BIT_FLAGS()

# Test endianness
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
IF(WORDS_BIGENDIAN)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DWORDS_BIGENDIAN=1")
ENDIF(WORDS_BIGENDIAN)

SC_TIME_HANDLER()

# Check for types - TODO: still need to define substitutes if these
# are not found
CHECK_TYPE_SIZE_D(mode_t MODE)
CHECK_TYPE_SIZE_D(pid_t PID)
CHECK_TYPE_SIZE_D(size_t SIZE)
CHECK_TYPE_SIZE_D(uid_t UID)
CHECK_TYPE_SIZE_D(blkcnt_t BLKCNT)
CHECK_TYPE_SIZE_D(intptr_t INTPTR)
CHECK_TYPE_SIZE_D(uintptr_t UINTPTR)

find_package(X11)
SET(TK_WINDOWINGSYSTEM X11)

IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
	CHECK_FUNCTION_EXISTS_D(getattrlist HAVE_GETATTRLIST)
	CHECK_INCLUDE_FILE_D(copyfile.h HAVE_COPYFILE_H)
	CHECK_FUNCTION_EXISTS_D(copyfile HAVE_COPYFILE)
	IF(${COREFOUNDATION_FRAMEWORK})
		CHECK_INCLUDE_FILE_D(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
		CHECK_FUNCTION_EXISTS_D(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
		CHECK_FUNCTION_EXISTS_D(pthread_atfork HAVE_PTHREAD_ATFORK)
	ENDIF(${COREFOUNDATION_FRAMEWORK})
	ADD_TCL_CFLAG(USE_VFORK)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_DEFAULT_ENCODING=\\\"utf-8\\\"")
	ADD_TCL_CFLAG(TCL_LOAD_FROM_MEMORY)
	ADD_TCL_CFLAG(TCL_WIDE_CLICKS)
	CHECK_INCLUDE_FILE_USABILITY_D(AvailabilityMacros.h HAVE_AVAILABILITYMACROS_H)
	IF(HAVE_AVAILABILITYMACROS_H)
		SET(WEAK_IMPORT_SRCS "
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
#error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
#endif
#elif MAC_OS_X_VERSION_MIN_REQUIRED < 1020
#error MAC_OS_X_VERSION_MIN_REQUIRED < 1020
#endif
int rand(void) __attribute__((weak_import));
int main() {
rand();
return 0;
}
		")
		CHECK_C_SOURCE_COMPILES("${WEAK_IMPORT_SRCS}" WEAK_IMPORT_WORKING)
		IF(WEAK_IMPORT_WORKING)
			ADD_TCL_CFLAG(HAVE_WEAK_IMPORT)
		ENDIF(WEAK_IMPORT_WORKING)
		SET(SUSV3_SRCS "
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
#error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
#endif
#elif MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#error MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#endif
#define _DARWIN_C_SOURCE 1
#include <sys/cdefs.h>

int main ()	{return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${SUSV3_SRCS}" SUSV3_WORKING)
		IF(SUSV3_WORKING)
			ADD_TCL_CFLAG(_DARWIN_C_SOURCE)
		ENDIF(SUSV3_WORKING)

	ENDIF(HAVE_AVAILABILITYMACROS_H)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TCL_CFLAGS}") 

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
