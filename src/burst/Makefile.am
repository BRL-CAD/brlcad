# $Id$
                                                                                                                                            
bin_PROGRAMS = burst

burst_SOURCES = \
	burst.c \
	error.c \
	fb.c \
	glob.c \
	grid.c \
	gridrotate.c \
	Hm.c \
	HmGetc.c \
	HmGlob.c \
	idents.c \
	paint.c \
	plot.c \
	prnt.c \
	Sc.c \
	Tc.c \
	trie.c \
	ui.c

burst_LDADD = @RT@ @FB@ @TERMIO@ @CURSES@

noinst_HEADERS = \
	ascii.h \
	burst.h \
	extern.h \
	Hm.h \
	Mm.h \
	Sc.h \
	Tc.h \
	trie.h \
	vecmath.h

test:	../.burst.MTYPE/burst tfiles/ktank.g
	../.burst.MTYPE/burst -b < tfiles/ktank.in
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	diff tfiles/ktank.burst tfiles/ktank.burst.bak4.0
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	diff tfiles/ktank.shotlines tfiles/ktank.shotlines.bak4.0
	@if test $? -ne 0
	then echo "Test failed." ; exit 1
	fi
	@echo "Tested OK."

untest:
	rm -f \
	tfiles/ktank.burst \
	tfiles.ktank.shotlines \
	tfiles/ktank.plot \
	tfiles/ktank.pix \
	tfiles/ktank.g \
	tfiles/ktank.log

tfiles/ktank.g:
	asc2g < ../burst/tfiles/ktank.asc > tfiles/ktank.g

