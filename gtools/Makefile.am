# $Id$

bin_PROGRAMS = g_lint g_diff solshoot testfree

BN=$(top_builddir)/libbn/libbn.la $(BU)
BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm
RT=$(top_builddir)/librt/librt.la $(BN)

g_diff_SOURCES = g_diff.c
g_diff_LDADD = ${RT} @TCL_LIBS@

g_lint_SOURCES = g_lint.c
g_lint_LDADD = ${RT}  

solshoot_SOURCES = solshoot.c
solshoot_LDADD = ${RT} 

testfree_SOURCES = testfree.c
testfree_LDADD = ${RT} 

