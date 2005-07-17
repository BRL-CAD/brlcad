# $Id$

lib_LTLIBRARIES = \
	libtkstub.la \
	libtkstub@TK_VERSION@.la

libtkstub_la_SOURCES = tkStubLib.c
libtkstub@TK_VERSION@_la_SOURCES = $(libtkstub_la_SOURCES)

noinst_LTLIBRARIES = libtkg.la

libtkg_la_LDFLAGS = -static
libtkg_la_SOURCES = \
	tk3d.c \
	tkArgv.c \
	tkAtom.c \
	tkBind.c \
	tkBitmap.c \
	tkButton.c \
	tkCanvArc.c \
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
	tkListbox.c \
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
	tkStubImg.c \
	tkStubInit.c \
	tkStubLib.c \
	tkStyle.c \
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

EXTRA_DIST = \
	README \
	prolog.ps \
	tk.decls \
	tkFileFilter.c \
	tkImgUtil.c \
	tkInt.decls \
	tkMacWinMenu.c \
	tkPointer.c \
	tkSquare.c \
	tkTest.c

brlcadincludedir = $(includedir)/brlcad
brlcadinclude_HEADERS = $(include_HEADERS)

AM_CPPFLAGS = \
	-DTK_LIBRARY="\"${TK_LIBRARY}\"" \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtk/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	-I${top_srcdir}/src/other/libtk/mac \
	-I${top_srcdir}/src/other/libtk/win \
	@X_CFLAGS@

include $(top_srcdir)/misc/Makefile.defs

