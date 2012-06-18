SET(iwidgets_MANS
	buttonbox.n
	calendar.n
	canvasprintbox.n
	canvasprintdialog.n
	checkbox.n
	combobox.n
	dateentry.n
	datefield.n
	dialog.n
	dialogshell.n
	disjointlistbox.n
	entryfield.n
	extbutton.n
	extfileselectionbox.n
	extfileselectiondialog.n
	feedback.n
	fileselectionbox.n
	fileselectiondialog.n
	finddialog.n
	hierarchy.n
	hyperhelp.n
	labeledframe.n
	labeledwidget.n
	mainwindow.n
	menubar.n
	messagebox.n
	messagedialog.n
	notebook.n
	optionmenu.n
	panedwindow.n
	promptdialog.n
	pushbutton.n
	radiobox.n
	scopedobject.n
	scrolledcanvas.n
	scrolledframe.n
	scrolledhtml.n
	scrolledlistbox.n
	scrolledtext.n
	selectionbox.n
	selectiondialog.n
	shell.n
	spindate.n
	spinint.n
	spinner.n
	spintime.n
	tabnotebook.n
	tabset.n
	timeentry.n
	timefield.n
	toolbar.n
	watch.n
	)

foreach(manpage ${iwidgets_MANS})
  configure_file(${manpage} ${CMAKE_CURRENT_BINARY_DIR}/iwidgets_${manpage} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iwidgets_${manpage} DESTINATION ${MAN_DIR}/mann)
  if(CMAKE_CONFIGURATION_TYPES)
     foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
       string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
       configure_file(${manpage} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${MAN_DIR}/mann/iwidgets_${manpage} COPYONLY)
     endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  else(CMAKE_CONFIGURATION_TYPES)
     configure_file(${manpage} ${CMAKE_BINARY_DIR}/${MAN_DIR}/mann/iwidgets_${manpage} COPYONLY)
  endif(CMAKE_CONFIGURATION_TYPES)
endforeach(manpage ${iwidgets_MANS})

