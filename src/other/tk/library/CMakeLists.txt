MACRO(TK_SCRIPTS_BUILD files destination)
	FOREACH(file ${${files}})
		get_filename_component(filename ${file} NAME)
		configure_file(${file} ${CMAKE_BINARY_DIR}/${destination}/${filename} COPYONLY)
	ENDFOREACH(file ${${files}})
ENDMACRO(TK_SCRIPTS_BUILD)

SET(TK_LIBRARY_FILES
	bgerror.tcl
	button.tcl
	choosedir.tcl
	clrpick.tcl
	comdlg.tcl
	console.tcl
	dialog.tcl
	entry.tcl
	focus.tcl
	listbox.tcl
	menu.tcl
	mkpsenc.tcl
	msgbox.tcl
	obsolete.tcl
	optMenu.tcl
	palette.tcl
	panedwindow.tcl
	safetk.tcl
	scale.tcl
	scrlbar.tcl
	spinbox.tcl
	tearoff.tcl
	text.tcl
	tk.tcl
	tkfbox.tcl
	unsupported.tcl
	xmfbox.tcl
	tclIndex
	)
install(FILES ${TK_LIBRARY_FILES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR})
TK_SCRIPTS_BUILD(TK_LIBRARY_FILES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR})

SET(TK_LIBRARY_IMAGE_FILES
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
	)
install(FILES ${TK_LIBRARY_IMAGE_FILES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/images)
TK_SCRIPTS_BUILD(TK_LIBRARY_IMAGE_FILES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/images)

SET(TK_LIBRARY_MSG_FILES
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
	)
install(FILES ${TK_LIBRARY_MSG_FILES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/msgs)
TK_SCRIPTS_BUILD(TK_LIBRARY_MSG_FILES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/msgs)

SET(TTK_LIBRARY_FILES
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
)
install(FILES ${TTK_LIBRARY_FILES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/ttk)
TK_SCRIPTS_BUILD(TTK_LIBRARY_FILES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/ttk)

SET(TK_DEMO_PROGS
	demos/browse
	demos/hello
	demos/ixset
	demos/rmt
	demos/rolodex
	demos/tcolor
	demos/timer
	demos/widget
)
INSTALL(PROGRAMS ${TK_DEMO_PROGS} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos)
TK_SCRIPTS_BUILD(TK_DEMO_PROGS lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos)

SET(TK_DEMO_FILES
	demos/README
	demos/anilabel.tcl
	demos/aniwave.tcl
	demos/arrow.tcl
	demos/bind.tcl
	demos/bitmap.tcl
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
	demos/form.tcl
	demos/goldberg.tcl
	demos/hscale.tcl
	demos/icon.tcl
	demos/image1.tcl
	demos/image2.tcl
	demos/items.tcl
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
	demos/ruler.tcl
	demos/sayings.tcl
	demos/search.tcl
	demos/spin.tcl
	demos/square
	demos/states.tcl
	demos/style.tcl
	demos/tclIndex
	demos/text.tcl
	demos/textpeer.tcl
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
)
INSTALL(FILES ${TK_DEMO_FILES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos)
TK_SCRIPTS_BUILD(TK_DEMO_FILES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos)

SET(TK_DEMO_IMAGES
	demos/images/earth.gif
	demos/images/earthris.gif
	demos/images/face.xbm
	demos/images/flagdown.xbm
	demos/images/flagup.xbm
	demos/images/gray25.xbm
	demos/images/letters.xbm
	demos/images/noletter.xbm
	demos/images/pattern.xbm
	demos/images/tcllogo.gif
	demos/images/teapot.ppm
)
INSTALL(FILES ${TK_DEMO_IMAGES} DESTINATION lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos/images)
TK_SCRIPTS_BUILD(TK_DEMO_IMAGES lib/tk${TK_VERSION_MAJOR}.${TK_VERSION_MINOR}/demos/images)
