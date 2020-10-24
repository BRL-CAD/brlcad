# Unfortunately, there does not appear to be a reliable way to test for the
# presence of the Itk package on a system Tcl/Tk.  We key off of the presence
# of the TK_BLD and ITCL_BLD targets, but that may produce a false negative if
# those builds are both off but we still need Itk.  As far as I can tell the
# "package require Itk" test (which is what is required to properly test for an
# available Itk package) can ONLY be performed successfully on a system that
# supports creation of a graphics window. Window creation isn't typically
# available on continuous integration runners, which means the test will always
# fail there even when it shouldn't.

# We try to find the itk library, since that's the only test we can do without
# needing the graphical invocation.  Unfortunately, even a find_library search
# looking for libitk isn't completely reliable, since the presence of a shared
# library is not a guarantee it is correctly hooked into the "package require"
# mechanism of the system Tcl/Tk we want to use.  (It is possible to have more
# than one Tcl/Tk on a system - this situation is known to have occurred on the
# Mac when 3rd party package managers are used, for example.)

# Hopefully situations where a user has a complex Itcl/Itk setup are rare
# enough that it won't be a significant issue, since there appears to be
# only so much we can do to sort it out...

if (BRLCAD_ENABLE_TK)

  # Do what we can to make a sane decision on whether to build Itk
  set(DO_ITK_BUILD 0)
  if (TARGET TK_BLD OR TARGET ITCL_BLD OR "${BRLCAD_ITK}" STREQUAL "BUNDLED")
    set(DO_ITK_BUILD 1)
  else (TARGET TK_BLD OR TARGET ITCL_BLD OR "${BRLCAD_ITK}" STREQUAL "BUNDLED")
    find_library(ITK_SYS_LIBRARY NAMES itk3)
    if (NOT ITK_SYS_LIBRARY)
      set(DO_ITK_BUILD 1)
    endif (NOT ITK_SYS_LIBRARY)
  endif (TARGET TK_BLD OR TARGET ITCL_BLD OR "${BRLCAD_ITK}" STREQUAL "BUNDLED")

  if (DO_ITK_BUILD)

    set(BRLCAD_ITK_BUILD "ON" CACHE STRING "Enable Itk build" FORCE)

    set(ITK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITK_BLD-prefix/src/ITK_BLD")

    set(ITK_MAJOR_VERSION 3)
    set(ITK_MINOR_VERSION 4)
    set(ITK_VERSION ${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

    set(ITK_DEPS)
    if (TARGET tcl_stage)
      set(TCL_TARGET ON)
      list(APPEND ITK_DEPS tcl_stage)
      list(APPEND ITK_DEPS tclstub_stage)
      list(APPEND ITK_DEPS tclsh_exe_stage)
    endif (TARGET tcl_stage)

    if (TARGET itcl_stage)
      set(ITCL_TARGET ON)
      list(APPEND ITK_DEPS itcl_stage)
      list(APPEND ITK_DEPS itclstub_stage)
    endif (TARGET itcl_stage)

    if (TARGET tk_stage)
      list(APPEND ITK_DEPS tk_stage)
      list(APPEND ITK_DEPS tkstub_stage)
      list(APPEND ITK_DEPS wish_exe_stage)
    endif (TARGET tk_stage)

    set(ITK_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/itk3)

    ExternalProject_Add(ITK_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/itk3"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${ITK_INSTDIR}
      -DBIN_DIR=${BIN_DIR}
      -DLIB_DIR=${LIB_DIR}
      -DSHARED_DIR=${SHARED_DIR}
      -DINCLUDE_DIR=${INCLUDE_DIR}
      -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DTCL_ROOT=$<$<BOOL:${TCL_TARGET}>:${CMAKE_BINARY_ROOT}>
      -DITCL_ROOT=$<$<BOOL:${ITCL_TARGET}>:${CMAKE_BINARY_ROOT}>
      -DTCL_VERSION=${TCL_VERSION}
      DEPENDS ${ITK_DEPS}
      )

    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/ITK_BLD-prefix")

    if (NOT MSVC)
      set(ITK_BASENAME libitk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})
    else (NOT MSVC)
      set(ITK_BASENAME itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})
    endif (NOT MSVC)

    # Tell the parent build about files and libraries
    ExternalProject_Target(SHARED itk ITK_BLD ${ITK_INSTDIR}
      itk${ITK_VERSION}/${ITK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
      SUBDIR itk${ITK_VERSION}
      )

    ExternalProject_ByProducts(itk ITK_BLD ${ITK_INSTDIR} ${INCLUDE_DIR}
      itk.h
      itkDecls.h
      )

    ExternalProject_ByProducts(itk ITK_BLD ${ITK_INSTDIR} ${LIB_DIR}/itk${ITK_VERSION}
      Archetype.itk
      Toplevel.itk
      Widget.itk
      itk.tcl
      tclIndex
      )

    ExternalProject_ByProducts(itk ITK_BLD ${ITK_INSTDIR} ${LIB_DIR}/itk${ITK_VERSION}
      pkgIndex.tcl
      FIXPATH
      )

    set(ITK_LIBRARY itk CACHE STRING "Building bundled itcl" FORCE)
    set(ITK_LIBRARIES itk CACHE STRING "Building bundled itcl" FORCE)

    if (TARGET itcl_stage)
      add_dependencies(itk_stage itcl_stage)
    endif (TARGET itcl_stage)

    if (TARGET tk_stage)
      add_dependencies(itk_stage tk_stage)
    endif (TARGET tk_stage)

    SetTargetFolder(ITK_BLD "Third Party Libraries")

  else (DO_ITK_BUILD)

    set(BRLCAD_ITK_BUILD "OFF" CACHE STRING "Disable Itk build" FORCE)

  endif (DO_ITK_BUILD)

endif (BRLCAD_ENABLE_TK)

include("${CMAKE_CURRENT_SOURCE_DIR}/itk3.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

