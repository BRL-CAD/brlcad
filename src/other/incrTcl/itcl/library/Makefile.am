# $Id$

pkglibdir = $(libdir)/itcl3.3

tclscripts = $(top_srcdir)/src/other/incrTcl/itcl/library/itcl.tcl

install-data-hook:
	@mkdir -p $(DESTDIR)$(pkglibdir)
	@for i in $(tclscripts) ; do \
	    echo "Installing $$i" ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(pkglibdir)/. ; \
	done;

EXTRA_DIST = itcl.tcl
