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
cmake_minimum_required(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
	if("${CMAKE_VERSION}" VERSION_GREATER 2.99)
		CMAKE_POLICY(SET CMP0026 OLD)
		CMAKE_POLICY(SET CMP0048 OLD)
	endif("${CMAKE_VERSION}" VERSION_GREATER 2.99)
	if("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
		CMAKE_POLICY(SET CMP0053 OLD)
		CMAKE_POLICY(SET CMP0054 NEW)
	endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
project(TK)

# build shared libs by default
if(NOT DEFINED BUILD_SHARED_LIBS)
	option(BUILD_SHARED_LIBS "Build shared libraries" ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

# build static libs by default
if(NOT DEFINED BUILD_STATIC_LIBS)
	option(BUILD_STATIC_LIBS "Build shared libraries" ON)
endif(NOT DEFINED BUILD_STATIC_LIBS)

# version numbers
set(TK_VERSION_MAJOR 8)
set(TK_VERSION_MINOR 6)
set(TK_PATCH_LEVEL 5)

set(TK_VERSION "${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}.${TK_PATCH_LEVEL}")

# For Windows, we need the Resource Compiler language
if(WIN32)
  enable_language(RC)
endif(WIN32)

#-----------------------------------------------------------------------------
# Set CMake module path
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${TK_SOURCE_DIR}/CMake")
set(TK_CMAKE_DIR "${TCL_SOURCE_DIR}/CMake")

#-----------------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in the build
# directory AND install directory. Documentation of these options is available
# at http://www.cmake.org/Wiki/CMake_RPATH_handling

include("${TK_CMAKE_DIR}/RPath_Setup.cmake")

#---------------------------------------------------------------------
# Define relative install locations and output directories.  Don't set
# these if they have already been set by some other means (like a
# higher level CMakeLists.txt file including this one).

include("${TK_CMAKE_DIR}/Path_Setup.cmake")

#----------------------------------------------------------------------------
# Define some standard configuration settings passed to all Tcl build targets

# We're building Tk and ttk
add_definitions(-DBUILD_tk)
add_definitions(-DBUILD_ttk)

# Assume we have STDC_HEADERS until we meet a situation where we don't
add_definitions(-DSTDC_HEADERS=1)

# Get the SHLIB extension from CMake
add_definitions(-DTCL_SHLIB_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}")

#----------------------------------------------------------------------------
# Define some platform specific flags

if(MSVC)
	add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -Ot -Oi -fp:strict -Gs -GS -GL -MD -DSUPPORT_CONFIG_EMBEDDED -DHAVE_UXTHEME_H=1)
endif(MSVC)

if(MINGW)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DSUPPORT_CONFIG_EMBEDDED -DHAVE_UXTHEME_H=1)
  remove_definitions(-w)
  add_definitions(-DHAVE_NO_SEH -DEXCEPTION_DISPOSITION=int)
endif(MINGW)

if(APPLE)
  add_definitions(-DMAC_OSX_TCL=1)
  include(CMakeFindFrameworks)
  cmake_find_frameworks(CoreFoundation)
  if(CoreFoundation_FRAMEWORKS)
    set(CoreFoundation_LIBRARIES "-framework CoreFoundation")
    add_definitions(-DHAVE_COREFOUNDATION=1)	 
  endif(CoreFoundation_FRAMEWORKS)
  cmake_find_frameworks(IOKit)
  if(IOKit_FRAMEWORKS)
    set(IOKit_LIBRARIES "-framework IOKit")
    add_definitions(-DHAVE_COREFOUNDATION=1)	 
  endif(IOKit_FRAMEWORKS)
endif(APPLE)

#-----------------------------------------------------------------------------
# Check if the compiler supports pipe - if so, use it
include(CheckCCompilerFlag)
check_c_compiler_flag(-pipe PIPE_COMPILER_FLAG)
if(PIPE_COMPILER_FLAG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
endif(PIPE_COMPILER_FLAG)

#------------------------------------------------------------------------
# Threads support
#------------------------------------------------------------------------
find_package(Threads)
if(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")
	add_definitions(-DTCL_THREADS=1)
endif(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")

#-----------------------------------------------------------------------------
# Tcl/Tk's normal build system uses autotools macros, referred to as the
# TEA system.  An attempt to duplicate the required functionality from
# TEA is found in tcl.cmake
INCLUDE(${TK_SOURCE_DIR}/CMake/tcl.cmake)


#------------------------------------------------------------------------
# Call the LINK_LIBS macro to get M_LIBRARY properly defined (TEA
# doesn't do this, needs looking into...)
#------------------------------------------------------------------------
SC_TCL_LINK_LIBS()

#----------------------------------------------------------------------------
# 64-bit support - unlike TEA, we enable by default if the platform looks
# like it's 64-bit.  Can be overridden by specifying CMAKE_WORD_SIZE=32BIT
#----------------------------------------------------------------------------
SC_TCL_64BIT_FLAGS()

#--------------------------------------------------------------------
#       Check endianness because we can optimize some operations
#--------------------------------------------------------------------
IF(NOT WIN32)
  INCLUDE(TestBigEndian)
  TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
  IF(WORDS_BIGENDIAN)
    add_definitions(-DWORDS_BIGENDIAN=1)
  ENDIF(WORDS_BIGENDIAN)
ENDIF(NOT WIN32)

#------------------------------------------------------------------------------
#       Find out all about time handling differences.
#------------------------------------------------------------------------------
SC_TIME_HANDLER()

#TODO SC_BUGGY_STRTOD - Solaris 2.4 only?  Do we still care?

#--------------------------------------------------------------------
#       Check for various typedefs and provide substitutes if
#       they don't exist.
#--------------------------------------------------------------------
TCL_CHECK_TYPE_SIZE(mode_t MODE)
TCL_CHECK_TYPE_SIZE(pid_t PID)
TCL_CHECK_TYPE_SIZE(size_t SIZE)
TCL_CHECK_TYPE_SIZE(uid_t UID)
# The following tests may need to be more elaborate to function properly
TCL_CHECK_TYPE_SIZE(socklen_t SOCKLEN sys/types.h sys/socket.h)
TCL_CHECK_TYPE_SIZE(intptr_t INTPTR)
TCL_CHECK_TYPE_SIZE(uintptr_t UINTPTR)

IF(NOT HAVE_OPENDIR)
  ADD_TCL_CFLAG(USE_DIRENT2_H)
ENDIF(NOT HAVE_OPENDIR)

# The union wait test was for HP-UX - do we still need this?
SET(UNION_WAIT_SRC "
#include <sys/types.h>
#include <sys/wait.h>
int
main ()
{

union wait x;
WIFEXITED(x);           /* Generates compiler error if WIFEXITED
                         * uses an int. */
return 0;
}
")
if(NOT DEFINED HAVE_UNION_WAIT)
  CHECK_C_SOURCE_RUNS("${UNION_WAIT_SRC}" HAVE_UNION_WAIT)
endif(NOT DEFINED HAVE_UNION_WAIT)
IF(NOT HAVE_UNION_WAIT)
  ADD_TCL_CFLAG(NO_UNION_WAIT)
ENDIF(NOT HAVE_UNION_WAIT)

#--------------------------------------------------------------------
#       On Mac OS X, we can build either with X11 or with Aqua
#       For the moment, we don't allow enabling X11 on Windows,
#       eventually we may want to change that.
#       We support enabling only the new Cocoa backend, not the 
#       older Carbon code
#--------------------------------------------------------------------
IF(NOT WIN32)
  IF(APPLE)
    OPTION(TK_ENABLE_AQUA "Build Tk using the Aqua windowing system" OFF)
    IF(TK_ENABLE_AQUA)
      SET(TK_SYSTEM_GRAPHICS aqua)
      add_definitions(-DMAC_OSX_TK=1)
      add_definitions(-D_DARWIN_C_SOURCE=1)
      SET(X11_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/xlib")
      find_library(COCOA_FRAMEWORK Cocoa)	
      MARK_AS_ADVANCED(COCOA_FRAMEWORK)
      find_library(CARBON_FRAMEWORK Carbon)
      MARK_AS_ADVANCED(CARBON_FRAMEWORK)
      SET(OPENGL_USE_AQUA ON CACHE BOOL "Building Aqua Tk" FORCE)
    ELSE(TK_ENABLE_AQUA)
      find_package(X11)
      SET(TK_SYSTEM_GRAPHICS x11)
      SET(OPENGL_USE_AQUA OFF CACHE BOOL "Building X11 Tk" FORCE)
    ENDIF(TK_ENABLE_AQUA)
  ELSE(APPLE)
    find_package(X11)
    SET(TK_SYSTEM_GRAPHICS x11)
  ENDIF(APPLE)
ELSE(NOT WIN32)
  SET(TK_SYSTEM_GRAPHICS win32)
ENDIF(NOT WIN32)

#--------------------------------------------------------------------
# Check whether the header and library for the XScreenSaver
# extension are available, and set HAVE_XSS if so.
# XScreenSaver is needed for Tk_GetUserInactiveTime().
#--------------------------------------------------------------------
INCLUDE(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(TK_ENABLE_XSS "Use XScreenSaver" ON "TK_SYSTEM_GRAPHICS STREQUAL x11;${X11_Xscreensaver_LIB}" OFF)

#--------------------------------------------------------------------
#       Check for freetype / fontconfig / Xft support.
#--------------------------------------------------------------------
IF(NOT WIN32)
  find_package(Freetype)
  find_package(Fontconfig)
  IF(NOT FREETYPE_FOUND)
    SET(FREETYPE_LIBRARIES "")
  ENDIF(NOT FREETYPE_FOUND)
  IF(NOT FONTCONFIG_FOUND)
    SET(FONTCONFIG_LIBRARIES "")
  ENDIF(NOT FONTCONFIG_FOUND)
ENDIF(NOT WIN32)
CMAKE_DEPENDENT_OPTION(TK_ENABLE_XFT "Use freetype/fontconfig/xft" ON "TK_SYSTEM_GRAPHICS STREQUAL x11;FREETYPE_FOUND;FONTCONFIG_FOUND;X11_Xft_FOUND" OFF)

#----------------------------------------------------------------------------
# signed/unsigned char - does the Tcl code still need this? See
# http://lists.gnu.org/archive/html/autoconf/2008-06/msg00054.html
#----------------------------------------------------------------------------
SET(unsigned_char_srcs "
int
main ()
{
   static int test_array [1 - 2 * !(((char) -1) < 0)];
   test_array [0] = 0;
   return 0;
}
")
if(NOT DEFINED HAVE_UNION_WAIT)
  CHECK_C_SOURCE_RUNS("${unsigned_char_srcs}" CHAR_IS_UNSIGNED)
endif(NOT DEFINED HAVE_UNION_WAIT)
IF(CHAR_IS_UNSIGNED)
  ADD_TCL_CFLAG(__CHAR_UNSIGNED__)
ENDIF(CHAR_IS_UNSIGNED)
SET(signed_char_srcs "
int
main ()
{
   signed char *p;
   p = 0;
   return 0;
}
")
CHECK_C_SOURCE_COMPILES("${signed_char_srcs}" HAVE_SIGNED_CHAR)
IF(HAVE_SIGNED_CHAR)
  ADD_TCL_CFLAG(HAVE_SIGNED_CHAR)
ENDIF(HAVE_SIGNED_CHAR)



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

IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
  TCL_CHECK_FUNCTION_EXISTS(getattrlist HAVE_GETATTRLIST)
  TCL_CHECK_INCLUDE_FILE(copyfile.h HAVE_COPYFILE_H)
  TCL_CHECK_FUNCTION_EXISTS(copyfile HAVE_COPYFILE)
  IF(${CoreFoundation_LIBRARIES})
    TCL_CHECK_INCLUDE_FILE(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
    TCL_CHECK_FUNCTION_EXISTS(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
    TCL_CHECK_FUNCTION_EXISTS(pthread_atfork HAVE_PTHREAD_ATFORK)
  ENDIF(${CoreFoundation_LIBRARIES})
  ADD_TCL_CFLAG(USE_VFORK)
  add_definitions(-DTCL_DEFAULT_ENCODING="utf-8")
  ADD_TCL_CFLAG(TCL_LOAD_FROM_MEMORY)
  ADD_TCL_CFLAG(TCL_WIDE_CLICKS)
  TCL_CHECK_INCLUDE_FILE_USABILITY(AvailabilityMacros.h HAVE_AVAILABILITYMACROS_H)
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

add_definitions(-DTK_LAYOUT_WITH_BASE_CHUNKS=0)
add_definitions(-DTK_DRAW_IN_CONTEXT=0)
add_definitions(-DTILE_07_COMPAT=0)
add_definitions(-DDEBUG_FONTSEL=0)

ADD_SUBDIRECTORY(library)

SET(TK_GENERIC_SRCS
  generic/tk3d.c
  generic/tkArgv.c
  generic/tkAtom.c
  generic/tkBind.c
  generic/tkBitmap.c
	generic/tkBusy.c
  generic/tkClipboard.c
  generic/tkCmds.c
  generic/tkColor.c
  generic/tkConfig.c
  generic/tkCursor.c
  generic/tkError.c
  generic/tkEvent.c
  generic/tkFocus.c
  generic/tkFont.c
	generic/tkGet.c
  generic/tkGC.c
  generic/tkGeometry.c
  generic/tkGrab.c
  generic/tkGrid.c
	generic/tkConsole.c
  generic/tkMain.c
	generic/tkOption.c
	generic/tkPack.c
	generic/tkPlace.c
	generic/tkSelect.c
	generic/tkStyle.c
	generic/tkUndo.c
	generic/tkUtil.c
	generic/tkVisual.c
	generic/tkWindow.c
	generic/tkButton.c
	generic/tkObj.c
	generic/tkEntry.c
	generic/tkFrame.c
	generic/tkListbox.c
  generic/tkMenu.c
  generic/tkMenubutton.c
	generic/tkMenuDraw.c
  generic/tkMessage.c
  generic/tkPanedWindow.c
  generic/tkScale.c
  generic/tkScrollbar.c
  generic/tkStubInit.c
	generic/tkCanvas.c
	generic/tkCanvArc.c
	generic/tkCanvBmap.c
	generic/tkCanvImg.c
	generic/tkCanvLine.c
	generic/tkCanvPoly.c
	generic/tkCanvPs.c
	generic/tkCanvText.c
	generic/tkCanvUtil.c
	generic/tkCanvWind.c
	generic/tkRectOval.c
	generic/tkTrig.c
	generic/tkImage.c
	generic/tkImgBmap.c
	generic/tkImgGIF.c
	generic/tkImgPNG.c
	generic/tkImgPPM.c
	generic/tkImgPhoto.c
	generic/tkImgPhInstance.c
  generic/tkText.c
  generic/tkTextBTree.c
  generic/tkTextDisp.c
  generic/tkTextImage.c
  generic/tkTextIndex.c
  generic/tkTextMark.c
  generic/tkTextTag.c
  generic/tkTextWind.c
	generic/tkOldConfig.c
  )

SET(TK_HDRS
  generic/tk.h
  generic/tkDecls.h
  generic/tkPlatDecls.h
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
  unix/tkUnixScale.c
  unix/tkUnixScrlbr.c
  unix/tkUnixSelect.c
  unix/tkUnixSend.c
  unix/tkUnixWm.c
  unix/tkUnixXId.c
  )

IF(TK_ENABLE_XFT)
  SET(TK_X11_SRCS ${TK_X11_SRCS} unix/tkUnixRFont.c)
ELSE(TK_ENABLE_XFT)
  SET(TK_X11_SRCS ${TK_X11_SRCS} unix/tkUnixFont.c)
ENDIF(TK_ENABLE_XFT)

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
	macosx/ttkMacOSXTheme.c
  )

SET(TK_WIN_SRCS
  xlib/xcolors.c
  xlib/xdraw.c
  xlib/xgc.c
  xlib/ximage.c
  xlib/xutil.c
  win/stubs.c
  win/rc/tk.rc
  generic/tkFileFilter.c
  generic/tkMacWinMenu.c
  generic/tkPointer.c
  generic/tkImgUtil.c
  unix/tkUnixMenubu.c
  unix/tkUnixScale.c
  win/tkWin32Dll.c
  win/tkWin3d.c
  win/tkWinButton.c
  win/tkWinClipboard.c
  win/tkWinColor.c
  win/tkWinConfig.c
  win/tkWinCursor.c
  win/tkWinDialog.c
  win/tkWinDraw.c
  win/tkWinEmbed.c
  win/tkWinFont.c
  win/tkWinImage.c
  win/tkWinInit.c
  win/tkWinKey.c
  win/tkWinMenu.c
  win/tkWinPixmap.c
  win/tkWinPointer.c
  win/tkWinRegion.c
  win/tkWinScrlbr.c
  win/tkWinSend.c
  win/tkWinSendCom.c
  win/tkWinTest.c
  win/tkWinWindow.c
  win/tkWinWm.c
  win/tkWinX.c
  win/ttkWinMonitor.c
  win/ttkWinTheme.c
  win/ttkWinXPTheme.c
	win/tkMainASCII.c
  )

SET_PROPERTY(SOURCE win/tkMainASCII.c APPEND PROPERTY COMPILE_DEFINITIONS TK_ASCII_MAIN)

SET(TK_SRCS ${TK_GENERIC_SRCS} ${TTK_SRCS})

IF(WIN32)
  SET(TK_SRCS ${TK_SRCS} ${TK_WIN_SRCS})
ELSE(WIN32)
  IF(TK_ENABLE_AQUA)
    SET(TK_SRCS ${TK_SRCS} ${TK_AQUA_SRCS})
  ELSE(TK_ENABLE_AQUA)
    SET(TK_SRCS ${TK_SRCS} ${TK_X11_SRCS})
  ENDIF(TK_ENABLE_AQUA)
ENDIF(WIN32)

SET(TK_INCLUDE_PATH ${TK_SOURCE_DIR}/generic ${TK_SOURCE_DIR}/bitmaps ${TK_BINARY_DIR}/include)
IF(WIN32)
  SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/win ${TK_SOURCE_DIR}/xlib ${TK_SOURCE_DIR}/xlib/X11)
ELSE(WIN32)
  SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/unix)
  IF(APPLE)
    SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SOURCE_DIR}/macosx)
    IF(TK_ENABLE_AQUA)
      SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${X11_INCLUDE_DIR})
    ENDIF(TK_ENABLE_AQUA)
  ENDIF(APPLE)
ENDIF(WIN32)

IF(NOT WIN32)
  SET(WISH_SRCS unix/tkAppInit.c)
ELSE(NOT WIN32)
  SET(WISH_SRCS win/winMain.c)# win/rc/wish.rc)
ENDIF(NOT WIN32)

IF(X11_INCLUDE_DIR)
  SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS} ${X11_INCLUDE_DIR})
ENDIF(X11_INCLUDE_DIR)
IF(X11_Xft_INCLUDE_PATH)
  SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS} ${X11_Xft_INCLUDE_PATH})
ENDIF(X11_Xft_INCLUDE_PATH)
IF(X11_Xrender_INCLUDE_PATH)
  SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS} ${X11_Xrender_INCLUDE_PATH})
