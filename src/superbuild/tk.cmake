
# By the time we get here, we have run FindTCL and should know
# if we have TK.
if (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND NOT TK_LIBRARY)

  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)

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
    configure_file(${BDEPS_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
    add_executable(tcl_replace ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
  endif (NOT TARGET tcl_replace)

  set(TK_INSTDIR ${CMAKE_BINARY_DIR}/tk$<CONFIG>)


  if (NOT MSVC)

    set(TK_BASENAME libtk${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TK_STUBNAME libtkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TTK_STUBNAME libttkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

    set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")

    ExternalProject_Add(TK_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TK_PATCH_FILES}
      CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR} ${TK_SRC_DIR}/unix/configure --prefix=${TK_INSTDIR} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR},${TCLCONF_DIR}> --disable-xft --enable-64bit --enable-rpath
      BUILD_COMMAND make -j${pcnt}
      INSTALL_COMMAND make install
      DEPENDS ${TCL_TARGET} rpath_replace
      )

  else (NOT MSVC)

    set(TK_BASENAME tk${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TK_STUBNAME tkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TTK_STUBNAME ttkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

    ExternalProject_Add(TK_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CONFIGURE_COMMAND ""
      BINARY_DIR ${TK_SRC_DIR}/win
      BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${TK_INSTDIR} TCLDIR=${TCL_SRC_DIR}
      INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${TK_INSTDIR} TCLDIR=${TCL_SRC_DIR}
      DEPENDS ${TCL_TARGET}
      )

  endif (NOT MSVC)

  # Tell the parent build about files and libraries
  ExternalProject_Target(tk TK_BLD ${TK_INSTDIR}
    SHARED ${LIB_DIR}/${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(tkstub TK_BLD ${TK_INSTDIR}
    STATIC ${LIB_DIR}/${TK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
  ExternalProject_Target(ttkstub TK_BLD ${TK_INSTDIR}
    STATIC ${LIB_DIR}/${TTK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
  ExternalProject_Target(wish_exe TK_BLD ${TK_INSTDIR}
    EXEC ${BIN_DIR}/wish${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR} ${LIB_DIR}
    tkConfig.sh
    FIXPATH
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}
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
    tkAppInit.c
    tkfbox.tcl
    unsupported.tcl
    xmfbox.tcl
    )

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/images ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/images
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

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/msgs ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/msgs
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

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/ttk ${LIB_DIR}/tk8.${TCL_MINOR_VERSION}/ttk
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

  ExternalProject_ByProducts(tk TK_BLD ${TK_INSTDIR} ${INCLUDE_DIR} ${INCLUDE_DIR}
    tkDecls.h
    tk.h
    tkPlatDecls.h
    )

  set(TK_LIBRARY tk CACHE STRING "Building bundled tk" FORCE)
  set(TK_LIBRARIES tk CACHE STRING "Building bundled tk" FORCE)
  set(TK_STUB_LIBRARY tkstub CACHE STRING "Building bundled tk" FORCE)
  set(TTK_STUB_LIBRARY ttkstub CACHE STRING "Building bundled tk" FORCE)
  set(TK_WISH wish_exe CACHE STRING "Building bundled tk" FORCE)
  set(TK_INCLUDE_PATH "${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)
  set(TK_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

  SetTargetFolder(TK_BLD "Third Party Libraries")
  SetTargetFolder(tk "Third Party Libraries")

endif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND NOT TK_LIBRARY)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

