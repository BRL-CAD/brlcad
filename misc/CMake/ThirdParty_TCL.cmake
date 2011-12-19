#            T H I R D P A R T Y _ T C L . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
#     regarded as identical to conditon 4 above. If not found, 
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
MACRO(THIRD_PARTY_TCL_PACKAGE pkgname dir wishcmd depends required_vars NEEDS_LIBS aliases description)

    # Get uppercase version of pkg string
    STRING(TOUPPER ${pkgname} PKGNAME_UPPER)
    # If the pkg variable has been explicitly set, get
    # an uppercase version of it for easier matching
    IF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER})
	STRING(TOUPPER ${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}} OPT_STR_UPPER)
    ELSE(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER})
	SET(OPT_STR_UPPER "")
    ENDIF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER})

    # Initialize some variables
    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD "?")
    SET(${PKGNAME_UPPER}_DISABLED 0)
    SET(${PKGNAME_UPPER}_DISABLE_TEST 0)
    SET(${PKGNAME_UPPER}_MET_CONDITION 0)

    # 1. First up - If any of the required flags are off, not only is this extension 
    # build a no-go but so is the test - BRL-CAD doesn't require it.
    SET(DISABLE_STR "") 
    FOREACH(item ${required_vars})
	IF(NOT ${item})
	    SET(${PKGNAME_UPPER}_DISABLED 1)
	    SET(${PKGNAME_UPPER}_DISABLE_TEST 1)
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
	    IF(NOT DISABLE_STR) 
		SET(DISABLE_STR "${item}")
	    ELSE(NOT DISABLE_STR) 
		SET(DISABLE_STR "${DISABLE_STR},${item}")
	    ENDIF(NOT DISABLE_STR) 
	ENDIF(NOT ${item})
    ENDFOREACH(item ${required_vars})
    IF(DISABLE_STR)
	SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "DISABLED ${DISABLE_STR}")
	SET(${PKGNAME_UPPER}_MET_CONDITION 1)
    ENDIF(DISABLE_STR)

    # Now that we've sorted the feature based controls out, define an option.
    BRLCAD_BUNDLE_OPTION(${CMAKE_PROJECT_NAME}_TCL ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER} "${${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}}" ${aliases} ${description}) 

    # 2. Next - is the pkg explicitly set to system?  If it is,
    # we are NOT building it.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
	    SET(${PKGNAME_UPPER}_MET_CONDITION 2)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "SYSTEM")
    ENDIF(OPT_STR_UPPER)


    # 3. If we have an explicit BUNDLE request for this particular extension,
    # honor it as long as features are satisfied.  No point in testing
    # if we know we're turning it on - set vars accordingly.
    IF(OPT_STR_UPPER)
	IF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
	    IF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
		SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
		SET(${PKGNAME_UPPER}_DISABLE_TEST 1)
		SET(${PKGNAME_UPPER}_MET_CONDITION 3)
	    ENDIF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	ENDIF("${OPT_STR_UPPER}" STREQUAL "BUNDLED")
    ENDIF(OPT_STR_UPPER)


    # 4. If BRLCAD_TCL is *exactly* SYSTEM and we aren't already enabled,
    # compilation is off.  (Testing depends on #1).
    IF("${${CMAKE_PROJECT_NAME}_TCL}" STREQUAL "SYSTEM")
	IF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
	    SET(${PKGNAME_UPPER}_MET_CONDITION 4)
	ENDIF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
    ENDIF("${${CMAKE_PROJECT_NAME}_TCL}" STREQUAL "SYSTEM")


    # 5. If BRLCAD_TCL matches SYSTEM and BRLCAD_BUNDLED_LIBS is *exactly* 
    # SYSTEM, compilation is off. (Testing depends on #1).
    IF("${${CMAKE_PROJECT_NAME}_TCL}" MATCHES "SYSTEM" AND "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")
	IF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
	    SET(${PKGNAME_UPPER}_MET_CONDITION 5)
	ENDIF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
    ENDIF("${${CMAKE_PROJECT_NAME}_TCL}" MATCHES "SYSTEM" AND "${${CMAKE_PROJECT_NAME}_BUNDLED_LIBS}" STREQUAL "SYSTEM")

    # 6. If we're building Tcl itself (for whatever reason) and we aren't
    # already off, default to compilation.  If we're building, no point 
    # in testing - disable.
    IF(${CMAKE_PROJECT_NAME}_TCL_BUILD)
	IF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	    SET(${PKGNAME_UPPER}_DISABLE_TEST 1)
	    SET(${PKGNAME_UPPER}_MET_CONDITION 6)
	ENDIF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
    ENDIF(${CMAKE_PROJECT_NAME}_TCL_BUILD)
    # Check for the bizarre case of bundled Tcl + system package - error out if that
    # combination has been specified, since there is no expectation that a package
    # provided for a system installation of Tcl/Tk would work with BRL-CAD's local 
    # copy.  If the feature was disabled it doesn't matter, since that's a 
    # "disabled" condition instead of a SYSTEM condition.
    IF(NOT ${PKGNAME_UPPER}_DISABLED)
	IF(${CMAKE_PROJECT_NAME}_TCL_BUILD)
	    IF(NOT ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
		MESSAGE(FATAL_ERROR "Local Tcl compilation enabled, but compilation of Tcl/Tk extension ${pkgname} has been disabled - unsupported configuration!")
	    ENDIF(NOT ${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	ENDIF(${CMAKE_PROJECT_NAME}_TCL_BUILD)
    ENDIF(NOT ${PKGNAME_UPPER}_DISABLED)

    # 7. If none of the above conditions preclude us from doing so, start
    # testing.

    IF(NOT ${PKGNAME_UPPER}_DISABLE_TEST)
	# Initialize test tracking variable
	SET(${PKGNAME_UPPER}_TEST_FAIL 0)
	SET(${PKGNAME_UPPER}_TCL_PASSED 0)
	SET(${PKGNAME_UPPER}_LIBS_FAIL 0)
	# Stash the previous results (if any) so we don't repeatedly 
	# call out the tests - only report if something actually 
	# changes in subsequent runs.
	SET(${PKGNAME_UPPER}_FOUND_STATUS ${${PKGNAME_UPPER}_FOUND})

	# 1.  If we have no wish command, we fail immediately.
	IF(${wishcmd} STREQUAL "")
	    MESSAGE(WARNING "Test for ${pkgname} failed - no tclsh/wish command available for testing!")
	    SET(${PKGNAME_UPPER}_TEST_FAIL 1)
	ENDIF(${wishcmd} STREQUAL "")

	# 2.  We have wish - now things get interesting.  Write out
	# a Tcl script to probe for information on the package we're
	# interested in, and collect the results.
	IF(NOT ${PKGNAME_UPPER}_TEST_FAIL)
	    SET(CURRENT_TCL_PACKAGE_NAME ${pkgname})
	    SET(pkg_test_file ${CMAKE_BINARY_DIR}/CMakeTmp/${pkgname}_version.tcl)
	    configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/tcltest.tcl.in ${pkg_test_file} @ONLY)
	    EXEC_PROGRAM(${wishcmd} ARGS ${pkg_test_file} OUTPUT_VARIABLE EXECOUTPUT)
	    FILE(READ ${CMAKE_BINARY_DIR}/CMakeTmp/${PKGNAME_UPPER}_PKG_VERSION pkgversion)
	    STRING(REGEX REPLACE "\n" "" ${PKGNAME_UPPER}_PACKAGE_VERSION ${pkgversion})
	    IF(${PKGNAME_UPPER}_PACKAGE_VERSION)
		SET(${PKGNAME_UPPER}_TCL_PASSED 1)
	    ELSE(${PKGNAME_UPPER}_PACKAGE_VERSION)
		SET(${PKGNAME_UPPER}_TCL_PASSED 0)
	    ENDIF(${PKGNAME_UPPER}_PACKAGE_VERSION)
	ENDIF(NOT ${PKGNAME_UPPER}_TEST_FAIL)

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

	IF(${PKGNAME_UPPER}_TCL_PASSED)
	    IF(NOT "${NEEDS_LIBS}" STREQUAL "")
		# We have libraries to find - let's get started.
		FOREACH(libname ${NEEDS_LIBS})
		    # get the uppercase version of the lib name
		    STRING(TOUPPER "${libname}" LIBNAME_UPPER)
		    # deduce the variable to hold the library results 
		    IF(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
			SET(LOOKFOR_LIBRARY "${PKGNAME_UPPER}_LIBRARY")
			SET(CORENAME ${pkgname})
		    ELSE(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
			SET(LOOKFOR_LIBRARY "${PKGNAME_UPPER}_${LIBNAME_UPPER}_LIBRARY")
			SET(CORENAME ${libname})
		    ENDIF(${LIBNAME_UPPER} STREQUAL ${PKGNAME_UPPER})
		    SET(${PKGNAME_UPPER}_REQUIRED_LIBS ${${PKGNAME_UPPER}_REQUIRED_LIBS} ${${LOOKFOR_LIBRARY}})
		    # Check if this variable has already been set, and if
		    # so put it to the full path test
		    IF(${LOOKFOR_LIBRARY})
			GET_FILENAME_COMPONENT(PATH_ABS ${${LOOKFOR_LIBRARY}} ABSOLUTE)
			IF(NOT "${${LOOKFOR_LIBRARY}}" STREQUAL "${PATH_ABS}")
			    SET(${LOOKFOR_LIBRARY} "NOTFOUND" CACHE STRING "RESET" FORCE)
			ENDIF(NOT "${${LOOKFOR_LIBRARY}}" STREQUAL "${PATH_ABS}")
		    ENDIF(${LOOKFOR_LIBRARY})
		    # If we don't already have a library value, go looking
		    IF(NOT ${LOOKFOR_LIBRARY} OR "${${LOOKFOR_LIBRARY}}" MATCHES "NOTFOUND")
			find_library(${LOOKFOR_LIBRARY} NAMES ${libname} ${libname}${${PKGNAME_UPPER}_PACKAGE_VERSION} PATH_SUFFIXES ${libname}${${PKGNAME_UPPER}_PACKAGE_VERSION} ${pkgname}${${PKGNAME_UPPER}_PACKAGE_VERSION})
		    ENDIF(NOT ${LOOKFOR_LIBRARY} OR "${${LOOKFOR_LIBRARY}}" MATCHES "NOTFOUND")
		    # If we didn't find anything, we've failed to satisfy
		    # the package requirements
		    IF(NOT ${LOOKFOR_LIBRARY})
			SET(${PKGNAME_UPPER}_LIBS_FAIL 1)
		    ENDIF(NOT ${LOOKFOR_LIBRARY})
		    MARK_AS_ADVANCED(${LOOKFOR_LIBRARY})
		ENDFOREACH(libname ${NEEDS_LIBS})
	    ENDIF(NOT "${NEEDS_LIBS}" STREQUAL "")
	ENDIF(${PKGNAME_UPPER}_TCL_PASSED)
    ENDIF(NOT ${PKGNAME_UPPER}_DISABLE_TEST)

    # After all that, we finally know what to do - do it.
    STRING(TOLOWER ${pkgname} PKGNAME_LOWER)

    # If testing was disabled, we only care what the _BUILD variable says.
    # Even if DISABLED, we still need the distcheck ignore, so do the following
    # for ALL situations.
    IF(${PKGNAME_UPPER}_DISABLE_TEST)
	IF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	    add_subdirectory(${dir})
	    FOREACH(dep ${depends})
		string(TOUPPER ${dep} DEP_UPPER)
		if(BRLCAD_BUILD_${DEP_UPPER})
		    add_dependencies(${pkgname} ${dep})
		endif(BRLCAD_BUILD_${DEP_UPPER})
	    ENDFOREACH(dep ${depends})
	    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${PKGNAME_LOWER}.dist)
	    DISTCHECK_IGNORE(${dir} ${PKGNAME_LOWER}_ignore_files)
	ELSE(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	    DISTCHECK_IGNORE_ITEM(${dir})
	ENDIF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
    ENDIF(${PKGNAME_UPPER}_DISABLE_TEST)

    # If testing was NOT disabled, but we're forced to system, see if we
    # need to complain about not finding what we need - we can't enable, so
    # complain.  This situation is not as dangerous as the potential subtle
    # issues with system pkg + local Tcl, so we won't FATAL_ERROR - just
    # warn and let things proceed.
    IF(NOT ${PKGNAME_UPPER}_DISABLE_TEST)
	IF(${PKGNAME_UPPER}_MET_CONDITION STREQUAL "4" OR ${PKGNAME_UPPER}_MET_CONDITION STREQUAL "5")
	    IF(${PKGNAME_UPPER}_TEST_FAIL OR ${PKGNAME_UPPER}_LIBS_FAIL OR NOT ${PKGNAME_UPPER}_TCL_PASSED)
		SET(BRLCAD_${PKGNAME_UPPER}_NOTFOUND 1)
		MESSAGE(WARNING "Local compilation of Tcl/Tk extension ${pkgname} was disabled, but system version not found!")	
	    ENDIF(${PKGNAME_UPPER}_TEST_FAIL OR ${PKGNAME_UPPER}_LIBS_FAIL OR NOT ${PKGNAME_UPPER}_TCL_PASSED)
	ENDIF(${PKGNAME_UPPER}_MET_CONDITION STREQUAL "4" OR ${PKGNAME_UPPER}_MET_CONDITION STREQUAL "5")
    ENDIF(NOT ${PKGNAME_UPPER}_DISABLE_TEST)

    # If testing was NOT disable, and none of the MET_CONDITIONs are satisfied,
    # we base our decision on the test results.
    IF(NOT ${PKGNAME_UPPER}_MET_CONDITION)
	IF(NOT ${PKGNAME_UPPER}_TEST_FAIL)
	    IF(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
		IF(${PKGNAME_UPPER}_TCL_PASSED)
		    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD OFF)
		ELSE(${PKGNAME_UPPER}_TCL_PASSED)
		    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
		ENDIF(${PKGNAME_UPPER}_TCL_PASSED)
	    ELSE(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
		SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	    ENDIF(NOT ${PKGNAME_UPPER}_LIBS_FAIL)
	ELSE(NOT ${PKGNAME_UPPER}_TEST_FAIL)
	    SET(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD ON)
	ENDIF(NOT ${PKGNAME_UPPER}_TEST_FAIL)
	IF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	    add_subdirectory(${dir})
	    FOREACH(dep ${depends})
		string(TOUPPER ${dep} DEP_UPPER)
		if(BRLCAD_BUILD_${DEP_UPPER})
		    add_dependencies(${pkgname} ${dep})
		endif(BRLCAD_BUILD_${DEP_UPPER})
	    ENDFOREACH(dep ${depends})
	    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/${PKGNAME_LOWER}.dist)
	    DISTCHECK_IGNORE(${dir} ${PKGNAME_LOWER}_ignore_files)
	ELSE(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
	    DISTCHECK_IGNORE_ITEM(${dir})
	ENDIF(${CMAKE_PROJECT_NAME}_${PKGNAME_UPPER}_BUILD)
    ENDIF(NOT ${PKGNAME_UPPER}_MET_CONDITION)

ENDMACRO(THIRD_PARTY_TCL_PACKAGE)

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
