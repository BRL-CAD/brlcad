#---------------------------------------------------------------------------------
# Local Copies of External Libraries
#
# BRL-CAD depends on a variety of external libraries and tools - rather than
# fail if those requirements are not satisfied, we build local copies at need.
#
# There are three overall approaches to the handling of these dependencies:
#
# 1.  Auto  - detect system libraries and use them if suitable, otherwise
#     build and use the local copy.  This is the default approach.
#
# 2.  Bundled - regardless of system conditions, build and use all bundled
#     libraries.
#
# 3.  System - fail to build if the system libraries do not satisfy
#     requirements.  This is primarily useful for distributions that want
#     to ensure packages are using external libraries.
#
# In addition to the broad toplevel control, individual libraries can also
# be overridden - for example, if the toplevel setting is for Bundled libs,
# it is still possible to request a system library in individual cases.
#
#---------------------------------------------------------------------------------

# Quiet all warnings in this directory
IF(BRLCAD-DISABLE_SRC_OTHER_WARN)
	add_definitions(-w)
ENDIF(BRLCAD-DISABLE_SRC_OTHER_WARN)

# Ideally we wouldn't need this, but in a few cases we're using BRL-CAD's
# include dirs. 
include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)

# Most third party items have a list calling out files for distcheck - these
# are stored in files in the dlists directory.  Ignore that directory for
# distcheck
FILE(GLOB dlists "*.dist")
FOREACH(ITEM ${dlists})
	get_filename_component(dlist ${ITEM} NAME)
	DISTCHECK_IGNORE_ITEM(${dlist})
ENDFOREACH(ITEM ${dlists})

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)


# libregex library -  often needed by tools, so do this one first.
THIRD_PARTY(regex libregex)
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)
#-----------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# Local Copies of Tools
#---------------------------------------------------------------------------------
THIRD_PARTY_EXECUTABLE(lemon lemon)
THIRD_PARTY_EXECUTABLE(re2c re2c)

#---------------------------------------------------------------------------------
# Third Party Sources
#---------------------------------------------------------------------------------
# Ignore the boost subdir - it's only used for the boost headers and is not
# compiled
DISTCHECK_IGNORE_ITEM(boost)

# The incrtTcl subdirectory isn't directly referenced, but its subdirectories
# are used - mark incrTcl as ignored
DISTCHECK_IGNORE_ITEM(incrTcl)

# Ignore the osl subdir
DISTCHECK_IGNORE_ITEM(osl)

# zlib Library
# The autotools build needs this file, but the CMake zlib build doesn't tolerate
# it - remove it if it's there.
THIRD_PARTY(zlib libz)

# libtermlib Library
#Only on WIN32 will the combination of the build search tests and
#libtermlib fail to find a valid term, so set variables accordingly
IF(NOT WIN32)
	SET(HAVE_TERMLIB 1 CACHE BOOL "Have some termlib" FORCE)
ELSE(NOT WIN32)
	SET(BRLCAD_TERMLIB_BUILD "System" CACHE STRING "Disable libtermlib build on WIN32." FORCE)
	MARK_AS_ADVANCED(BRLCAD_TERMLIB_BUILD)
	SET(HAVE_TERMLIB 0 CACHE BOOL "Have some termlib" FORCE)
ENDIF(NOT WIN32)
MARK_AS_ADVANCED(HAVE_TERMLIB)

IF(HAVE_TERMLIB)
   THIRD_PARTY(termlib libtermlib)
	IF(BRLCAD_TERMLIB_BUILD)
		SET(HAVE_TERMCAP_H 1 CACHE BOOL "src/other termlib is on" FORCE)
		MARK_AS_ADVANCED(HAVE_TERMCAP_H)
	ENDIF(BRLCAD_TERMLIB_BUILD)
ELSE(HAVE_TERMLIB)
	DISTCHECK_IGNORE_ITEM(libtermlib)
ENDIF(HAVE_TERMLIB)

