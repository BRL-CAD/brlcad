lib_dir = $(DESTDIR)$(pkglibdir)
lib_LTLIBRARIES = lib@PACKAGE_NAME@@PACKAGE_VERSION@.la

htmltokens.c:   $(srcdir)/tokenlist.txt
	$(TCLSH_PROG) $?

$(srcdir)/htmldefaultstyle.c: $(srcdir)/tkhtml.tcl  $(srcdir)/html.css \
                    $(srcdir)/mkdefaultstyle.tcl
	$(TCLSH_PROG) $(srcdir)/mkdefaultstyle.tcl > $(srcdir)/htmldefaultstyle.c

htmltokens.h: htmltokens.c

cssprop.h: $(srcdir)/cssprop.tcl
	$(TCLSH_PROG) $? 

cssprop.c: cssprop.h

#
# Tkhtml Library
#
lib@PACKAGE_NAME@@PACKAGE_VERSION@_la_SOURCES = \
	css.c \
	cssprop.c \
	cssdynamic.c \
	cssparser.c \
	csssearch.c \
	htmldraw.c \
	htmlfloat.c \
	htmlhash.c \
	htmlimage.c \
	htmlinline.c \
	htmllayout.c \
	htmlparse.c \
	htmlprop.c \
	htmlstyle.c \
	htmltagdb.c \
	htmltable.c \
	htmltcl.c \
	htmltree.c \
	swproc.c \
	restrack.c \
	htmldecode.c \
	htmltext.c \
	htmlutil.c	

lib@PACKAGE_NAME@@PACKAGE_VERSION@_la_LDFLAGS = @TCL_LIB_SPEC@
lib@PACKAGE_NAME@@PACKAGE_VERSION@_la_LIBADD = @TCL_LIBS@ @TK_LIBS@
lib@PACKAGE_NAME@@PACKAGE_VERSION@_la_CPPFLAGS = @TCL_INCLUDES@ @TK_INCLUDES@

pkgIndex.tcl:
	echo 'package ifneeded @PACKAGE_NAME@ @PACKAGE_VERSION@ [list load [file join $(lib_dir) ".." @PKG_LIB_FILE@] @PACKAGE_NAME@]' > pkgIndex.tcl

tkhtmlscriptdir = $(DESTDIR)$(pkglibdir)

tkhtmlscript_SCRIPTS = pkgIndex.tcl

BUILT_SOURCES = \
	cssprop.c \
	cssprop.h \
	htmltokens.h \
	$(srcdir)/htmldefaultstyle.c \
	htmltokens.c

DISTCLEANFILES = \
	$(BUILT_SOURCES) \
	pkgIndex.tcl

include $(top_srcdir)/misc/Makefile.defs
