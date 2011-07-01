SET(IWIDGETS_PKGNAME Iwidgets)
SET(IWIDGETS_PKGVERSION "4.0.1")
SET(VERSION ${IWIDGETS_PKGVERSION})
SET(IWIDGETS_VERSION ${IWIDGETS_PKGVERSION})

SET(IWIDGETS_SCRIPTS
		  generic/buttonbox.itk
		  generic/calendar.itk
		  generic/canvasprintbox.itk
		  generic/canvasprintdialog.itk
		  generic/checkbox.itk
		  generic/colors.itcl
		  generic/combobox.itk
		  generic/dateentry.itk
		  generic/datefield.itk
		  generic/dialog.itk
		  generic/dialogshell.itk
		  generic/disjointlistbox.itk
		  generic/entryfield.itk
		  generic/extbutton.itk
		  generic/extfileselectionbox.itk
		  generic/extfileselectiondialog.itk
		  generic/feedback.itk
		  generic/fileselectionbox.itk
		  generic/fileselectiondialog.itk
		  generic/finddialog.itk
		  generic/hierarchy.itk
		  generic/hyperhelp.itk
		  generic/labeledframe.itk
		  generic/labeledwidget.itk
		  generic/mainwindow.itk
		  generic/menubar.itk
		  generic/messagebox.itk
		  generic/messagedialog.itk
		  generic/notebook.itk
		  generic/optionmenu.itk
		  generic/panedwindow.itk
		  generic/pane.itk
		  generic/promptdialog.itk
		  generic/pushbutton.itk
		  generic/radiobox.itk
		  generic/regexpfield.itk
		  generic/roman.itcl
		  generic/scopedobject.itcl
		  generic/scrolledcanvas.itk
		  generic/scrolledframe.itk
		  generic/scrolledhtml.itk
		  generic/scrolledlistbox.itk
		  generic/scrolledtext.itk
		  generic/scrolledwidget.itk
		  generic/selectionbox.itk
		  generic/selectiondialog.itk
		  generic/shell.itk
		  generic/spindate.itk
		  generic/spinint.itk
		  generic/spinner.itk
		  generic/spintime.itk
		  generic/tabnotebook.itk
		  generic/tabset.itk
		  generic/tclIndex
		  generic/timeentry.itk
		  generic/timefield.itk
		  generic/toolbar.itk
		  generic/unknownimage.gif
		  generic/watch.itk
)

FOREACH(script ${IWIDGETS_SCRIPTS})
	install(FILES ${script} DESTINATION lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/scripts)
ENDFOREACH(script ${IWIDGETS_SCRIPTS})
CMAKEFILES(${IWIDGETS_SCRIPTS})

FOREACH(file ${IWIDGETS_SCRIPTS})
		  get_filename_component(filename ${file} NAME)
		  configure_file(${file}
					 ${CMAKE_BINARY_DIR}/lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/scripts/${filename} COPYONLY)
ENDFOREACH(file ${ITK_LIBRARY_FILES})

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${IWIDGETS_PKGNAME} ${IWIDGETS_PKGVERSION} [list source [file join $dir iwidgets.tcl]]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})
configure_file(${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl ${CMAKE_BINARY_DIR}/lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/pkgIndex.tcl COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.tcl.in ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl DESTINATION lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})
configure_file(${CMAKE_CURRENT_BINARY_DIR}/iwidgets.tcl ${CMAKE_BINARY_DIR}/lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION}/iwidgets.tcl COPYONLY)
install(FILES license.terms DESTINATION lib/${IWIDGETS_PKGNAME}${IWIDGETS_PKGVERSION})

ADD_SUBDIRECTORY(doc)
