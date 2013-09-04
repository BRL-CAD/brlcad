# Makefile for popt library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT =		splint
MCCABE =	pmccabe

EXTRA_DIST = config.rpath lookup3.c autogen.sh CHANGES $(man_MANS) \
	m4/Makefile.in \
	footer_no_timestamp.html libpopt.vers \
	testit.sh test-poptrc \
	popt.xcodeproj/project.pbxproj \
	popt.ps

SUBDIRS = po . auto

AM_CPPFLAGS = -I. -I$(top_srcdir)

noinst_HEADERS = poptint.h system.h

noinst_PROGRAMS = test1 test2 tdict # test3
test1_SOURCES = test1.c
test1_LDFLAGS = 
test1_LDADD = $(usrlib_LTLIBRARIES)
test2_SOURCES = test2.c
test2_LDFLAGS = 
test2_LDADD = $(usrlib_LTLIBRARIES)
#test3_SOURCES = test3.c
#test3_LDFLAGS = 
#test3_LDADD = $(usrlib_LTLIBRARIES)
tdict_SOURCES = tdict.c
tdict_LDFLAGS = 
tdict_LDADD = $(usrlib_LTLIBRARIES)

noinst_SCRIPTS = testit.sh

TESTS_ENVIRONMENT = \
test1="$(top_builddir)/test1"

TESTS = $(top_srcdir)/testit.sh

include_HEADERS = popt.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = libpopt.la

libpopt_la_SOURCES = popt.c poptparse.c poptconfig.c popthelp.c poptint.c
libpopt_la_LDFLAGS = -no-undefined @LTLIBINTL@ @LTLIBICONV@

pkgconfigdir = $(prefix)/lib/pkgconfig
pkgconfig_DATA = popt.pc

if HAVE_LD_VERSION_SCRIPT
libpopt_la_LDFLAGS += -Wl,--version-script=$(top_srcdir)/libpopt.vers
endif

man_MANS = popt.3

BUILT_SOURCES = popt.pc # popt.lcd

distclean-local:
	rm -rf .ccache

.PHONY:	updatepo
updatepo:
	rsync -Lrtvz  translationproject.org::tp/latest/popt/  po

popt.lcd: Makefile.am ${libpopt_la_SOURCES} ${include_HEADERS} ${noinst_HEADERS}
	lclint -dump $@ ${libpopt_la_SOURCES}

.PHONY:	sources
sources:
	@echo $(libpopt_la_SOURCES:%=popt/%)

.PHONY: lint
lint:
	$(LINT) ${DEFS} ${INCLUDES} test1.c ${libpopt_la_SOURCES}

.PHONY:	mccabe
mccabe:
	$(MCCABE) $(libpopt_la_SOURCES) | sort -n -r | head -n 10

.PHONY: doxygen
doxygen: Doxyfile
	rm -rf doxygen
	mkdir -p doxygen
	doxygen

.PHONY:	lcov-reset	# run lcov from scratch, always
lcov-reset:
	make lcov-run
	make lcov-report

.PHONY:	lcov		# run lcov from scratch if the dir is not there
lcov:
	make lcov-reset

.PHONY:	lcov-run	# reset run coverage tests
lcov-run:
	@-rm -rf lcov
	find . -name "*.gcda" -exec rm {} \;
	make check

.PHONY:	lcov-report	# generate report based on current coverage data
lcov-report:
	mkdir lcov
	lcov --directory . --capture --output-file lcov/lcov.info
	lcov -l lcov/lcov.info | grep -v "`cd $(top_srcdir) && pwd`" | cut -d: -f1 > lcov/remove
	lcov -r lcov/lcov.info `cat lcov/remove` > lcov/lcov.cleaned.info
	rm lcov/remove
	mv lcov/lcov.cleaned.info lcov/lcov.info
	genhtml -t "$(PACKAGE_STRING)" -o lcov lcov/lcov.info

#.PHONY:	lcov-upload
#lcov-upload: lcov
#	rsync -rvz -e ssh --delete lcov/* ???

ACLOCAL_AMFLAGS = -I m4
