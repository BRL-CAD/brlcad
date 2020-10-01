# Unfortunately, there does not appear to be a reliable way to test for the
# presence of the Tktable package on a system Tcl/Tk.  We key off of the presence
# of the TK_BLD and ITCL_BLD targets, but that may produce a false negative if
# those builds are both off but we still need Tktable.  As far as I can tell the
# "package require Tktable" test (which is what is required to properly test for an
# available Tktable package) can ONLY be performed successfully on a system that
# supports creation of a graphics window. Window creation isn't typically
# available on continuous integration runners, which means the test will always
# fail there even when it shouldn't.

# We try to find the Tktable library, since that's the only test we can do without
# needing the graphical invocation.  Unfortunately, even a find_library search
# looking for libTktable isn't completely reliable, since the presence of a shared
# library is not a guarantee it is correctly hooked into the "package require"
# mechanism of the system Tcl/Tk we want to use.  (It is possible to have more
# than one Tcl/Tk on a system - this situation is known to have occurred on the
# Mac when 3rd party package managers are used, for example.)

if (BRLCAD_ENABLE_TK)

  # Do what we can to make a sane decision on whether to build Tktable
  set(DO_TKTABLE_BUILD 0)
  if (TARGET TK_BLD OR "${BRLCAD_TKTABLE}" STREQUAL "BUNDLED")
    set(DO_TKTABLE_BUILD 1)
  else (TARGET TK_BLD OR "${BRLCAD_TKTABLE}" STREQUAL "BUNDLED")
    find_library(TKTABLE_SYS_LIBRARY NAMES Tktable tktable)
    if (NOT TKTABLE_SYS_LIBRARY)
      set(DO_TKTABLE_BUILD 1)
    endif (NOT TKTABLE_SYS_LIBRARY)
  endif (TARGET TK_BLD OR "${BRLCAD_TKTABLE}" STREQUAL "BUNDLED")

  if (DO_TKTABLE_BUILD)

    set(TKTABLE_VERSION_MAJOR 2)
    set(TKTABLE_VERSION_MINOR 10)
    set(TKTABLE_VERSION ${TKTABLE_VERSION_MAJOR}.${TKTABLE_VERSION_MINOR})

    if (MSVC)
      set(TKTABLE_BASENAME Tktable${TKTABLE_VERSION_MAJOR}.${TK_VERSION_MINOR})
      set(TKTABLE_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    else (MSVC)
      set(TKTABLE_BASENAME libTktable)
      set(TKTABLE_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${TKTABLE_VERSION_MAJOR}.${TKTABLE_VERSION_MINOR})
    endif (MSVC)

    if (TARGET TCL_BLD)
      set(TCL_TARGET TCL_BLD)
    endif (TARGET TCL_BLD)

    if (TARGET TK_BLD)
      set(TK_TARGET TK_BLD)
    endif (TARGET TK_BLD)

    ExternalProject_Add(TKTABLE_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/tktable"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
      -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DTCL_ROOT=$<$<BOOL:${TCL_TARGET}>:${CMAKE_BINARY_DIR}>
      DEPENDS ${TCL_TARGET} ${TK_TARGET}
      )
    ExternalProject_Target(tktable TKTABLE_BLD
      #SUBDIR Tktable${TKTABLE_VERSION}
      OUTPUT_FILE ${TKTABLE_BASENAME}${TKTABLE_SUFFIX}
      SYMLINKS "${TKTABLE_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      LINK_TARGET "${TKTABLE_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      )
    ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
      Tktable${TKTABLE_VERSION}/tktable.py
      Tktable${TKTABLE_VERSION}/tkTable.tcl
      )
    ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
      Tktable${TKTABLE_VERSION}/pkgIndex.tcl
      FIXPATH
      )

    list(APPEND BRLCAD_DEPS TKTABLE_BLD)

    SetTargetFolder(TKTABLE_BLD "Third Party Libraries")

  endif (DO_TKTABLE_BUILD)
endif (BRLCAD_ENABLE_TK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