ENDIF(X11_Xrender_INCLUDE_PATH)
IF(X11_Xscreensaver_INCLUDE_PATH)
  SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS} ${X11_Xscreensaver_INCLUDE_PATH})
ENDIF(X11_Xscreensaver_INCLUDE_PATH)

#TODO - figure out how to properly test this
add_definitions(-DHAVE_XKBKEYCODETOKEYSYM=1)
add_definitions(-DXKEYCODETOKEYSYM_IS_DEPRECATED=1)

include_directories(
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
	${FREETYPE_INCLUDE_DIRS}
	${TK_X11_INCLUDE_DIRS}
	)

SET(X11_TK_LIBS ${X11_X11_LIB} ${X11_Xext_LIB})
IF(TK_ENABLE_XSS AND X11_Xscreensaver_LIB)
	SET(X11_TK_LIBS ${X11_TK_LIBS} ${X11_Xscreensaver_LIB})
ENDIF(TK_ENABLE_XSS AND X11_Xscreensaver_LIB)
IF(X11_Xft_LIB)
  SET(X11_TK_LIBS ${X11_TK_LIBS} ${X11_Xft_LIB})
  add_definitions(-DHAVE_LIBXFT=1)
  add_definitions(-DHAVE_XFT=1)
ENDIF(X11_Xft_LIB)
IF(X11_Xrender_LIB)
  SET(X11_TK_LIBS ${X11_TK_LIBS} ${X11_Xrender_LIB})
