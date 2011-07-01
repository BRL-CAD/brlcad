IF(NOT MAN_DIR)
	SET(MAN_DIR "man")
ENDIF(NOT MAN_DIR)

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")
FIND_PROGRAM(SED_EXEC NAMES sed DOC "path to sed executable")
MARK_AS_ADVANCED(SH_EXEC)
MARK_AS_ADVANCED(SED_EXEC)

FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man1)
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man3)
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mann)

IF(SED_EXEC AND SH_EXEC AND NOT TKMAN_GENERATED)
	MESSAGE("Generating Tk man pages...")

	SET(tk_MAN1
		wish.1
		)
	FOREACH(manfile ${tk_MAN1})
		EXEC_PROGRAM(${SH_EXEC} ARGS
			${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/man1
			OUTPUT_VARIABLE MAN1_SH)
	ENDFOREACH(manfile ${tk_MAN1})

	SET(tk_MAN3
		3DBorder.3
		AddOption.3
		BindTable.3
		CanvPsY.3
		CanvTkwin.3
		CanvTxtInfo.3
		Clipboard.3
		ClrSelect.3
		ConfigWidg.3
		ConfigWind.3
		CoordToWin.3
		CrtCmHdlr.3
		CrtConsoleChan.3
		CrtErrHdlr.3
		CrtGenHdlr.3
		CrtImgType.3
		CrtItemType.3
		CrtPhImgFmt.3
		CrtSelHdlr.3
		CrtWindow.3
		DeleteImg.3
		DrawFocHlt.3
		EventHndlr.3
		FindPhoto.3
		FontId.3
		FreeXId.3
		GeomReq.3
		GetAnchor.3
		GetBitmap.3
		GetCapStyl.3
		GetClrmap.3
		GetColor.3
		GetCursor.3
		GetDash.3
		GetFont.3
		GetGC.3
		GetHINSTANCE.3
		GetHWND.3
		GetImage.3
		GetJoinStl.3
		GetJustify.3
		GetOption.3
		GetPixels.3
		GetPixmap.3
		GetRelief.3
		GetRootCrd.3
		GetScroll.3
		GetSelect.3
		GetUid.3
		GetVRoot.3
		GetVisual.3
		Grab.3
		HWNDToWindow.3
		HandleEvent.3
		IdToWindow.3
		ImgChanged.3
		Inactive.3
		InternAtom.3
		MainLoop.3
		MainWin.3
		MaintGeom.3
		ManageGeom.3
		MapWindow.3
		MeasureChar.3
		MoveToplev.3
		Name.3
		NameOfImg.3
		OwnSelect.3
		ParseArgv.3
		QWinEvent.3
		Restack.3
		RestrictEv.3
		SetAppName.3
		SetCaret.3
		SetClass.3
		SetClassProcs.3
		SetGrid.3
		SetOptions.3
		SetVisual.3
		StrictMotif.3
		TextLayout.3
		TkInitStubs.3
		Tk_Init.3
		Tk_Main.3
		WindowId.3
		ttk_Geometry.3
		ttk_Theme.3
		)
	FOREACH(manfile ${tk_MAN3})
		EXEC_PROGRAM(${SH_EXEC} ARGS
			${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/man3
			OUTPUT_VARIABLE MAN3_SH)
	ENDFOREACH(manfile ${tk_MAN3})

	SET(tk_MANN
		bell.n
		bind.n
		bindtags.n
		bitmap.n
		button.n
		canvas.n
		checkbutton.n
		chooseColor.n
		chooseDirectory.n
		clipboard.n
		colors.n
		console.n
		cursors.n
		destroy.n
		dialog.n
		entry.n
		event.n
		focus.n
		focusNext.n
		font.n
		frame.n
		getOpenFile.n
		grab.n
		grid.n
		image.n
		keysyms.n
		label.n
		labelframe.n
		listbox.n
		loadTk.n
		lower.n
		menu.n
		menubar.n
		menubutton.n
		message.n
		messageBox.n
		option.n
		optionMenu.n
		options.n
		pack-old.n
		pack.n
		palette.n
		panedwindow.n
		photo.n
		place.n
		popup.n
		radiobutton.n
		raise.n
		scale.n
		scrollbar.n
		selection.n
		send.n
		spinbox.n
		text.n
		tk.n
		tkerror.n
		tkvars.n
		tkwait.n
		toplevel.n
		ttk_button.n
		ttk_checkbutton.n
		ttk_combobox.n
		ttk_entry.n
		ttk_frame.n
		ttk_image.n
		ttk_intro.n
		ttk_label.n
		ttk_labelframe.n
		ttk_menubutton.n
		ttk_notebook.n
		ttk_panedwindow.n
		ttk_progressbar.n
		ttk_radiobutton.n
		ttk_scale.n
		ttk_scrollbar.n
		ttk_separator.n
		ttk_sizegrip.n
		ttk_style.n
		ttk_treeview.n
		ttk_vsapi.n
		ttk_widget.n
		winfo.n
		wm.n
		)
	FOREACH(manfile ${tk_MANN})
		EXEC_PROGRAM(${SH_EXEC} ARGS
			${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/mann
			OUTPUT_VARIABLE MANN_SH)
	ENDFOREACH(manfile ${tk_MANN})
	SET(TKMAN_GENERATED 1 CACHE BOOL "Flag signifying Tk man pages are generated" FORCE)
	MARK_AS_ADVANCED(TKMAN_GENERATED)

ENDIF(SED_EXEC AND SH_EXEC AND NOT TKMAN_GENERATED)

IF(TKMAN_GENERATED)
	FILE(GLOB TK_MAN1	"${CMAKE_CURRENT_BINARY_DIR}/man1/*")
	FILE(COPY ${TK_MAN1} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
	INSTALL(FILES ${TK_MAN1} DESTINATION ${MAN_DIR}/man1)

	FILE(GLOB TK_MAN3	"${CMAKE_CURRENT_BINARY_DIR}/man3/*")
	FILE(COPY ${TK_MAN3} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man3)
	INSTALL(FILES ${TK_MAN3} DESTINATION ${MAN_DIR}/man3)

	FILE(GLOB TK_MANN	"${CMAKE_CURRENT_BINARY_DIR}/mann/*")
	FILE(COPY ${TK_MANN} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/mann)
	INSTALL(FILES ${TK_MANN} DESTINATION ${MAN_DIR}/mann)
ENDIF(TKMAN_GENERATED)
