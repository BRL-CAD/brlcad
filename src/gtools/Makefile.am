# $Id$

bin_PROGRAMS = \
	g_lint \
	g_diff \
	solshoot \
	testfree

g_diff_SOURCES = g_diff.c
g_diff_LDADD = ${RT} ${TCL}

g_lint_SOURCES = g_lint.c
g_lint_LDADD = ${RT}  

solshoot_SOURCES = solshoot.c
solshoot_LDADD = ${RT} 

testfree_SOURCES = testfree.c
testfree_LDADD = ${RT} 

man_MANS = g_diff.1 g_lint.1
EXTRA_DIST = $(man_MANS)
