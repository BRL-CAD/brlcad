include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	)

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)

# Quiet all warnings in this directory
IF(BRLCAD-DISABLE_SRC_OTHER_WARN)
	add_definitions(-w)
ENDIF(BRLCAD-DISABLE_SRC_OTHER_WARN)

# Most third party items have a list calling out files for distcheck - these
# are stored in files in the dlists directory.  Ignore that directory for
# distcheck
FILE(GLOB dlists "*.dist")
FOREACH(ITEM ${dlists})
	get_filename_component(dlist ${ITEM} NAME)
	DISTCHECK_IGNORE_ITEM(${dlist})
ENDFOREACH(ITEM ${dlists})

#---------------------------------------------------------------------------------
# Optionally Building Local Copies of External Libraries 
#
# Need to get a bit fancy here - since we want to be able to turn local libs
# on/off at will, need to be careful about how we handle key variables. Approach
# is to stash values in BRLCAD_* prefixed versions, then restore them after
# the various Find*.cmake commands do their thing.  If NOT doing local build,
# then the BRLCAD_* and standard variables will be the same.  It remains to be
# seen if there will be enough consistency for a macro - the variables that
# need to be overridden will vary depending on the particular tests made by each
# subdirectory's CMake logic.
#---------------------------------------------------------------------------------

# libregex library -  needed by tools, so do this one first.
THIRD_PARTY_OPTION(REGEX regex)
THIRD_PARTY_SUBDIR(REGEX libregex)
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)

#---------------------------------------------------------------------------------
# Local Copies of Tools
#---------------------------------------------------------------------------------
OPTION(BUILD_LOCAL_TOOLS "Build local copies of m4, byacc and flex" OFF)
MARK_AS_ADVANCED(BUILD_LOCAL_TOOLS)
IF(BUILD_LOCAL_TOOLS)
# Build Berkeley Yacc - guarantees certain extensions added originally by GNU
# Bison are supported, as well as yacc itself (problematic on, say, Windows.)
# Need to upgrade FindYacc.cmake to test for advanced Bison options
# options and be smarter - in the meantime just turn this on.  Not used yet,
# for now just testing the build - the CMake script is basic and doesn't
# implement any of the checks in byacc's aclocal.m4 or use a config.h file.
ADD_SUBDIRECTORY(byacc)

# Preparing to build flex as well - modern implementations apparently are
# heavily dependent on m4, so the simplest path is to get the simplest
# m4 that can compile flex into a working state.  Using NetBSD's port of the OpenBSD
# m4 implementation - very small, about 280K for m4 plus portability files,
# and that will be smaller once we hook into byacc to generate a couple
# of the files.
ADD_SUBDIRECTORY(m4)

# This is a very minimal flex build.  Need to improve this by getting the
# LOCATION of the m4 target from the above build and pass that in to flex
# for it's M4 location definition.  Also should probably remove the
# environment variable check for M4 from flex to avoid unexpected behavior - 
# using our m4 will give consistent behavior (once we test it out and make
# sure it IS correct...) and a system m4 might do unexpected things.
ADD_SUBDIRECTORY(flex)
ENDIF(BUILD_LOCAL_TOOLS)

DISTCHECK_IGNORE_ITEM(byacc)
DISTCHECK_IGNORE_ITEM(m4)
DISTCHECK_IGNORE_ITEM(flex)

# Ignore the boost subdir - it's only used for the boost headers and is not
# compiled
DISTCHECK_IGNORE_ITEM(boost)

# The incrtTcl subdirectory isn't directly referenced, but its subdirectories
# are used - mark incrTcl as ignored
DISTCHECK_IGNORE_ITEM(incrTcl)

# zlib Library
# The autotools build needs this file, but the CMake zlib build doesn't tolerate 
# it - remove it if it's there. 
THIRD_PARTY_OPTION(ZLIB zlib)
THIRD_PARTY_SUBDIR(ZLIB libz)

# libtermlib Library
#Only on WIN32 will the combination of the build search tests and
#libtermlib fail to find a valid term, so set variables accordingly
IF(NOT WIN32)
       SET(HAVE_TERMLIB 1 CACHE BOOL "Have some termlib" FORCE)
ELSE(NOT WIN32)
       SET(BRLCAD_BUILD_LOCAL_TERMLIB_FORCE_OFF ON CACHE BOOL "Disable libtermlib build." FORCE)
		 MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TERMLIB_FORCE_OFF)
       SET(HAVE_TERMLIB 0 CACHE BOOL "Have some termlib" FORCE)
ENDIF(NOT WIN32)
MARK_AS_ADVANCED(HAVE_TERMLIB)

