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

DISTCLEANFILES = \
	$(CONFIG_CACHE) \
	aclocal.m4 \
	autom4te.cache \
	brlcad.spec \
	config.log \
	config.status \
	configure \
	libtool \
	so_locations

MAINTAINERCLEANFILES = Makefile.in


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


# Print out an informative summary.  As just about everything seems to
# end up calling the all-am hook, which in turns calls the all-local
# hook.  The initial target goal is used to print out a custom summary
# message.  If the make being used doesn't set the MAKECMDGOALS
# variable, something generic is printed still.  For a make install,
# print out rule lines according to the size of the installation path
# to emphasize the achievement.
#
all-local:
	@@ECHO@ "Done."
	@@ECHO@
	@@ECHO@ "BRL-CAD Release @BRLCAD_VERSION@, Build @CONFIG_DATE@"
	@@ECHO@
	@if test "x$(MAKECMDGOALS)" = "xall-am" ; then \
	  @ECHO@ @ECHO_N@ "Elapsed compilation time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  @ECHO@ @ECHO_N@ "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  @ECHO@ "---" ;\
	  @ECHO@ "Run 'make install' to begin installation into $(prefix)" ;\
	  @ECHO@ "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	elif test "x$(MAKECMDGOALS)" = "xinstall-am" ; then \
	  @ECHO@ @ECHO_N@ "Elapsed installation time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  @ECHO@ @ECHO_N@ "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  @ECHO@ "---" ;\
	  @ECHO@ "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	  @ECHO@ ;\
	  line1="  BRL-CAD @BRLCAD_VERSION@ is now installed into $(prefix)" ;\
	  line2="  Be sure to add $(prefix)/bin to your PATH" ;\
	  length="`@ECHO@ $(prefix)@BRLCAD_VERSION@ | wc -c`" ;\
	  length="`expr $$length + 34`" ;\
	  separator="" ;\
	  while test $$length -gt 0  ; do \
	    separator="$${separator}***" ;\
	    length="`expr $$length - 3`" ;\
          done ;\
	  @ECHO@ "$$separator" ;\
	  @ECHO@ "$$line1" ;\
	  @ECHO@ "$$line2" ;\
	  @ECHO@ "$$separator" ;\
	elif test "x$(MAKECMDGOALS)" = "x" ; then \
	  @ECHO@ @ECHO_N@ "Elapsed time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  @ECHO@ @ECHO_N@ "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  @ECHO@ "---" ;\
	  @ECHO@ "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	fi
	@@ECHO@
