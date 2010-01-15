
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	src \
	doc

EXTRA_DIST = \
	AUTHORS \
	COPYING \
	ChangeLog \
	INSTALL \
	NEWS \
	README

TKHTMLLIB_DESTDIR = @TKHTML_TCL_TARGET@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TKHTMLLIB_DESTDIR)
	cp -r tkhtml/* $(DESTDIR)$(TKHTMLLIB_DESTDIR)
	-( cd  $(DESTDIR)$(TKHTMLLIB_DESTDIR) ; echo 'eval pkg_mkIndex -verbose [pwd] init.tcl [glob [file join packages * *.tcl] [file join *[info sharedlibextension]]]' | @TCLSH_PROG@ ; )

uninstall-local:
	rm -fr $(DESTDIR)$(TKHTMLLIB_DESTDIR)
