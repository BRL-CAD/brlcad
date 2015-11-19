#            T H I R D P A R T Y _ T C L . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

# The question of whether and when to build Tcl/Tk extensions is among
# the most complex issues in BRL-CAD's build logic.  In order of priority,
# these are the factors considered when evaluating whether to compile
# a Tcl/Tk extension:
#
# 1.  Are the variables listed in required_vars enabled?  If one or more
#     of them is not, there is never any point in building or searching for
#     packages requiring those variables. Examples of variables used here
#     include BRLCAD_ENABLE_TK and BRLCAD_ENABLE_OPENGL.  If the BRLCAD_<PKG>
#     variable is set to SYSTEM, the only impact this will have if triggered
#     is to disable the execution of the system test.
# 2.  Is the extension variable set to SYSTEM explicitly?  If so, the
#     extension will not be built under any circumstances.  Whether it
#     is tested for depends on condition 1 above.  If it is tested for and
#     not detected (e.g. BRL-CAD's Tk features are enabled and Tkhtml
#     is not found) a BRLCAD_<PKG>_NOTFOUND variable will be set, but
#     the extension will not be enabled.
# 3.  Is the extension variable set to BUNDLED explicitly?  If so, only
#     a disabled required_vars variable will disable it, and only then if
#     the extension requires that variable.
# 4.  If BRLCAD_TCL has been set explicitly to SYSTEM (not automatically
#     as a consequence of detecting it in an AUTO search) Tcl/Tk
#     extensions will also need to be SYSTEM unless explicitly enabled
#     by their own BRLCAD_<PKG> variable (condition 3 above).  If not
#     found, BRLCAD_<PKG>_NOTFOUND will also be set in this instance.
# 5.  If BRLCAD_TCL has been automatically set to SYSTEM as a consequence
#     of BRLCAD_BUNDLED_LIBS being set to SYSTEM, the situation is
#     regarded as identical to condition 4 above. If not found,
#     BRLCAD_<PKG>_NOTFOUND will also be set in this instance.
# 6.  If BRLCAD_TCL_BUILD is ON (either because BRLCAD_TCL is set to
#     BUNDLED explicitly, because it has been set to BUNDLED (Auto) due
#     to BRLCAD_BUNDLED_LIBS being set to BUNDLED, or because the AUTO
#     search failed to find an acceptable Tcl on the system) Tcl/Tk
#     extensions also default to ON, subject to the above conditions.
#     If the extension has been forced to OFF for any reason other than
#     a feature-based disabling and BRLCAD_TCL is hard-set to
#     bundled (or BRLCAD_BUNDLED_LIBS is set to BUNDLED and BRLCAD_TCL
#     is not hard-set to SYSTEM) CMake will FATAL_ERROR - using a system
#     package when we're NOT using a system Tcl/Tk is probably not
#     workable - even if it somehow compiled and ran, the probability
#     of subtle issues would be high.
# 7.  If none of the above conditions preclude the usefulness of doing
#     so, test the available Tcl/Tk to see if it supplies the package
#     in question.  If NEEDS_LIBS is populated, the Tcl/Tk "package require"
#     mechanism is not enough and a system version of the package must
#     also have locatable C libraries  If a system version is found that
#     satisfies all criteria, compilation of the extension is disabled -
#
#     Note:  For the time being, this routine does not attempt to address
#     the problem of header identification.  In principle it should do so,
#     but the current reality is that oftentimes "private" headers will be
#     needed by BRL-CAD when using Tcl/Tk extensions - these headers may
#     not be installed on system versions of packages, or may be in
#     non-standard places if they are.  In those situations, if the system
#     version of the Tcl/Tk package in question is compatible with the
#     copy in src/other (usually the case, since BRL-CAD requires a fairly
#     modern Tcl/Tk to work), the build can use the system package with
#     the *local* copy of the package's private headers, even when building
#     with system extensions.  This is sub-optimal, and eventually logic
#     should be added to