ENDIF(X11_Xrender_LIB)

IF(NOT LIB_DIR)
  SET(LIB_DIR lib)
ENDIF(NOT LIB_DIR)

IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${LIB_DIR})
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)

add_library(tkstub STATIC ${TK_STUB_SRCS} ${TTK_STUB_SRCS})
CHECK_C_COMPILER_FLAG(-fPIC PIC_COMPILER_FLAG)
IF(PIC_COMPILER_FLAG)
  SET_TARGET_PROPERTIES(tkstub PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF(PIC_COMPILER_FLAG)
install(TARGETS tkstub 
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
SET_PROPERTY(TARGET tkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)

IF(MINGW)
  set(TK_MINGW_LIBS comctl32 m dinput8 dxguid dxerr8 user32 gdi32 winmm imm32 ole32 oleaut32 shell32 version uuid)
ENDIF(MINGW)

add_library(tk ${TK_SRCS})
IF(WIN32)
  target_link_libraries(tk tkstub ${TCL_STUB_LIBRARY} ${TCL_THREADS_LIB} ${X11_TK_LIBS} ${TK_MINGW_LIBS})
ELSE(WIN32)
  target_link_libraries(tk tkstub ${TCL_STUB_LIBRARY} ${TCL_THREADS_LIB}
	  ${X11_TK_LIBS} ${CoreFoundation_LIBRARIES} ${CARBON_FRAMEWORK}
	  ${COCOA_FRAMEWORK} ${IOKit_LIBRARIES} ${FREETYPE_LIBRARIES}
	  ${PNG_LIBRARY} ${FONTCONFIG_LIBRARIES} ${M_LIBRARY})
ENDIF(WIN32)
install(TARGETS tk
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
SET_TARGET_PROPERTIES(tk PROPERTIES VERSION ${TK_VERSION} SOVERSION ${TK_VERSION_MAJOR}.${TK_VERSION_MINOR})
SET_PROPERTY(TARGET tk APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
IF(TK_ENABLE_AQUA)
  SET_PROPERTY(TARGET tk APPEND PROPERTY COMPILE_FLAGS "-std=gnu99 -x objective-c -fobjc-gc")
ENDIF(TK_ENABLE_AQUA)

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/tcl_pkgindex.cmake)
tcl_pkgindex(tk Tk ${TK_VERSION})

install(FILES ${TK_HDRS} DESTINATION include)

# Set up build directory copies of the public headers
foreach(hdrfile ${TK_HDRS})
	get_filename_component(hf ${hdrfile} NAME)
	if(NOT CMAKE_CONFIGURATION_TYPES)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/${hf} COPYONLY)
	else(NOT CMAKE_CONFIGURATION_TYPES)
		foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
			string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
			configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${INCLUDE_DIR}/${hf} COPYONLY)
		endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(hdrfile ${LIBRARY_FILES})


IF(WIN32)
  add_executable(wish WIN32 ${WISH_SRCS})
  target_link_libraries(wish tcl tk imm32.lib comctl32.lib)
ELSE(WIN32)
  add_executable(wish ${WISH_SRCS})
  target_link_libraries(wish ${TCL_LIBRARY} tk ${CoreFoundation_LIBRARIES}
	  ${CARBON_FRAMEWORK} ${COCOA_FRAMEWORK} ${IOKit_LIBRARIES}
	  ${TCL_THREADS_LIB} ${M_LIBRARY} ${FREETYPE_LIBRARIES} ${PNG_LIBRARY} ${FONTCONFIG_LIBRARIES})
  IF(TK_ENABLE_AQUA)
    SET_PROPERTY(TARGET wish APPEND PROPERTY COMPILE_FLAGS "-std=gnu99 -x objective-c -fobjc-gc")
  ENDIF(TK_ENABLE_AQUA)
ENDIF(WIN32)
SET_TARGET_PROPERTIES(wish PROPERTIES VERSION ${TK_VERSION_MAJOR}.${TK_VERSION_MINOR})
install(TARGETS wish DESTINATION bin)

IF(NOT WIN32)
	FILE(COPY unix/tkAppInit.c DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/tk${TK_VERSION})
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	 FILE(COPY unix/tkAppInit.c DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/tk${TK_VERSION})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  INSTALL(FILES unix/tkAppInit.c DESTINATION lib/tk${TK_VERSION})
ENDIF(NOT WIN32)
