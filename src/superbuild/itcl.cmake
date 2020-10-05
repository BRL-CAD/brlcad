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
    set(ITCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITCL_BLD-prefix/src/ITCL_BLD")

    set(ITCL_MAJOR_VERSION 3)
    set(ITCL_MINOR_VERSION 4)
    set(ITCL_VERSION ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

    set(ITCL_PATCH_FILES "${ITCL_SRC_DIR}/configure" "${ITCL_SRC_DIR}/tclconfig/tcl.m4")

    if (TARGET TCL_BLD)
      set(TCL_TARGET TCL_BLD)
    else (TARGET TCL_BLD)
      get_filename_component(TCLCONF_DIR "${TCL_LIBRARY}" DIRECTORY)
    endif (TARGET TCL_BLD)

    if (NOT MSVC)

      set(ITCL_BASENAME libitcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
      set(ITCL_STUBNAME libitclstub${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

      ExternalProject_Add(ITCL_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/itcl3"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITCL_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_INSTALL_PREFIX}/${LIB_DIR} ${ITCL_SRC_DIR}/configure --prefix=${CMAKE_INSTALL_PREFIX} --exec-prefix=${CMAKE_INSTALL_PREFIX} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_INSTALL_PREFIX}/${LIB_DIR},${TCLCONF_DIR}>
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS ${TCL_TARGET}
	)

    else (NOT MSVC)

      set(ITCL_BASENAME itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
      set(ITCL_STUBNAME itclstub${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

      ExternalProject_Add(ITCL_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/itcl3"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${ITCL_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_INSTALL_PREFIX} TCLDIR=${TCL_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_INSTALL_PREFIX} TCLDIR=${TCL_SRC_DIR}
	DEPENDS ${TCL_TARGET}
	)

    endif (NOT MSVC)

    ExternalProject_Target(itcl ITCL_BLD
      SUBDIR itcl${ITCL_VERSION}
      OUTPUT_FILE ${ITCL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE ${ITCL_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )

    ExternalProject_ByProducts(ITCL_BLD ${INCLUDE_DIR}
      itcl.h
      itclDecls.h
      itclInt.h
      itclIntDecls.h
      )

    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itcl${ITCL_VERSION}/itcl.tcl
      )
    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itcl${ITCL_VERSION}/pkgIndex.tcl
      FIXPATH
      )

    list(APPEND BRLCAD_DEPS ITCL_BLD)

    SetTargetFolder(ITCL_BLD "Third Party Libraries")
  endif (BUILD_ITCL)

endif (BRLCAD_ENABLE_TCL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

