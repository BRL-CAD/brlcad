# $Id$

pkglibdir = $(libdir)/itk3.3

itkscripts =\
	$(top_srcdir)/src/other/incrTcl/itk/library/Archetype.itk\
	$(top_srcdir)/src/other/incrTcl/itk/library/Toplevel.itk\
	$(top_srcdir)/src/other/incrTcl/itk/library/Widget.itk\
	$(top_srcdir)/src/other/incrTcl/itk/library/itk.tcl\
	$(top_srcdir)/src/other/incrTcl/itk/library/tclIndex

install-data-hook:
	@mkdir -p $(DESTDIR)$(pkglibdir)
	@for i in $(itkscripts) ; do \
	    echo "Installing $$i" ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(pkglibdir)/. ; \
	done;

EXTRA_DIST = $(itkscripts)
