# $Id$

stuffdir = $(prefix)/pro-engineer/sgi_mips4
stuff_DATA = proe-brl proe-brl.c proe-brl.dll
EXTRA_DIST = $(stuff_DATA) mk.in

install-data-hook:
	${INSTALL_DATA} mk ${stuffdir}/Makefile

# I don't think this works
uninstall-hook:
	${RM} ${stuffdir}/Makefile
