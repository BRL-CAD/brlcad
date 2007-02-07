# $Id$

SUBDIRS = \
	doc \
	generic \
	library \
	unix

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

libtk_la_SOURCES =
libtk_la_LDFLAGS = -version-info 19:1
libtk_la_LIBADD = \
	generic/tk3d.lo \
	generic/tkArgv.lo \
	generic/tkAtom.lo \
	generic/tkBind.lo \
	generic/tkBitmap.lo \
	generic/tkButton.lo \
	generic/tkCanvArc.lo \
	generic/tkCanvBmap.lo \
	generic/tkCanvImg.lo \
	generic/tkCanvLine.lo \
	generic/tkCanvPoly.lo \
	generic/tkCanvPs.lo \
	generic/tkCanvText.lo \
	generic/tkCanvUtil.lo \
	generic/tkCanvWind.lo \
	generic/tkCanvas.lo \
	generic/tkClipboard.lo \
	generic/tkCmds.lo \
	generic/tkColor.lo \
	generic/tkConfig.lo \
	generic/tkConsole.lo \
	generic/tkCursor.lo \
	generic/tkEntry.lo \
	generic/tkError.lo \
	generic/tkEvent.lo \
	generic/tkFocus.lo \
	generic/tkFont.lo \
	generic/tkFrame.lo \
	generic/tkGC.lo \
	generic/tkGeometry.lo \
	generic/tkGet.lo \
	generic/tkGrab.lo \
	generic/tkGrid.lo \
	generic/tkImage.lo \
	generic/tkImgBmap.lo \
	generic/tkImgGIF.lo \
	generic/tkImgPPM.lo \
	generic/tkImgPhoto.lo \
	generic/tkListbox.lo \
	generic/tkMain.lo \
	generic/tkMenu.lo \
	generic/tkMenuDraw.lo \
	generic/tkMenubutton.lo \
	generic/tkMessage.lo \
	generic/tkObj.lo \
	generic/tkOldConfig.lo \
	generic/tkOption.lo \
	generic/tkPack.lo \
	generic/tkPanedWindow.lo \
	generic/tkPlace.lo \
	generic/tkRectOval.lo \
	generic/tkScale.lo \
	generic/tkScrollbar.lo \
	generic/tkSelect.lo \
	generic/tkStubImg.lo \
	generic/tkStubInit.lo \
	generic/tkStubLib.lo \
	generic/tkStyle.lo \
	generic/tkText.lo \
	generic/tkTextBTree.lo \
	generic/tkTextDisp.lo \
	generic/tkTextImage.lo \
	generic/tkTextIndex.lo \
	generic/tkTextMark.lo \
	generic/tkTextTag.lo \
	generic/tkTextWind.lo \
	generic/tkTrig.lo \
	generic/tkUndo.lo \
	generic/tkUtil.lo \
	generic/tkVisual.lo \
	generic/tkWindow.lo \
	unix/tkUnix.lo \
	unix/tkUnix3d.lo \
	unix/tkUnixButton.lo \
	unix/tkUnixColor.lo \
	unix/tkUnixConfig.lo \
	unix/tkUnixCursor.lo \
	unix/tkUnixDialog.lo \
	unix/tkUnixDraw.lo \
	unix/tkUnixEmbed.lo \
	unix/tkUnixEvent.lo \
	unix/tkUnixFocus.lo \
	unix/tkUnixFont.lo \
	unix/tkUnixInit.lo \
	unix/tkUnixKey.lo \
	unix/tkUnixMenu.lo \
	unix/tkUnixMenubu.lo \
	unix/tkUnixScale.lo \
	unix/tkUnixScrlbr.lo \
	unix/tkUnixSelect.lo \
	unix/tkUnixSend.lo \
	unix/tkUnixWm.lo \
	unix/tkUnixXId.lo \
	${X_LIBS}

libtk@TK_VERSION@_la_SOURCES = $(libtk_la_SOURCES)
libtk@TK_VERSION@_la_LDFLAGS = $(libtk_la_LDFLAGS)
libtk@TK_VERSION@_la_LIBADD = $(libtk_la_LIBADD)

noinst_PROGRAMS = wish

# MIPSpro 7.3 has a linker bug with long rpaths
if LINK_STATIC_REQUIRED
wish_LDFLAGS = -static
endif

# moved from unix to this dir
wish_SOURCES =
wish_LDADD = \
	unix/tkAppInit.lo \
	libtk.la \
	${TCL}

tklibdir = ${libdir}

AM_CFLAGS =
AM_CPPFLAGS = \
	-DTK_LIBRARY="\"${TK_LIBRARY}\"" \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtk/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	-I${top_srcdir}/src/other/libtk/mac \
	-I${top_srcdir}/src/other/libtk/win \
	@X_CFLAGS@

DEPS=src/other/libtcl

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

