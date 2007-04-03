# $Id$

bin_PROGRAMS = g-jack jack-g

AM_CPPFLAGS = ${TCL_CPPFLAGS}

g_jack_SOURCES = g-jack.c
g_jack_LDADD = \
	${WDB} ${WDB_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

jack_g_SOURCES = jack-g.c
jack_g_LDADD = \
	${WDB} ${WDB_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_jack_OBJECTS) \
	$(jack_g_OBJECTS) \
	$(bin_PROGRAMS)