#-----------------------------------------------------------------------------
macro(THIRD_PARTY_TCL_PACKAGE pkgname dir wishcmd depends required_vars NEEDS_LIBS aliases description)

  # Get uppercase version of pkg string
  string(TOUPPER ${pkgname} PKGNAME_UPPER)
  # If the pkg variable has been explicitly set, get
  # an uppercase version of it for easier matching
  if(NOT ${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}} STREQUAL "")
    string(TOUPPER "${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}}" OPT_STR_UPPER)
  else(NOT ${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}} STREQUAL "")
    set(OPT_STR_UPPER "")
  endif(NOT ${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}} STREQUAL "")
  if(NOT ${OPT_STR_UPPER} STREQUAL "")
    if(${OPT_STR_UPPER} STREQUAL "ON")
      set(OPT_STR_UPPER "BUNDLED")
    endif(${OPT_STR_UPPER} STREQUAL "ON")
    if(${OPT_STR_UPPER} STREQUAL "OFF")
      set(OPT_STR_UPPER "SYSTEM")
    endif(${OPT_STR_UPPER} STREQUAL "OFF")
  endif(NOT ${OPT_STR_UPPER} STREQUAL "")


  # Initialize some variables
  set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD "?")
  set(${PKGNAME_UPPER}_DISABLED 0)
  set(${PKGNAME_UPPER}_DISABLE_TEST 0)
  set(${PKGNAME_UPPER}_MET_CONDITION 0)

  # 1. First up - If any of the required flags are off, not only is this extension
  # build a no-go but so is the test - BRL-CAD doesn't require it.
  set(DISABLE_STR "")
  foreach(item ${required_vars})
    if(NOT ${item})
      set(${PKGNAME_UPPER}_DISABLED 1)
      set(${PKGNAME_UPPER}_DISABLE_TEST 1)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
      if(NOT DISABLE_STR)
	set(DISABLE_STR "${item}")
      else(NOT DISABLE_STR)
	set(DISABLE_STR "${DISABLE_STR},${item}")
      endif(NOT DISABLE_STR)
    endif(NOT ${item})
  endforeach(item ${required_vars})
  if(DISABLE_STR)
    set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "DISABLED ${DISABLE_STR}" CACHE STRING "DISABLED ${DISABLED_STR}" FORCE)
    mark_as_advanced(FORCE ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER})
    set(${PKGNAME_UPPER}_MET_CONDITION 1)
  else(DISABLE_STR)
    # If we have a leftover disabled setting in the cache from earlier runs, clear it.
    if("${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}" MATCHES "DISABLED")
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "" CACHE STRING "Clear DISABLED setting" FORCE)
      mark_as_advanced(CLEAR ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER})
    endif("${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}" MATCHES "DISABLED")
  endif(DISABLE_STR)

  # 2. Next - is the pkg explicitly set to system?  If it is,
  # we are NOT building it.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "SYSTEM" CACHE STRING "User forced to SYSTEM" FORCE)
      set(${PKGNAME_UPPER}_MET_CONDITION 2)
    endif("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
  endif(OPT_STR_UPPER)


  # 3. If we have an explicit BUNDLE request for this particular extension,
  # honor it as long as features are satisfied.  No point in testing
  # if we know we're turning it on - set vars accordingly.
  if(OPT_STR_UPPER)
    if("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
      if(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "BUNDLED" CACHE STRING "User forced to BUNDLED" FORCE)
	set(${PKGNAME_UPPER}_DISABLE_TEST 1)
	set(${PKGNAME_UPPER}_MET_CONDITION 3)
      endif(NOT ${PKGNAME_UPPER}_MET_CONDITION)
    endif("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
  endif(OPT_STR_UPPER)


  # 4. If BRLCAD_TCL is *exactly* SYSTEM and we aren't already enabled,
  # compilation is off.  (Testing depends on #1).
  if("${${CMAKE_PROJECT_NAME}_TCL}" STREQUAL "SYSTEM")
    if(NOT ${PKGNAME_UPPER}_MET_CONDITION)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "SYSTEM (AUTO)" CACHE STRING "From ${CMAKE_PROJECT_NAME}_TCL - SYSTEM" FORCE)
      set(${PKGNAME_UPPER}_MET_CONDITION 4)
    endif(NOT ${PKGNAME_UPPER}_MET_CONDITION)
  endif("${${CMAKE_PROJECT_NAME}_TCL}" STREQUAL "SYSTEM")


  # 5. If BRLCAD_TCL matches SYSTEM and BRLCAD_BUNDLED_LIBS is *exactly*
  # SYSTEM, compilation is off. (Testing depends on #1).
  if("${${CMAKE_PROJECT_NAME}_TCL}" MATCHES "SYSTEM" AND "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")
    if(NOT ${PKGNAME_UPPER}_MET_CONDITION)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "SYSTEM (AUTO)" CACHE STRING "From ${CMAKE_PROJECT_NAME}_BUNDLED_LIBS - SYSTEM" FORCE)
      set(${PKGNAME_UPPER}_MET_CONDITION 5)
    endif(NOT ${PKGNAME_UPPER}_MET_CONDITION)
  endif("${${CMAKE_PROJECT_NAME}_TCL}" MATCHES "SYSTEM" AND "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")

  # 6. If we're building Tcl itself (for whatever reason) and we aren't
  # already off, default to compilation.  If we're building, no point
  # in testing - disable.
  if(${CMAKE_PROJECT_NAME}_TCL_BUILD)
    if(NOT ${PKGNAME_UPPER}_MET_CONDITION)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
      set(${PKGNAME_UPPER}_DISABLE_TEST 1)
      set(${PKGNAME_UPPER}_MET_CONDITION 6)
    endif(NOT ${PKGNAME_UPPER}_MET_CONDITION)
  endif(${CMAKE_PROJECT_NAME}_TCL_BUILD)
  # Check for the bizarre case of bundled Tcl + system package - error out if that
  # combination has been specified, since there is no expectation that a package
  # provided for a system installation of Tcl/Tk would work with BRL-CAD's local
  # copy.  If the feature was disabled it doesn't matter, since that's a
  # "disabled" condition instead of a SYSTEM condition.  If we don't hit such a
  # case, properly set ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}
  if(NOT ${PKGNAME_UPPER}_DISABLED)
    if(${CMAKE_PROJECT_NAME}_TCL_BUILD)
      if(NOT ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	message(FATAL_ERROR "Local Tcl compilation enabled, but compilation of Tcl/Tk extension ${pkgname} has been disabled - unsupported configuration!")
      else(NOT ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "BUNDLED (AUTO)" CACHE STRING "From ${CMAKE_PROJECT_NAME}_TCL - BUNDLED" FORCE)
      endif(NOT ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
    endif(${CMAKE_PROJECT_NAME}_TCL_BUILD)
  endif(NOT ${PKGNAME_UPPER}_DISABLED)

  # 7. If none of the above conditions preclude us from doing so, start
  # testing.

  if(NOT ${PKGNAME_UPPER}_DISABLE_TEST)
    # Initialize test tracking variable
    set(${PKGNAME_UPPER}_TEST_FAIL 0)
    set(${PKGNAME_UPPER}_TCL_PASSED 0)
    set(${PKGNAME_UPPER}_LIBS_FAIL 0)
    # Stash the previous results (if any) so we don't repeatedly
    # call out the tests - only report if something actually
    # changes in subsequent runs.
    set(${PKGNAME_UPPER}_FOUND_STATUS ${${PKGNAME_UPPER}_FOUND})

    # 1.  If we have no wish command, we fail immediately.
    if("${wishcmd}" STREQUAL "" OR "${wishcmd}" MATCHES "NOTFOUND")
      message(WARNING "Test for ${pkgname} failed - no tclsh/wish command available for testing!")
      set(${PKGNAME_UPPER}_TEST_FAIL 1)
    endif("${wishcmd}" STREQUAL "" OR "${wishcmd}" MATCHES "NOTFOUND")

    # 2.  We have wish - now things get interesting.  Write out
    # a Tcl script to probe for information on the package we're
    # interested in, and collect the results.
    if(NOT ${PKGNAME_UPPER}_TEST_FAIL)
      set(CURRENT_TCL_PACKAGE_NAME ${pkgname})
      set(pkg_test_file ${CMAKE_BINARY_DIR}/CMakeTmp/${pkgname}_version.tcl)
      configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/tcltest.tcl.in ${pkg_test_file} @ONLY)
      EXEC_PROGRAM(${wishcmd} ARGS ${pkg_test_file} OUTPUT_VARIABLE EXECOUTPUT)
      file(READ ${CMAKE_BINARY_DIR}/CMakeTmp/${PKGNAME_UPPER}_PKG_VERSION pkgversion)
      string(REGEX REPLACE "\n" "" ${PKGNAME_UPPER}_PACKAGE_VERSION ${pkgversion})
      if(${PKGNAME_UPPER}_PACKAGE_VERSION)
	set(${PKGNAME_UPPER}_TCL_PASSED 1)
      else(${PKGNAME_UPPER}_PACKAGE_VERSION)
	set(${PKGNAME_UPPER}_TCL_PASSED 0)
      endif(${PKGNAME_UPPER}_PACKAGE_VERSION)
    endif(NOT ${PKGNAME_UPPER}_TEST_FAIL)

    # 3.  After 2, we *almost* know what to do.  Check if we need to base
    # the decision on C libraries and headers, as well.  If we didn't pass
    # the package version test, then the libraries and headers are moot -
    # we can't assume the package is set up correctly, and need to build
    # our own or report find failure, as the case may be.
    #
    # It might be that the variables we need have already been set by
    # another routine (FindTCL.cmake, for example, does some sophisticated
    # work to set the correct library paths for Tk as part of its search)
    # The user might also be setting variables manually to identify locations
    # outside normal search parameters. On the other hand, if we are changing
    # CMake build settings, we don't want to let build-target settings satisfy
    # the requirements.
    #
    # To handle this, each library variable is first checked to see if it's
    # full path expansion matches the default variable setting.  If so, accept
    # the setting.  If not, the setting is assumed to be a build target from
    # a bundled configuration and cleared.  (If the bundled sources are built
    # in the end, they will be restored.)

    if(${PKGNAME_UPPER}_TCL_PASSED)
      if(NOT "${NEEDS_LIBS}" STREQUAL "")
	# We have libraries to find - let's get started.
	foreach(libname ${NEEDS_LIBS})
	  # get the uppercase version of the lib name
	  string(TOUPPER "${libname}" LIBNAME_UPPER)
	  # deduce the variable to hold the library results
	  if(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
	    set(LOOKFOR_LIBRARY "${PKGNAME_UPPER}_LIBRARY")
	    set(CORENAME ${pkgname})
	  else(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
	    set(LOOKFOR_LIBRARY "${PKGNAME_UPPER}_${LIBNAME_UPPER}_LIBRARY")
	    set(CORENAME ${libname})
	  endif(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
	  set(${PKGNAME_UPPER}_REQUIRED_LIBS ${${PKGNAME_UPPER}_REQUIRED_LIBS} ${${LOOKFOR_LIBRARY}})
	  # Check if this variable has already been set, and if
	  # so put it to the full path test
	  if(${LOOKFOR_LIBRARY})
	    get_filename_component(PATH_ABS ${${LOOKFOR_LIBRARY}} ABSOLUTE)
	    if(NOT "${${LOOKFOR_LIBRARY}}" STREQUAL "${PATH_ABS}")
	      set(${LOOKFOR_LIBRARY} "NOTFOUND" CACHE STRING "RESET" FORCE)
	    endif(NOT "${${LOOKFOR_LIBRARY}}" STREQUAL "${PATH_ABS}")
	  endif(${LOOKFOR_LIBRARY})
	  # If we don't already have a library value, go looking
	  if(NOT ${LOOKFOR_LIBRARY} OR "${${LOOKFOR_LIBRARY}}" MATCHES "NOTFOUND")
	    find_library(${LOOKFOR_LIBRARY} NAMES ${libname} ${libname}${${PKGNAME_UPPER}_PACKAGE_VERSION} PATH_SUFFIXES ${libname}${${PKGNAME_UPPER}_PACKAGE_VERSION} ${pkgname}${${PKGNAME_UPPER}_PACKAGE_VERSION})
	  endif(NOT ${LOOKFOR_LIBRARY} OR "${${LOOKFOR_LIBRARY}}" MATCHES "NOTFOUND")
	  # If we didn't find anything, we've failed to satisfy
	  # the package requirements
	  if(NOT ${LOOKFOR_LIBRARY})
	    set(${PKGNAME_UPPER}_LIBS_FAIL 1)
	  endif(NOT ${LOOKFOR_LIBRARY})
	  mark_as_advanced(${LOOKFOR_LIBRARY})
	endforeach(libname ${NEEDS_LIBS})
      endif(NOT "${NEEDS_LIBS}" STREQUAL "")
    endif(${PKGNAME_UPPER}_TCL_PASSED)
  endif(NOT ${PKGNAME_UPPER}_DISABLE_TEST)

  # After all that, we finally know what to do - do it.
  string(TOLOWER ${pkgname} PKGNAME_LOWER)

  # If testing was disabled, we only care what the _BUILD variable says.
  # Even if DISABLED, we still need the distcheck ignore, so do the following
  # for ALL situations.
  if(${PKGNAME_UPPER}_DISABLE_TEST)
    if(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
      add_subdirectory(${dir})
      foreach(dep ${depends})
	string(TOUPPER ${dep} DEP_UPPER)
	if(BRLCAD_BUILD_${DEP_UPPER})
	  add_dependencies(${pkgname} ${dep})
	endif(BRLCAD_BUILD_${DEP_UPPER})
      endforeach(dep ${depends})
      include(${CMAKE_CURRENT_SOURCE_DIR}/${PKGNAME_LOWER}.dist)
      CMAKEFILES_IN_DIR(${PKGNAME_LOWER}_ignore_files ${dir})
    else(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
      CMAKEFILES(${dir})
    endif(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
  endif(${PKGNAME_UPPER}_DISABLE_TEST)

  # If testing was NOT disabled, but we're forced to system, see if we
  # need to complain about not finding what we need - we can't enable, so
  # complain.  This situation is not as dangerous as the potential subtle
  # issues with system pkg + local Tcl, so we won't FATAL_ERROR - just
  # warn and let things proceed.
  if(NOT ${PKGNAME_UPPER}_DISABLE_TEST)
    if(${PKGNAME_UPPER}_MET_CONDITION STREQUAL "4" OR ${PKGNAME_UPPER}_MET_CONDITION STREQUAL "5")
      if(${PKGNAME_UPPER}_TEST_FAIL OR ${PKGNAME_UPPER}_LIBS_FAIL OR NOT ${PKGNAME_UPPER}_TCL_PASSED)
	set(BRLCAD_${PKGNAME_UPPER}_NOTFOUND 1)
	if(NOT "${wishcmd}" STREQUAL "" AND NOT "${wishcmd}" MATCHES "NOTFOUND")
	  message(WARNING "Local compilation of Tcl/Tk extension ${pkgname} was disabled, but system version not found!")
	endif(NOT "${wishcmd}" STREQUAL "" AND NOT "${wishcmd}" MATCHES "NOTFOUND")
      endif(${PKGNAME_UPPER}_TEST_FAIL OR ${PKGNAME_UPPER}_LIBS_FAIL OR NOT ${PKGNAME_UPPER}_TCL_PASSED)
    endif(${PKGNAME_UPPER}_MET_CONDITION STREQUAL "4" OR ${PKGNAME_UPPER}_MET_CONDITION STREQUAL "5")
  endif(NOT ${PKGNAME_UPPER}_DISABLE_TEST)

  # If testing was NOT disable, and none of the MET_CONDITIONs are satisfied,
  # we base our decision on the test results.
  if(NOT ${PKGNAME_UPPER}_MET_CONDITION)
    if(NOT ${PKGNAME_UPPER}_TEST_FAIL)
      if(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
	if(${PKGNAME_UPPER}_TCL_PASSED)
	  set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
	  set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "SYSTEM (AUTO)" CACHE STRING "Detection based - SYSTEM" FORCE)
	else(${PKGNAME_UPPER}_TCL_PASSED)
	  set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	  set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "BUNDLED (AUTO)" CACHE STRING "Detection based - BUNDLED" FORCE)
	endif(${PKGNAME_UPPER}_TCL_PASSED)
      else(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
	set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "BUNDLED (AUTO)" CACHE STRING "Detection based - BUNDLED" FORCE)
      endif(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
    else(NOT ${PKGNAME_UPPER}_TEST_FAIL)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
      set(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "BUNDLED (AUTO)" CACHE STRING "Detection based - BUNDLED" FORCE)
    endif(NOT ${PKGNAME_UPPER}_TEST_FAIL)
    if(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
      add_subdirectory(${dir})
      foreach(dep ${depends})
	string(TOUPPER ${dep} DEP_UPPER)
	if(BRLCAD_BUILD_${DEP_UPPER})
	  add_dependencies(${pkgname} ${dep})
	endif(BRLCAD_BUILD_${DEP_UPPER})
      endforeach(dep ${depends})
      include(${CMAKE_CURRENT_SOURCE_DIR}/${PKGNAME_LOWER}.dist)
      CMAKEFILES_IN_DIR(${PKGNAME_LOWER}_ignore_files ${dir})
    else(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
      CMAKEFILES(${dir})
    endif(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
  endif(NOT ${PKGNAME_UPPER}_MET_CONDITION)

  OPTION_ALIASES("${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}" "${aliases}" "ABS")
  OPTION_DESCRIPTION("${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}" "${aliases}" "${description}")

  # For drop-down menus in CMake gui - set STRINGS property
  set_property(CACHE ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} PROPERTY STRINGS AUTO BUNDLED SYSTEM)

endmacro(THIRD_PARTY_TCL_PACKAGE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
