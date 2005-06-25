# $Id$

mann_MANS = \
	buttonbox.n \
	calendar.n \
	canvasprintbox.n \
	canvasprintdialog.n \
	checkbox.n \
	combobox.n \
	dateentry.n \
	datefield.n \
	dialog.n \
	dialogshell.n \
	disjointlistbox.n \
	entryfield.n \
	extbutton.n \
	extfileselectionbox.n \
	extfileselectiondialog.n \
	feedback.n \
	fileselectionbox.n \
	fileselectiondialog.n \
	finddialog.n \
	hierarchy.n \
	hyperhelp.n \
	labeledframe.n \
	labeledwidget.n \
	mainwindow.n \
	menubar.n \
	messagebox.n \
	messagedialog.n \
	notebook.n \
	optionmenu.n \
	panedwindow.n \
	promptdialog.n \
	pushbutton.n \
	radiobox.n \
	scopedobject.n \
	scrolledcanvas.n \
	scrolledframe.n \
	scrolledhtml.n \
	scrolledlistbox.n \
	scrolledtext.n \
	selectionbox.n \
	selectiondialog.n \
	shell.n \
	spindate.n \
	spinint.n \
	spinner.n \
	spintime.n \
	tabnotebook.n \
	tabset.n \
	timeentry.n \
	timefield.n \
	toolbar.n \
	watch.n

EXTRA_DIST = \
	$(mann_MANS) \
	generateHtml.sh \
	iwidgets.ps \
	man.macros \
	mkitclman \
	tk2html \
	tk2html.awk \
	tk2html.perl \
	tk2html2.awk

transform = s,^,iwidgets_,

install-data-hook:
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/iwidgets/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/mann/iwidgets_$$manpage > $(DESTDIR)$(mandir)/mann/iwidgets_$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/mann/iwidgets_$$manpage.new $(DESTDIR)$(mandir)/mann/iwidgets_$$manpage ; \
	done

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

