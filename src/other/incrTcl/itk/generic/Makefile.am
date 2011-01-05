
lib_LTLIBRARIES = \
	libitkstub.la \
	libitkstub@ITCL_VERSION@.la

noinst_LTLIBRARIES = libitg.la
 
AM_CFLAGS = ${X_CFLAGS} -DSTDC_HEADERS

# make sure we find Tcl's private headers
if TCL_8_4_HEADERS
compat_cppflags = \
	-I$(top_srcdir)/src/other/incrTcl/compat/8.4
else
compat_cppflags = \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix
endif # TCL_8_4_HEADERS

AM_CPPFLAGS = \
	${TCL_CPPFLAGS} \
	${ITCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	${compat_cppflags} \
	${NOWARN}

libitkstub_la_SOURCES = itkStubLib.c
libitkstub@ITCL_VERSION@_la_SOURCES = $(libitkstub_la_SOURCES)

libitg_la_SOURCES = \
	itkStubInit.c \
	itk_archetype.c \
	itk_cmds.c \
	itk_option.c \
	itk_util.c

include_HEADERS = \
	itk.h \
	itkDecls.h

EXTRA_DIST = itk.decls

include $(top_srcdir)/misc/Makefile.defs

