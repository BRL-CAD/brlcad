if BUILD_REGEX
noinst_LIBRARIES = libregex.a
endif

libregex_a_SOURCES  =	\
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
