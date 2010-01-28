
mann_MANS = \
	Archetype.n \
	Toplevel.n \
	Widget.n \
	itk.n \
	itkvars.n \
	usual.n

EXTRA_DIST = \
	$(mann_MANS) \
	license.terms \
	man.macros

install-data-hook:
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/incrTcl/itk/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/mann/$$manpage > $(DESTDIR)$(mandir)/mann/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/mann/$$manpage.new $(DESTDIR)$(mandir)/mann/$$manpage ; \
	done

include $(top_srcdir)/misc/Makefile.defs

