
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

  set(TK_MAJOR_VERSION 8)
  set(TK_MINOR_VERSION 6)
  set(TK_PATCH_VERSION 10)
  set(TK_DEPS)

  if (TARGET tcl_stage)
    # If we're building against a compiled Tcl and not a system Tcl,
    # set some vars accordingly
    set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
    set(TCL_MAJOR_VERSION 8)
    set(TCL_MINOR_VERSION 6)
    set(TCL_PATCH_VERSION 10)
    set(TCL_TARGET tcl_stage)
    list(APPEND TK_DEPS tcl_stage tclstub_stage tclsh_exe_stage)
  else (TARGET tcl_stage)
    get_filename_component(TCLCONF_DIR "${TCL_LIBRARY}" DIRECTORY)
  endif (TARGET tcl_stage)

  set(TK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")

  set(TK_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/tk)

  if (MSVC)
    set(TK_BASENAME tk)
    set(TK_STUBNAME tkstub)
    set(TK_WISHNAME wish${TK_MAJOR_VERSION}${TK_MINOR_VERSION})
    set(TK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_SYMLINK_1 ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_SYMLINK_2 ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})
    set(TK_APPINIT)
  elseif (APPLE)
    set(TK_BASENAME libtk)
    set(TK_STUBNAME libtkstub)
    set(TK_EXECNAME wish-${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})
    set(TK_SUFFIX .${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}.${TK_PATCH_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_SYMLINK_1 ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_SYMLINK_2 ${TK_BASENAME}.${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_APPINIT tkAppInit.c)
  else (MSVC)
    set(TK_BASENAME libtk)
    set(TK_STUBNAME libtkstub)
    set(TK_WISHNAME wish-${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})
    set(TK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${TK_MAJOR_VERSION}.${TK_MINOR_VERSION}.${TK_PATCH_VERSION})
    set(TK_SYMLINK_1 ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(TK_SYMLINK_2 ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${TK_MAJOR_VERSION}.${TK_MINOR_VERSION})
    set(TK_APPINIT tkAppInit.c)
  endif (MSVC)


  ExternalProject_Add(TK_BLD
    URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TK_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DTCL_ROOT=$<$<BOOL:${TCL_TARGET}>:${CMAKE_BINARY_ROOT}>
    -DTCL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tcl
    DEPENDS ${TK_DEPS}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED tk TK_BLD ${TK_INSTDIR}
    ${TK_BASENAME}${TK_SUFFIX}
    SYMLINKS ${TK_SYMLINK_1};${TK_SYMLINK_2}
    LINK_TARGET ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(STATIC tkstub TK_BLD ${TK_INSTDIR}
    ${TK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

  ExternalProject_Target(EXEC wish_exe TK_BLD ${TK_INSTDIR}
    ${TK_WISHNAME}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )

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
    ${TK_APPINIT}
    tkfbox.tcl
    unsupported.tcl
    xmfbox.tcl
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