IF(HAVE_TERMLIB)
   THIRD_PARTY_OPTION(TERMLIB termlib)
	THIRD_PARTY_SUBDIR(TERMLIB libtermlib)
	IF(BRLCAD_BUILD_LOCAL_TERMLIB)
		SET(HAVE_TERMCAP_H 1 CACHE BOOL "src/other termlib is on" FORCE)
	ENDIF(BRLCAD_BUILD_LOCAL_TERMLIB)
ELSE(HAVE_TERMLIB)
	DISTCHECK_IGNORE_ITEM(libtermlib)
ENDIF(HAVE_TERMLIB)

# libpng Library - Checks for ZLIB, so need to handle libpng AFTER zlib to set the variables
# if using a local copy of zlib.  If not using a local copy of zlib, FindZLIB results will 
# be identical in both cases so there is no danger of harming the libpng setup.  The PNG
# CMake system needs some options set and some variables translated to fit BRL-CAD's
# standard assumptions, so handle that here as well.
THIRD_PARTY_OPTION(PNG png)
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
THIRD_PARTY_SUBDIR(PNG libpng)
IF(BRLCAD_BUILD_LOCAL_PNG)
	# PNG_LIB_NAME doesn't automatically propagate to toplevel due to scoping of CMake variables - 
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)
	SET(PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_PNG)
	# The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than PNG_INCLUDE_DIR, so
	# a generic macro won't pick up the setting - handle it here
	SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_PNG)
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
THIRD_PARTY_OPTION(UTAHRLE utahrle)
THIRD_PARTY_SUBDIR(UTAHRLE libutahrle)
IF(BRLCAD_BUILD_LOCAL_UTAHRLE)
	SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
	MARK_AS_ADVANCED(UTAHRLE_INCLUDE_DIR)
	MARK_AS_ADVANCED(BRLCAD_UTAHRLE_INCLUDE_DIR)
ENDIF(BRLCAD_BUILD_LOCAL_UTAHRLE)


# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.
ADD_SUBDIRECTORY(URToolkit)
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/URToolkit.dist URToolkit_ignore_files)
DISTCHECK_IGNORE(URToolkit URToolkit_ignore_files)

# Tcl/Tk presents a number of complexities for BRL-CAD and CMake.  BRL-CAD requires
# far more knowledge of the details of a Tcl/Tk installation than most programs,
# which unfortunately means the standard FindTCL.cmake is not sufficient. We have our
# own FindTCL routine which handles Tcl/Tk and its stubs, so trigger this using
# the THIRD_PARTY_OPTION for TCL.

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
SET(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
SET(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

IF(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 1)
ELSE(BRLCAD-ENABLE_TK)
	SET(TCL_REQUIRE_TK 0)
ENDIF(BRLCAD-ENABLE_TK)

THIRD_PARTY_OPTION(TCL tcl)
IF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE BOOL "local tk build" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" OFF)
	SET(BRLCAD_BUILD_LOCAL_TK OFF CACHE BOOL "local tk build" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TK)
IF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	ADD_SUBDIRECTORY(tcl)
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
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tcl.dist tcl_ignore_files)
	DISTCHECK_IGNORE(tcl tcl_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
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
	DISTCHECK_IGNORE_ITEM(tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)


IF(BRLCAD_BUILD_LOCAL_TK)
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
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tk.dist tk_ignore_files)
	DISTCHECK_IGNORE(tk tk_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_TK)
	BRLCAD_INCLUDE_FILE(tk.h HAVE_TK_H)
	DISTCHECK_IGNORE_ITEM(tk)
ENDIF(BRLCAD_BUILD_LOCAL_TK)
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

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindTclPackage.cmake)
IF(NOT BRLCAD_BUILD_LOCAL_TCL)
	IF(TCL_TCLSH_EXECUTABLE)
		FIND_TCL_PACKAGE(${TCL_TCLSH_EXECUTABLE} Itcl)
	ENDIF(TCL_TCLSH_EXECUTABLE)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(TCL_WISH_EXECUTABLE)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Itk)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Iwidgets)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Tkhtml)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} tkpng)
			FIND_TCL_PACKAGE(${TCL_WISH_EXECUTABLE} Tktable)
		ENDIF(TCL_WISH_EXECUTABLE)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
