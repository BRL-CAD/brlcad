# - Find Tcl/Tk includes and libraries.

# There are quite a number of potential compilcations when it comes to
# Tcl/Tk, particularly in cases where multiple versions of Tcl/Tk are
# present on a system and the case of OSX, which my have Tk built for
# either X11 or Aqua.  On Windows there may be Cygwin installs of
# Tcl/Tk as well. 
#
# Several "design philosophy" decisions have to be made - what to report
# when multiple instances of Tcl/Tk are available, how much control to
# allow users, how to expose those controls, etc.  Here are the rules
# this particular implementation of FindTCL will strive to express:
#
# 1. If a parent CMakeLists.txt defines a specific TCL_PREFIX 
#    directory, don't look for or return any settings using
#    other Tcl/Tk installations, even if nothing is found
#    below TCL_PREFIX and other installations are present.
#    Report NOTFOUND instead.
#
# 2. There will be three possible variables for controlling
#    which versions to report:
#
#    TCL_MIN_VERSION
#    TCL_MAX_VERSION
#    TCL_EXACT_VERSION
#
#    All of these will be expected to have the form:
#
#    TCL_MAJOR_VERSION.TCL_MINOR_VERSION.TCL_PATCH_VERSION
#
#    although the PATCH_VERSION will be optional.  If
#    no PATCH_VERSION is specified, any patch version will
#    be regarded as satisfying the criteria of any version
#    number test applied.  If no versions are specified it
#    is assumed any will do.  Higher versions are preferred
#    over lower, within the above constraints.
#
# 3. Tk provides the option to compile either for
#    the "native" graphics system (aqua, win32, etc.) or for
#    X11 (which is also the native graphics system on Unix-type
#    platforms other than Mac OSX.)  There are situations were
#    a program or developer may want to require a particular
#    windowing system.  If that is the case they can make use
#    of the following two options:
#
#    TK_NATIVE_GRAPHICS
#    TK_X11_GRAPHICS
#
#    If NATIVE_GRAPHICS is set to ON, a Tcl/Tk system is 
#    reported found only if the reported graphics system
#    matches that of the current platform.  If X11_GRAPHICS
#    is on, a match is reported only if the windowing system
#    reports X11.  If neither option is ON, the windowing
#    system is not a factor is deciding to report
#    FOUND or NOTFOUND.  If BOTH are ON (why??) NATIVE_GRAPHICS
#    will override the TK_X11_GRAPHICS setting and set it
#    to OFF.
#
# 4. By default, if no prefix is specified, FindTCL will search
#    a list of directories and the system path for tcl libraries.
#    This list can be expanded by a parent CMakeLists.txt file
#    by specifying additional paths in this variable, which will
#    be checked before the system path - essentially, this lets
#    a configure process do a "soft set" of the TCL prefix:
#
#    TCL_SEARCH_ADDITIONAL_PATHS
#
# 5. On Apple platforms, there may be a "Framework" install of
#    Tcl/Tk. By default, FindTCL will start with this version
#    on OSX platforms if no TCL_PREFIX is specified, but will
#    move on to another installation if the Framework Tcl/Tk doesn't
#    satisfy other criteria.  If a developer wishes to REQUIRE a 
#    Framework build of Tcl/Tk and reject other installs even though
#    they may satisfy other criteria, they can enable the following 
#    option:
#
#    TCL_USE_FRAMEWORK_ONLY
#
# 6. If a developer needs ONLY Tcl, without the Tk graphics library,
#    they can disable the following option (on by default)
#  
#    TCL_REQUIRE_TK
#
# 7. If a developer needs Threads support in Tcl, they can enable
#    the following option (disabled by default)
#
#    TCL_REQUIRE_THREADS
#
# 8. Normally, FindTCL will assume that the intent is to compile
#    C code and will require headers.  If a developer needs Tcl/Tk
#    ONLY for the purposes of running tclsh or wish scripts and is
#    not planning to do any compiling, they can disable the following
#    option and FindTCL will report a Tcl/Tk installation without
#    headers as FOUND.
#
#    TCL_NEED_HEADERS
#
# The following "results" variables will be set (Tk variables only
# set when TCKTK_REQUIRE_TK is ON):
#
#   TCL_FOUND          = Tcl (and Tk) found (see TCKTK_REQUIRE_TK)
#   TCL_FOUND_VERSION  = Version of selected Tcl/TK
#   TCL_LIBRARY        = path to Tcl library
#   TCL_INCLUDE_PATH   = path to directory containing tcl.h
#   TK_LIBRARY         = path to Tk library
#   TK_INCLUDE_PATH    = path to directory containing tk.h
#   TCL_TCLSH          = full path to tclsh binary
#   TK_WISH            = full path wo wish binary

