# $Id$

INSTALL_DIR = ${libdir}/iwidgets4.0.1/scripts
FILES = buttonbox.itk calendar.itk canvasprintbox.itk canvasprintdialog.itk checkbox.itk combobox.itk dateentry.itk datefield.itk dialog.itk dialogshell.itk disjointlistbox.itk entryfield.itk extbutton.itk extfileselectionbox.itk extfileselectiondialog.itk feedback.itk fileselectionbox.itk fileselectiondialog.itk finddialog.itk hierarchy.itk hyperhelp.itk labeledframe.itk labeledwidget.itk mainwindow.itk menubar.itk messagebox.itk messagedialog.itk notebook.itk optionmenu.itk panedwindow.itk pane.itk promptdialog.itk pushbutton.itk radiobox.itk regexpfield.itk scrolledcanvas.itk scrolledframe.itk scrolledhtml.itk scrolledlistbox.itk scrolledtext.itk scrolledwidget.itk selectionbox.itk selectiondialog.itk shell.itk spindate.itk spinint.itk spinner.itk spintime.itk tabnotebook.itk tabset.itk timeentry.itk timefield.itk toolbar.itk watch.itk

install:
	mkdir -p ${INSTALL_DIR}
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a ${INSTALL_DIR}/$$a ; done
	for a in $(SUBDIRS) ; do (cd $$a && make install) ; done

