# $Id$

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
	$(mann_MANS) \
	license.terms \
	man.macros

install-data-hook:
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/incrTcl/itcl/doc/man.macros' -e '/man\.macros/d' $(mandir)/mann/$$manpage > $(mandir)/mann/$$manpage.new ; \
		mv $(mandir)/mann/$$manpage.new $(mandir)/mann/$$manpage ; \
	done

MAINTAINERCLEANFILES = Makefile.in
