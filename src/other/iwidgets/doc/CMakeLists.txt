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

FOREACH(manpage ${iwidgets_MANS})
	configure_file(${manpage} ${CMAKE_CURRENT_BINARY_DIR}/iwidgets_${manpage} COPY_ONLY)
	SET(iwidgets_named_MANS ${iwidgets_named_MANS} ${CMAKE_CURRENT_BINARY_DIR}/iwidgets_${manpage})
ENDFOREACH(manpage ${iwidgets_MANS})

ADD_MAN_PAGES(n iwidgets_named_MANS)
