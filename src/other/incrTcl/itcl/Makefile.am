# $Id$

lib_LTLIBRARIES = \
	libitcl3.3.la \
	libitclstub3.3.la

libitclstub3_3_la_SOURCES = \
	generic/itclStubLib.c

libitcl3_3_la_SOURCES = \
	generic/itclStubInit.c \
	generic/itcl_bicmds.c \
	generic/itcl_class.c \
	generic/itcl_cmds.c \
	generic/itcl_ensemble.c \
	generic/itcl_linkage.c \
	generic/itcl_methods.c \
	generic/itcl_migrate.c \
	generic/itcl_objects.c \
	generic/itcl_parse.c \
	generic/itcl_util.c

include_HEADERS = \
	generic/itcl.h \
	generic/itclDecls.h \
	generic/itclInt.h \
	generic/itclIntDecls.h


MAINTAINERCLEANFILES = \
	Makefile.in

pkglibdir = $(libdir)/itcl3.3

install-data-hook:
	@mkdir -p $(DESTDIR)$(pkglibdir)
	@for i in $(top_srcdir)/src/other/incrTcl/itcl/library/*.tcl ; do \
	    echo "Installing $$i" ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(pkglibdir)/. ; \
	done;
	$(INSTALL_DATA) itclConfig.sh $(DESTDIR)$(libdir)/. 
	$(INSTALL_DATA) pkgIndex.tcl $(DESTDIR)$(pkglibdir)/. 
