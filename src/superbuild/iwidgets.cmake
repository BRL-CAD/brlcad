# Unfortunately, there does not appear to be a reliable way to test for the
# presence of the IWidgets package on a system Tcl/Tk.  As far as I can tell
# the "package require Iwidgets" test (which is what is required to properly
# test for an available Iwidgets package) can ONLY be performed successfully on
# a system that supports creation of a graphics window. Window creation isn't
# typically available on continuous integration runners, which means the test
# will always fail there even when it shouldn't.

# Unless we have been specifically instructed to use a system version, provide
# the bundled version.

if (BRLCAD_ENABLE_TK)

  # Do what we can to make a sane decision on whether to build Itk
  set(DO_IWIDGETS_BUILD 1)
  if ("${BRLCAD_IWIDGETS}" STREQUAL "SYSTEM")
    set(DO_IWIDGETS_BUILD 0)
  endif ("${BRLCAD_IWIDGETS}" STREQUAL "SYSTEM")

  if (DO_IWIDGETS_BUILD)

    set(IWIDGETS_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/IWIDGETS_BLD-prefix/src/IWIDGETS_BLD")

    set(IWIDGETS_MAJOR_VERSION 4)
    set(IWIDGETS_MINOR_VERSION 1)
    set(IWIDGETS_PATCH_VERSION 1)
    set(IWIDGETS_VERSION ${IWIDGETS_MAJOR_VERSION}.${IWIDGETS_MINOR_VERSION}.${IWIDGETS_PATCH_VERSION})

    set(IWIDGETS_PATCH_FILES "${IWIDGETS_SRC_DIR}/configure" "${IWIDGETS_SRC_DIR}/tclconfig/tcl.m4")

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

    if (TARGET ITK_BLD)
      set(ITK_TARGET ITK_BLD)
    endif (TARGET ITK_BLD)
    # The Iwidgets build doesn't seem to work with Itk the same way it does with the other
    # dependencies - just point it to our local source copy
    set(ITK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/itk3")

    if (NOT MSVC)

      ExternalProject_Add(IWIDGETS_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/iwidgets"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${IWIDGETS_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_INSTALL_PREFIX}/${LIB_DIR} ${IWIDGETS_SRC_DIR}/configure --prefix=${CMAKE_INSTALL_PREFIX} --exec-prefix=${CMAKE_INSTALL_PREFIX} --with-tcl=$<IF:$<BOOL:${TCL_TARGET}>,${CMAKE_INSTALL_PREFIX}/${LIB_DIR},${TCLCONF_DIR}> --with-tk=$<IF:$<BOOL:${TK_TARGET}>,${CMAKE_INSTALL_PREFIX}/${LIB_DIR},${TKCONF_DIR}> --with-itcl=$<IF:$<BOOL:${ITCL_TARGET}>,${CMAKE_INSTALL_PREFIX}/${LIB_DIR},${ITCLCONF_DIR}> --with-itk=${ITK_SOURCE_DIR}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS ${TCL_TARGET} ${TK_TARGET} ${ITCL_TARGET} ${ITK_TARGET}
	)

    else (NOT MSVC)

      ExternalProject_Add(IWIDGETS_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/iwidgets"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${IWIDGETS_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_INSTALL_PREFIX} TCLDIR=${TCL_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_INSTALL_PREFIX} TCLDIR=${TCL_SRC_DIR}
	DEPENDS ${TCL_TARGET}
	)

    endif (NOT MSVC)

    ExternalProject_ByProducts(IWIDGETS_BLD ${LIB_DIR}/iwidgets${IWIDGETS_VERSION}
      demos/buttonbox
      demos/calendar
      demos/canvasprintbox
      demos/canvasprintdialog
      demos/catalog
      demos/checkbox
      demos/combobox
      demos/dateentry
      demos/datefield
      demos/demo.html
      demos/dialog
      demos/dialogshell
      demos/disjointlistbox
      demos/entryfield
      demos/extbutton
      demos/extfileselectionbox
      demos/extfileselectiondialog
      demos/feedback
      demos/fileselectionbox
      demos/fileselectiondialog
      demos/finddialog
      demos/hierarchy
      demos/html/buttonbox.n.html
      demos/html/calendar.n.html
      demos/html/canvasprintbox.n.html
      demos/html/canvasprintdialog.n.html
      demos/html/checkbox.n.html
      demos/html/combobox.n.html
      demos/html/dateentry.n.html
      demos/html/datefield.n.html
      demos/html/dialog.n.html
      demos/html/dialogshell.n.html
      demos/html/disjointlistbox.n.html
      demos/html/entryfield.n.html
      demos/html/extbutton.n.html
      demos/html/extfileselectionbox.n.html
      demos/html/extfileselectiondialog.n.html
      demos/html/feedback.n.html
      demos/html/fileselectionbox.n.html
      demos/html/fileselectiondialog.n.html
      demos/html/finddialog.n.html
      demos/html/hierarchy.n.html
      demos/html/hyperhelp.n.html
      demos/html/iwidgets4.0.0UserCmds.html
      demos/html/labeledframe.n.html
      demos/html/labeledwidget.n.html
      demos/html/mainwindow.n.html
      demos/html/menubar.n.html
      demos/html/messagebox.n.html
      demos/html/messagedialog.n.html
      demos/html/notebook.n.html
      demos/html/optionmenu.n.html
      demos/html/panedwindow.n.html
      demos/html/promptdialog.n.html
      demos/html/pushbutton.n.html
      demos/html/radiobox.n.html
      demos/html/scopedobject.n.html
      demos/html/scrolledcanvas.n.html
      demos/html/scrolledframe.n.html
      demos/html/scrolledhtml.n.html
      demos/html/scrolledlistbox.n.html
      demos/html/scrolledtext.n.html
      demos/html/selectionbox.n.html
      demos/html/selectiondialog.n.html
      demos/html/shell.n.html
      demos/html/spindate.n.html
      demos/html/spinint.n.html
      demos/html/spinner.n.html
      demos/html/spintime.n.html
      demos/html/tabnotebook.n.html
      demos/html/tabset.n.html
      demos/html/timeentry.n.html
      demos/html/timefield.n.html
      demos/html/toolbar.n.html
      demos/html/watch.n.html
      demos/hyperhelp
      demos/images/box.xbm
      demos/images/clear.gif
      demos/images/close.gif
      demos/images/copy.gif
      demos/images/cut.gif
      demos/images/exit.gif
      demos/images/find.gif
      demos/images/help.gif
      demos/images/line.xbm
      demos/images/mag.gif
      demos/images/new.gif
      demos/images/open.gif
      demos/images/oval.xbm
      demos/images/paste.gif
      demos/images/points.xbm
      demos/images/poly.gif
      demos/images/print.gif
      demos/images/ruler.gif
      demos/images/save.gif
      demos/images/select.gif
      demos/images/text.xbm
      demos/iwidgets.gif
      demos/labeledframe
      demos/labeledwidget
      demos/mainwindow
      demos/menubar
      demos/messagebox
      demos/messagedialog
      demos/notebook
      demos/optionmenu
      demos/panedwindow
      demos/promptdialog
      demos/pushbutton
      demos/radiobox
      demos/scopedobject
      demos/scrolledcanvas
      demos/scrolledframe
      demos/scrolledhtml
      demos/scrolledlistbox
      demos/scrolledtext
      demos/selectionbox
      demos/selectiondialog
      demos/shell
      demos/spindate
      demos/spinint
      demos/spinner
      demos/spintime
      demos/tabnotebook
      demos/tabset
      demos/timeentry
      demos/timefield
      demos/toolbar
      demos/watch
      iwidgets.tcl
      license.terms
      pkgIndex.tcl
      scripts/buttonbox.itk
      scripts/calendar.itk
      scripts/canvasprintbox.itk
      scripts/canvasprintdialog.itk
      scripts/checkbox.itk
      scripts/colors.itcl
      scripts/combobox.itk
      scripts/dateentry.itk
      scripts/datefield.itk
      scripts/dialog.itk
      scripts/dialogshell.itk
      scripts/disjointlistbox.itk
      scripts/entryfield.itk
      scripts/extbutton.itk
      scripts/extfileselectionbox.itk
      scripts/extfileselectiondialog.itk
      scripts/feedback.itk
      scripts/fileselectionbox.itk
      scripts/fileselectiondialog.itk
      scripts/finddialog.itk
      scripts/hierarchy.itk
      scripts/hyperhelp.itk
      scripts/labeledframe.itk
      scripts/labeledwidget.itk
      scripts/mainwindow.itk
      scripts/menubar.itk
      scripts/messagebox.itk
      scripts/messagedialog.itk
      scripts/notebook.itk
      scripts/optionmenu.itk
      scripts/pane.itk
      scripts/panedwindow.itk
      scripts/promptdialog.itk
      scripts/pushbutton.itk
      scripts/radiobox.itk
      scripts/regexpfield.itk
      scripts/roman.itcl
      scripts/scopedobject.itcl
      scripts/scrolledcanvas.itk
      scripts/scrolledframe.itk
      scripts/scrolledhtml.itk
      scripts/scrolledlistbox.itk
      scripts/scrolledtext.itk
      scripts/scrolledwidget.itk
      scripts/selectionbox.itk
      scripts/selectiondialog.itk
      scripts/shell.itk
      scripts/spindate.itk
      scripts/spinint.itk
      scripts/spinner.itk
      scripts/spintime.itk
      scripts/tabnotebook.itk
      scripts/tabset.itk
      scripts/tclIndex
      scripts/timeentry.itk
      scripts/timefield.itk
      scripts/toolbar.itk
      scripts/unknownimage.gif
      scripts/watch.itk
      )

    list(APPEND BRLCAD_DEPS IWIDGETS_BLD)

    SetTargetFolder(IWIDGETS_BLD "Third Party Libraries")

  endif (DO_IWIDGETS_BUILD)

endif (BRLCAD_ENABLE_TK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

