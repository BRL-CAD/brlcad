# $Id$

man1_MANS = wish.1

man3_MANS = \
	3DBorder.3 \
	AddOption.3 \
	BindTable.3 \
	CanvPsY.3 \
	CanvTkwin.3 \
	CanvTxtInfo.3 \
	Clipboard.3 \
	ClrSelect.3 \
	ConfigWidg.3 \
	ConfigWind.3 \
	CoordToWin.3 \
	CrtCmHdlr.3 \
	CrtErrHdlr.3 \
	CrtGenHdlr.3 \
	CrtImgType.3 \
	CrtItemType.3 \
	CrtPhImgFmt.3 \
	CrtSelHdlr.3 \
	CrtWindow.3 \
	DeleteImg.3 \
	DrawFocHlt.3 \
	EventHndlr.3 \
	FindPhoto.3 \
	FontId.3 \
	FreeXId.3 \
	GeomReq.3 \
	GetAnchor.3 \
	GetBitmap.3 \
	GetCapStyl.3 \
	GetClrmap.3 \
	GetColor.3 \
	GetCursor.3 \
	GetDash.3 \
	GetFont.3 \
	GetGC.3 \
	GetHINSTANCE.3 \
	GetHWND.3 \
	GetImage.3 \
	GetJoinStl.3 \
	GetJustify.3 \
	GetOption.3 \
	GetPixels.3 \
	GetPixmap.3 \
	GetRelief.3 \
	GetRootCrd.3 \
	GetScroll.3 \
	GetSelect.3 \
	GetUid.3 \
	GetVRoot.3 \
	GetVisual.3 \
	Grab.3 \
	HWNDToWindow.3 \
	HandleEvent.3 \
	IdToWindow.3 \
	ImgChanged.3 \
	InternAtom.3 \
	MainLoop.3 \
	MainWin.3 \
	MaintGeom.3 \
	ManageGeom.3 \
	MapWindow.3 \
	MeasureChar.3 \
	MoveToplev.3 \
	Name.3 \
	NameOfImg.3 \
	OwnSelect.3 \
	ParseArgv.3 \
	QWinEvent.3 \
	Restack.3 \
	RestrictEv.3 \
	SetAppName.3 \
	SetCaret.3 \
	SetClass.3 \
	SetClassProcs.3 \
	SetGrid.3 \
	SetOptions.3 \
	SetVisual.3 \
	StrictMotif.3 \
	TextLayout.3 \
	TkInitStubs.3 \
	Tk_Init.3 \
	Tk_Main.3 \
	WindowId.3

mann_MANS = \
	bell.n \
	bind.n \
	bindtags.n \
	bitmap.n \
	button.n \
	canvas.n \
	checkbutton.n \
	chooseColor.n \
	chooseDirectory.n \
	clipboard.n \
	colors.n \
	console.n \
	cursors.n \
	destroy.n \
	dialog.n \
	entry.n \
	event.n \
	focus.n \
	focusNext.n \
	font.n \
	frame.n \
	getOpenFile.n \
	grab.n \
	grid.n \
	image.n \
	keysyms.n \
	label.n \
	labelframe.n \
	listbox.n \
	loadTk.n \
	lower.n \
	menu.n \
	menubar.n \
	menubutton.n \
	message.n \
	messageBox.n \
	option.n \
	optionMenu.n \
	options.n \
	pack-old.n \
	pack.n \
	palette.n \
	panedwindow.n \
	photo.n \
	place.n \
	popup.n \
	radiobutton.n \
	raise.n \
	scale.n \
	scrollbar.n \
	selection.n \
	send.n \
	spinbox.n \
	text.n \
	tk.n \
	tkerror.n \
	tkvars.n \
	tkwait.n \
	toplevel.n \
	winfo.n \
	wm.n

EXTRA_DIST = \
	$(man1_MANS) \
	$(man3_MANS) \
	$(mann_MANS) \
	license.terms \
	man.macros

install-data-hook:
	@for manpage in $(man1_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/man1/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtk/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/man1/$$manpage > $(DESTDIR)$(mandir)/man1/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/man1/$$manpage.new $(DESTDIR)$(mandir)/man1/$$manpage ; \
	done
	@for manpage in $(man3_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/man3/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtk/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/man3/$$manpage > $(DESTDIR)$(mandir)/man3/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/man3/$$manpage.new $(DESTDIR)$(mandir)/man3/$$manpage ; \
	done
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtk/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/mann/$$manpage > $(DESTDIR)$(mandir)/mann/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/mann/$$manpage.new $(DESTDIR)$(mandir)/mann/$$manpage ; \
	done

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