# Tcl/Tk tends to name things using version numbers, so we need a
# list of numbers to check 
SET(TCL_POSSIBLE_MAJOR_VERSIONS 8)
SET(TCL_POSSIBLE_MINOR_VERSIONS 6 5 4 3 2 1 0)

# Create the Tcl/Tk options if not already present
OPTION(TK_NATIVE_GRAPHICS "Require native Tk graphics." OFF)
OPTION(TK_X11_GRAPHICS "Require X11 Tk graphics." OFF)
OPTION(TCL_USE_FRAMEWORK_ONLY "Don't use any Tcl/Tk installation that isn't a Framework." OFF)
OPTION(TCL_REQUIRE_TK "Look for Tk installation, not just Tcl." ON)
OPTION(TCL_NEED_HEADERS "Don't report a found Tcl/Tk unless headers are present." ON)

# Sanity check the settings - can't require both Native and X11 if X11 isn't
# the native windowing system on the platform
IF(WIN32 OR APPLE)
  IF(TK_NATIVE_GRAPHICS AND TK_X11_GRAPHICS)
    MESSAGE(STATUS "Warning - both Native and X11 graphics required on platform where X11 is not a native graphics layer - disabling X11 graphics.")
    SET(TK_X11_GRAPHICS OFF CACHE BOOL "Require X11 Tk graphics." FORCE)
  ENDIF(TK_NATIVE_GRAPHICS AND TK_X11_GRAPHICS)
ENDIF(WIN32 OR APPLE)

# Set up the logic for determing the tk windowingsystem.  This requires
# running a small wish script and recovering the results from a file -
# wrap this logic in a macro