ELSE(NOT BRLCAD_BUILD_LOCAL_TCL)
	OPTION(BRLCAD_BUILD_LOCAL_ITCL "Build the local ITCL package" ON)
	SET(BRLCAD_BUILD_LOCAL_ITCL ON CACHE BOOL "Build the local ITCL package" FORCE)
	IF(BRLCAD_BUILD_LOCAL_TK)
		OPTION(BRLCAD_BUILD_LOCAL_ITK "Build the local ITK package" ON)
		SET(BRLCAD_BUILD_LOCAL_ITK ON CACHE BOOL "Build the local ITK package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_IWIDGETS "Build the local IWIDGETS package" ON)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS ON CACHE BOOL "Build the local IWIDGETS package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKHTML "Build the local TKHTML package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKHTML ON CACHE BOOL "Build the local TKHTML package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKPNG "Build the local TKPNG package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKPNG ON CACHE BOOL "Build the local TKPNG package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKTABLE "Build the local TKTABLE package" ON)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE ON CACHE BOOL "Build the local TKTABLE package" FORCE)
	ELSE(BRLCAD_BUILD_LOCAL_TK)
		OPTION(BRLCAD_BUILD_LOCAL_ITK "Build the local ITK package" OFF)
		SET(BRLCAD_BUILD_LOCAL_ITK OFF CACHE BOOL "Build the local ITK package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_IWIDGETS "Build the local IWIDGETS package" OFF)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS OFF CACHE BOOL "Build the local IWIDGETS package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKHTML "Build the local TKHTML package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKHTML OFF CACHE BOOL "Build the local TKHTML package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKPNG "Build the local TKPNG package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKPNG OFF CACHE BOOL "Build the local TKPNG package" FORCE)
		OPTION(BRLCAD_BUILD_LOCAL_TKTABLE "Build the local TKTABLE package" OFF)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE OFF CACHE BOOL "Build the local TKTABLE package" FORCE)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)

