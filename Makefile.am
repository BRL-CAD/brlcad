# $Id$

required_dirs = \
	include \
	src

bench_dirs = \
	bench \
	db \
	pix

other_dirs = \
	doc \
	misc \
	sh

if ONLY_BENCHMARK
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs)

else !ONLY_BENCHMARK

if ONLY_RTS
SUBDIRS = \
	$(required_dirs)

else !ONLY_RTS
SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(other_dirs)

endif
# ONLY_RTS

endif
# ONLY_BENCHMARK


DIST_SUBDIRS = \
	$(required_dirs) \
	$(bench_dirs) \
	$(other_dirs)

EXTRA_DIST = \
	AUTHORS \
	BUGS \
	COPYING \
	ChangeLog \
	HACKING \
	INSTALL \
	NEWS \
	README \
	TODO \
	autogen.sh \
	brlcad.spec.in \
	libtool

MAINTAINERCLEANFILES = \
	@CONFIG_CACHE@ \
	Makefile.in \
	aclocal.m4 \
	autom4te.cache \
	brlcad.spec \
	config.log \
	config.status \
	configure \
	libtool


##############
# the rules  #
##############

# using home is probably not a great choice..
.PHONY : rpm
rpm: dist
	@CP@ $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	@CP@ ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm .
	@CP@ ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .

packages: rpm

benchmark: all
	@SH@ $(top_srcdir)/bench/run.sh

bench: benchmark

all-local:
	@@ECHO@
	@@ECHO@ "Done."
	@@ECHO@
	@@ECHO@ @ECHO_N@ "Elapsed compilation time: @ECHO_C@"
	@sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/build_date.h | grep Compilation`
	@@ECHO@ @ECHO_N@ "Elapsed time since configuration: @ECHO_C@"
	@sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@
	@@ECHO@ "---"
	@@ECHO@ "Run 'make install' to begin installation into $(prefix)"
	@@ECHO@ "Run 'make benchmark' to run the BRL-CAD Benchmark Suite"
	@@ECHO@
