#
#  Makefile which merely invokes MAKE properly using the combination of
#  the common Makefiles ../Makefile.defs and ../Makefile.rules
#  and the local Makefile ./Makefile.loc
#
#  This version isn't entirely satisfying yet

.SUFFIXES:
.SUFFIXES:  .zz

SHELL		= /bin/sh

everything:
	make $(MFLAGS) -$(MAKEFLAGS) \
		-f ../Makefile.defs -f Makefile.loc -f ../Makefile.rules

.c.o:
	make $(MFLAGS) -$(MAKEFLAGS) \
		-f ../Makefile.defs -f Makefile.loc -f ../Makefile.rules $@

# Make each explicit parameter, one at a time
.DEFAULT:
	make $(MFLAGS) -$(MAKEFLAGS) \
		-f ../Makefile.defs -f Makefile.loc -f ../Makefile.rules $@
