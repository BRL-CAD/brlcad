# $Id$

SUBDIRS = \
	doc \
	library

DIST_SUBDIRS = \
	doc \
	generic \
	library \
	unix

generic_src = \
	$(srcdir)/generic/tk3d.c \
	$(srcdir)/generic/tkArgv.c \
	$(srcdir)/generic/tkAtom.c \
	$(srcdir)/generic/tkBind.c \
	$(srcdir)/generic/tkBitmap.c \
	$(srcdir)/generic/tkButton.c \
	$(srcdir)/generic/tkCanvArc.c \
	$(srcdir)/generic/tkCanvBezier.c \
	$(srcdir)/generic/tkCanvBmap.c \
	$(srcdir)/generic/tkCanvImg.c \
	$(srcdir)/generic/tkCanvLine.c \
	$(srcdir)/generic/tkCanvPoly.c \
	$(srcdir)/generic/tkCanvPs.c \
	$(srcdir)/generic/tkCanvText.c \
	$(srcdir)/generic/tkCanvUtil.c \
	$(srcdir)/generic/tkCanvWind.c \
	$(srcdir)/generic/tkCanvas.c \
	$(srcdir)/generic/tkClipboard.c \
	$(srcdir)/generic/tkCmds.c \
	$(srcdir)/generic/tkColor.c \
	$(srcdir)/generic/tkConfig.c \
	$(srcdir)/generic/tkConsole.c \
	$(srcdir)/generic/tkCursor.c \
	$(srcdir)/generic/tkEntry.c \
	$(srcdir)/generic/tkError.c \
	$(srcdir)/generic/tkEvent.c \
	$(srcdir)/generic/tkFocus.c \
	$(srcdir)/generic/tkFont.c \
	$(srcdir)/generic/tkFrame.c \
	$(srcdir)/generic/tkGC.c \
	$(srcdir)/generic/tkGeometry.c \
	$(srcdir)/generic/tkGet.c \
	$(srcdir)/generic/tkGrab.c \
	$(srcdir)/generic/tkGrid.c \
	$(srcdir)/generic/tkImage.c \
	$(srcdir)/generic/tkImgBmap.c \
	$(srcdir)/generic/tkImgGIF.c \
	$(srcdir)/generic/tkImgPPM.c \
	$(srcdir)/generic/tkImgPhoto.c \
	$(srcdir)/generic/tkListbox.c \
	$(srcdir)/generic/tkMain.c \
	$(srcdir)/generic/tkMenu.c \
	$(srcdir)/generic/tkMenuDraw.c \
	$(srcdir)/generic/tkMenubutton.c \
	$(srcdir)/generic/tkMessage.c \
	$(srcdir)/generic/tkObj.c \
	$(srcdir)/generic/tkOldConfig.c \
	$(srcdir)/generic/tkOption.c \
	$(srcdir)/generic/tkPack.c \
	$(srcdir)/generic/tkPanedWindow.c \
	$(srcdir)/generic/tkPlace.c \
	$(srcdir)/generic/tkRectOval.c \
	$(srcdir)/generic/tkScale.c \
	$(srcdir)/generic/tkScrollbar.c \
	$(srcdir)/generic/tkSelect.c \
	$(srcdir)/generic/tkStubImg.c \
	$(srcdir)/generic/tkStubInit.c \
	$(srcdir)/generic/tkStubLib.c \
	$(srcdir)/generic/tkStyle.c \
	$(srcdir)/generic/tkText.c \
	$(srcdir)/generic/tkTextBTree.c \
	$(srcdir)/generic/tkTextDisp.c \
	$(srcdir)/generic/tkTextImage.c \
	$(srcdir)/generic/tkTextIndex.c \
	$(srcdir)/generic/tkTextMark.c \
	$(srcdir)/generic/tkTextTag.c \
	$(srcdir)/generic/tkTextWind.c \
	$(srcdir)/generic/tkTrig.c \
	$(srcdir)/generic/tkUndo.c \
	$(srcdir)/generic/tkUtil.c \
	$(srcdir)/generic/tkVisual.c \
	$(srcdir)/generic/tkWindow.c


unix_src = \
	$(srcdir)/unix/tkUnix.c \
	$(srcdir)/unix/tkUnix3d.c \
	$(srcdir)/unix/tkUnixButton.c \
	$(srcdir)/unix/tkUnixColor.c \
	$(srcdir)/unix/tkUnixConfig.c \
	$(srcdir)/unix/tkUnixCursor.c \
	$(srcdir)/unix/tkUnixDialog.c \
	$(srcdir)/unix/tkUnixDraw.c \
	$(srcdir)/unix/tkUnixEmbed.c \
	$(srcdir)/unix/tkUnixEvent.c \
	$(srcdir)/unix/tkUnixFocus.c \
	$(srcdir)/unix/tkUnixFont.c \
	$(srcdir)/unix/tkUnixInit.c \
	$(srcdir)/unix/tkUnixKey.c \
	$(srcdir)/unix/tkUnixMenu.c \
	$(srcdir)/unix/tkUnixMenubu.c \
	$(srcdir)/unix/tkUnixScale.c \
	$(srcdir)/unix/tkUnixScrlbr.c \
	$(srcdir)/unix/tkUnixSelect.c \
	$(srcdir)/unix/tkUnixSend.c \
	$(srcdir)/unix/tkUnixWm.c \
	$(srcdir)/unix/tkUnixXId.c

EXTRA_DIST = \
	README \
	bitmaps \
	changes \
	compat \
	license.terms \
	mac \
	macosx \
	tests \
	win \
	xlib

lib_LTLIBRARIES = \
	libtk.la \
	libtk@TK_VERSION@.la 

libtk_la_SOURCES = $(generic_src) $(unix_src)

libtk_la_LDFLAGS = -version-info 19:1

libtk_la_LIBADD = ${X_LIBS}


libtk@TK_VERSION@_la_SOURCES = $(libtk_la_SOURCES)

libtk@TK_VERSION@_la_LDFLAGS = $(libtk_la_LDFLAGS)

libtk@TK_VERSION@_la_LIBADD = $(libtk_la_LIBADD)


bin_PROGRAMS = wish

# moved from unix to this dir
wish_SOURCES = tkAppInit.c

wish_LDADD = $(lib_LTLIBRARIES) ${TCL}


tklibdir = ${libdir}

AM_CPPFLAGS = \
	-DTK_LIBRARY="\"${TK_LIBRARY}\"" \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtk/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	@X_CFLAGS@

