#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)


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

# zlib Library
# The autotools build needs this file, but the CMake zlib build doesn't tolerate 
# it - remove it if it's there. 
FILE(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/libz/zconf.h)
THIRD_PARTY_OPTION(ZLIB zlib)
THIRD_PARTY_SUBDIR(ZLIB libz)

# libtermlib Library
THIRD_PARTY_OPTION(TERMLIB termlib)
THIRD_PARTY_SUBDIR(TERMLIB libtermlib)

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
THIRD_PARTY_SUBDIR(PNG libpng)
IF(BRLCAD_BUILD_LOCAL_PNG)
	# PNG_LIB_NAME doesn't automatically propogate to toplevel due to scoping of CMake variables - 
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)
	SET(PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_PNG)
	# The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than PNG_INCLUDE_DIR, so
	# a generic macro won't pick up the setting - handle it here
	SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_PNG)

# libregex Library
THIRD_PARTY_OPTION(REGEX regex)
THIRD_PARTY_SUBDIR(REGEX libregex)
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)

# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on and the
# include directory for utahrle are different, so override the macro's setting
# of UTAHRLE_INCLUDE_DIR here.
THIRD_PARTY_OPTION(UTAHRLE utahrle)
THIRD_PARTY_SUBDIR(UTAHRLE libutahrle)
IF(BRLCAD_BUILD_LOCAL_UTAHRLE)
	SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_UTAHRLE)


# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.
ADD_SUBDIRECTORY(URToolkit)


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
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE STRING "local tk build" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" OFF)
	SET(BRLCAD_BUILD_LOCAL_TK OFF CACHE STRING "local tk build" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
IF(BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE STRING "local tk build" FORCE)
ENDIF(BRLCAD-ENABLE_ALL_LOCAL_LIBS)
IF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	ADD_SUBDIRECTORY(tcl)
	SET(TCL_LIBRARY tcl CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES tclstub CACHE STRING "TCL_LIBRARY" FORCE)
	get_directory_property(TCL_INCLUDE_DIRS DIRECTORY tcl DEFINITION TCL_INCLUDE_DIRS)
	SET(TCL_INCLUDE_DIRS "${TCL_INCLUDE_DIRS}" CACHE STRING "Tcl include paths" FORCE)
	SET(TCL_TCLSH_EXECUTABLE tclsh CACHE STRING "Tcl shell" FORCE)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TCL_H 1\n")
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"\"\n")
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
	FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SYSTEM_INITTCL_PATH \"${tcl_inittcl}\"\n")
ENDIF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)

