#                   F I N D T C L . C M A K E
# BRL-CAD
#
# Copyright (c) 2010-2014 United States Government as represented by
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
#
###
# - Find Tcl/Tk commands, includes and libraries.
#
# Copyright (c) 2010-2014 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Copyright 2001-2009 Kitware, Inc.
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
# the United States Government, the U.S. Army Research Laboratory,
# nor the names of their contributors may be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#-----------------------------------------------------------------------
#
# There are quite a number of potential complications when it comes to
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
# 2. There will be two variables for controlling which versions to
#    report, above and beyond the standard version option to
#    find_package:
#
#    TCL_MIN_VERSION
#    TCL_MAX_VERSION
#
#    These will be expected to have the form:
#
#    TCL_VERSION_MAJOR.TCL_VERSION_MINOR.TCL_VERSION_PATCH
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
#    system is not a factor in deciding to report
#    FOUND or NOTFOUND.  If BOTH are ON (why??) NATIVE_GRAPHICS
#    will override the TK_X11_GRAPHICS setting and set it
#    to OFF.
#
# 4. By default, if no prefix is specified, FindTCL will search
#    a list of directories and the system path for tcl libraries.
#    This list can be expanded by a parent CMakeLists.txt file
#    by specifying additional paths in this variable, which will
#    be checked before the system path.  Essentially, this lets
#    a configure process do a "soft set" of the TCL prefix - look
#    here first but if not found or constraints aren't satisfied
#    check system paths:
#
#    TCL_ADDITIONAL_SEARCH_PATHS
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
# 9. FindTCL will also typically require the stub libraries be present
#    If they are not needed, the parent CMakeLists.txt file can disable
#    the following variable (defaults to ON)
#
#    TCL_NEED_STUB_LIBS
#
#10. It may be that there are Tcl/Tk installations on a machine that
#    should NOT be found by Tcl/Tk for reasons other than those
#    available above - in that case, the parent CMakeLists.txt file
#    can list REGEX patterns identifying those locations in the
#    variable (empty by default):
#
#    TCL_PATH_NOMATCH_PATTERNS
#
#
# The following variables are set as the "standard" results from
# this script:
#
#  TCL_INCLUDE_DIRS         (will also include Tk header paths if Tk is enabled)
#  TCL_LIBRARIES            (will also include Tk libraries if Tk is enabled)
#  TCL_STUB_LIBRARIES       (will also include Tk stub libraries if Tk is enabled)
#  TCL_TCLSH_EXECUTABLE     (path to tclsh binary)
#  TCL_WISH_EXECUTABLE      (path to wish binary, set only if Tk is enabled)
#  TCL_ROOT_PATH
#  TCL_FOUND                (set if all required features (Tk, threads, etc.) are found)
#  TCL_VERSION_STRING       (in the case where both Tcl and Tk are returned they,
#  TCL_VERSION_MAJOR         should both share the same version information.  If
#  TCL_VERSION_MINOR         a situation arises where they do not, it is a bug.)
#  TCL_VERSION_PATCH
#
# The following are not specifically called out by readme.txt but are
# useful when building TEA based extensions to Tcl/Tk
#
#  TCL_CONF_PREFIX          (path to parent dir of tclConfig.sh file)
#  TCL_TK_CONF_PREFIX       (path to parent dir of tkConfig.sh file)
#
# The readme.txt file discourages the use of XXX_LIBRARY settings in CMakeLists.txt files,
# but in the specific case of Tcl/Tk it is quite plausible to desire to specifically use
# only (say) the Tcl library, even if in the broader system Tk usage is also present.
# For that reason, the following variables will be maintained:
#
#  TCL_LIBRARY              (path to just the Tcl library)
#  TCL_TK_LIBRARY           (path to just the Tk library, set only if Tk is enabled)
#  TCL_STUB_LIBRARY         (path to just the Tcl stub library)
#  TCL_TK_STUB_LIBRARY      (path to just the Tk stub library)
#
# Previous FindTCL implementations set a series of less verbose
# variables that did not strictly conform to the format defined by
# the readme.txt file in the CMake modules directory.  These are set,
# but should be regarded as deprecated.
#
#  TK_LIBRARY         = path to Tk library
#  TK_STUB_LIBRARY    = path to Tk stub library
#  TCL_INCLUDE_PATH   = path to directory containing tcl.h
#  TK_INCLUDE_PATH    = path to directory containing tk.h
#  TCL_TCLSH          = full path to tclsh binary
#  TK_WISH            = full path wo wish binary
#
# Note:  In the special case where headers are not required, the *LIBRARIES, *LIBRARY,
# *CONF_PREFIX, and *_INCLUDE_DIRS variables are also not required. If
# all that is required is tclsh and/or wish, those are the only fixed
# requirements.  In most cases the other variables will also be populated
# but it is not guaranteed.

# Tcl/Tk tends to name things using version numbers, so we need a
# list of numbers to check
set(TCL_POSSIBLE_MAJOR_VERSIONS 8)
set(TCL_POSSIBLE_MINOR_VERSIONS 6 5 4 3 2 1 0)

# Create the Tcl/Tk options if not already present
OPTION(TK_NATIVE_GRAPHICS "Require native Tk graphics." OFF)
OPTION(TK_X11_GRAPHICS "Require X11 Tk graphics." OFF)
OPTION(TCL_USE_FRAMEWORK_ONLY "Don't use any Tcl/Tk installation that isn't a Framework." OFF)
OPTION(TCL_REQUIRE_TK "Look for Tk installation, not just Tcl." ON)
OPTION(TCL_REQUIRE_THREADS "Reject a Tcl/Tk installation unless threads are enabled." OFF)
OPTION(TCL_NEED_HEADERS "Don't report a found Tcl/Tk unless headers are present." ON)
OPTION(TCL_NEED_STUB_LIBS "Don't report a found Tcl/Tk unless the stub libraries are present." ON)

# Sanity check the settings - can't require both Native and X11 if X11 isn't
# the native windowing system on the platform
if(WIN32 OR APPLE)
  if(TK_NATIVE_GRAPHICS AND TK_X11_GRAPHICS)
    message(STATUS "Warning - both Native and X11 graphics required on platform where X11 is not a native graphics layer - disabling X11 graphics.")
    set(TK_X11_GRAPHICS OFF CACHE BOOL "Require X11 Tk graphics." FORCE)
  endif(TK_NATIVE_GRAPHICS AND TK_X11_GRAPHICS)
endif(WIN32 OR APPLE)

#-----------------------------------------------------------------------------
#
#                  Run-Time testing for Tcl/Tk features
#
#  This section specifically contains macros that write tcl scripts to
#  files in order to be run by tclsh/wish binaries found by other routines.
#  Routines here are intended to probe the details of the Tcl/Tk installation,
#  and should only be used to get information not obtainable through config
#  files or environment variables.
#
#-----------------------------------------------------------------------------

