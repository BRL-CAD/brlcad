# $Id$
noinst_PROGRAMS = \
	beset

AM_CPPFLAGS = ${TCL_CPPFLAGS}

beset_SOURCES = fitness.c beset.c
beset_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}
include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(beset_OBJECTS) \
	$(noinst_PROGRAMS) 