IF(BRLCAD_BUILD_LOCAL_TK OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	ADD_SUBDIRECTORY(tk)
	SET(TCL_TK_LIBRARY tk CACHE STRING "TCL_TK_LIBRARY" FORCE)
	SET(TCL_STUB_LIBRARIES ${TCL_STUB_LIBRARIES} tkstub CACHE STRING "TCL_TK_LIBRARY" FORCE)
	get_directory_property(TK_INCLUDE_PATH DIRECTORY tk DEFINITION TK_INCLUDE_PATH)
	SET(TK_INCLUDE_PATH "${TK_INCLUDE_PATH}" CACHE STRING "Tk include paths" FORCE)
	SET(TCL_WISH_EXECUTABLE wish CACHE STRING "Tk shell" FORCE)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_TK_H 1\n")
	ADD_DEPENDENCIES(tk tcl)
ELSE(BRLCAD_BUILD_LOCAL_TK OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	BRLCAD_INCLUDE_FILE(tk.h HAVE_TK_H)
ENDIF(BRLCAD_BUILD_LOCAL_TK OR BRLCAD-ENABLE_ALL_LOCAL_LIBS)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h file
IF(TCL_TK_LIBRARY)
	SET(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
	FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_TK 1\n")
ELSE(TCL_TK_LIBRARY)
	SET(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
ENDIF(TCL_TK_LIBRARY)

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
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)

# Handle Tcl/Tk packages
#THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(ITCL itcl src/other/incrTcl/itcl "" "--with-tcl=${TCL_CONF_PREFIX};--with-tcl-private=${CMAKE_SOURCE_DIR}/src/other/tcl")
IF(BRLCAD_BUILD_LOCAL_ITCL)
	IF(NOT BRLCAD_BUILD_LOCAL_TCL)
		IF(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ELSE(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
		ENDIF(WIN32)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)
	ADD_SUBDIRECTORY(incrTcl/itcl)
	ADD_DEPENDENCIES(itcl tcl)
	SET(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
	get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
	SET(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
IF(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")

#THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(ITK itk src/other/incrTcl/itk "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX};--with-tcl-private=${CMAKE_SOURCE_DIR}/src/other/tcl;--with-tk-private=${CMAKE_SOURCE_DIR}/src/other/tk")
IF(BRLCAD_BUILD_LOCAL_ITK)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ELSE(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
		ENDIF(WIN32)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
	ADD_SUBDIRECTORY(incrTcl/itk)
	ADD_DEPENDENCIES(itk itcl tcl tk)
	SET(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
	SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITK)
   SET(ITK_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITK)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")

IF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL ON CACHE STRING "incrtcl build status" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF CACHE STRING "incrtcl build status" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)

#THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(IWIDGETS iwidgets src/other/incrTcl/iwidgets "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX};--with-itcl=${BRLCAD_SOURCE_DIR}/src/other/incrTcl")
IF(BRLCAD_BUILD_LOCAL_IWIDGETS)
	ADD_SUBDIRECTORY(incrTcl/iwidgets)
	SET(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_IWIDGETS)
   SET(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_IWIDGETS)
FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")

#THIRD_PARTY_AUTORECONF_EXTERNAL_PROJECT(TKHTML tkhtml src/other/tkhtml "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TKHTML)
        ADD_SUBDIRECTORY(tkhtml)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tkhtml tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tkhtml tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKHTML)

#THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TKPNG tkpng src/other/tkpng "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TKPNG)
	ADD_SUBDIRECTORY(tkpng)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(tkpng tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(tkpng tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKPNG)

#THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TKTABLE tktable src/other/tktable "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TKTABLE)
	ADD_SUBDIRECTORY(tktable)
	IF(BRLCAD_BUILD_LOCAL_TCL)
	   ADD_DEPENDENCIES(Tktable tcl)
	ENDIF(BRLCAD_BUILD_LOCAL_TCL)
	IF(BRLCAD_BUILD_LOCAL_TK)
	   ADD_DEPENDENCIES(Tktable tk)
	ENDIF(BRLCAD_BUILD_LOCAL_TK)
ENDIF(BRLCAD_BUILD_LOCAL_TKTABLE)

# OpenNURBS Library
THIRD_PARTY_OPTION(OPENNURBS openNURBS)
THIRD_PARTY_SUBDIR(OPENNURBS openNURBS)

# STEP Class Libraries
# If we don't have Lex/Yacc, we can't do STEP - conditionalize
IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	THIRD_PARTY_OPTION(SCL scl)
	THIRD_PARTY_SUBDIR(SCL step)
	IF(BRLCAD_BUILD_LOCAL_SCL)
		SET(BRLCAD_SCL_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/step/include" CACHE STRING "Directory containing SCL headers" FORCE)
		SET(BRLCAD_SCL_EXPRESS_EXECUTABLE "express" CACHE STRING "SCL express binary" FORCE)
		SET(BRLCAD_SCL_SYMLINK_EXECUTABLE "symlink" CACHE STRING "SCL symlink binary" FORCE)
		SET(BRLCAD_SCL_EXPPP_EXECUTABLE "exppp" CACHE STRING "SCL exppp binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_OS_EXECUTABLE "fedex_os" CACHE STRING "SCL fedex_os binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_IDL_EXECUTABLE "fedex_idl" CACHE STRING "SCL fedex_idl binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_PLUS_EXECUTABLE "fedex_plus" CACHE STRING "SCL fedex_plus binary" FORCE)
		SET(BRLCAD_SCL_EXPPP_LIB "libexppp" CACHE STRING "SCL exppp library" FORCE)
		SET(BRLCAD_SCL_CORE_LIB "stepcore" CACHE STRING "SCL core library" FORCE)
		SET(BRLCAD_SCL_UTILS_LIB "steputils" CACHE STRING "SCL utilities library" FORCE)
		SET(BRLCAD_SCL_DAI_LIB "stepdai" CACHE STRING "SCL DAI library" FORCE)
		SET(BRLCAD_SCL_EDITOR_LIB "stepeditor" CACHE STRING "SCL editor library" FORCE)
	ELSE(BRLCAD_BUILD_LOCAL_SCL)
		SET(BRLCAD_SCL_INCLUDE_DIR "${SCL_INCLUDE_DIR}" CACHE STRING "Directory containing SCL headers" FORCE)
		SET(BRLCAD_SCL_EXPRESS_EXECUTABLE "${SCL_EXPRESS_EXECUTABLE}" CACHE STRING "SCL express binary" FORCE)
		SET(BRLCAD_SCL_SYMLINK_EXECUTABLE "${SCL_SYMLINK_EXECUTABLE}" CACHE STRING "SCL symlink binary" FORCE)
		SET(BRLCAD_SCL_EXPPP_EXECUTABLE "${SCL_EXPPP_EXECUTABLE}" CACHE STRING "SCL exppp binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_OS_EXECUTABLE "${SCL_FEDEX_OS_EXECUTABLE}" CACHE STRING "SCL fedex_os binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_IDL_EXECUTABLE "${SCL_FEDEX_IDL_EXECUTABLE}" CACHE STRING "SCL fedex_idl binary" FORCE)
		SET(BRLCAD_SCL_FEDEX_PLUS_EXECUTABLE "${SCL_FEDEX_PLUS_EXECUTABLE}" CACHE STRING "SCL fedex_plus binary" FORCE)
		SET(BRLCAD_SCL_EXPPP_LIB "${SCL_EXPPP_LIB}" CACHE STRING "SCL exppp library" FORCE)
		SET(BRLCAD_SCL_CORE_LIB "${SCL_CORE_LIB}" CACHE STRING "SCL core library" FORCE)
		SET(BRLCAD_SCL_UTILS_LIB "${SCL_UTILS_LIB}" CACHE STRING "SCL utilities library" FORCE)
		SET(BRLCAD_SCL_DAI_LIB "${SCL_DAI_LIB}" CACHE STRING "SCL DAI library" FORCE)
		SET(BRLCAD_SCL_EDITOR_LIB "${SCL_EDITOR_LIB}" CACHE STRING "SCL editor library" FORCE)
	ENDIF(BRLCAD_BUILD_LOCAL_SCL)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)

# The jama/tnt headers are installed by default - BRL-CAD requires the altered headers
# to build (I think?)
ADD_SUBDIRECTORY(tnt)
SET(TNT_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/tnt" CACHE STRING "Directory containing tnt headers" FORCE)
