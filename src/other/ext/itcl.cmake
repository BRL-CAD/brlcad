# Because we're using Itcl 3, we have to check and see if it's present A system
# install of Tcl/Tk may or may not have the older version.
#
# This is the only Tcl package we use that we can safely/reliably test for -
# the others all depend on Tcl/Tk, and it is not possible to successfully load
# Tcl/Tk packages requiring Tk on a non-graphical system (such as a Continuous
# Integration runner.)  Tk itself cannot be loaded successfully without
# creating a graphical window, so there is no option for a "headless" start of
# Tk for testing purposes.  That limitation thus propagates to all packages
# that require Tk.
#
# This limitation is unfortunate in the case of Itk, Tkhtml and Tktable. The
# best we will be able to do will be to key off of the Itcl/Tk build settings,
# which will mean that we won't be able to detect cases where Itcl and Tk *are*
# installed but the others are not.

function(ITCL_TEST bvar)

  if (TARGET TCL_BLD)
    # If we are building Tcl, we know the answer - we need to build Itcl3 as well.
    set(${bvar} 1 PARENT_SCOPE)
    return()
  endif (TARGET TCL_BLD)

  # If we're looking at a system Tcl, check for the package.  If it is there,
  # we don't need to build.  If it's not there and we're set to "AUTO", build.
  # If it's not there and we're set to SYSTEM, error.  By the time we are here,
  # we should already have performed the find_package operation to locate Tcl
  # and the necessary variables should be set.
  if (NOT "${BRLCAD_ITCL}" STREQUAL "BUNDLED")
    if (NOT TCL_TCLSH)
      message(FATAL_ERROR "Need to test for Itcl3, but TCL_TCLSH is not set")
    endif (NOT TCL_TCLSH)
    file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/itcl_test.tcl" "package require Itcl 3")
    execute_process(
      COMMAND ${TCL_TCLSH} "${CMAKE_BINARY_DIR}/CMakeTmp/itcl_test.tcl"
      RESULT_VARIABLE ITCL_TEST_FAILED
      )
    file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/itcl_test.tcl")
    if (ITCL_TEST_FAILED)
      if ("${BRLCAD_ITCL}" STREQUAL "SYSTEM")
	# Test failed, but user has specified system - this is fatal.
	message(FATAL_ERROR "System-installed Itcl3 specified, but package is not available")
      else ("${BRLCAD_ITCL}" STREQUAL "SYSTEM")
	set(${bvar} 1 PARENT_SCOPE)
      endif ("${BRLCAD_ITCL}" STREQUAL "SYSTEM")
    else (ITCL_TEST_FAILED)
      # We have Itcl 3 - no need to build.
      set(${bvar} 0 PARENT_SCOPE)
    endif (ITCL_TEST_FAILED)

  endif (NOT "${BRLCAD_ITCL}" STREQUAL "BUNDLED")
endfunction(ITCL_TEST bvar)

if (BRLCAD_ENABLE_TCL)

  ITCL_TEST(BUILD_ITCL)

  if (BUILD_ITCL)

    # If we're building ITCL, it's path setup must take into account the
    # subdirectory in which we are storing the library.
    relative_rpath(RELPATH SUFFIX itcl3.4)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELPATH}")
    ext_build_rpath(SUFFIX itcl3.4)

    set(BRLCAD_ITCL_BUILD "ON" CACHE STRING "Enable Itcl build" FORCE)

    set(ITCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITCL_BLD-prefix/src/ITCL_BLD")

    set(ITCL_MAJOR_VERSION 3)
    set(ITCL_MINOR_VERSION 4)
    set(ITCL_VERSION ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

    set(ITCL_DEPS)
    if (TARGET tcl_stage)
      set(TCL_TARGET ON)
      list(APPEND ITCL_DEPS tcl_stage)
      list(APPEND ITCL_DEPS tclstub_stage)
      list(APPEND ITCL_DEPS tclsh_exe_stage)
    endif (TARGET tcl_stage)
    if (TARGET tk_stage)
      list(APPEND ITCL_DEPS tk_stage)
      list(APPEND ITCL_DEPS tkstub_stage)
    endif (TARGET tk_stage)

    set(ITCL_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/itcl3.4)

    ExternalProject_Add(ITCL_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/itcl3"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${ITCL_INSTDIR}
      $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
      -DBIN_DIR=${BIN_DIR}
      -DLIB_DIR=${LIB_DIR}
      -DSHARED_DIR=${SHARED_DIR}
      -DINCLUDE_DIR=${INCLUDE_DIR}
      -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DITCL_STATIC=${BUILD_STATIC_LIBS}
      -DTCL_ROOT=$<$<BOOL:${TCL_TARGET}>:${CMAKE_BINARY_ROOT}>
      -DTCL_VERSION=${TCL_VERSION}
      DEPENDS ${ITCL_DEPS}
      )

    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/ITCL_BLD-prefix")

    if (NOT MSVC)
      set(ITCL_BASENAME libitcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
      set(ITCL_STUBNAME libitclstub)
    else (NOT MSVC)
      set(ITCL_BASENAME itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
      set(ITCL_STUBNAME itclstub)
    endif (NOT MSVC)

    # Tell the parent build about files and libraries
    ExternalProject_Target(SHARED itcl ITCL_BLD ${ITCL_INSTDIR}
      itcl${ITCL_VERSION}/${ITCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
      SUBDIR  itcl${ITCL_VERSION}
      )

    ExternalProject_Target(STATIC itclstub ITCL_BLD ${ITCL_INSTDIR}
      itcl${ITCL_VERSION}/${ITCL_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      SUBDIR  itcl${ITCL_VERSION}
      )


    ExternalProject_ByProducts(itcl ITCL_BLD ${ITCL_INSTDIR} ${INCLUDE_DIR}
      itcl.h
      itclDecls.h
      itclInt.h
      itclIntDecls.h
      )

    ExternalProject_ByProducts(itcl ITCL_BLD ${ITCL_INSTDIR} ${LIB_DIR}/itcl${ITCL_VERSION}
      itcl.tcl
      pkgIndex.tcl
      )

    set(ITCL_LIBRARY itcl CACHE STRING "Building bundled itcl" FORCE)
    set(ITCL_LIBRARIES itcl CACHE STRING "Building bundled itcl" FORCE)

    if (TARGET tcl_stage)
      add_dependencies(itcl_stage tcl_stage)
    endif (TARGET tcl_stage)

    SetTargetFolder(ITCL_BLD "Third Party Libraries")

    # Restore default rpath settings
    relative_rpath(RELPATH)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELPATH}")
    ext_build_rpath()

  else (BUILD_ITCL)

    set(BRLCAD_ITCL_BUILD "OFF" CACHE STRING "Disable Itcl build" FORCE)

  endif (BUILD_ITCL)
endif (BRLCAD_ENABLE_TCL)

include("${CMAKE_CURRENT_SOURCE_DIR}/itcl3.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

