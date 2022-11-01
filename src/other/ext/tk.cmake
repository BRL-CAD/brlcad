
# By the time we get here, we have run FindTCL and should know
# if we have TK.

if (NOT TK_LIBRARY OR TARGET tcl_stage)
  set(TK_DO_BUILD 1)
else (NOT TK_LIBRARY OR TARGET tcl_stage)
  set(TK_DO_BUILD 0)
endif (NOT TK_LIBRARY OR TARGET tcl_stage)

if (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND TK_DO_BUILD)

  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
  set(BRLCAD_TK_BUILD ON CACHE STRING "Enabling Tk build" FORCE)

  if (TARGET tcl_stage)
    # If we're building against a compiled Tcl and not a system Tcl,
    # set some vars accordingly
    set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
    set(TCL_MAJOR_VERSION 8)
    set(TCL_MINOR_VERSION 6)
    set(TCL_TARGET tcl_stage)
  else (TARGET tcl_stage)
    get_filename_component(TCLCONF_DIR "${TCL_LIBRARY}" DIRECTORY)
  endif (TARGET tcl_stage)

  set(TK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")

  # We need to set internal Tcl variables to the final install paths, not the intermediate install paths that
  # Tcl's own build will think are the final paths.  Rather than attempt build system trickery we simply
  # hard set the values in the source files by rewriting them.
  if (NOT TARGET tcl_replace)
    configure_file(${BDEPS_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx @ONLY)
    add_executable(tcl_replace ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
    set_target_properties(tcl_replace PROPERTIES FOLDER "Compilation Utilities")
  endif (NOT TARGET tcl_replace)

  set(TK_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/tk)

  if (NOT MSVC)

    # Check for spaces in the source and build directories - those won't work
    # reliably with the Tk autotools based build.
    if ("${CMAKE_CURRENT_SOURCE_DIR}" MATCHES ".* .*")
      message(FATAL_ERROR "Bundled Tk enabled, but the path \"${CMAKE_CURRENT_SOURCE_DIR}\" contains spaces.  On this platform, Tk uses autotools to build; paths with spaces are not supported.  To continue relocate your source directory to a path that does not use spaces.")
    endif ("${CMAKE_CURRENT_SOURCE_DIR}" MATCHES ".* .*")
    if ("${CMAKE_CURRENT_BINARY_DIR}" MATCHES ".* .*")
      message(FATAL_ERROR "Bundled Tk enabled, but the path \"${CMAKE_CURRENT_BINARY_DIR}\" contains spaces.  On this platform, Tk uses autotools to build; paths with spaces are not supported.  To continue you must select a build directory with a path that does not use spaces.")
    endif ("${CMAKE_CURRENT_BINARY_DIR}" MATCHES ".* .*")

    if (OPENBSD)
      set(TK_BASENAME libtk${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})
      set(TK_STUBNAME libtkstub${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})
      set(TK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0)
    else (OPENBSD)
      set(TK_BASENAME libtk${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
      set(TK_STUBNAME libtkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
      set(TK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (OPENBSD)

    set(TK_WISHNAME wish${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

    set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")

    ExternalProject_Add(TK_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
      BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
      PATCH_COMMAND rpath_replace ${TK_PATCH_FILES}
      CONFIGURE_COMMAND LD_LIBRARY_PATH=${CMAKE_BINARY_ROOT}/${LIB_DIR} CPPFLAGS=-I${CMAKE_BINARY_ROOT}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_ROOT}/${LIB_DIR} TK_SHLIB_LD_EXTRAS=-L${CMAKE_BINARY_ROOT}/${LIB_DIR} ${TK_SRC_DIR}/unix/configure --prefix=${TK_INSTDIR} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_BINARY_ROOT}/${LIB_DIR},${TCLCONF_DIR}> --disable-xft --enable-64bit --enable-rpath
      BUILD_COMMAND make -j${pcnt}
      INSTALL_COMMAND make install
      DEPENDS ${TCL_TARGET} rpath_replace
      # Note - LOG_CONFIGURE doesn't seem to be compatible with complex CONFIGURE_COMMAND setups
      LOG_BUILD ${EXT_BUILD_QUIET}
      LOG_INSTALL ${EXT_BUILD_QUIET}
      LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
      )

    set(TK_APPINIT tkAppInit.c)

  else (NOT MSVC)

    set(TK_BASENAME tk${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})
    set(TK_STUBNAME tkstub${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})
    set(TK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_WISHNAME wish${TCL_MAJOR_VERSION}${TCL_MINOR_VERSION})

    ExternalProject_Add(TK_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
      BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
      CONFIGURE_COMMAND ""
      BINARY_DIR ${TK_SRC_DIR}/win
      BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${TK_INSTDIR} TCLDIR=${TCL_SRC_DIR} SUFX=
      INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${TK_INSTDIR} TCLDIR=${TCL_SRC_DIR} SUFX=
      DEPENDS ${TCL_TARGET}
      LOG_BUILD ${EXT_BUILD_QUIET}
      LOG_INSTALL ${EXT_BUILD_QUIET}
      LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
      )

    set(TK_APPINIT)

  endif (NOT MSVC)

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED tk TK_BLD ${TK_INSTDIR}
    ${TK_BASENAME}${TK_SUFFIX}
    RPATH
    )
  ExternalProject_Target(STATIC tkstub TK_BLD ${TK_INSTDIR}
    ${TK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

  ExternalProject_Target(EXEC wish_exe TK_BLD ${TK_INSTDIR}
    ${TK_WISHNAME}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )

  if (NOT MSVC)
    ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}
      tkConfig.sh
      FIXPATH
      )
  endif (NOT MSVC)

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}
    bgerror.tcl
    button.tcl
    choosedir.tcl
    clrpick.tcl
    comdlg.tcl
    console.tcl
    dialog.tcl
    entry.tcl
    focus.tcl
    fontchooser.tcl
    iconlist.tcl
    icons.tcl
    listbox.tcl
    megawidget.tcl
    menu.tcl
    mkpsenc.tcl
    msgbox.tcl
    obsolete.tcl
    optMenu.tcl
    palette.tcl
    panedwindow.tcl
    pkgIndex.tcl
    safetk.tcl
    scale.tcl
    scrlbar.tcl
    spinbox.tcl
    tclIndex
    tearoff.tcl
    text.tcl
    tk.tcl
    tkfbox.tcl
    unsupported.tcl
    xmfbox.tcl
    ${TK_APPINIT}
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/images
    README
    logo.eps
    logo100.gif
    logo64.gif
    logoLarge.gif
    logoMed.gif
    pwrdLogo.eps
    pwrdLogo100.gif
    pwrdLogo150.gif
    pwrdLogo175.gif
    pwrdLogo200.gif
    pwrdLogo75.gif
    tai-ku.gif
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/msgs
    cs.msg
    da.msg
    de.msg
    el.msg
    en.msg
    en_gb.msg
    eo.msg
    es.msg
    fr.msg
    hu.msg
    it.msg
    nl.msg
    pl.msg
    pt.msg
    ru.msg
    sv.msg
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/ttk
    altTheme.tcl
    aquaTheme.tcl
    button.tcl
    clamTheme.tcl
    classicTheme.tcl
    combobox.tcl
    cursors.tcl
    defaults.tcl
    entry.tcl
    fonts.tcl
    menubutton.tcl
    notebook.tcl
    panedwindow.tcl
    progress.tcl
    scale.tcl
    scrollbar.tcl
    sizegrip.tcl
    spinbox.tcl
    treeview.tcl
    ttk.tcl
    utils.tcl
    vistaTheme.tcl
    winTheme.tcl
    xpTheme.tcl
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${INCLUDE_DIR}
    tkDecls.h
    tk.h
    tkPlatDecls.h
    )

  if (MSVC)
    ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${INCLUDE_DIR}
      tkIntXlibDecls.h
      X11/ap_keysym.h
      X11/cursorfont.h
      X11/DECkeysym.h
      X11/HPkeysym.h
      X11/keysymdef.h
      X11/keysym.h
      X11/Sunkeysym.h
      X11/Xatom.h
      X11/XF86keysym.h
      X11/Xfuncproto.h
      X11/X.h
      X11/Xlib.h
      X11/Xutil.h
      )
  endif (MSVC)

  # If something uses the stub, we're going to want the headers, etc. in place
  add_dependencies(tkstub tk_stage)

  if (TARGET tcl_stage)
    add_dependencies(tk_stage tcl_stage)
  endif (TARGET tcl_stage)

  set(TK_LIBRARY tk CACHE STRING "Building bundled tk" FORCE)
  set(TK_LIBRARIES tk CACHE STRING "Building bundled tk" FORCE)
  set(TK_STUB_LIBRARY tkstub CACHE STRING "Building bundled tk" FORCE)
  #set(TTK_STUB_LIBRARY ttkstub CACHE STRING "Building bundled tk" FORCE)
  set(TK_WISH wish_exe CACHE STRING "Building bundled tk" FORCE)
  set(TK_INCLUDE_PATH "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)
  set(TK_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

  SetTargetFolder(TK_BLD "Third Party Libraries")
  SetTargetFolder(tk "Third Party Libraries")

elseif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK)

  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
  set(BRLCAD_TK_BUILD OFF CACHE STRING "Disabling Tk build" FORCE)

elseif (NOT BRLCAD_ENABLE_TK)

  set(BRLCAD_TK_BUILD "Disabled" CACHE STRING "Disabling Tk build" FORCE)

endif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND TK_DO_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/tk.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

