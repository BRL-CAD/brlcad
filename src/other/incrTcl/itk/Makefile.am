# $Id$

lib_LTLIBRARIES = \
	libitk3.3.la \
	libitkstub3.3.la

libitkstub3_3_la_SOURCES = \
	generic/itkStubLib.c

libitk3_3_la_SOURCES = \
	generic/itk_cmds.c \
	generic/itk_option.c \
	generic/itk_archetype.c \
	generic/itk_util.c \
	generic/itkStubInit.c

include_HEADERS = \
	generic/itk.h \
	generic/itkDecls.h

MAINTAINERCLEANFILES = \
	Makefile.in

pkglibdir = $(libdir)/itk3.3

install-data-hook:
	@mkdir -p $(DESTDIR)$(pkglibdir)
	@for i in $(top_srcdir)/src/other/incrTcl/itk/library/*.itk ; do \
	    echo "Installing $$i" ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(pkglibdir)/. ; \
	done;
	$(INSTALL_DATA) $(top_srcdir)/src/other/incrTcl/itk/pkgIndex.tcl $(DESTDIR)$(pkglibdir)/. 
	$(INSTALL_DATA) $(top_srcdir)/src/other/incrTcl/itk/library/itk.tcl $(DESTDIR)$(pkglibdir)/. 
	$(INSTALL_DATA) $(top_srcdir)/src/other/incrTcl/itk/library/tclIndex $(DESTDIR)$(pkglibdir)/.