SET(tkwin_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM\"
set fileId [open $filename \"w\"]
set windowingsystem [tk windowingsystem]
puts $fileId $windowingsystem
close $fileId
exit
")

SET(tkwin_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tk_windowingsystem.tcl")

MACRO(TK_GRAPHICS_SYSTEM wishcmd resultvar)
   SET(${resultvar} "wm-NOTFOUND")
   FILE(WRITE ${tkwin_scriptfile} ${tkwin_script})
   EXEC_PROGRAM(${wishcmd} ARGS ${tkwin_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
   FILE(READ ${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM ${resultvar})
ENDMACRO()

# For ActiveState's Tcl/Tk install on Windows, there are some specific
# paths that may be needed.  This is a macro-ized version of the paths
# found in CMake's FindTCL.cmake
MACRO(WIN32TCLPATHS vararray extension)
  IF(WIN32)
    GET_FILENAME_COMPONENT(
      ActiveTcl_CurrentVersion 
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl;CurrentVersion]" 
      NAME)
      SET(${vararray} ${${vararray}}
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/${extension}")
      FOREACH(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
        FOREACH(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
         SET(${vararray} ${${vararray}} "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\${MAJORNUM}.${MINORNUM};Root]/${extension}")
        ENDFOREACH()
      ENDFOREACH()
   ENDIF(WIN32)
ENDMACRO(WIN32TCLPATHS)

# Need some routines to chop version numbers up
MACRO(SPLIT_TCL_VERSION_NUM versionnum)
     STRING(REGEX REPLACE "([0-9]*).[0-9]*.?[0-9]*" "\\1" ${versionnum}_MAJOR "${${versionnum}}")
     STRING(REGEX REPLACE "[0-9]*.([0-9]*).?[0-9]*" "\\1" ${versionnum}_MINOR "${${versionnum}}")
     STRING(REGEX REPLACE "[0-9]*.[0-9]*.?([0-9]*)" "\\1" ${versionnum}_PATCH "${${versionnum}}")
ENDMACRO()    

# If an exact version is set, peg min and max at the same value and restrict the possible
# TCL version numbers to match exactly the required version.  
IF(TCL_EXACT_VERSION)
   SET(TCL_MIN_VERSION ${TCL_EXACT_VERSION})
   SET(TCL_MAX_VERSION ${TCL_EXACT_VERSION})
   SPLIT_TCL_VERSION_NUM(TCL_EXACT_VERSION)
   SET(TCL_POSSIBLE_MAJOR_VERSIONS ${TCL_EXACT_VERSION_MAJOR})
   SET(TCL_POSSIBLE_MINOR_VERSIONS ${TCL_EXACT_VERSION_MINOR})
ENDIF(TCL_EXACT_VERSION)


MACRO(FIND_LIBRARY_VERSIONS targetname pathnames options)
   FOREACH(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
      FOREACH(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
         SET(CURRENT_SEARCH_VERSION "${MAJORNUM}.${MINORNUM}")
         VALIDATE_VERSION(dosearch ${CURRENT_SEARCH_VERSION})
	 IF(dosearch)
         SET(TCL_${targetname}${MAJORNUM}${MINORNUM} TCL_${targetname}${MAJORNUM}${MINORNUM}-NOTFOUND)
         FOREACH(SPATH ${${pathnames}})
            FIND_LIBRARY(TCL_${targetname}${MAJORNUM}${MINORNUM} NAMES ${targetname}${MAJORNUM}.${MINORNUM} ${targetname}${MAJORNUM}${MINORNUM} PATHS ${SPATH} ${options})
            IF(NOT TCL_${targetname}${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
               # On repeat configures, CMAKE_INSTALL_PREFIX apparently gets added to the FIND_* paths.  This is a problem
               # if re-installing over a previous install so strip these out - don't use output from the previous build to 
	       # do the current build!  If the goal is to use an ExternalProject_ADD target that will be in CMAKE_INSTALL_PREFIX,
	       # the relevant variables should be set in the CMakeLists.txt file with the ExternalProject_ADD definition.
               IF(NOT TCL_${targetname}${MAJORNUM}${MINORNUM} MATCHES "^${CMAKE_INSTALL_PREFIX}")
                  SET(TCL_${targetname}_LIST ${TCL_${targetname}_LIST} ${TCL_${targetname}${MAJORNUM}${MINORNUM}})
               endif()
            endif()
            SET(TCL_${targetname}${MAJORNUM}${MINORNUM} TCL_${targetname}${MAJORNUM}${MINORNUM}-NOTFOUND)
         ENDFOREACH()
	 ENDIF(dosearch)
      ENDFOREACH()
   ENDFOREACH()
   MESSAGE("TCL_${targetname}_LIST: ${TCL_${targetname}_LIST}")
ENDMACRO()

MACRO(FIND_PROGRAM_VERSIONS targetname pathnames options)
   FOREACH(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
      FOREACH(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
         SET(CURRENT_SEARCH_VERSION "${MAJORNUM}.${MINORNUM}")
         VALIDATE_VERSION(dosearch ${CURRENT_SEARCH_VERSION})
	 IF(dosearch)
         SET(TCL_${targetname}${MAJORNUM}${MINORNUM} TCL_${targetname}${MAJORNUM}${MINORNUM}-NOTFOUND)
         FOREACH(SPATH ${${pathnames}})
            FIND_PROGRAM(TCL_${targetname}${MAJORNUM}${MINORNUM} NAMES ${targetname}${MAJORNUM}.${MINORNUM} ${targetname}${MAJORNUM}${MINORNUM} PATHS ${SPATH} ${options})
            IF(NOT TCL_${targetname}${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
               # On repeat configures, CMAKE_INSTALL_PREFIX apparently gets added to the FIND_* paths.  This is a problem
               # if re-installing over a previous install so strip these out - don't use output from the previous build to 
	       # do the current build!  If the goal is to use an ExternalProject_ADD target that will be in CMAKE_INSTALL_PREFIX,
	       # the relevant variables should be set in the CMakeLists.txt file with the ExternalProject_ADD definition.
               IF(NOT TCL_${targetname}${MAJORNUM}${MINORNUM} MATCHES "^${CMAKE_INSTALL_PREFIX}")
                  SET(TCL_${targetname}_LIST ${TCL_${targetname}_LIST} ${TCL_${targetname}${MAJORNUM}${MINORNUM}})
               endif()
            endif()
            SET(TCL_${targetname}${MAJORNUM}${MINORNUM} TCL_${targetname}${MAJORNUM}${MINORNUM}-NOTFOUND)
         ENDFOREACH()
	 ENDIF(dosearch)
      ENDFOREACH()
   ENDFOREACH()
   MESSAGE("TCL_${targetname}_LIST: ${TCL_${targetname}_LIST}")
ENDMACRO()

MACRO(PULL_PREFERRED_VERSION  targetname targetvar)
   FOREACH(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
      if(NOT MAJORNUM LESS TCL_MIN_VERSION_MAJOR)
      if(NOT MAJORNUM GREATER TCL_MAX_VERSION_MAJOR)
      if(${${targetvar}} MATCHES "NOTFOUND$")
      FOREACH(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
         if(NOT MINORNUM LESS TCL_MIN_VERSION_MINOR)
         if(NOT MINORNUM GREATER TCL_MAX_VERSION_MINOR)
         if(${${targetvar}} MATCHES "NOTFOUND$")
	    LIST(LENGTH TCL_${targetname}${MAJORNUM}${MINORNUM}_LIST LISTLENGTH)
            IF(LISTLENGTH GREATER 0)
               LIST(GET TCL_${targetname}${MAJORNUM}${MINORNUM}_LIST 0 ${targetvar})
	       LIST(REMOVE_AT TCL_${targetname}${MAJORNUM}${MINORNUM}_LIST 0)
            endif()
         endif()
         endif()
         endif()
      ENDFOREACH()
      endif()
      endif()
      endif()
   ENDFOREACH()
ENDMACRO()

MACRO(RESET_TCL_VARS)
   SET(TCL_MAJOR_VERSION "NOTFOUND")
   SET(TCL_MINOR_VERSION "NOTFOUND")
   SET(TCL_PATCH_LEVEL "NOTFOUND")
   SET(TCL_INCLUDE_PATH "NOTFOUND")
   SET(TCL_LIBRARY "NOTFOUND")
ENDMACRO()

MACRO(RESET_TK_VARS)
   SET(TK_MAJOR_VERSION "NOTFOUND")
   SET(TK_MINOR_VERSION "NOTFOUND")
   SET(TK_PATCH_LEVEL "NOTFOUND")
   SET(TK_INCLUDE_PATH "NOTFOUND")
   SET(TK_LIBRARY "NOTFOUND")
ENDMACRO()


MACRO(READ_TCLCONFIG_FILE tclconffile)
   RESET_TCL_VARS()
   MESSAGE("tclconffile: ${tclconffile}")
   FILE(READ ${tclconffile} TCL_CONF_FILE)
   STRING(REGEX REPLACE "\r?\n" ";" ENT "${TCL_CONF_FILE}")
   FOREACH(line ${ENT})
	IF(${line} MATCHES "TCL_MAJOR_VERSION")
	  STRING(REGEX REPLACE ".*TCL_MAJOR_VERSION='([0-9]*)'.*" "\\1" TCL_MAJOR_VERSION ${line})
	endif(${line} MATCHES "TCL_MAJOR_VERSION")
 	IF(${line} MATCHES "TCL_MINOR_VERSION")
	  STRING(REGEX REPLACE ".*TCL_MINOR_VERSION='([0-9]*)'.*" "\\1" TCL_MINOR_VERSION ${line})
	endif(${line} MATCHES "TCL_MINOR_VERSION")
	IF(${line} MATCHES "TCL_PATCH_LEVEL")
	  STRING(REGEX REPLACE ".*TCL_PATCH_LEVEL='.([0-9]*)'.*" "\\1" TCL_PATCH_LEVEL ${line})
	endif(${line} MATCHES "TCL_PATCH_LEVEL")
	IF(${line} MATCHES "TCL_INCLUDE")
	   STRING(REGEX REPLACE ".*TCL_INCLUDE_SPEC='-I(.+)'.*" "\\1" TCL_INCLUDE_PATH ${line})
	   MESSAGE("INCLUDEDIR: ${TCL_INCLUDE_PATH}")
	endif()
	IF(${line} MATCHES "TCL_EXEC_PREFIX")
	   STRING(REGEX REPLACE ".*TCL_EXEC_PREFIX='(.+)'.*" "\\1" TCL_TCLSH ${line})
	   IF (EXISTS "${TCL_TCLSH}/bin/tclsh${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION}")
	      SET(TCL_TCLSH "${TCL_TCLSH}/bin/tclsh${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION}")
	   ELSE()
	      IF (EXISTS "${TCL_TCLSH}/bin/tclsh${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}")
	         SET(TCL_TCLSH "${TCL_TCLSH}/bin/tclsh${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}")
              ENDIF()
	   ENDIF()
	   MESSAGE("Tclsh command: ${TCL_TCLSH}")
	endif()
   ENDFOREACH(line ${ENT})
   MESSAGE("Reported TCL version: ${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_LEVEL}")
ENDMACRO()


MACRO(READ_TKCONFIG_FILE tkconffile)
   RESET_TK_VARS()
   MESSAGE("tkconffile: ${tkconffile}")
   FILE(READ ${tkconffile} TK_CONF_FILE)
   STRING(REGEX REPLACE "\r?\n" ";" ENT "${TK_CONF_FILE}")
   FOREACH(line ${ENT})
	IF(${line} MATCHES "TK_MAJOR_VERSION")
	  STRING(REGEX REPLACE ".*TK_MAJOR_VERSION='([0-9]*)'.*" "\\1" TK_MAJOR_VERSION ${line})
	endif(${line} MATCHES "TK_MAJOR_VERSION")
 	IF(${line} MATCHES "TK_MINOR_VERSION")
	  STRING(REGEX REPLACE ".*TK_MINOR_VERSION='([0-9]*)'.*" "\\1" TK_MINOR_VERSION ${line})
	endif(${line} MATCHES "TK_MINOR_VERSION")
	IF(${line} MATCHES "TK_PATCH_LEVEL")
	  STRING(REGEX REPLACE ".*TK_PATCH_LEVEL='.([0-9]*)'.*" "\\1" TK_PATCH_LEVEL ${line})
	endif(${line} MATCHES "TK_PATCH_LEVEL")
	IF(${line} MATCHES "TK_.*INCLUDE")
	   STRING(REGEX REPLACE ".*TK_.*INCLUDE.*='-I(.+)'.*" "\\1" TK_INCLUDE_PATH ${line})
	   MESSAGE("INCLUDEDIR: ${TK_INCLUDE_PATH}")
	endif()
	IF(${line} MATCHES "TK_EXEC_PREFIX")
	   STRING(REGEX REPLACE ".*TK_EXEC_PREFIX='(.+)'.*" "\\1" TK_WISH ${line})
	   IF (EXISTS "${TK_WISH}/bin/wish${TK_MAJOR_VERSION}${TK_MINOR_VERSION}")
	      SET(TK_WISH "${TK_WISH}/bin/wish${TK_MAJOR_VERSION}${TK_MINOR_VERSION}")
	   ELSE()
	      IF (EXISTS "${TK_WISH}/bin/wish${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}")
	         SET(TK_WISH "${TK_WISH}/bin/wish${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}")
              ENDIF()
	   ENDIF()
	   MESSAGE("Wish command: ${TK_WISH}")
	endif()
   ENDFOREACH(line ${ENT})
   MESSAGE("Reported TK version: ${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}.${TK_PATCH_LEVEL}")
ENDMACRO()

include(CompareVersions)
MACRO(VALIDATE_VERSION vstatus versionnum)
   SET(${vstatus} "NOTFOUND")
   SET(vcompare "NOTFOUND")
   if(TCL_EXACT_VERSION)
	COMPARE_VERSIONS(${TCL_EXACT_VERSION} ${versionnum} vcompare)
	MESSAGE("vcompare: ${vcompare}")
        IF(NOT vcompare)
	  SET(${vstatus} 1)
        ELSE()
          SET(${vstatus} 0)
        endif()
   else()
	if(TCL_MIN_VERSION)
           COMPARE_VERSIONS(${TCL_MIN_VERSION} ${versionnum} vmincompare)
	   if(NOT vmincompare)
	     SET(vmincompare 2)
	   endif()
	else()
	   SET(vmincompare 2)
        endif()
        if(TCL_MAX_VERSION)
           COMPARE_VERSIONS(${TCL_MAX_VERSION} ${versionnum} vmaxcompare)
	   if(NOT vmaxcompare)
	     SET(vmaxcompare 1)
	   endif()
        else()
	   SET(vmaxcompare 1)
        endif() 
	IF(${vmincompare} EQUAL 2 AND ${vmaxcompare} EQUAL 1)
	   SET(${vstatus} 1)
        ELSE()
           SET(${vstatus} 0)
        endif()
   endif()
ENDMACRO()

# Try to be a bit forgiving with the TCL prefix - if someone gives the
# full path to the lib directory, catch that by adding the parent path
# to the list to check
IF(TCL_PREFIX)
   SET(TCL_LIBRARY "NOTFOUND")
   SET(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_PREFIX}/lib)
   SET(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_PREFIX})
   GET_FILENAME_COMPONENT(TCL_LIB_PATH_PARENT "${TCL_PREFIX}" PATH)
   SET(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_LIB_PATH_PARENT}/lib)
   SET(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_LIB_PATH_PARENT})
   LIST(REMOVE_DUPLICATES TCL_PREFIX_LIBDIRS)
   MESSAGE("TCL_PREFIX_LIBDIRS: ${TCL_PREFIX_LIBDIRS}")
   FIND_LIBRARY_VERSIONS(tcl TCL_PREFIX_LIBDIRS NO_SYSTEM_PATH)
   PULL_PREFERRED_VERSION(tcl TCL_LIBRARY)
   MESSAGE("TCL_LIBRARY: ${TCL_LIBRARY}")
   GET_FILENAME_COMPONENT(TCL_LIB_PATH_PARENT "${TCL_LIBRARY}" PATH)
   MESSAGE("TCL_PATH: ${TCL_LIB_PATH_PARENT}")
   set(tclconffile "tclConfig.sh-NOTFOUND") 
   find_file(tclconffile tclConfig.sh PATHS ${TCL_LIB_PATH_PARENT})
   if(NOT tclconffile MATCHES "NOTFOUND$")
      MESSAGE("Found config file: ${tclconffile}")
      READ_TCLCONFIG_FILE(${tclconffile})
      SET(CURRENTTCLVERSION "${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_LEVEL}")
      VALIDATE_VERSION(TCLVALID ${CURRENTTCLVERSION})
   endif(NOT tclconffile MATCHES "NOTFOUND$")
ELSE(TCL_PREFIX)
   SET(PATHLIST "$ENV{PATH}")
   STRING(REGEX REPLACE ":" ";" PATHLIST "${PATHLIST}")
   MESSAGE("PATHLIST: ${PATHLIST}")
   FIND_PROGRAM_VERSIONS(tclsh PATHLIST NO_SYSTEM_PATH)
   FIND_PROGRAM_VERSIONS(wish PATHLIST NO_SYSTEM_PATH)
   SET(LIBPATHLIST "$ENV{PATH}")
   STRING(REGEX REPLACE "/s?bin" "" LIBPATHLIST "${LIBPATHLIST}")
   SET(LIBPATHLIST "$ENV{LD_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}:$ENV{DYLD_FALLBACK_LIBRARY_PATH}:${LIBPATHLIST}")
   STRING(REGEX REPLACE ":" "/lib;" LIBPATHLIST "${LIBPATHLIST}")
   LIST(REMOVE_DUPLICATES LIBPATHLIST)
   MESSAGE("LIBPATHLIST: ${LIBPATHLIST}")
   FIND_LIBRARY_VERSIONS(tcl LIBPATHLIST NO_SYSTEM_PATH)
   FOREACH(libfile ${TCL_tcl_LIST})
   ENDFOREACH()
 
   IF(APPLE)
        INCLUDE(CMakeFindFrameworks)
	CMAKE_FIND_FRAMEWORKS(Tcl)
        CMAKE_FIND_FRAMEWORKS(Tk)
	IF(Tcl_FRAMEWORKS)
           SET(TCLVALID "NOTFOUND")
   	   FOREACH(dir ${Tcl_FRAMEWORKS})
              IF(NOT TCLVALID)
 	         set(tclconf "tclConfig.sh-NOTFOUND") 
                 find_file(tclconf tclConfig.sh PATHS ${dir})
                 if(NOT tclconf MATCHES "NOTFOUND$")
 	            MESSAGE("Found config file: ${tclconf}")
                    READ_TCLCONFIG_FILE(${tclconf})
		    SET(CURRENTTCLVERSION "${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_LEVEL}")
 		    MESSAGE("cv: ${CURRENTTCLVERSION}")
	            VALIDATE_VERSION(TCLVALID ${CURRENTTCLVERSION})
		    MESSAGE("TCLVALID: ${TCLVALID}")
		    if(TCLVALID)
			SET(TCL_FRAMEWORK_DIR ${dir})
		    else()
			RESET_TCL_VARS()
			RESET_TK_VARS()
		    endif(TCLVALID)
                 endif(NOT tclconf MATCHES "NOTFOUND$")
	      ENDIF(NOT TCLVALID)
           ENDFOREACH(dir ${Tcl_FRAMEWORKS})
	   IF(Tk_FRAMEWORKS AND TCLVALID)
		SET(TKVALID "NOTFOUND")
    	        FOREACH(dir ${Tk_FRAMEWORKS})
                  IF(NOT TKVALID)
 	            set(tkconf "tkConfig.sh-NOTFOUND") 
                    find_file(tkconf tkConfig.sh PATHS ${dir})
                    if(NOT tkconf MATCHES "NOTFOUND$")
 	               MESSAGE("Found config file: ${tkconf}")
                       READ_TKCONFIG_FILE(${tkconf})
		       SET(CURRENTTKVERSION "${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}.${TK_PATCH_LEVEL}")
		       COMPARE_VERSIONS(${CURRENTTCLVERSION} ${CURRENTTKVERSION} vtcltkcompare)
		       MESSAGE("tcl: ${CURRENTTCLVERSION} tk: ${CURRENTTKVERSION} comp: ${vtcltkcompare}")
		       IF(${vtcltkcompare})
		          MESSAGE("WARNING: Tk version is different than Tcl Version - rejecting")
		          SET(TKVALID 0)
			  RESET_TK_VARS()
		       ELSE()
			  SET(TK_FRAMEWORK_DIR ${dir})
			  # check for specific graphics, if needed
			  IF(TK_NATIVE_GRAPHICS OR TK_X11_GRAPHICS)
			    TK_GRAPHICS_SYSTEM(${TK_WISH} TK_SYSTEM_GRAPHICS)
			    IF(APPLE AND TK_NATIVE_GRAPHICS)
			    MESSAGE("TK_SYSTEM_GRAPHICS: ${TK_SYSTEM_GRAPHICS}")
			      IF(${TK_SYSTEM_GRAPHICS} MATCHES "aqua")
			    MESSAGE("TK_SYSTEM_GRAPHICS: ${TK_SYSTEM_GRAPHICS}")
			         SET(TKVALID 1)
			      ELSE()
			         SET(TKVALID 0)
		              ENDIF()
		            ENDIF()
			    IF(WIN32 AND TK_NATIVE_GRAPHICS)
			      IF(${TK_SYSTEM_GRAPHICS} MATCHES "win32")
			         SET(TKVALID 1)
			      ELSE()
			         SET(TKVALID 0)
		              ENDIF()
		            ENDIF()
			    IF(TK_X11_GRAPHICS)
			      IF(${TK_SYSTEM_GRAPHICS} MATCHES "x11")
			         SET(TKVALID 1)
			      ELSE()
			         SET(TKVALID 0)
		              ENDIF()
		            ENDIF()
			  ELSE(TK_NATIVE_GRAPHICS OR TK_X11_GRAPHICS)
			     SET(TKVALID 1)
			  ENDIF(TK_NATIVE_GRAPHICS OR TK_X11_GRAPHICS)
		       ENDIF()
	               MESSAGE("valid: ${TKVALID}")
                    endif(NOT tkconf MATCHES "NOTFOUND$")
	          ENDIF(NOT TKVALID)
                ENDFOREACH(dir ${Tk_FRAMEWORKS})
	    ENDIF(Tk_FRAMEWORKS AND TCLVALID)
	ENDIF(Tcl_FRAMEWORKS)
	IF(TCLVALID)
	   GET_FILENAME_COMPONENT(TCL_FRAMEWORK_PARENT_DIR "${TCL_FRAMEWORK_DIR}" PATH)
	   MESSAGE("framework dir: ${TCL_FRAMEWORK_PARENT_DIR}")
	   FIND_LIBRARY(TCL_LIBRARY tcl Tcl ${TCL_FRAMEWORK_PARENT_DIR} NO_SYSTEM_PATH)
	   IF(TKVALID)
	     GET_FILENAME_COMPONENT(TK_FRAMEWORK_PARENT_DIR "${TK_FRAMEWORK_DIR}" PATH)
	     MESSAGE("framework dir: ${TK_FRAMEWORK_PARENT_DIR}")
	     FIND_LIBRARY(TK_LIBRARY tk Tk ${TK_FRAMEWORK_PARENT_DIR} NO_SYSTEM_PATH)
	   ELSE(TKVALID)
	     RESET_TK_VARS()
	     IF(TCL_REQUIRES_TK)
	        RESET_TCL_VARS()
	     ENDIF(TCL_REQUIRES_TK)
	   ENDIF(TKVALID)
        ELSE(TCLVALID)
	     RESET_TCL_VARS()
        ENDIF(TCLVALID)
        MESSAGE("TCL_LIBRARY: ${TCL_LIBRARY}")
        MESSAGE("TK_LIBRARY: ${TK_LIBRARY}")
   ENDIF(APPLE)
   IF(NOT TCL_LIBRARY)


   ENDIF()	

ENDIF(TCL_PREFIX)

#FOREACH(MAJORNUM ${TCL_FOUND_MAJOR_VERSIONS})
#   FOREACH(MINORNUM ${TCL_FOUND_MINOR_VERSIONS})
#       SET(LISTITEM 0)
#       LIST(LENGTH TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST LISTLENGTH)
#       IF(LISTLENGTH GREATER 0)
#          WHILE (LISTLENGTH GREATER LISTITEM)
#             LIST(GET TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST ${LISTITEM} TCL_TCLSH)
#             GET_FILENAME_COMPONENT(TCL_TCLSH_PATH "${TCL_TCLSH}" PATH)
#             GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_TCLSH_PATH}" PATH)
#             IF (TCL_USE_TK)
#		 # FIXME: There is a risk here of picking up a wish in a system path that we don't want to consider, but if
#		 # the NO_SYSTEM_PATH option is added valid entires in our list will not find their corresponding wish
#		 # even if it is there - looks like we're going to have to special case tclsh/wish found in system paths
#                 FIND_PROGRAM(TCL_WISH${MAJORNUM}${MINORNUM} NAMES wish${MAJORNUM}.${MINORNUM} wish${MAJORNUM}${MINORNUM} PATHS ${TCL_TCLSH_PATH_PARENT}/bin})
#                 IF(NOT TCL_WISH${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
#                    SET(TCL_POSSIBLE_PATHS ${TCL_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})
#		 endif()
#             ELSE(TCL_USE_TK)
#               SET(TCL_POSSIBLE_PATHS ${TCL_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})
#             ENDIF(TCL_USE_TK)
#	     math(EXPR LISTITEM "${LISTITEM} + 1")
#	  endwhile()
#       endif()
#   ENDFOREACH()
#ENDFOREACH()

#LIST(REMOVE_DUPLICATES TCL_POSSIBLE_PATHS)

# handle the QUIETLY and REQUIRED arguments and set TCL_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
IF(TCL_REQUIRE_TK)
   FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH TK_LIBRARY TK_INCLUDE_PATH)
   SET(TCL_FIND_REQUIRED ${TCL_FIND_REQUIRED})
   SET(TCL_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
   SET(TK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
   SET(TK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
ELSE(TCL_REQUIRE_TK)
   FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH)
   SET(TCL_FIND_REQUIRED ${TCL_FIND_REQUIRED})
   SET(TCL_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
endif(TCL_REQUIRE_TK)

MARK_AS_ADVANCED(
  TCL_INCLUDE_PATH
  TK_INCLUDE_PATH
  TCL_LIBRARY
  TK_LIBRARY
  )

MESSAGE("TCL_INCLUDE_PATH: ${TCL_INCLUDE_PATH}")
MESSAGE("TK_INCLUDE_PATH: ${TK_INCLUDE_PATH}")
MESSAGE("TCL_LIBRARY: ${TCL_LIBRARY}")
MESSAGE("TK_LIBRARY: ${TK_LIBRARY}")

