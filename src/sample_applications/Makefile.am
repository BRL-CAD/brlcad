# $Id$

stuffdir = $(prefix)/sample_applications
 
stuff_DATA = debug.h $(top_srcdir)/src/conv/g-xxx.c $(top_srcdir)/src/conv/g-xxx_facets.c pl-dm.c rtexample.c
EXTRA_DIST = $(stuff_DATA) mk.in

install-data-hook:
	$(stuffDATA_INSTALL) mk $(DESTDIR)$(stuffdir)/makefile

# I don't think this works
uninstall-hook:
	${RM} ${stuffdir}/makefile