# Set up the logic for determining the tk windowingsystem.
set(tkwin_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM\"
set fileId [open $filename \"w\"]
set windowingsystem [tk windowingsystem]
puts $fileId $windowingsystem
close $fileId
exit
")
set(tkwin_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tk_windowingsystem.tcl")
macro(TK_GRAPHICS_SYSTEM wishcmd resultvar)
  set(${resultvar} "wm-NOTFOUND")
  file(WRITE ${tkwin_scriptfile} ${tkwin_script})
  EXEC_PROGRAM(${wishcmd} ARGS ${tkwin_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
  if(EXISTS ${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM)
    file(READ ${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM readresultvar)
    string(REGEX REPLACE "\n" "" "${resultvar}" "${readresultvar}")
  endif(EXISTS ${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM)
endmacro()


# Set up the logic for determining the version of Tcl/Tk via running a script.
set(tclversion_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TCL_VERSION\"
set fileId [open $filename \"w\"]
puts $fileId $tcl_patchLevel
close $fileId
exit
")
set(tclversion_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_version.tcl")
macro(TCL_GET_VERSION tclshcmd resultvar)
  set(${resultvar} "NOTFOUND")
  file(WRITE ${tclversion_scriptfile} ${tclversion_script})
  EXEC_PROGRAM(${tclshcmd} ARGS ${tclversion_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
  file(READ ${CMAKE_BINARY_DIR}/CMakeTmp/TCL_VERSION readresultvar)
  string(REGEX REPLACE "\n" "" "${resultvar}" "${readresultvar}")
endmacro()


# Set up the logic for determining if a particular Tcl is compiled threaded.
set(tclthreaded_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TCL_THREADED\"
set fileId [open $filename \"w\"]
if {[info exists tcl_platform(threaded)]} {puts $fileId 1}
close $fileId
exit
")
	set(tclthreaded_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_threaded.tcl")
	macro(TCL_ISTHREADED tclshcmd resultvar)
	  set(${resultvar} "NOTFOUND")
	  file(WRITE ${tclthreaded_scriptfile} ${tclthreaded_script})
	  EXEC_PROGRAM(${tclshcmd} ARGS ${tclthreaded_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
	  file(READ ${CMAKE_BINARY_DIR}/CMakeTmp/TCL_THREADED readresultvar)
	  string(REGEX REPLACE "\n" "" "${resultvar}" "${readresultvar}")
	endmacro()

	#-----------------------------------------------------------------------------
	#
	#                 Routines dealing with version numbers
	#
	#-----------------------------------------------------------------------------

	# A routine to chop version numbers up into individual variables
	macro(SPLIT_TCL_VERSION_NUM versionnum)
	  string(REGEX REPLACE "([0-9]*).[0-9]*.?[0-9]*" "\\1" ${versionnum}_MAJOR "${${versionnum}}")
	  string(REGEX REPLACE "[0-9]*.([0-9]*).?[0-9]*" "\\1" ${versionnum}_MINOR "${${versionnum}}")
	  string(REGEX REPLACE "[0-9]*.[0-9]*.?([0-9]*)" "\\1" ${versionnum}_PATCH "${${versionnum}}")
	endmacro()

	# If version information is supplied, use it to restrict the search space.  If EXACT,
	# peg min and max at the same value.
	if(TCL_FIND_VERSION_MAJOR)
	  set(TCL_POSSIBLE_MAJOR_VERSIONS ${TCL_FIND_VERSION_MAJOR})
	endif(TCL_FIND_VERSION_MAJOR)
	if(TCL_FIND_VERSION_MINOR)
	  set(TCL_POSSIBLE_MINOR_VERSIONS ${TCL_FIND_VERSION_MINOR})
	endif(TCL_FIND_VERSION_MINOR)
	if(TCL_FIND_VERSION_EXACT)
	  set(TCL_MIN_VERSION ${TCL_FIND_VERSION})
	  set(TCL_MAX_VERSION ${TCL_FIND_VERSION})
	endif(TCL_FIND_VERSION_EXACT)

	# In various loops, we don't want to waste time checking for paths containing
	# version numbers incompatible with restrictions imposed by the min/max/exact
	# variables.  Define a version to validate a given version against those
	# variables, if they are defined.
	macro(VALIDATE_VERSION vstatus versionnum)
	  set(${vstatus} "NOTFOUND")
	  if(TCL_EXACT_VERSION)
	    if(${TCL_EXACT_VERSION} VERSION_EQUAL ${versionnum})
	      set(${vstatus} 1)
	    else()
	      set(${vstatus} 0)
	    endif()
	  else()
	    set(vminpass "NOTFOUND")
	    set(vmaxpass "NOTFOUND")
	    if(TCL_MIN_VERSION)
	      if(${versionnum} VERSION_LESS ${TCL_MIN_VERSION})
		set(vminpass 0)
	      else()
		set(vminpass 1)
	      endif()
	    else()
	      set(vminpass 1)
	    endif()
	    if(TCL_MAX_VERSION)
	      if(${versionnum} VERSION_GREATER ${TCL_MIN_VERSION})
		set(vmaxpass 0)
	      else()
		set(vmaxpass 1)
	      endif()
	    else()
	      set(vmaxpass 1)
	    endif()
	    if(${vminpass} EQUAL 1 AND ${vmaxpass} EQUAL 1)
	      set(${vstatus} 1)
	    else()
	      set(${vstatus} 0)
	    endif()
	  endif()
	endmacro()


	#-----------------------------------------------------------------------------
	#
	#              Routines for defining sets of file system paths
	#
	#-----------------------------------------------------------------------------

	# For ActiveState's Tcl/Tk install on Windows, there are some specific
	# paths that may be needed.  This is a macro-ized version of the paths
	# found in CMake's FindTCL.cmake
	macro(WIN32TCLPATHS vararray extension)
	  if(WIN32)
	    get_filename_component(
	      ActiveTcl_CurrentVersion
	      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl;CurrentVersion]"
	      NAME)
	    set(${vararray} ${${vararray}}
	      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/${extension}")
	    foreach(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
	      foreach(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
		set(${vararray} ${${vararray}} "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\${MAJORNUM}.${MINORNUM};Root]/${extension}")
	      endforeach()
	    endforeach()
	    set(${vararray} ${${vararray}} "C:/Tcl")
	  endif(WIN32)
	endmacro(WIN32TCLPATHS)




	macro(FIND_PROGRAM_PATHS PROGRAM_PATHS FOUND_PATHS majorlibnum minorlibnum)
	  set(PROGRAM_MIDPATH "bin;sbin")
	  set(PROGRAM_APPENDPATH "")
	  set(PROGRAM_SEARCH_PATHS "")
	  foreach(foundpath ${${FOUND_PATHS}})
	    foreach(midpath ${PROGRAM_MIDPATH})
	      set(PROGRAM_SEARCH_PATHS "${PROGRAM_SEARCH_PATHS};${foundpath}/${midpath}")
	      foreach(appendpath ${PROGRAM_APPENDPATH})
		set(PROGRAM_SEARCH_PATHS "${PROGRAM_SEARCH_PATHS};${foundpath}/${midpath}/${appendpath}")
	      endforeach(appendpath ${PROGRAM_APPENDPATH})
	    endforeach(midpath ${PROGRAM_MIDPATH})
	    set(CONFGENPATHS "")
	    foreach(genpath ${PROGRAM_SEARCH_PATHS})
	      set(CONFGENPATHS "${CONFGENPATHS};${genpath}${majorlibnum}${minorlibnum};${genpath}${majorlibnum}.${minorlibnum}")
	    endforeach(genpath ${PROGRAM_SEARCH_PATHS})
	    set(${PROGRAM_SEARCH_PATHS} "${PROGRAM_SEARCH_PATHS};${CONFGENPATHS}")
	  endforeach()
	  set(${PROGRAM_PATHS} ${PROGRAM_SEARCH_PATHS})
	endmacro()

	macro(FIND_LIBRARY_PATHS LIBRARY_PATHS FOUND_PATHS majorlibnum minorlibnum)
	  set(LIBRARY_MIDPATH "lib;lib64")
	  set(LIBRARY_APPENDPATH "tcl;tk;tcltk;tcltk/tcl;tcltk/tk")
	  set(LIBRARY_SEARCH_PATHS "")
	  foreach(foundpath ${${FOUND_PATHS}})
	    foreach(midpath ${LIBRARY_MIDPATH})
	      set(LIBRARY_SEARCH_PATHS "${LIBRARY_SEARCH_PATHS};${foundpath}/${midpath}")
	      foreach(appendpath ${LIBRARY_APPENDPATH})
		set(LIBRARY_SEARCH_PATHS "${LIBRARY_SEARCH_PATHS};${foundpath}/${midpath}/${appendpath}")
	      endforeach(appendpath ${LIBRARY_APPENDPATH})
	    endforeach(midpath ${LIBRARY_MIDPATH})
	    set(CONFGENPATHS "")
	    foreach(genpath ${LIBRARY_SEARCH_PATHS})
	      set(CONFGENPATHS "${CONFGENPATHS};${genpath}${majorlibnum}${minorlibnum};${genpath}${majorlibnum}.${minorlibnum}")
	    endforeach(genpath ${LIBRARY_SEARCH_PATHS})
	    set(${LIBRARY_SEARCH_PATHS} "${LIBRARY_SEARCH_PATHS};${CONFGENPATHS}")
	  endforeach()
	  set(${LIBRARY_PATHS} ${LIBRARY_SEARCH_PATHS})
	endmacro()

	macro(FIND_INCLUDE_PATHS INCLUDE_PATHS FOUND_PATHS majorlibnum minorlibnum)
	  set(INCLUDE_MIDPATH "include")
	  set(INCLUDE_APPENDPATH "tcl;tk;tcltk;tcltk/tcl;tcltk/tk")
	  set(INCLUDE_SEARCH_PATHS "")
	  foreach(foundpath ${${FOUND_PATHS}})
	    foreach(midpath ${INCLUDE_MIDPATH})
	      set(INCLUDE_SEARCH_PATHS "${INCLUDE_SEARCH_PATHS};${foundpath}/${midpath}")
	      foreach(appendpath ${INCLUDE_APPENDPATH})
		set(INCLUDE_SEARCH_PATHS "${INCLUDE_SEARCH_PATHS};${foundpath}/${midpath}/${appendpath}")
	      endforeach(appendpath ${INCLUDE_APPENDPATH})
	    endforeach(midpath ${INCLUDE_MIDPATH})
	    set(CONFGENPATHS "")
	    foreach(genpath ${INCLUDE_SEARCH_PATHS})
	      set(CONFGENPATHS "${CONFGENPATHS};${genpath}${majorlibnum}${minorlibnum};${genpath}${majorlibnum}.${minorlibnum}")
	    endforeach(genpath ${INCLUDE_SEARCH_PATHS})
	    set(${INCLUDE_SEARCH_PATHS} "${INCLUDE_SEARCH_PATHS};${CONFGENPATHS}")
	  endforeach()
	  set(${INCLUDE_PATHS} ${INCLUDE_SEARCH_PATHS})
	endmacro()


	macro(FIND_POSSIBLE_PATHS targetbinnames targetlibnames pathnames options)
	  set(PATH_RESULTS "")
	  foreach(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
	    foreach(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
	      foreach(SPATH ${${pathnames}})
		set(TCL_CURRENTPATH TCL-NOTFOUND)
		set(CURRENT_SEARCH_VERSION "${MAJORNUM}.${MINORNUM}")
		VALIDATE_VERSION(dosearch ${CURRENT_SEARCH_VERSION})
		if(dosearch)
		  FIND_PROGRAM_PATHS(BIN_POSSIBLE_PATHS SPATH ${MAJORNUM} ${MINORNUM})
		  FIND_LIBRARY_PATHS(LIB_POSSIBLE_PATHS SPATH ${MAJORNUM} ${MINORNUM})
		  foreach(targetbin ${targetbinnames})
		    if(TCL_CURRENTPATH MATCHES "NOTFOUND$")
		      find_program(TCL_CURRENTPATH NAMES ${targetbin}${MAJORNUM}.${MINORNUM} ${targetbin}${MAJORNUM}${MINORNUM} PATHS ${BIN_POSSIBLE_PATHS} ${options})
		    endif(TCL_CURRENTPATH MATCHES "NOTFOUND$")
		  endforeach(targetbin ${targetbinnames})
		  foreach(targetlib ${targetbinnames})
		    if(TCL_CURRENTPATH MATCHES "NOTFOUND$")
		      find_library(TCL_CURRENTPATH NAMES ${targetlib}${MAJORNUM}.${MINORNUM} ${targetlib}${MAJORNUM}${MINORNUM} PATHS ${LIB_POSSIBLE_PATHS} ${options})
		    endif(TCL_CURRENTPATH MATCHES "NOTFOUND$")
		  endforeach(targetlib ${targetbinnames})
		endif(dosearch)
		if(TCL_CURRENTPATH)
		  list(APPEND PATH_RESULTS ${SPATH})
		  list(REMOVE_ITEM ${pathnames} ${SPATH})
		endif(TCL_CURRENTPATH)
	      endforeach(SPATH ${${pathnames}})
	    endforeach()
	  endforeach()
	  set(${pathnames} ${PATH_RESULTS})
	endmacro()


	macro(FIND_CONFIG_FILES FOUND_PATHS TARGET_LIST filename)
	  set(CONFIG_MIDPATH "lib;lib64;share")
	  set(CONFIG_APPENDPATH "tcl;tk;tcltk;tcltk/tcl;tcltk/tk")
	  foreach(foundpath ${${FOUND_PATHS}})
	    set(CONFIG_SEARCH_PATHS "")
	    foreach(midpath ${CONFIG_MIDPATH})
	      set(CONFIG_SEARCH_PATHS "${CONFIG_SEARCH_PATHS};${foundpath}/${midpath}")
	      foreach(appendpath ${CONFIG_APPENDPATH})
		set(CONFIG_SEARCH_PATHS "${CONFIG_SEARCH_PATHS};${foundpath}/${midpath}/${appendpath}")
	      endforeach(appendpath ${CONFIG_APPENDPATH})
	    endforeach(midpath ${CONFIG_MIDPATH})
	    set(CONFGENPATHS "")
	    foreach(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
	      foreach(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
		set(CURRENT_SEARCH_VERSION "${MAJORNUM}.${MINORNUM}")
		VALIDATE_VERSION(isvalid ${CURRENT_SEARCH_VERSION})
		if(isvalid)
		  foreach(genpath ${CONFIG_SEARCH_PATHS})
		    set(CONFGENPATHS "${CONFGENPATHS};${genpath}${MAJORNUM}${MINORNUM};${genpath}${MAJORNUM}.${MINORNUM}")
		  endforeach(genpath ${CONFIG_SEARCH_PATHS})
		endif(isvalid)
	      endforeach(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
	    endforeach(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
	    set(CONFIG_SEARCH_PATHS "${CONFIG_SEARCH_PATHS};${CONFGENPATHS}")
	    set(conffile "${filename}-NOTFOUND")
	    find_file(conffile ${filename} PATHS ${CONFIG_SEARCH_PATHS} NO_SYSTEM_PATH)
	    if(NOT conffile MATCHES "NOTFOUND$")
	      set(${TARGET_LIST} "${${TARGET_LIST}}${conffile};")
	    endif(NOT conffile MATCHES "NOTFOUND$")
	  endforeach()
	endmacro()


	#-----------------------------------------------------------------------------
	#
	#              Routines for resetting Tcl/Tk variables
	#
	#-----------------------------------------------------------------------------


	macro(RESET_TCL_VARS)
	  set(TCL_VERSION_MAJOR "NOTFOUND")
	  set(TCL_VERSION_MINOR "NOTFOUND")
	  set(TCL_VERSION_PATCH "NOTFOUND")
	  set(TCL_INCLUDE_DIRS "NOTFOUND")
	  set(TCL_INCLUDE_PATH "NOTFOUND")
	  set(TCL_LIBRARIES "NOTFOUND")
	  set(TCL_LIBRARY "NOTFOUND")
	  set(TCL_STUB_LIBRARIES "NOTFOUND")
	  set(TCL_STUB_LIBRARY "NOTFOUND")
	  set(TCL_TCLSH "NOTFOUND")
	  set(TCL_TCLSH_EXECUTABLE "NOTFOUND")
	endmacro()

	macro(RESET_TK_VARS)
	  set(TK_VERSION_MAJOR "NOTFOUND")
	  set(TK_VERSION_MINOR "NOTFOUND")
	  set(TK_VERSION_PATCH "NOTFOUND")
	  set(TK_INCLUDE_PATH "NOTFOUND")
	  set(TCL_TK_LIBRARY "NOTFOUND")
	  set(TCL_TK_STUB_LIBRARY "NOTFOUND")
	  set(TCL_WISH_EXECUTABLE "NOTFOUND")
	  set(TK_WISH "NOTFOUND")
	endmacro()

	macro(VALIDATE_TCL_VARIABLES validvar)
	  if(NOT TCL_INCLUDE_PATH AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TCL_INCLUDE_PATH AND TCL_NEED_HEADERS)
	  if(NOT TCL_LIBRARY AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TCL_LIBRARY AND TCL_NEED_HEADERS)
	  if(NOT TCL_STUB_LIBRARY AND TCL_NEED_STUB_LIBS AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TCL_STUB_LIBRARY AND TCL_NEED_STUB_LIBS AND TCL_NEED_HEADERS)
	  if(NOT TCL_TCLSH_EXECUTABLE)
	    set(${validvar} 0)
	  endif(NOT TCL_TCLSH_EXECUTABLE)
	endmacro(VALIDATE_TCL_VARIABLES)


	macro(VALIDATE_TK_VARIABLES validvar)
	  if(NOT TK_INCLUDE_PATH AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TK_INCLUDE_PATH AND TCL_NEED_HEADERS)
	  if(NOT TCL_TK_LIBRARY AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TCL_TK_LIBRARY AND TCL_NEED_HEADERS)
	  if(NOT TCL_TK_STUB_LIBRARY AND TCL_NEED_STUB_LIBS AND TCL_NEED_HEADERS)
	    set(${validvar} 0)
	  endif(NOT TCL_TK_STUB_LIBRARY AND TCL_NEED_STUB_LIBS AND TCL_NEED_HEADERS)
	  if(NOT TCL_WISH_EXECUTABLE)
	    set(${validvar} 0)
	  endif(NOT TCL_WISH_EXECUTABLE)
	endmacro(VALIDATE_TK_VARIABLES)



	#-----------------------------------------------------------------------------
	#
	#       Routines for extracting variable values from Tcl/Tk config files
	#
	#-----------------------------------------------------------------------------

	macro(READ_TCLCONFIG_FILE tclconffile)
	  RESET_TCL_VARS()
	  set(TCL_CONF_FILE "")
	  file(READ ${tclconffile} TCL_CONF_FILE)
	  string(REGEX REPLACE "\r?\n" ";" ENT "${TCL_CONF_FILE}")
	  foreach(line ${ENT})
	    if(${line} MATCHES "TCL_MAJOR_VERSION")
	      string(REGEX REPLACE ".*TCL_MAJOR_VERSION='([0-9]*)'.*" "\\1" TCL_VERSION_MAJOR ${line})
	    endif(${line} MATCHES "TCL_MAJOR_VERSION")
	    if(${line} MATCHES "TCL_MINOR_VERSION")
	      string(REGEX REPLACE ".*TCL_MINOR_VERSION='([0-9]*)'.*" "\\1" TCL_VERSION_MINOR ${line})
	    endif(${line} MATCHES "TCL_MINOR_VERSION")
	    if(${line} MATCHES "TCL_PATCH_LEVEL")
	      string(REGEX REPLACE ".*TCL_PATCH_LEVEL='.([0-9]*)'.*" "\\1" TCL_VERSION_PATCH ${line})
	    endif(${line} MATCHES "TCL_PATCH_LEVEL")
	    if(${line} MATCHES "TCL_INCLUDE")
	      string(REGEX REPLACE ".*TCL_INCLUDE_SPEC='-I(.+)'.*" "\\1" TCL_INCLUDE_PATH ${line})
	    endif()
	    if(${line} MATCHES "TCL_PREFIX")
	      string(REGEX REPLACE ".*TCL_PREFIX='(.+)'" "\\1" TCL_PREFIX ${line})
	    endif()
	    if(${line} MATCHES "TCL_EXEC_PREFIX")
	      if(MSVC)
		set(TCL_EXE_SUFFIX ".exe")
	      else(MSVC)
		set(TCL_EXE_SUFFIX "")
	      endif(MSVC)
	      string(REGEX REPLACE ".*TCL_EXEC_PREFIX='(.+)'.*" "\\1" TCL_TCLSH_EXECUTABLE ${line})
	      IF (EXISTS "${TCL_TCLSH_EXECUTABLE}/bin/tclsh${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
		set(TCL_TCLSH_EXECUTABLE "${TCL_TCLSH_EXECUTABLE}/bin/tclsh${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_TCLSH_EXECUTABLE}/bin/tclsh${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
		set(TCL_TCLSH_EXECUTABLE "${TCL_TCLSH_EXECUTABLE}/bin/tclsh${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_TCLSH_EXECUTABLE}/bin/tclsh-${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
		set(TCL_TCLSH_EXECUTABLE "${TCL_TCLSH_EXECUTABLE}/bin/tclsh-${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_TCLSH_EXECUTABLE}/bin/tclsh-${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
		set(TCL_TCLSH_EXECUTABLE "${TCL_TCLSH_EXECUTABLE}/bin/tclsh-${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${TCL_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_TCLSH_EXECUTABLE}/bin/tclsh")
		set(TCL_TCLSH_EXECUTABLE "${TCL_TCLSH_EXECUTABLE}/bin/tclsh")
	      endif()
	    endif()
	    if(${line} MATCHES "TCL_STUB_LIB_PATH")
	      string(REGEX REPLACE ".*TCL_STUB_LIB_PATH='(.+)/lib.*" "\\1" TCL_STUB_LIB_PATH ${line})
	    endif()
	    if(${line} MATCHES "TCL_THREADS")
	      string(REGEX REPLACE ".*TCL_THREADS=(.+)" "\\1" TCL_THREADS ${line})
	    endif()
	  endforeach(line ${ENT})
	endmacro()


	macro(READ_TKCONFIG_FILE tkconffile)
	  RESET_TK_VARS()
	  set(TK_CONF_FILE "")
	  file(READ ${tkconffile} TK_CONF_FILE)
	  string(REGEX REPLACE "\r?\n" ";" ENT "${TK_CONF_FILE}")
	  foreach(line ${ENT})
	    if(${line} MATCHES "TK_MAJOR_VERSION")
	      string(REGEX REPLACE ".*TK_MAJOR_VERSION='([0-9]*)'.*" "\\1" TK_VERSION_MAJOR ${line})
	    endif(${line} MATCHES "TK_MAJOR_VERSION")
	    if(${line} MATCHES "TK_MINOR_VERSION")
	      string(REGEX REPLACE ".*TK_MINOR_VERSION='([0-9]*)'.*" "\\1" TK_VERSION_MINOR ${line})
	    endif(${line} MATCHES "TK_MINOR_VERSION")
	    if(${line} MATCHES "TK_PATCH_LEVEL")
	      string(REGEX REPLACE ".*TK_PATCH_LEVEL='.([0-9]*)'.*" "\\1" TK_VERSION_PATCH ${line})
	    endif(${line} MATCHES "TK_PATCH_LEVEL")
	    if(${line} MATCHES "TK_.*INCLUDE")
	      string(REGEX REPLACE ".*TK_.*INCLUDE.*='-I(.+)'.*" "\\1" TK_INCLUDE_PATH ${line})
	    endif()
	    if(${line} MATCHES "TK_PREFIX")
	      string(REGEX REPLACE ".*TK_PREFIX='(.+)'" "\\1" TK_PREFIX ${line})
	    endif()
	    if(${line} MATCHES "TK_EXEC_PREFIX")
	      if(MSVC)
		set(TK_EXE_SUFFIX ".exe")
	      else(MSVC)
		set(TK_EXE_SUFFIX "")
	      endif(MSVC)
	      string(REGEX REPLACE ".*TK_EXEC_PREFIX='(.+)'.*" "\\1" TCL_WISH_EXECUTABLE ${line})
	      IF (EXISTS "${TCL_WISH_EXECUTABLE}/bin/wish${TK_VERSION_MAJOR}${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
		set(TCL_WISH_EXECUTABLE "${TCL_WISH_EXECUTABLE}/bin/wish${TK_VERSION_MAJOR}${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_WISH_EXECUTABLE}/bin/wish${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
		set(TCL_WISH_EXECUTABLE "${TCL_WISH_EXECUTABLE}/bin/wish${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_WISH_EXECUTABLE}/bin/wish-${TK_VERSION_MAJOR}${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
		set(TCL_WISH_EXECUTABLE "${TCL_WISH_EXECUTABLE}/bin/wish-${TK_VERSION_MAJOR}${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_WISH_EXECUTABLE}/bin/wish-${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
		set(TCL_WISH_EXECUTABLE "${TCL_WISH_EXECUTABLE}/bin/wish-${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}${TK_EXE_SUFFIX}")
	      ELSEif(EXISTS "${TCL_WISH_EXECUTABLE}/bin/wish")
		set(TCL_WISH_EXECUTABLE "${TCL_WISH_EXECUTABLE}/bin/wish")
	      endif()
	    endif()
	  endforeach(line ${ENT})
	  if(NOT TK_INCLUDE_PATH)
	    set(TK_INCLUDE_PATH ${TCL_INCLUDE_PATH})
	  endif(NOT TK_INCLUDE_PATH)
	endmacro()


	#-----------------------------------------------------------------------------
	#
	#             Validate a Tcl/Tk installation based on options
	#
	#-----------------------------------------------------------------------------

	macro(VALIDATE_TCL validvar)
	  set(${validvar} 1)
	  VALIDATE_TCL_VARIABLES(${validvar})
	  if(TCL_TCLSH_EXECUTABLE)
	    TCL_GET_VERSION(${TCL_TCLSH_EXECUTABLE} CURRENT_SEARCH_VERSION)
	    VALIDATE_VERSION(dosearch ${CURRENT_SEARCH_VERSION})
	    if(NOT dosearch)
	      set(${validvar} 0)
	    endif(NOT dosearch)
	  endif(TCL_TCLSH_EXECUTABLE)
	  if(TCL_REQUIRE_THREADS AND TCL_TCLSH_EXECUTABLE)
	    TCL_ISTHREADED(${TCL_TCLSH_EXECUTABLE} TCL_THREADS)
	    if(NOT TCL_THREADS)
	      set(${validvar} 0)
	    endif(NOT TCL_THREADS)
	  endif(TCL_REQUIRE_THREADS AND TCL_TCLSH_EXECUTABLE)
	endmacro(VALIDATE_TCL)

	macro(VALIDATE_TK validvar)
	  set(${validvar} 1)
	  VALIDATE_TK_VARIABLES(${validvar})
	  if(TK_NATIVE_GRAPHICS OR TK_X11_GRAPHICS)
	    TK_GRAPHICS_SYSTEM(${TCL_WISH_EXECUTABLE} TK_SYSTEM_GRAPHICS)
	    if(APPLE AND TK_NATIVE_GRAPHICS)
	      if(NOT ${TK_SYSTEM_GRAPHICS} MATCHES "aqua")
		set(${validvar} 0)
	      endif()
	    endif()
	    if(WIN32 AND TK_NATIVE_GRAPHICS)
	      if(NOT ${TK_SYSTEM_GRAPHICS} MATCHES "win32")
		set(${validvar} 0)
	      endif()
	    endif()
	    if(TK_X11_GRAPHICS)
	      if(NOT ${TK_SYSTEM_GRAPHICS} MATCHES "x11")
		set(${validvar} 0)
	      endif()
	    endif()
	  endif(TK_NATIVE_GRAPHICS OR TK_X11_GRAPHICS)
	endmacro(VALIDATE_TK)


	#-----------------------------------------------------------------------------
	#
	#            Main Search Logic - Search for Tcl/Tk installations
	#
	# The general rules for searching are as follows:
	#
	# 1.  If a prefix is specified, use only that prefix in the search -
	#     do not attempt to find Tcl/Tk elsewhere on the system.
	#
	# 2.  If on Apple, check for Framework installations - they will be the
	#     first to be checked - potentially the only one(s) checked, depending
	#     on option settings.
	#
	# 3.  If options allow/dictate, pull all environment variables that might
	#     point to Tcl/Tk paths and look for tclsh/wish/libtcl/libtk variations
	#     in those paths.  Use the paths that contain at least one of the
	#     binary/library files as a basis for generating paths to look for
	#     config files.
	#
	# 4.  Search for config files, and if found read key variables from the
	#     files.  Config files are preferred when available, since they
	#     help to ensure the Find routines don't accidentally find mis-matched
	#     binary/library/include files.
	#
	# 5.  If config files found, use those paths to find full paths to binaries
	#     and libraries.
	#
	# 6.  If config files are NOT found, fall back on an ordered path search,
	#     with higher versions being preferred over lower.  This is done only
	#     to locate a runnable Tcl/Tk and will not set variables related to
	#     config files or include paths.  It is a last resort intended to
	#     allow systems that provide a stripped down Tcl/Tk to return useful
	#     results when the only functionality needed by the CMake project
	#     is to run tclsh/wish.
	#
	#-----------------------------------------------------------------------------

	# Because it is not out of the question for a build system to be building
	# its own local copy of Tcl/Tk (and hence define the needed variables
	# on its own) we check up front to see if all of the options have already
	# been satisfied by a mechanism other than find_package
	include(FindPackageHandleStandardArgs)
	set(PACKAGE_HANDLE_VARS "TCL_TCLSH_EXECUTABLE")
	set(TCL_TCLSH ${TCL_TCLSH_EXECUTABLE}) #Deprecated
	set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TCL_TCLSH")
	if(TCL_NEED_HEADERS)
	  set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TCL_LIBRARIES TCL_INCLUDE_DIRS TCL_CONF_PREFIX TCL_LIBRARY TCL_INCLUDE_PATH")
	  if(TCL_NEED_STUB_LIBS)
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TCL_STUB_LIBRARIES TCL_STUB_LIBRARY")
	  endif(TCL_NEED_STUB_LIBS)

	  if(TCL_REQUIRE_TK)
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TK_INCLUDE_PATH TCL_TK_CONF_PREFIX TCL_TK_LIBRARY TCL_WISH_EXECUTABLE")
	    set(TK_LIBRARY ${TCL_TK_LIBRARY}) # Deprecated
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TK_LIBRARY")
	    if(TCL_NEED_STUB_LIBS)
	      set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TCL_TK_STUB_LIBRARY")
	      set(TK_STUB_LIBRARY ${TCL_TK_STUB_LIBRARY}) #Deprecated
	      set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TK_STUB_LIBRARY")
	    endif(TCL_NEED_STUB_LIBS)
	  endif(TCL_REQUIRE_TK)
	else(TCL_NEED_HEADERS)
	  if(TCL_REQUIRE_TK)
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TCL_WISH_EXECUTABLE")
	    set(TK_WISH ${TCL_WISH_EXECUTABLE}) #Deprecated
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS} TK_WISH")
	  endif(TCL_REQUIRE_TK)
	endif(TCL_NEED_HEADERS)
	string(REGEX REPLACE " " ";" PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS}")
	set(TCL_FOUND TRUE)
	foreach(REQUIRED_VAR ${PACKAGE_HANDLE_VARS})
	  if(NOT ${REQUIRED_VAR})
	    set(TCL_FOUND FALSE)
	  endif(NOT ${REQUIRED_VAR})
	endforeach(REQUIRED_VAR ${PACKAGE_HANDLE_VARS})


	# Try to be a bit forgiving with the TCL prefix - if someone gives the
	# full path to the lib directory, catch that by adding the parent path
	# to the list to check
	if(NOT TCL_FOUND)
	  if(TCL_PREFIX)
	    set(TCL_LIBRARY "NOTFOUND")
	    set(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_PREFIX}/lib)
	    set(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_PREFIX})
	    get_filename_component(TCL_LIB_PATH_PARENT "${TCL_PREFIX}" PATH)
	    set(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_LIB_PATH_PARENT}/lib)
	    set(TCL_PREFIX_LIBDIRS ${TCL_PREFIX_LIBDIRS} ${TCL_LIB_PATH_PARENT})
	    list(REMOVE_DUPLICATES TCL_PREFIX_LIBDIRS)
	    FIND_LIBRARY_VERSIONS(tcl TCL_PREFIX_LIBDIRS NO_SYSTEM_PATH)
	    set(FOUND_FILES "${TCL_tcl_LIST}")
	    FIND_CONFIG_FILES(FOUND_FILES TCLCONFIG_LIST tclConfig.sh)
	    if(TCL_REQUIRE_TK)
	      FIND_LIBRARY_VERSIONS(tk TCL_PREFIX_LIBDIRS NO_SYSTEM_PATH)
	      set(FOUND_FILES "${TCL_tk_LIST}")
	      FIND_CONFIG_FILES(FOUND_FILES TKCONFIG_LIST tkConfig.sh)
	    endif(TCL_REQUIRE_TK)
	  else(TCL_PREFIX)
	    set(TCLCONFIG_LIST "")
	    if(APPLE)
	      include(CMakeFindFrameworks)
	      CMAKE_FIND_FRAMEWORKS(Tcl)
	      foreach(dir ${Tcl_FRAMEWORKS})
		set(tclconf "tclConfig.sh-NOTFOUND")
		find_file(tclconf tclConfig.sh PATHS ${dir})
		mark_as_advanced(tclconf)
		if(NOT tclconf MATCHES "NOTFOUND$")
		  set(TCLCONFIG_LIST "${TCLCONFIG_LIST}${tclconf};")
		endif(NOT tclconf MATCHES "NOTFOUND$")
	      endforeach(dir ${Tcl_FRAMEWORKS})
	      if(TCL_REQUIRE_TK)
		CMAKE_FIND_FRAMEWORKS(Tk)
		foreach(dir ${Tk_FRAMEWORKS})
		  set(tkconf "tkConfig.sh-NOTFOUND")
		  find_file(tkconf tkConfig.sh PATHS ${dir})
		  mark_as_advanced(tkconf)
		  if(NOT tkconf MATCHES "NOTFOUND$")
		    set(TKCONFIG_LIST "${TKCONFIG_LIST}${tkconf};")
		  endif(NOT tkconf MATCHES "NOTFOUND$")
		endforeach(dir ${Tk_FRAMEWORKS})
	      endif(TCL_REQUIRE_TK)
	    endif(APPLE)
	    if(NOT APPLE OR NOT TCL_USE_FRAMEWORK_ONLY)
	      set(PATHLIST "$ENV{LD_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}:$ENV{DYLD_FALLBACK_LIBRARY_PATH}:$ENV{PATH}")
	      set(PATHLIST "${TCL_ADDITIONAL_SEARCH_PATHS}:${PATHLIST}")
	      if(WIN32)
		WIN32TCLPATHS(PATHLIST "")
	      endif(WIN32)
	      string(REGEX REPLACE "/s?bin:" ":" PATHLIST "${PATHLIST}")
	      string(REGEX REPLACE "/s?bin;" ":" PATHLIST "${PATHLIST}")
	      string(REGEX REPLACE "/lib6?4?:" ":" PATHLIST "${PATHLIST}")
	      string(REGEX REPLACE "/lib6?4?;" ":" PATHLIST "${PATHLIST}")
	      string(REGEX REPLACE ":" ";" PATHLIST "${PATHLIST}")
	      list(REMOVE_DUPLICATES PATHLIST)
	      foreach(foundpath ${PATHLIST})
		foreach(pattern ${TCL_PATH_NOMATCH_PATTERNS})
		  if(foundpath MATCHES "${pattern}")
		    list(REMOVE_ITEM PATHLIST ${foundpath})
		  endif(foundpath MATCHES "${pattern}")
		endforeach(pattern ${TCL_PATH_NOMATCH_PATTERNS})
	      endforeach()
	      set(TCLPATHLIST "${PATHLIST}")
	      set(TKPATHLIST "${PATHLIST}")
	      FIND_POSSIBLE_PATHS("tclsh" "tcl" TCLPATHLIST NO_SYSTEM_PATH)
	      if(TCL_REQUIRE_TK)
		FIND_POSSIBLE_PATHS("wish" "tk" TKPATHLIST NO_SYSTEM_PATH)
	      endif(TCL_REQUIRE_TK)
	      # Hunt up tclConfig.sh files
	      FIND_CONFIG_FILES(TCLPATHLIST TCLCONFIG_LIST tclConfig.sh)
	      # Hunt up tkConfig.sh files
	      FIND_CONFIG_FILES(TKPATHLIST TKCONFIG_LIST tkConfig.sh)
	    endif(NOT APPLE OR NOT TCL_USE_FRAMEWORK_ONLY)
	  endif(TCL_PREFIX)
	  set(TCLVALID 0)
	  RESET_TCL_VARS()
	  foreach(tcl_config_file ${TCLCONFIG_LIST})
	    if(NOT TCLVALID)
	      RESET_TCL_VARS()
	      READ_TCLCONFIG_file(${tcl_config_file})
	      set(TCLVALID 1)
	      set(CURRENTTCLVERSION "${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}.${TCL_VERSION_PATCH}")
	      VALIDATE_VERSION(TCLVALID ${CURRENTTCLVERSION})
	      if(TCLVALID)
		get_filename_component(TCL_CONF_PREFIX "${tcl_config_file}" PATH)
		get_filename_component(TCL_LIBRARY_DIR2 "${TCL_CONF_PREFIX}" PATH)
		FIND_LIBRARY_PATHS(TCL_LIBRARY_SEARCH_PATHS TCL_PREFIX ${TCL_VERSION_MAJOR} ${TCL_VERSION_MINOR})
		set(TCL_LIBRARY_SEARCH_PATHS "${TCL_CONF_PREFIX};${TCL_LIBRARY_DIR2};${TCL_LIBRARY_SEARCH_PATHS}")
		find_library(TCL_LIBRARY tcl Tcl tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR} tcl${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR} PATHS ${TCL_LIBRARY_SEARCH_PATHS} NO_SYSTEM_PATH)
		find_library(TCL_STUB_LIBRARY tclstub${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR} tclstub${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR} PATHS ${TCL_LIBRARY_SEARCH_PATHS} NO_SYSTEM_PATH)
	      endif(TCLVALID)
	      VALIDATE_TCL(TCLVALID)
	      if(TCLVALID)
		if(TCL_REQUIRE_TK)
		  set(TKVALID 0)
		  foreach(tk_config_file ${TKCONFIG_LIST})
		    if(NOT TKVALID)
		      RESET_TK_VARS()
		      READ_TKCONFIG_file(${tk_config_file})
		      set(TKVALID 0)
		      set(CURRENTTKVERSION "${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}.${TK_VERSION_PATCH}")
		      set(vtcltkcompare 0)
		      if(${CURRENTTCLVERSION} VERSION_EQUAL ${CURRENTTKVERSION})
			set(TKVALID 1)
		      else()
			set(TKVALID 0)
		      endif()
		      if(TKVALID)
			get_filename_component(TCL_TK_CONF_PREFIX "${tk_config_file}" PATH)
			get_filename_component(TCL_TK_LIBRARY_DIR2 "${TCL_TK_CONF_PREFIX}" PATH)
			FIND_LIBRARY_PATHS(TCL_TK_LIBRARY_SEARCH_PATHS TK_PREFIX ${TK_VERSION_MAJOR} ${TK_VERSION_MINOR})
			set(TCL_TK_LIBRARY_SEARCH_PATHS "${TCL_TK_CONF_PREFIX};${TCL_TK_LIBRARY_DIR2};${TCL_TK_LIBRARY_SEARCH_PATHS}")
			find_library(TCL_TK_LIBRARY tk Tk tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR} tk${TK_VERSION_MAJOR}${TK_VERSION_MINOR} PATHS ${TCL_TK_LIBRARY_SEARCH_PATHS} NO_SYSTEM_PATH)
			find_library(TCL_TK_STUB_LIBRARY tkstub tkstub${TK_VERSION_MAJOR}.${TK_VERSION_MINOR} tkstub${TK_VERSION_MAJOR}${TK_VERSION_MINOR} PATHS ${TCL_TK_LIBRARY_SEARCH_PATHS} NO_SYSTEM_PATH)
			VALIDATE_TK(TKVALID)
		      endif(TKVALID)
		    endif(NOT TKVALID)
		  endforeach(tk_config_file ${TKCONFIG_LIST})
		  if(NOT TKVALID)
		    set(TCLVALID 0)
		    RESET_TCL_VARS()
		    RESET_TK_VARS()
		  endif(NOT TKVALID)
		endif(TCL_REQUIRE_TK)
	      endif(TCLVALID)
	    endif(NOT TCLVALID)
	  endforeach(tcl_config_file ${TCLCONFIG_LIST})

	  # If we still don't have anything by now, we may have a system without tclConfig.sh and tkConfig.sh
	  # Back to trying to guess values, using the TCLPATHLIST and TKPATHLIST arrays of paths. This is
	  # attempted ONLY if we are looking for a Tcl/Tk installation to call as a scripting engine and not
	  # as C libraries to build against - the autotools/TEA based Tcl/Tk world requires those files be
	  # present and any ExternalProject build attempting to use a Tcl/Tk installation without them would
	  # not succeed.
	  if(NOT TCLVALID AND NOT TCL_NEED_HEADERS)
	    set(PATHLIST "${TCLPATHLIST};${TKPATHLIST}")
	    list(REMOVE_DUPLICATES PATHLIST)
	    foreach(MAJORNUM ${TCL_POSSIBLE_MAJOR_VERSIONS})
	      foreach(MINORNUM ${TCL_POSSIBLE_MINOR_VERSIONS})
		foreach(SPATH ${PATHLIST})
		  if(NOT TCLVALID)
		    RESET_TCL_VARS()
		    set(CURRENT_SEARCH_VERSION "${MAJORNUM}.${MINORNUM}")
		    VALIDATE_VERSION(dosearch ${CURRENT_SEARCH_VERSION})
		    if(dosearch)
		      FIND_LIBRARY_PATHS(TCL_LIBRARY_SEARCH_PATHS SPATH ${MAJORNUM} ${MINORNUM})
		      FIND_PROGRAM_PATHS(TCL_PROGRAM_SEARCH_PATHS SPATH ${MAJORNUM} ${MINORNUM})
		      find_program(TCL_TCLSH_EXECUTABLE NAMES tclsh${MAJORNUM}.${MINORNUM} tclsh${MAJORNUM}${MINORNUM} PATHS ${TCL_PROGRAM_SEARCH_PATHS} NO_SYSTEM_PATH)
		    endif(dosearch)
		    VALIDATE_TCL(TCLVALID)
		    if(TCLVALID)
		      if(TCL_REQUIRE_TK)
			set(TKVALID 0)
			RESET_TK_VARS()
			find_program(TCL_WISH_EXECUTABLE NAMES wish${MAJORNUM}.${MINORNUM} wish${MAJORNUM}${MINORNUM} PATHS ${TCL_PROGRAM_SEARCH_PATHS} NO_SYSTEM_PATH)
			VALIDATE_TK(TKVALID)
			if(NOT TKVALID)
			  set(TCLVALID 0)
			  RESET_TCL_VARS()
			  RESET_TK_VARS()
			endif(NOT TKVALID)
		      endif(TCL_REQUIRE_TK)
		    else(TCLVALID)
		      RESET_TCL_VARS()
		    endif(TCLVALID)
		  endif(NOT TCLVALID)
		endforeach(SPATH ${PATHLIST})
	      endforeach()
	    endforeach()
	  endif(NOT TCLVALID AND NOT TCL_NEED_HEADERS)

	  # By this point we have found everything we're going to find - set variables to be exposed as results
	  set(TCL_TCLSH ${TCL_TCLSH_EXECUTABLE})
	  set(TK_WISH ${TCL_WISH_EXECUTABLE})
	  set(TK_LIBRARY ${TCL_TK_LIBRARY}) # Deprecated
	  set(TK_STUB_LIBRARY ${TCL_TK_STUB_LIBRARY})
	  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_PATH} ${TK_INCLUDE_PATH})
	  set(TCL_LIBRARIES ${TCL_LIBRARY} ${TK_LIBRARY})
	  set(TCL_STUB_LIBRARIES ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY})
	  set(TCL_ROOT_PATH ${TCL_CONF_PREFIX})
	  set(TCL_VERSION_STRING "${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}.${TCL_VERSION_PATCH}")
	  FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL DEFAULT_MSG TCL_LIBRARY ${PACKAGE_HANDLE_VARS})
	  if(TCL_REQUIRE_TK)
	    set(TK_FIND_QUIETLY ${TCL_FIND_QUIETLY})
	    set(PACKAGE_HANDLE_VARS "${PACKAGE_HANDLE_VARS};TCL_LIBRARY")
	    FIND_PACKAGE_HANDLE_STANDARD_ARGS(TK DEFAULT_MSG TCL_TK_LIBRARY ${PACKAGE_HANDLE_VARS})
	  endif(TCL_REQUIRE_TK)

	  foreach(pkgvar ${PACKAGE_HANDLE_VARS})
	    set(${pkgvar} ${${pkgvar}} CACHE STRING "set by FindTCL" FORCE)
	  endforeach(pkgvar ${PACKAGE_HANDLE_VARS})

	endif(NOT TCL_FOUND)

	mark_as_advanced(
	  TCL_INCLUDE_DIRS
	  TCL_INCLUDE_PATH
	  TK_INCLUDE_PATH
	  TCL_LIBRARIES
	  TCL_LIBRARY
	  TCL_TK_LIBRARY
	  TK_LIBRARY
	  TCL_STUB_LIBRARIES
	  TCL_STUB_LIBRARY
	  TCL_TK_STUB_LIBRARY
	  TK_STUB_LIBRARY
	  TCL_TCLSH_EXECUTABLE
	  TCL_TCLSH
	  TCL_WISH_EXECUTABLE
	  TK_WISH
	  TCL_CONF_PREFIX
	  TCL_TK_CONF_PREFIX
	  )

	# Other variables to hide
	mark_as_advanced(
	  TCL_CURRENTPATH
	  TCL_MIN_VERSION
	  TCL_MAX_VERSION
	  TCL_NEED_HEADERS
	  TCL_NEED_STUB_LIBS
	  TCL_PATH_NOMATCH_PATTERNS
	  TCL_REQUIRE_THREADS
	  TCL_REQUIRE_TK
	  TCL_USE_FRAMEWORK_ONLY
	  TK_NATIVE_GRAPHICS
	  TK_X11_GRAPHICS
	  conffile
	  USE_TCL_STUBS
	  USE_TK_STUBS
	  )


	# Local Variables:
	# tab-width: 8
	# mode: cmake
	# indent-tabs-mode: t
	# End:
	# ex: shiftwidth=2 tabstop=8
