
SUBDIRS = beset
bin_PROGRAMS = \
	g_diff \
	g_lint \
	g_qa \
	remapid

noinst_PROGRAMS = \
	g_transfer

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

g_diff_SOURCES = g_diff.c
g_diff_LDADD = ${RT} ${GED} ${TCLCAD}

g_transfer_SOURCES = g_transfer.c
g_transfer_LDADD = ${RT} ${PKG}

g_lint_SOURCES = g_lint.c
g_lint_LDADD = ${RT}

g_qa_SOURCES = g_qa.c
g_qa_LDADD = ${RT} ${GED}

remapid_SOURCES = remapid.c
remapid_LDADD = ${RT} ${BU}

man_MANS = \
	g_diff.1 \
	g_lint.1 \
	remapid.1

EXTRA_DIST = $(man_MANS) CMakeLists.txt

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
