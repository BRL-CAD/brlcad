lib_libexecdir = @TKHTML_TCL_TARGET@
lib_libexec_LTLIBRARIES = libtkhtml.la

htmltokens.c:   $(srcdir)/tokenlist.txt
        $(TCLSH) $?

htmldefaultstyle.c: $(srcdir)/tkhtml.tcl  $(srcdir)/html.css \
                    $(srcdir)/mkdefaultstyle.tcl
	$(TCLSH) $(srcdir)/mkdefaultstyle.tcl > htmldefaultstyle.c

htmltokens.h: htmltokens.c

cssprop.h: $(srcdir)/cssprop.tcl
	$(TCLSH) $? 

cssprop.c: cssprop.h

#
# Tkhtml Library
#
libtkhtml_la_SOURCES = \
	css.c \
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

libtkhtml_la_LDFLAGS = @TCL_LIB_SPEC@ -module -avoid-version
libtkhtml_la_LIBADD = @TCL_LIBS@
libtkhtml_la_CPPFLAGS = @TCL_INCLUDES@

BUILT_SOURCES = \
	cssprop.c \
	cssprop.h \
	htmltokens.h \
	htmldefaultstyle.c \
	htmltokens.c

include $(top_srcdir)/misc/Makefile.defs
