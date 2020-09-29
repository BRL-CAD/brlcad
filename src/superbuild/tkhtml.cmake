# Unfortunately, there does not appear to be a reliable way to test for the
# presence of the Tkhtml package on a system Tcl/Tk.  We key off of the presence
# of the TK_BLD and ITCL_BLD targets, but that may produce a false negative if
# those builds are both off but we still need Tkhtml.  As far as I can tell the
# "package require Tkhtml" test (which is what is required to properly test for an
# available Tkhtml package) can ONLY be performed successfully on a system that
# supports creation of a graphics window. Window creation isn't typically
# available on continuous integration runners, which means the test will always
# fail there even when it shouldn't.

# We try to find the Tkhtml library, since that's the only test we can do without
# needing the graphical invocation.  Unfortunately, even a find_library search
# looking for libTkhtml isn't completely reliable, since the presence of a shared
# library is not a guarantee it is correctly hooked into the "package require"
# mechanism of the system Tcl/Tk we want to use.  (It is possible to have more
# than one Tcl/Tk on a system - this situation is known to have occurred on the
# Mac when 3rd party package managers are used, for example.)

if (BRLCAD_ENABLE_TK)

  # Do what we can to make a sane decision on whether to build Tkhtml
  set(DO_TKHTML_BUILD 0)
  if (TARGET TK_BLD OR "${BRLCAD_TKHTML}" STREQUAL "BUNDLED")
    set(DO_TKHTML_BUILD 1)
  else (TARGET TK_BLD OR "${BRLCAD_TKHTML}" STREQUAL "BUNDLED")
    find_library(TKHTML_SYS_LIBRARY NAMES Tkhtml tkhtml)
    if (NOT TKHTML_SYS_LIBRARY)
      set(DO_TKHTML_BUILD 1)
    endif (NOT TKHTML_SYS_LIBRARY)
  endif (TARGET TK_BLD OR "${BRLCAD_TKHTML}" STREQUAL "BUNDLED")

  if (DO_TKHTML_BUILD)

    set(TKHTML_VERSION_MAJOR 3)
    set(TKHTML_VERSION_MINOR 0)
    set(TKHTML_VERSION ${TKHTML_VERSION_MINOR}.${TKHTML_VERSION_MINOR})

    if (MSVC)
      set(TKHTML_BASENAME Tkhtml${TKHTML_VERSION_MAJOR}.${TK_VERSION_MINOR})
      set(TKHTML_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    else (MSVC)
      set(TKHTML_BASENAME libTkhtml)
      set(TKHTML_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${TKHTML_VERSION_MAJOR}.${TKHTML_VERSION_MINOR})
    endif (MSVC)

    if (TARGET TCL_BLD)
      set(TCL_TARGET TCL_BLD)
    endif (TARGET TCL_BLD)

    if (TARGET TK_BLD)
      set(TK_TARGET TK_BLD)
    endif (TARGET TK_BLD)

    ExternalProject_Add(TKHTML_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/tkhtml"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
      -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DTCL_ROOT=$<$<BOOL:${TCL_TARGET}>:${CMAKE_BINARY_DIR}>
      DEPENDS ${TCL_TARGET} ${TK_TARGET}
      )
    ExternalProject_Target(tkhtml TKHTML_BLD
      SUBDIR Tkhtml${TKHTML_VERSION}
      OUTPUT_FILE ${TKHTML_BASENAME}${TKHTML_SUFFIX}
      SYMLINKS "${TKHTML_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      LINK_TARGET "${TKHTML_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      )
    ExternalProject_ByProducts(TKHTML_BLD ${LIB_DIR}
      Tkhtml${TKHTML_VERSION}/pkgIndex.tcl
      FIXPATH
      )

    list(APPEND BRLCAD_DEPS TKHTML_BLD)

    SetTargetFolder(TKHTML_BLD "Third Party Libraries")

  endif (DO_TKHTML_BUILD)
endif (BRLCAD_ENABLE_TK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

