# $Id$

#
# DO NOT EDIT -- THIS DIRECTORY IS GOING AWAY
# put/leave your example code with related code (e.g. rtexample in src/rt)
#

stuffdir = $(SAMPLE_APPLICATIONS_DIR)
 
stuff_DATA = \
	debug.h \
	pl-dm.c

EXTRA_DIST = \
	$(stuff_DATA) \
	mk.in

install-data-hook:
	$(stuffDATA_INSTALL) mk $(DESTDIR)$(stuffdir)/makefile

# I don't think this works
uninstall-hook:
	${RM} ${stuffdir}/makefile
