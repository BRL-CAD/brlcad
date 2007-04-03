# $Id$

bin_PROGRAMS = \
	g_diff \
	g_lint \
	g_qa \
	solshoot \
	testfree

noinst_PROGRAMS = \
	g_transfer

AM_CPPFLAGS = ${TCL_CPPFLAGS}

g_diff_SOURCES = g_diff.c
g_diff_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${TCL} \
	${SYSV} ${SYSV_LIBS}

g_transfer_SOURCES = g_transfer.c
g_transfer_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS} \
	${SYSV} ${SYSV_LIBS}

g_lint_SOURCES = g_lint.c
g_lint_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

g_qa_SOURCES = g_qa.c
g_qa_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

solshoot_SOURCES = solshoot.c
solshoot_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

testfree_SOURCES = testfree.c
testfree_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

man_MANS = \
	g_diff.1 \
	g_lint.1 \
	g_qa.1

EXTRA_DIST = $(man_MANS)

sample_applicationsdir=$(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = g_transfer.c

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_lint_OBJECTS) \
	$(g_diff_OBJECTS) \
	$(solshoot_OBJECTS) \
	$(testfree_OBJECTS) \
	$(noinst_PROGRAMS) \
	$(bin_PROGRAMS)
