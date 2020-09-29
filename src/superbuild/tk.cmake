
# By the time we get here, we have run FindTCL and should know
# if we have TK.
if (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND NOT TK_LIBRARY)

  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)

  if (TARGET TCL_BLD)
    # If we're building against a compiled Tcl and not a system Tcl,
    # set some vars accordingly
    set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
    set(TCL_MAJOR_VERSION 8)
    set(TCL_MINOR_VERSION 6)
    set(TCL_TARGET TCL_BLD)
  else (TARGET TCL_BLD)
    get_filename_component(TCLCONF_DIR "${TCL_LIBRARY}" DIRECTORY)
  endif (TARGET TCL_BLD)

  set(TK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")

  # We need to set internal Tcl variables to the final install paths, not the intermediate install paths that
  # Tcl's own build will think are the final paths.  Rather than attempt build system trickery we simply
  # hard set the values in the source files by rewriting them.
  if (NOT TARGET tcl_replace)
    configure_file(${BRLCAD_CMAKE_DIR}/tcl_replace.cxx.in ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
    add_executable(tcl_replace ${CMAKE_CURRENT_BINARY_DIR}/tcl_replace.cxx)
  endif (NOT TARGET tcl_replace)

  set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")

  if (NOT MSVC)

    set(TK_BASENAME libtk${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TK_STUBNAME libtkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

    ExternalProject_Add(TK_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/../other/tk"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TK_PATCH_FILES}
      CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TK_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_BINARY_DIR}/${LIB_DIR},${TCLCONF_DIR}> --disable-xft --enable-64bit --enable-rpath
      BUILD_COMMAND make -j${pcnt}
      INSTALL_COMMAND make install
      DEPENDS ${TCL_TARGET}
      )

  else (NOT MSVC)

    set(TCL_BASENAME tk${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
    set(TCL_STUBNAME tkstub${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

    ExternalProject_Add(TK_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/tk"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CONFIGURE_COMMAND ""
      BINARY_DIR ${TK_SRC_DIR}/win
      BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
      INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
      DEPENDS ${TCL_TARGET}
      )

  endif (NOT MSVC)

  ExternalProject_Target(tk TK_BLD
    OUTPUT_FILE ${TK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${TK_STUBNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(wish TK_BLD
    OUTPUT_FILE wish${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH EXEC
    )

  ExternalProject_ByProducts(TK_BLD ${LIB_DIR} FIXPATH
    tkConfig.sh
    )

  ExternalProject_ByProducts(TK_BLD ${LIB_DIR}/tk8.${TK_MINOR_VERSION}
    bgerror.tcl
    button.tcl
    choosedir.tcl
    clrpick.tcl
    comdlg.tcl
    console.tcl
    demos/README
    demos/anilabel.tcl
    demos/aniwave.tcl
    demos/arrow.tcl
    demos/bind.tcl
    demos/bitmap.tcl
    demos/browse
    demos/button.tcl
    demos/check.tcl
    demos/clrpick.tcl
    demos/colors.tcl
    demos/combo.tcl
    demos/cscroll.tcl
    demos/ctext.tcl
    demos/dialog1.tcl
    demos/dialog2.tcl
    demos/en.msg
    demos/entry1.tcl
    demos/entry2.tcl
    demos/entry3.tcl
    demos/filebox.tcl
    demos/floor.tcl
    demos/fontchoose.tcl
    demos/form.tcl
    demos/goldberg.tcl
    demos/hello
    demos/hscale.tcl
    demos/icon.tcl
    demos/image1.tcl
    demos/image2.tcl
    demos/images/earth.gif
    demos/images/earthmenu.png
    demos/images/earthris.gif
    demos/images/flagdown.xbm
    demos/images/flagup.xbm
    demos/images/gray25.xbm
    demos/images/letters.xbm
    demos/images/noletter.xbm
    demos/images/ouster.png
    demos/images/pattern.xbm
    demos/images/tcllogo.gif
    demos/images/teapot.ppm
    demos/items.tcl
    demos/ixset
    demos/knightstour.tcl
    demos/label.tcl
    demos/labelframe.tcl
    demos/license.terms
    demos/mclist.tcl
    demos/menu.tcl
    demos/menubu.tcl
    demos/msgbox.tcl
    demos/nl.msg
    demos/paned1.tcl
    demos/paned2.tcl
    demos/pendulum.tcl
    demos/plot.tcl
    demos/puzzle.tcl
    demos/radio.tcl
    demos/rmt
    demos/rolodex
    demos/ruler.tcl
    demos/sayings.tcl
    demos/search.tcl
    demos/spin.tcl
    demos/states.tcl
    demos/style.tcl
    demos/tclIndex
    demos/tcolor
    demos/text.tcl
    demos/textpeer.tcl
    demos/timer
    demos/toolbar.tcl
    demos/tree.tcl
    demos/ttkbut.tcl
    demos/ttkmenu.tcl
    demos/ttknote.tcl
    demos/ttkpane.tcl
    demos/ttkprogress.tcl
    demos/ttkscale.tcl
    demos/twind.tcl
    demos/unicodeout.tcl
    demos/vscale.tcl
    demos/widget
    dialog.tcl
    entry.tcl
    focus.tcl
    fontchooser.tcl
    iconlist.tcl
    icons.tcl
    images/README
    images/logo.eps
    images/logo100.gif
    images/logo64.gif
    images/logoLarge.gif
    images/logoMed.gif
    images/pwrdLogo.eps
    images/pwrdLogo100.gif
    images/pwrdLogo150.gif
    images/pwrdLogo175.gif
    images/pwrdLogo200.gif
    images/pwrdLogo75.gif
    images/tai-ku.gif
    listbox.tcl
    megawidget.tcl
    menu.tcl
    mkpsenc.tcl
    msgbox.tcl
    msgs/cs.msg
    msgs/da.msg
    msgs/de.msg
    msgs/el.msg
    msgs/en.msg
    msgs/en_gb.msg
    msgs/eo.msg
    msgs/es.msg
    msgs/fr.msg
    msgs/hu.msg
    msgs/it.msg
    msgs/nl.msg
    msgs/pl.msg
    msgs/pt.msg
    msgs/ru.msg
    msgs/sv.msg
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
    ttk/altTheme.tcl
    ttk/aquaTheme.tcl
    ttk/button.tcl
    ttk/clamTheme.tcl
    ttk/classicTheme.tcl
    ttk/combobox.tcl
    ttk/cursors.tcl
    ttk/defaults.tcl
    ttk/entry.tcl
    ttk/fonts.tcl
    ttk/menubutton.tcl
    ttk/notebook.tcl
    ttk/panedwindow.tcl
    ttk/progress.tcl
    ttk/scale.tcl
    ttk/scrollbar.tcl
    ttk/sizegrip.tcl
    ttk/spinbox.tcl
    ttk/treeview.tcl
    ttk/ttk.tcl
    ttk/utils.tcl
    ttk/vistaTheme.tcl
    ttk/winTheme.tcl
    ttk/xpTheme.tcl
    unsupported.tcl
    xmfbox.tcl
    )
  ExternalProject_ByProducts(TK_BLD ${INCLUDE_DIR}
    tkDecls.h
    tk.h
    tkPlatDecls.h
    )

  list(APPEND BRLCAD_DEPS TK_BLD)

  set(TK_LIBRARIES tk CACHE STRING "Building bundled tk" FORCE)
  set(TK_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

  SetTargetFolder(TK_BLD "Third Party Libraries")
  SetTargetFolder(tk "Third Party Libraries")

endif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND NOT TK_LIBRARY)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

