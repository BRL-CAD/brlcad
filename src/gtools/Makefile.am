
SUBDIRS = beset
bin_PROGRAMS = \
	g_diff \
	g_lint \
	g_qa \
	remapid

noinst_PROGRAMS = \
	g_transfer

AM_CPPFLAGS = \
	${STD_C99_FLAGS} \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

g_diff_SOURCES = g_diff.c
g_diff_LDADD = ${TCLCAD} ${RT}

g_transfer_SOURCES = g_transfer.c
g_transfer_LDADD = ${RT} ${PKG}

g_lint_SOURCES = g_lint.c
g_lint_LDADD = ${RT}

g_qa_SOURCES = g_qa.c
g_qa_LDADD = ${GED} ${RT}

remapid_SOURCES = remapid.c
remapid_LDADD = ${RT} ${BU}


EXTRA_DIST = CMakeLists.txt

sample_applicationsdir=$(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = g_transfer.c

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_diff_OBJECTS) \
	$(g_lint_OBJECTS) \
	$(g_qa_OBJECTS) \
	$(remapid_OBJECTS) \
	$(noinst_PROGRAMS) \
	$(bin_PROGRAMS)
