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

    set(BRLCAD_IWIDGETS_BUILD "ON" CACHE STRING "Enable Iwidgets build" FORCE)

    set(IWIDGETS_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/IWIDGETS_BLD-prefix/src/IWIDGETS_BLD")

    set(IWIDGETS_MAJOR_VERSION 4)
    set(IWIDGETS_MINOR_VERSION 1)
    set(IWIDGETS_PATCH_VERSION 1)
    set(IWIDGETS_VERSION ${IWIDGETS_MAJOR_VERSION}.${IWIDGETS_MINOR_VERSION}.${IWIDGETS_PATCH_VERSION} CACHE STRING "IWidgets version")

    # If we have build targets, set the variables accordingly.  Otherwise,
    # we need to find the *Config.sh script locations.
    set(IWIDGETS_DEPS)
    if (TARGET tcl_stage)
      list(APPEND IWIDGETS_DEPS tcl_stage)
    endif (TARGET tcl_stage)
    if (TARGET itcl_stage)
      list(APPEND IWIDGETS_DEPS itcl_stage)
    endif (TARGET itcl_stage)
    if (TARGET tk_stage)
      list(APPEND IWIDGETS_DEPS tk_stage)
    endif (TARGET tk_stage)
    if (TARGET itk_stage)
      list(APPEND IWIDGETS_DEPS itk_stage)
    endif (TARGET itk_stage)

    # The Iwidgets build doesn't seem to work with Itk the same way it does with the other
    # dependencies - just point it to our local source copy
    set(ITK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/itk3")

    set(IWIDGETS_INSTDIR "${CMAKE_BINARY_INSTALL_ROOT}/iwidgets")

    ExternalProject_Add(IWIDGETS_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/iwidgets"
      BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
      CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${IWIDGETS_INSTDIR}
      -DLIB_DIR=${LIB_DIR}
      DEPENDS ${IWIDGETS_DEPS}
      )

    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/IWIDGETS_BLD-prefix")

    # Tell the parent build about files and libraries
    ExternalProject_ByProducts(iwidgets IWIDGETS_BLD ${IWIDGETS_INSTDIR} ${LIB_DIR}/Iwidgets${IWIDGETS_VERSION}
      iwidgets.tcl
      license.terms
      pkgIndex.tcl
      )

    ExternalProject_ByProducts(iwidgets IWIDGETS_BLD ${IWIDGETS_INSTDIR} ${LIB_DIR}/Iwidgets${IWIDGETS_VERSION}/scripts
      buttonbox.itk
      calendar.itk
      canvasprintbox.itk
      canvasprintdialog.itk
      checkbox.itk
      colors.itcl
      combobox.itk
      dateentry.itk
      datefield.itk
      dialog.itk
      dialogshell.itk
      disjointlistbox.itk
      entryfield.itk
      extbutton.itk
      extfileselectionbox.itk
      extfileselectiondialog.itk
      feedback.itk
      fileselectionbox.itk
      fileselectiondialog.itk
      finddialog.itk
      hierarchy.itk
      hyperhelp.itk
      labeledframe.itk
      labeledwidget.itk
      mainwindow.itk
      menubar.itk
      messagebox.itk
      messagedialog.itk
      notebook.itk
      optionmenu.itk
      pane.itk
      panedwindow.itk
      promptdialog.itk
      pushbutton.itk
      radiobox.itk
      regexpfield.itk
      roman.itcl
      scopedobject.itcl
      scrolledcanvas.itk
      scrolledframe.itk
      scrolledhtml.itk
      scrolledlistbox.itk
      scrolledtext.itk
      scrolledwidget.itk
      selectionbox.itk
      selectiondialog.itk
      shell.itk
      spindate.itk
      spinint.itk
      spinner.itk
      spintime.itk
      tabnotebook.itk
      tabset.itk
      tclIndex
      timeentry.itk
      timefield.itk
      toolbar.itk
      unknownimage.gif
      watch.itk
      )

    SetTargetFolder(IWIDGETS_BLD "Third Party Libraries")

  else (DO_IWIDGETS_BUILD)

    set(BRLCAD_IWIDGETS_BUILD "OFF" CACHE STRING "Disable Iwidgets build" FORCE)

  endif (DO_IWIDGETS_BUILD)

endif (BRLCAD_ENABLE_TK)

include("${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