# libpng Library - Checks for ZLIB, so need to handle libpng AFTER zlib to set the variables
# if using a local copy of zlib.  If not using a local copy of zlib, FindZLIB results will
# be identical in both cases so there is no danger of harming the libpng setup.  The PNG
# CMake system needs some options set and some variables translated to fit BRL-CAD's
# standard assumptions, so handle that here as well.
# BRL-CAD needs PNG's IO - turn it on (i.e. turn off the disabling flags)
SET(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_CONSOLE_IO)
SET(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_STDIO)
SET(SKIP_INSTALL_EXPORT ON CACHE BOOL "We dont't want export for this application" FORCE)
MARK_AS_ADVANCED(SKIP_INSTALL_EXPORT)
SET(PNG_MAN_DIR ${MAN_DIR} CACHE STRING "Set PNG_MAN_DIR to the global MAN_DIR" FORCE)
MARK_AS_ADVANCED(PNG_MAN_DIR)
SET(CMAKE_INSTALL_LIBDIR ${LIB_DIR})
THIRD_PARTY(png libpng)
IF(BRLCAD_PNG_BUILD)
	# PNG_LIB_NAME doesn't automatically propagate to toplevel due to scoping of CMake variables -
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)
	SET(PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_PNG_BUILD)
	# The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than PNG_INCLUDE_DIR, so
	# a generic macro won't pick up the setting - handle it here
	SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
ENDIF(BRLCAD_PNG_BUILD)
# Other PNG options to mark as advanced
MARK_AS_ADVANCED(PNGARG)
MARK_AS_ADVANCED(PNG_DEBUG)
MARK_AS_ADVANCED(PNG_SHARED)
MARK_AS_ADVANCED(PNG_STATIC)
MARK_AS_ADVANCED(PNG_TESTS)
MARK_AS_ADVANCED(uname_executable)


# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on and the
# include directory for utahrle are different, so override the macro's setting
# of UTAHRLE_INCLUDE_DIR here.
THIRD_PARTY(utahrle libutahrle NOSYS)
IF(BRLCAD_UTAHRLE_BUILD)
	SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
	MARK_AS_ADVANCED(UTAHRLE_INCLUDE_DIR)
	MARK_AS_ADVANCED(BRLCAD_UTAHRLE_INCLUDE_DIR)
ENDIF(BRLCAD_UTAHRLE_BUILD)


# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.
ADD_SUBDIRECTORY(URToolkit)
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/URToolkit.dist)
DISTCHECK_IGNORE(URToolkit URToolkit_ignore_files)

