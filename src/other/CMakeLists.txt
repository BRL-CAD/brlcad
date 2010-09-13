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

# This is our "heavy hammer" flag - if this is set, assume system for
# everything.  As yet, there is no "automatic" logic to toggle specific
# libraries on if detection fails, but that may be possible.
OPTION(BRLCAD_SYSTEM_LIBS_ONLY "Use the system's libraries." OFF)
OPTION(BRLCAD_BUILD_LOCAL_LIBS "Build all the local copies of libraries." OFF)
IF(BRLCAD_BUILD_LOCAL_LIBS)
	SET(BRLCAD_SYSTEM_LIBS_ONLY OFF CACHE BOOL "Use the system's libraries." FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_LIBS)

# zlib Library
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
THIRD_PARTY_SUBDIR(PNG libpng)
IF(BRLCAD_BUILD_LOCAL_PNG)
	# PNG_LIB_NAME doesn't automatically propogate to toplevel due to scoping of CMake variables - 
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)
	SET(BRLCAD_PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_PNG)
	# The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than PNG_INCLUDE_DIR, so
	# a generic macro won't pick up the setting - handle it here
	SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
	SET(BRLCAD_PNG_INCLUDE_DIR ${PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_PNG)

# libregex Library
THIRD_PARTY_OPTION(REGEX regex)
THIRD_PARTY_SUBDIR(REGEX libregex)

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


# Tcl/Tk presents a number of complexities for BRL-CAD and CMake - BRL-CAD requires
# far more knowledge of the details of a Tcl/Tk installation than most programs,
# which unfortunately means the standard FindTCL.cmake is not sufficient.  Rather than
# use THIRD_PARTY_OPTION here, we will instead provide the option manually and ensure
# that BRL-CAD's own FindTCL.cmake file is loaded.  

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
SET(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
SET(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

THIRD_PARTY_OPTION(TCL tcl)
IF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE STRING "local tk build" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" OFF)
	SET(BRLCAD_BUILD_LOCAL_TK OFF CACHE STRING "local tk build" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
IF(BRLCAD_BUILD_LOCAL_LIBS)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
	SET(BRLCAD_BUILD_LOCAL_TK ON CACHE STRING "local tk build" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_LIBS)
THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TCL tcl src/other/tcl unix "")
IF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_BUILD_LOCAL_LIBS)
	SET(TCL_LIBRARY "${BRLCAD_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tcl8.5${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE STRING "TCL_LIBRARY" FORCE)
	SET(TCL_INCLUDE_PATH "${BRLCAD_PREFIX}/include" CACHE STRING "TCL_INCLUDE_PATH" FORCE)
	SET(TCL_CONF_PREFIX "${BRLCAD_PREFIX}/lib" CACHE STRING "TCL_CONF_PREFIX" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_BUILD_LOCAL_LIBS)
THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TK tk src/other/tk unix "--with-tcl=${TCL_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TK OR BRLCAD_BUILD_LOCAL_LIBS)
	SET(TK_LIBRARY "${BRLCAD_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tk8.5${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE STRING "TK_LIBRARY" FORCE)
	SET(TK_INCLUDE_PATH "${BRLCAD_PREFIX}/include" CACHE STRING "TK_INCLUDE_PATH" FORCE)
	SET(TK_CONF_PREFIX "${BRLCAD_PREFIX}/lib" CACHE STRING "TK_CONF_PREFIX" FORCE)
	ADD_DEPENDENCIES(tk tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TK OR BRLCAD_BUILD_LOCAL_LIBS)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h file
IF(TK_LIBRARY)
	SET(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
	FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_TK 1\n")
ELSE(TK_LIBRARY)
	SET(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
ENDIF(TK_LIBRARY)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindTclPackage.cmake)
IF(NOT BRLCAD_BUILD_LOCAL_TCL)
	IF(TCL_TCLSH)
		FIND_TCL_PACKAGE(${TCL_TCLSH} Itcl)
	ENDIF(TCL_TCLSH)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(TK_WISH)
			FIND_TCL_PACKAGE(${TK_WISH} Itk)
			FIND_TCL_PACKAGE(${TK_WISH} Iwidgets)
			FIND_TCL_PACKAGE(${TK_WISH} Tkhtml)
			FIND_TCL_PACKAGE(${TK_WISH} tkpng)
			FIND_TCL_PACKAGE(${TK_WISH} Tktable)
		ENDIF(TK_WISH)
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
THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(ITCL itcl src/other/incrTcl/itcl "" "--with-tcl=${TCL_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TCL)
	ADD_DEPENDENCIES(itcl tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL)
IF(BRLCAD_BUILD_LOCAL_ITCL)
	SET(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")

THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(ITK itk src/other/incrTcl/itk "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TK)
	ADD_DEPENDENCIES(itk itcl tcl tk)
ENDIF(BRLCAD_BUILD_LOCAL_TK)
IF(BRLCAD_BUILD_LOCAL_ITK)
	SET(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITK)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")

IF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL ON CACHE STRING "incrtcl build status" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF CACHE STRING "incrtcl build status" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITCL OR BRLCAD_BUILD_LOCAL_ITK)

THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(IWIDGETS iwidgets src/other/incrTcl/iwidgets "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX};--with-itcl=${BRLCAD_SOURCE_DIR}/src/other/incrTcl")
IF(BRLCAD_BUILD_LOCAL_ITK)
	ADD_DEPENDENCIES(iwidgets tcl tk itcl itk)
	SET(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ITK)
FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")

THIRD_PARTY_AUTORECONF_EXTERNAL_PROJECT(TKHTML tkhtml src/other/tkhtml "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TK)
	ADD_DEPENDENCIES(tkhtml tk tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TK)

THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TKPNG tkpng src/other/tkpng "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TK)
	ADD_DEPENDENCIES(tkpng tk tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TK)

THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT(TKTABLE tktable src/other/tktable "" "--with-tcl=${TCL_CONF_PREFIX};--with-tk=${TK_CONF_PREFIX}")
IF(BRLCAD_BUILD_LOCAL_TK)
	ADD_DEPENDENCIES(tktable tk tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TK)


# OpenNURBS Library
THIRD_PARTY_OPTION(OPENNURBS openNURBS)
THIRD_PARTY_SUBDIR(OPENNURBS openNURBS)

# STEP Class Libraries
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

# The jama/tnt headers are installed by default - BRL-CAD requires the altered headers
# to build (I think?)
ADD_SUBDIRECTORY(tnt)
