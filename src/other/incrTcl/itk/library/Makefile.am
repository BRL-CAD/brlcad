# $Id$

pkglibdir = $(libdir)/itk3.3

itkscripts =\
	$(top_srcdir)/src/other/incrTcl/itk/libary/Archetype.itk\
	$(top_srcdir)/src/other/incrTcl/itk/libary/Toplevel.itk\
	$(top_srcdir)/src/other/incrTcl/itk/libary/Widget.itk\
	$(top_srcdir)/src/other/incrTcl/itk/libary/itk.tcl\
	$(top_srcdir)/src/other/incrTcl/itk/libary/tclIndex

install-data-hook:
	@mkdir -p $(DESTDIR)$(pklibdir)
	@for i in $(itkscripts) ; do \
	    echo "Installing $$i" ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(pkglibdir)/. ; \
	done;

EXTRA_DIST = $(itkscripts)