# Tcl/Tk presents a number of complexities for BRL-CAD and CMake.  BRL-CAD requires
# far more knowledge of the details of a Tcl/Tk installation than most programs,
# which unfortunately means the standard FindTCL.cmake is not sufficient. We have our
# own FindTCL routine which handles Tcl/Tk and its stubs, so trigger this using
# the THIRD_PARTY macro for TCL.

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
SET(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
SET(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

IF(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 1)
ELSE(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 0)
ENDIF(BRLCAD-ENABLE_TK)

THIRD_PARTY(tcl tcl)
IF(BRLCAD_TCL_BUILD)
   # Set the variables ourselves since FindTCL.cmake doesn't know about our build
	SET(TCL_LIBRARY tcl CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_LIBRARIES tcl CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_CONF_PREFIX" FORCE)
	SET(TCL_STUB_LIBRARY tclstub CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES tclstub CACHE STRING "TCL_LIBRARY" FORCE)
	get_directory_property(TCL_INCLUDE_DIRS DIRECTORY tcl DEFINITION TCL_INCLUDE_DIRS)
	SET(TCL_INCLUDE_DIRS "${TCL_INCLUDE_DIRS}" CACHE STRING "Tcl include paths" FORCE)
	SET(TCL_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic" CACHE STRING "Tcl include path" FORCE)
	SET(TCL_TCLSH_EXECUTABLE tclsh CACHE STRING "Tcl shell" FORCE)
	SET(TCL_TCLSH tclsh CACHE STRING "Tcl shell" FORCE)
	get_directory_property(TCL_VERSION_MAJOR DIRECTORY tcl DEFINITION TCL_VERSION_MAJOR)
	SET(TCL_VERSION_MAJOR "${TCL_VERSION_MAJOR}" CACHE STRING "Tcl MAJOR version" FORCE)
	get_directory_property(TCL_VERSION_MINOR DIRECTORY tcl DEFINITION TCL_VERSION_MINOR)
	SET(TCL_VERSION_MINOR "${TCL_VERSION_MINOR}" CACHE STRING "Tcl MINOR version" FORCE)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TCL_H 1\n")
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"\"\n")
	INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/tcl.dist)
	DISTCHECK_IGNORE(tcl tcl_ignore_files)
ELSE(BRLCAD_TCL_BUILD)
	BRLCAD_INCLUDE_FILE(tcl.h HAVE_TCL_H)
	# We're going to need the path to the system init.tcl for btclsh and bwish
	SET(inittcl_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl\"
set fileID [open $filename \"w\"]
puts $fileID $auto_path
close $fileID
exit
")
   SET(inittcl_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl.tcl")
	FILE(WRITE ${inittcl_scriptfile} ${inittcl_script})
	EXEC_PROGRAM(${TCL_TCLSH_EXECUTABLE} ARGS ${inittcl_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
   FILE(READ ${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl tcl_inittcl_raw)
   STRING(REGEX REPLACE "\n" "" tcl_inittcl_paths_1 ${tcl_inittcl_raw})
   STRING(REGEX REPLACE " " ";" tcl_inittcl_paths ${tcl_inittcl_paths_1})
	FIND_PATH(tcl_inittcl NAMES init.tcl PATHS ${tcl_inittcl_paths})
	MARK_AS_ADVANCED(tcl_inittcl)
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"${tcl_inittcl}\"\n")
ENDIF(BRLCAD_TCL_BUILD)
IF(TCL_REQUIRE_TK)
	IF(BRLCAD_TCL_BUILD)
		SET(BRLCAD_TK_BUILD ON)
	ENDIF(BRLCAD_TCL_BUILD)
ENDIF(TCL_REQUIRE_TK)

IF(BRLCAD_TK_BUILD)
	SET(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
	SET(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ADD_SUBDIRECTORY(tk)
   # Set the variables ourselves since FindTCL.cmake doesn't know about our build
	SET(TCL_TK_LIBRARY tk CACHE STRING "TCL_TK_LIBRARY" FORCE)
	SET(TCL_TK_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_TK_CONF_PREFIX" FORCE)
	SET(TCL_LIBRARIES ${TCL_LIBRARIES} tk CACHE STRING "TCL_LIBRARIES" FORCE)
	SET(TK_LIBRARY tk CACHE STRING "TK_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES tkstub ${TCL_STUB_LIBRARIES} CACHE STRING "TCL_TK_STUBLIBRARIES" FORCE)
	SET(TCL_TK_STUB_LIBRARY tkstub CACHE STRING "TCL_TK_STUB_LIBRARY" FORCE)
	SET(TK_STUB_LIBRARY tkstub CACHE STRING "TK_STUB_LIBRARY" FORCE)
	SET(TK_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tk/generic" CACHE STRING "Tk include path" FORCE)
	SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TK_INCLUDE_PATH} CACHE STRING "Tcl include paths" FORCE)
	IF(WIN32)
		SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib CACHE STRING "Tcl include paths" FORCE)
	ENDIF(WIN32)
	SET(TCL_WISH_EXECUTABLE wish CACHE STRING "Tk shell" FORCE)
	SET(TK_WISH wish CACHE STRING "Tk shell" FORCE)
	IF(WIN32)
		SET(TK_SYSTEM_GRAPHICS "win32" CACHE STRING "Tk system graphics" FORCE)
	ELSEIF(APPLE)
		IF(OPENGL_USE_AQUA)
			SET(TK_SYSTEM_GRAPHICS "aqua" CACHE STRING "Tk system graphics" FORCE)
		ELSE(OPENGL_USE_AQUA)
			SET(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
		ENDIF(OPENGL_USE_AQUA)
	ELSEIF(UNIX)
		SET(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
	ENDIF(WIN32)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TK_H 1\n")
	ADD_DEPENDENCIES(tk tcl)
	INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/tk.dist)
	DISTCHECK_IGNORE(tk tk_ignore_files)
	MARK_AS_ADVANCED(TK-ENABLE_FREETYPE)
ELSE(BRLCAD_TK_BUILD)
	BRLCAD_INCLUDE_FILE(tk.h HAVE_TK_H)
	DISTCHECK_IGNORE_ITEM(tk)
ENDIF(BRLCAD_TK_BUILD)
MARK_AS_ADVANCED(TK_INCLUDE_PATH)
MARK_AS_ADVANCED(TK_LIBRARY)
MARK_AS_ADVANCED(TK_STUB_LIBRARY)
MARK_AS_ADVANCED(TK_SYSTEM_GRAPHICS)
MARK_AS_ADVANCED(TK_WISH)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h file
IF(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
	SET(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
	FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_TK\n")
ELSE(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
	SET(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
ENDIF(TCL_TK_LIBRARY AND BRLCAD-ENABLE_TK)
MARK_AS_ADVANCED(HAVE_TK)

# Tcl/Tk extensions need a variety of settings provided for them - take care
# of those here.  Because system Tcl/Tk installations are not guaranteed to have
# what is needed in the way of headers, go ahead and assign includedir settings based
# on the local sources even if system versions are enabled.  Ugly, but an
# unfortunate consequence of current Tcl/Tk coding styles
SET(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
SET(TK_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tk)
SET(ITCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl)
IF(WIN32)
	SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
	SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
ELSE(WIN32)
	SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
	SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
ENDIF(WIN32)
SET(ITCL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl/generic)
IF(BRLCAD_TCL_BUILD)
	SET(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
ELSE(BRLCAD_TCL_BUILD)
	get_filename_component(TCL_BIN_PREFIX ${TCL_LIBRARY} PATH)
ENDIF(BRLCAD_TCL_BUILD)
IF(BRLCAD_TK_BUILD)
	SET(TK_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	get_directory_property(TK_X11_INCLUDE_DIRS DIRECTORY tk DEFINITION TK_X11_INCLUDE_DIRS)
ELSE(BRLCAD_TK_BUILD)
	get_filename_component(TK_BIN_PREFIX ${TCL_TK_LIBRARY} PATH)
	IF(X11_INCLUDE_DIR)
		SET(TK_X11_INCLUDE_DIRS ${X11_INCLUDE_DIR})
	ENDIF(X11_INCLUDE_DIR)
	IF(X11_Xft_INCLUDE_PATH)
		SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
			${X11_Xft_INCLUDE_PATH})
	ENDIF(X11_Xft_INCLUDE_PATH)
	IF(X11_Xrender_INCLUDE_PATH)
		SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
			${X11_Xrender_INCLUDE_PATH})
	ENDIF(X11_Xrender_INCLUDE_PATH)
	IF(X11_Xscreensaver_INCLUDE_PATH)
		SET(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
			${X11_Xscreensaver_INCLUDE_PATH})
	ENDIF(X11_Xscreensaver_INCLUDE_PATH)
ENDIF(BRLCAD_TK_BUILD)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty_TCL.cmake)

THIRD_PARTY_TCL_PACKAGE(Itcl incrTcl/itcl "${TCL_TCLSH_EXECUTABLE}" "tcl")
IF(BRLCAD_ITCL_BUILD)
	SET(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_LIBRARY)
	SET(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_STUB_LIBRARY)
	get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
	SET(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
	MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)
	SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
	SET(ITCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
ELSE(BRLCAD_ITCL_BUILD)
	find_library(ITCL_LIBRARY NAMES itcl itcl${ITCL_PACKAGE_VERSION} PATH_SUFFIXES itcl${ITCL_PACKAGE_VERSION})
	IF(NOT ITCL_LIBRARY)
		SET(${CMAKE_PROJECT_NAME}_ITCL "BUNDLED" CACHE STRING "Couldn't find ITCL_LIBRARY, even though Itcl package is present.  Build bundled itcl library." FORCE)
		THIRD_PARTY_TCL_PACKAGE(Itcl incrTcl/itcl "${TCL_TCLSH_EXECUTABLE}" "tcl")
		SET(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
		MARK_AS_ADVANCED(ITCL_LIBRARY)
		SET(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
		MARK_AS_ADVANCED(ITCL_STUB_LIBRARY)
		get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
		SET(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
		MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)
		SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
		SET(ITCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ELSE(NOT ITCL_LIBRARY)
		SET(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
		SET(ITCL_LIBRARY ${ITCL_LIBRARY} CACHE STRING "ITCL_LIBRARY" FORCE)
		get_filename_component(ITCL_BIN_PREFIX ${ITCL_LIBRARY} PATH)
	ENDIF(NOT ITCL_LIBRARY)
	MARK_AS_ADVANCED(ITCL_LIBRARY)
ENDIF(BRLCAD_ITCL_BUILD)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
MARK_AS_ADVANCED(ITCL_VERSION)

IF(BRLCAD-ENABLE_TK)
	THIRD_PARTY_TCL_PACKAGE(Itk incrTcl/itk "${TCL_WISH_EXECUTABLE}" "tcl;itcl;tk")
	IF(BRLCAD_ITK_BUILD)
		SET(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
		MARK_AS_ADVANCED(ITK_LIBRARY)
		SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
	ELSE(BRLCAD_ITK_BUILD)
		find_library(ITK_LIBRARY NAMES itk itk${ITCL_PACKAGE_VERSION} PATH_SUFFIXES itk${ITCL_PACKAGE_VERSION})
		IF(NOT ITK_LIBRARY OR NOT ITCL_LIBRARY)
			SET(${CMAKE_PROJECT_NAME}_ITK "BUNDLED" CACHE STRING "Couldn't find incrTcl libraries, even though packages are present.  Build bundled itk library." FORCE)
			THIRD_PARTY_TCL_PACKAGE(Itk incrTcl/itk "${TCL_WISH_EXECUTABLE}" "tcl;itcl;tk")
			SET(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
			MARK_AS_ADVANCED(ITK_LIBRARY)
			SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
		ELSE(NOT ITK_LIBRARY OR NOT ITCL_LIBRARY)
			SET(ITK_VERSION "${ITCL_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
			SET(ITK_LIBRARY ${ITK_LIBRARY} CACHE STRING "ITK_LIBRARY" FORCE)
			get_filename_component(ITK_BIN_PREFIX ${ITK_LIBRARY} PATH)
		ENDIF(NOT ITK_LIBRARY OR NOT ITCL_LIBRARY)
		MARK_AS_ADVANCED(ITK_LIBRARY)
	ENDIF(BRLCAD_ITK_BUILD)
	MARK_AS_ADVANCED(ITK_VERSION)

	THIRD_PARTY_TCL_PACKAGE(Iwidgets iwidgets "${TCL_WISH_EXECUTABLE}" "tcl;tk;itcl;itk")
	IF(BRLCAD_IWIDGETS_BUILD)
		SET(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
	ELSE(BRLCAD_IWIDGETS_BUILD)
		SET(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
	ENDIF(BRLCAD_IWIDGETS_BUILD)
	FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
	MARK_AS_ADVANCED(IWIDGETS_VERSION)

	THIRD_PARTY_TCL_PACKAGE(Tkhtml tkhtml "${TCL_WISH_EXECUTABLE}" "tcl;tk")
	#ADD_SUBDIRECTORY(hv3)
	#INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/hv3.dist)
	#DISTCHECK_IGNORE(hv3 hv3_ignore_files)
	#ADD_SUBDIRECTORY(sqlite3)
	#INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.dist)
	#DISTCHECK_IGNORE(sqlite3 sqlite3_ignore_files)
	DISTCHECK_IGNORE_ITEM(hv3)
	DISTCHECK_IGNORE_ITEM(sqlite3)

	THIRD_PARTY_TCL_PACKAGE(tkpng tkpng "${TCL_WISH_EXECUTABLE}" "tcl;tk")

	THIRD_PARTY_TCL_PACKAGE(Tktable tktable "${TCL_WISH_EXECUTABLE}" "tcl;tk")
ELSE(BRLCAD-ENABLE_TK)
	DISTCHECK_IGNORE_ITEM(incrTcl/itk)
	DISTCHECK_IGNORE_ITEM(iwidgets)
	DISTCHECK_IGNORE_ITEM(tkhtml)
	DISTCHECK_IGNORE_ITEM(tkpng)
	DISTCHECK_IGNORE_ITEM(tktable)
	DISTCHECK_IGNORE_ITEM(hv3)
	DISTCHECK_IGNORE_ITEM(sqlite3)
ENDIF(BRLCAD-ENABLE_TK)

# OpenNURBS Library
THIRD_PARTY(openNURBS openNURBS NOSYS)

# STEP Class Libraries
# If we don't have Lex/Yacc, we can't do STEP - conditionalize
IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	THIRD_PARTY(scl step NOSYS)
	SET(FEDEX_PLUS_EXEC fedex_plus CACHE STRING "Fedex plus executable" FORCE)
ELSE(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	DISTCHECK_IGNORE_ITEM(step)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)


# Clipper polygon clipping library - for now, we're going to use our copy
ADD_SUBDIRECTORY(clipper)
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/clipper.dist)
DISTCHECK_IGNORE(clipper clipper_ignore_files)
SET(CLIPPER_LIBRARY "clipper" CACHE STRING "Clipper library" FORCE)
SET(CLIPPER_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/clipper" CACHE STRING "Directory containing clipper header" FORCE)
MARK_AS_ADVANCED(CLIPPER_LIBRARY)
MARK_AS_ADVANCED(CLIPPER_INCLUDE_DIR)

# The jama/tnt headers are installed by default - BRL-CAD requires the altered headers
# to build (I think?)
ADD_SUBDIRECTORY(tnt)
SET(TNT_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/tnt" CACHE STRING "Directory containing tnt headers" FORCE)
DISTCHECK_IGNORE_ITEM(tnt)
MARK_AS_ADVANCED(TNT_INCLUDE_DIR)

# For now only turn on Togl for X11, but will need it everywhere eventually.  Will need
# some logic to both look for package require and the C library, as both APIs will be needed.
IF(BRLCAD-ENABLE_OPENGL)
	SET(TOGL_LIBRARIES togl CACHE STRING "TOGL_LIBRARIES" FORCE)
	SET(TOGL_STUB_LIBRARIES toglstub CACHE STRING "TOGL_STUB_LIBRARIES" FORCE)
	SET(TOGL_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/togl/include;${CMAKE_CURRENT_SOURCE_DIR}/togl/include" CACHE STRING "Togl include path" FORCE)
	MARK_AS_ADVANCED(TOGL_LIBRARIES)
	MARK_AS_ADVANCED(TOGL_STUB_LIBRARIES)
	MARK_AS_ADVANCED(TOGL_INCLUDE_DIRS)
	ADD_SUBDIRECTORY(togl)
	INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/togl.dist)
	DISTCHECK_IGNORE(togl togl_ignore_files)
ELSE(BRLCAD-ENABLE_OPENGL)
	SET(TOGL_LIBRARIES "" CACHE STRING "OpenGL disabled" FORCE)
	SET(TOGL_STUB_LIBRARIES "" CACHE STRING "OpenGL disabled" FORCE)
	SET(TOGL_INCLUDE_DIRS "" CACHE STRING "OpenGL disabled" FORCE)
	DISTCHECK_IGNORE_ITEM(togl)
ENDIF(BRLCAD-ENABLE_OPENGL)

# jove editor - technically this isn't a library,
# but traditionally it's grouped into the "enable-all"
# logic. Almost certainly won't build on Windows, and
# it's not worth trying to make it do so.  Defaults to
# on if BRLCAD_BUNDLED_LIBS is set to Bundled before
# the first run of CMake, otherwise defaults to off.
IF(BRLCAD_ENABLE_JOVE)
	IF(NOT WIN32)
		ADD_SUBDIRECTORY(jove)
		INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/jove.dist)
		DISTCHECK_IGNORE(jove jove_ignore_files)
	ELSE(NOT WIN32)
		MESSAGE("jove was requested, but does not build on Windows - disabling")
		SET(BRLCAD-ENABLE_JOVE OFF CACHE BOOL "jove disabled on WIN32" FORCE)
		DISTCHECK_IGNORE_ITEM(jove)
	ENDIF(NOT WIN32)
ELSE(BRLCAD_ENABLE_JOVE)
  DISTCHECK_IGNORE_ITEM(jove)
ENDIF(BRLCAD_ENABLE_JOVE)

IF(BRLCAD-ENABLE_OSL)
    ADD_SUBDIRECTORY(osl)
ENDIF(BRLCAD-ENABLE_OSL)

CMAKEFILES(Makefile.am)
CMAKEFILES(perplex)
