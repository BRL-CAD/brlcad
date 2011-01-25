
man3_MANS = \
	Preserve.3 \
	Class.3 \
	Object.3 \
	List.3 \
	RegisterC.3 \
	Stack.3

mann_MANS = \
	body.n \
	class.n \
	code.n \
	configbody.n \
	delete.n \
	ensemble.n \
	find.n \
	is.n \
	itcl.n \
	itclvars.n \
	local.n \
	scope.n

EXTRA_DIST = \
	$(man3_MANS) \
	$(mann_MANS) \
	license.terms \
	man.macros

install-data-hook:
	@for manpage in $(man3_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/man3/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/incrTcl/itcl/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/man3/$$manpage > $(DESTDIR)$(mandir)/man3/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/man3/$$manpage.new $(DESTDIR)$(mandir)/man3/$$manpage ; \
	done
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/incrTcl/itcl/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/mann/$$manpage > $(DESTDIR)$(mandir)/mann/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/mann/$$manpage.new $(DESTDIR)$(mandir)/mann/$$manpage ; \
	done

include $(top_srcdir)/misc/Makefile.defs

