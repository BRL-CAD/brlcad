# $Id$

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
		echo "Configuring $(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/incrTcl/itk/doc/man.macros' -e '/man\.macros/d' $(mandir)/mann/$$manpage > $(mandir)/mann/$$manpage.new ; \
		mv $(mandir)/mann/$$manpage.new $(mandir)/mann/$$manpage ; \
	done

MAINTAINERCLEANFILES = Makefile.in
