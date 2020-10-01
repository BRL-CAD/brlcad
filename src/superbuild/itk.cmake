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

    set(ITK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITK_BLD-prefix/src/ITK_BLD")

    set(ITK_MAJOR_VERSION 3)
    set(ITK_MINOR_VERSION 4)
    set(ITK_VERSION ${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

    set(ITK_PATCH_FILES "${ITK_SRC_DIR}/configure" "${ITK_SRC_DIR}/tclconfig/tcl.m4")

    # If we have build targets, set the variables accordingly.  Otherwise,
    # we need to find the *Config.sh script locations.
    if (TARGET TCL_BLD)
      set(TCL_TARGET TCL_BLD)
    else (TARGET TCL_BLD)
      get_filename_component(TCLCONF_DIR "${TCL_LIBRARY}" DIRECTORY)
    endif (TARGET TCL_BLD)

    if (TARGET ITCL_BLD)
      set(ITCL_TARGET ITCL_BLD)
    else (TARGET ITCL_BLD)
      find_library(ITCL_LIBRARY NAMES itcl itcl3)
      get_filename_component(ITCLCONF_DIR "${ITCL_LIBRARY}" DIRECTORY)
    endif (TARGET ITCL_BLD)

    if (TARGET TK_BLD)
      set(TK_TARGET TK_BLD)
    else (TARGET TK_BLD)
      get_filename_component(TKCONF_DIR "${TK_LIBRARY}" DIRECTORY)
    endif (TARGET TK_BLD)

    if (NOT MSVC)

      set(ITK_BASENAME libitk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})
      set(ITK_STUBNAME libitkstub${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

      ExternalProject_Add(ITK_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/../other/itk3"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITK_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${ITK_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --exec-prefix=${CMAKE_BINARY_DIR} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_BINARY_DIR}/${LIB_DIR},${TCLCONF_DIR}> --with-tk=$<IF:$<BOOL:${TK_TARGET}>,${CMAKE_BINARY_DIR}/${LIB_DIR},${TKCONF_DIR}> --with-itcl=$<IF:$<BOOL:${ITCL_TARGET}>,${CMAKE_BINARY_DIR}/${LIB_DIR},${ITCLCONF_DIR}>
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS ${TCL_TARGET} ${TK_TARGET} ${ITCL_TARGET}
	)

    else (NOT MSVC)

      set(ITK_BASENAME itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})
      set(ITK_STUBNAME itkstub${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

      ExternalProject_Add(ITK_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/itk3"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${ITK_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	DEPENDS ${TCL_TARGET}
	)

    endif (NOT MSVC)

    ExternalProject_Target(itk ITK_BLD
      SUBDIR itk${ITK_VERSION}
      OUTPUT_FILE ${ITK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE ${ITK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )

    ExternalProject_ByProducts(ITK_BLD ${INCLUDE_DIR}
      itk.h
      itkDecls.h
      )

    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itk${ITK_VERSION}/itk.tcl
      )
    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itk${ITK_VERSION}/pkgIndex.tcl
      FIXPATH
      )

    list(APPEND BRLCAD_DEPS ITK_BLD)

    SetTargetFolder(ITK_BLD "Third Party Libraries")

  endif (DO_ITK_BUILD)

endif (BRLCAD_ENABLE_TK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

