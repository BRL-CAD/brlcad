# $Id$

noinst_LTLIBRARIES = libtkgeneric.la

libtkgeneric_la_SOURCES = \
	tk3d.c \
	tkArgv.c \
	tkAtom.c \
	tkBind.c \
	tkBitmap.c \
	tkButton.c \
	tkCanvArc.c \
	tkCanvBezier.c \
	tkCanvBmap.c \
	tkCanvImg.c \
	tkCanvLine.c \
	tkCanvPoly.c \
	tkCanvPs.c \
	tkCanvText.c \
	tkCanvUtil.c \
	tkCanvWind.c \
	tkCanvas.c \
	tkClipboard.c \
	tkCmds.c \
	tkColor.c \
	tkConfig.c \
	tkConsole.c \
	tkCursor.c \
	tkEntry.c \
	tkError.c \
	tkEvent.c \
	tkFileFilter.c \
	tkFocus.c \
	tkFont.c \
	tkFrame.c \
	tkGC.c \
	tkGeometry.c \
	tkGet.c \
	tkGrab.c \
	tkGrid.c \
	tkImage.c \
	tkImgBmap.c \
	tkImgGIF.c \
	tkImgPPM.c \
	tkImgPhoto.c \
	tkImgUtil.c \
	tkListbox.c \
	tkMacWinMenu.c \
	tkMain.c \
	tkMenu.c \
	tkMenuDraw.c \
	tkMenubutton.c \
	tkMessage.c \
	tkObj.c \
	tkOldConfig.c \
	tkOption.c \
	tkPack.c \
	tkPanedWindow.c \
	tkPlace.c \
	tkRectOval.c \
	tkScale.c \
	tkScrollbar.c \
	tkSelect.c \
	tkSquare.c \
	tkStubImg.c \
	tkStubInit.c \
	tkStubLib.c \
	tkStyle.c \
	tkTest.c \
	tkText.c \
	tkTextBTree.c \
	tkTextDisp.c \
	tkTextImage.c \
	tkTextIndex.c \
	tkTextMark.c \
	tkTextTag.c \
	tkTextWind.c \
	tkTrig.c \
	tkUndo.c \
	tkUtil.c \
	tkVisual.c \
	tkWindow.c

noinst_HEADERS = \
	default.h \
	ks_names.h \
	tk3d.h \
	tkButton.h \
	tkCanvas.h \
	tkColor.h \
	tkFileFilter.h \
	tkFont.h \
	tkInitScript.h \
	tkInt.h \
	tkIntDecls.h \
	tkIntPlatDecls.h \
	tkIntXlibDecls.h \
	tkMenu.h \
	tkMenubutton.h \
	tkPort.h \
	tkScale.h \
	tkScrollbar.h \
	tkSelect.h \
	tkText.h \
	tkUndo.h

include_HEADERS = \
	tk.h \
	tkDecls.h \
	tkPlatDecls.h

brlcadinclude_HEADERS = $(include_HEADERS)

EXTRA_DIST = \
	README \
	prolog.ps \
	tk.decls. \
	tkInt.decls

MAINTAINERCLEANFILES = \
	Makefile.in

brlcadincludedir = $(includedir)/brlcad

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	-I${top_srcdir}/src/other/libtk/xlib

libtkgeneric_la_LIBADD = ${X_LIBS}

AM_CFLAGS = ${X_CFLAGS}