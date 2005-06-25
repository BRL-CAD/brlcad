# $Id$

lib_LTLIBRARIES = libregex.la

libregex_la_SOURCES  =	\
	regcomp.c	\
	regerror.c	\
	regexec.c	\
	regfree.c

noinst_HEADERS = \
	cclass.h	\
	cname.h		\
	regex2.h	\
	utils.h

EXTRA_DIST =		\
	engine.c

MAINTAINERCLEANFILES = \
	Makefile.in

include $(top_srcdir)/misc/Makefile.defs