# Handle Tcl/Tk packages
IF(BRLCAD_BUILD_LOCAL_ITCL)
	SET(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
	IF(NOT BRLCAD_BUILD_LOCAL_TCL)
		IF(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ELSE(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ENDIF(WIN32)
		get_filename_component(TCL_BIN_PREFIX ${TCL_LIBRARY} PATH)
	ELSE(NOT BRLCAD_BUILD_LOCAL_TCL)
		SET(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)
	ADD_SUBDIRECTORY(incrTcl/itcl)
	ADD_DEPENDENCIES(itcl tcl)
	SET(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_LIBRARY)
	SET(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITCL_STUB_LIBRARY)
	get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
	SET(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
	MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
IF(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/itcl.dist itcl_ignore_files)
	DISTCHECK_IGNORE(incrTcl/itcl itcl_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
	DISTCHECK_IGNORE_ITEM(incrTcl/itcl)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
MARK_AS_ADVANCED(ITCL_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_ITCL)


IF(BRLCAD_BUILD_LOCAL_ITK)
	SET(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
	SET(TK_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tk)
	SET(ITCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl)
	IF(NOT BRLCAD_BUILD_LOCAL_TCL)
		IF(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ELSE(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ENDIF(WIN32)
		get_filename_component(TCL_BIN_PREFIX ${TCL_LIBRARY} PATH)
	ELSE(NOT BRLCAD_BUILD_LOCAL_TCL)
		SET(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)

	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ELSE(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ENDIF(WIN32)
		get_filename_component(TK_BIN_PREFIX ${TK_LIBRARY} PATH)
	ELSE(NOT BRLCAD_BUILD_LOCAL_TK)
		SET(TK_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
	IF(NOT BRLCAD_BUILD_LOCAL_ITCL)
		SET(ITCL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl/generic)
		get_filename_component(ITCL_BIN_PREFIX ${ITCL_LIBRARY} PATH)
	ELSE(NOT BRLCAD_BUILD_LOCAL_ITCL)
		SET(ITCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
	ENDIF(NOT BRLCAD_BUILD_LOCAL_ITCL)
	ADD_SUBDIRECTORY(incrTcl/itk)
	ADD_DEPENDENCIES(itk itcl tcl tk)
	SET(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
	MARK_AS_ADVANCED(ITK_LIBRARY)
	SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITK)
   SET(ITK_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITK)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")
MARK_AS_ADVANCED(ITK_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_ITK)
IF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL ON CACHE BOOL "incrtcl build status" FORCE)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/itk.dist itk_ignore_files)
	DISTCHECK_IGNORE(incrTcl/itk itk_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF CACHE BOOL "incrtcl build status" FORCE)
	DISTCHECK_IGNORE_ITEM(incrTcl/itk)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_INCRTCL)


IF(BRLCAD_BUILD_LOCAL_IWIDGETS)
	ADD_SUBDIRECTORY(iwidgets)
	SET(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.dist iwidgets_ignore_files)
	DISTCHECK_IGNORE(iwidgets iwidgets_ignore_files)
ELSE(BRLCAD_BUILD_LOCAL_IWIDGETS)
   SET(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
	DISTCHECK_IGNORE_ITEM(iwidgets)
ENDIF(BRLCAD_BUILD_LOCAL_IWIDGETS)
FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
MARK_AS_ADVANCED(IWIDGETS_VERSION)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_IWIDGETS)


IF(BRLCAD_BUILD_LOCAL_TKHTML)
	ADD_SUBDIRECTORY(tkhtml)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tkhtml.dist tkhtml_ignore_files)
	DISTCHECK_IGNORE(tkhtml tkhtml_ignore_files)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tkhtml tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tkhtml tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ELSE(BRLCAD_BUILD_LOCAL_TKHTML)
	DISTCHECK_IGNORE_ITEM(tkhtml)
ENDIF(BRLCAD_BUILD_LOCAL_TKHTML)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKHTML)


IF(BRLCAD_BUILD_LOCAL_TKPNG)
	ADD_SUBDIRECTORY(tkpng)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tkpng.dist tkpng_ignore_files)
	DISTCHECK_IGNORE(tkpng tkpng_ignore_files)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(tkpng tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(tkpng tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ELSE(BRLCAD_BUILD_LOCAL_TKPNG)
	DISTCHECK_IGNORE_ITEM(tkpng)
ENDIF(BRLCAD_BUILD_LOCAL_TKPNG)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKPNG)


IF(BRLCAD_BUILD_LOCAL_TKTABLE)
	ADD_SUBDIRECTORY(tktable)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tktable.dist tktable_ignore_files)
	DISTCHECK_IGNORE(tktable tktable_ignore_files)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tktable tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tktable tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ELSE(BRLCAD_BUILD_LOCAL_TKTABLE)
	DISTCHECK_IGNORE_ITEM(tktable)
ENDIF(BRLCAD_BUILD_LOCAL_TKTABLE)
MARK_AS_ADVANCED(BRLCAD_BUILD_LOCAL_TKTABLE)


# OpenNURBS Library
THIRD_PARTY_OPTION(OPENNURBS openNURBS)
THIRD_PARTY_SUBDIR(OPENNURBS openNURBS)
IF(BRLCAD_BUILD_LOCAL_OPENNURBS)
	get_directory_property(OPENNURBS_HEADERS DIRECTORY openNURBS DEFINITION	OPENNURBS_HEADERS)
	DISTCHECK_IGNORE(openNURBS OPENNURBS_HEADERS)
ENDIF(BRLCAD_BUILD_LOCAL_OPENNURBS)

# STEP Class Libraries
# If we don't have Lex/Yacc, we can't do STEP - conditionalize
IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	THIRD_PARTY_OPTION(SCL scl)
	THIRD_PARTY_SUBDIR(SCL step)
	IF(BRLCAD_BUILD_LOCAL_SCL)
		SET(BRLCAD_SCL_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/step/include" CACHE STRING "Directory containing SCL headers" FORCE)
		MARK_AS_ADVANCED(BRLCAD_SCL_INCLUDE_DIR)
	ENDIF(BRLCAD_BUILD_LOCAL_SCL)
ELSE(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	DISTCHECK_IGNORE_ITEM(step)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)

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
	MARK_AS_ADVANCED(TOGL_INCLUDE_DIRS)
	ADD_SUBDIRECTORY(togl)
	FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/togl.dist togl_ignore_files)
	DISTCHECK_IGNORE(togl togl_ignore_files)
ELSE(BRLCAD-ENABLE_OPENGL)
	DISTCHECK_IGNORE_ITEM(togl)
ENDIF(BRLCAD-ENABLE_OPENGL)

# jove editor - technically this isn't a library,
# but traditionally it's grouped into the "enable-all"
# logic. Almost certainly won't build on Windows, and
# it's not worth trying to make it do so.  Defaults to
# on if BRLCAD-ENABLE_ALL_LOCAL_LIBS is enabled before
# the first run of CMake, otherwise defaults to off.
IF(BRLCAD-ENABLE_JOVE)
	IF(NOT WIN32)
		ADD_SUBDIRECTORY(jove)
		FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/jove.dist jove_ignore_files)
		DISTCHECK_IGNORE(jove jove_ignore_files)
	ELSE(NOT WIN32)
		MESSAGE("jove was requested, but does not build on Windows - disabling")
		SET(BRLCAD-ENABLE_JOVE OFF CACHE BOOL "jove disabled on WIN32" FORCE)
		DISTCHECK_IGNORE_ITEM(jove)
	ENDIF(NOT WIN32)
ELSE(BRLCAD-ENABLE_JOVE)
  DISTCHECK_IGNORE_ITEM(jove)
ENDIF(BRLCAD-ENABLE_JOVE)

IF(BRLCAD-ENABLE_OSL)
    ADD_SUBDIRECTORY(osl)
ENDIF(BRLCAD-ENABLE_OSL)

