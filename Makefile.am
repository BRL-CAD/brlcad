#                     M A K E F I L E . A M
# BRL-CAD
#
# Copyright (c) 2005-2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

AUTOMAKE_OPTIONS = 1.6 dist-zip dist-bzip2
ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-all

required_dirs = \
	include \
	src

bench_dirs = \
	bench \
	db \
	pix

other_dirs = \
	doc \
	m4 \
	misc \
	regress \
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

documentationdir = $(BRLCAD_DATA)

documentation_DATA = \
	AUTHORS \
	COPYING \
	HACKING \
	INSTALL \
	NEWS \
	README

EXTRA_DIST = \
	$(documentation_DATA) \
	BUGS \
	CMakeLists.txt \
	ChangeLog \
	HACKING.cmake \
	INSTALL.cmake \
	README.cmake \
	TODO \
	TODO.cmake \
	autogen.sh \
	configure.cmake.sh \
	libtool

DISTCLEANFILES = \
	$(CONFIG_CACHE) \
	aclocal.m4 \
	config.log \
	config.status \
	install.$(host_triplet).log \
	libtool \
	so_locations


##########################
# packaged distributions #
##########################

$(PACKAGE)-$(BRLCAD_VERSION).tar.gz: dist

# Build a Mac OS X Installer .pkg -- make sure the package resources
# reflect the correct version.  we specifically do not use an autoconf
# template so that the file may be edited without doing a configure
# (which would also cause a need to regenerate the template to keep in
# sync every time it's edited).
$(PACKAGE_NAME)_$(BRLCAD_VERSION).pkg:
	@if ! test -f "install.$(host_triplet).log" ; then \
	  echo ; \
	  echo "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  echo "  WARNING: It seems as though you might not have" ; \
	  echo "           installed BRL-CAD yet?" ; \
	  echo ; \
	  echo "  You should run 'make install' before creating a" ; \
	  echo "  binary distribution package.  Trying regardless." ; \
	  echo "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  echo ; \
	fi
	@echo "Building an Installer package for Mac OS X"
	@echo
	@echo "Warning: this package target presently assumes that the install directory"
	@echo "         does not contain extra files and comprises a full clean install."
	@echo
	@if test -f "$(top_srcdir)/misc/macosx/Resources/ReadMe.rtfd/TXT.rtf" ; then \
	  perl -pi -e "s/BRL-CAD [0-9]*\.[0-9]*\.[0-9]*/BRL-CAD $(BRLCAD_VERSION)/g" $(top_srcdir)/misc/macosx/Resources/ReadMe.rtfd/TXT.rtf ; \
	fi
	@if test -f "$(top_srcdir)/misc/macosx/Resources/Welcome.rtfd/TXT.rtf" ; then \
	  perl -pi -e "s/BRL-CAD [0-9]*\.[0-9]*\.[0-9]*/BRL-CAD $(BRLCAD_VERSION)/g" $(top_srcdir)/misc/macosx/Resources/Welcome.rtfd/TXT.rtf ; \
	fi
	@${SH} $(top_srcdir)/sh/make_pkg.sh "$(PACKAGE_NAME)" "$(BRLCAD_VERSION)" "$(DESTDIR)$(prefix)" "$(top_srcdir)/misc/macosx/Resources"

openUp:
	cd misc/macosx && $(MAKE) openUp

$(PACKAGE_NAME)_$(BRLCAD_VERSION).dmg: $(PACKAGE_NAME)_$(BRLCAD_VERSION).pkg openUp
	@echo "Building a Disk Image for Mac OS X"
	@echo
	@${SH} $(top_srcdir)/sh/make_dmg.sh "$(PACKAGE_NAME)" "$(BRLCAD_VERSION)" "$(top_srcdir)/misc/macosx/background.png"

# using home is probably not a great choice..
$(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm: $(PACKAGE)-$(BRLCAD_VERSION).tar.gz
	@echo "Building binary RPM for RedHat Linux"
	@echo
	@CP@ $(PACKAGE)-$(BRLCAD_VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(top_builddir)/misc/brlcad.spec
	@CP@ ${HOME}/rpm/RPMS/i386/$(PACKAGE_NAME)_$(BRLCAD_VERSION)_*.i386.rpm .
	@touch $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm
	@echo "WARNING: Fake make placeholder created as $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm"
	@echo

# using home is probably not a great choice..
# use PACKAGE instead of PACKAGE_NAME because it's a source release
$(PACKAGE)-$(BRLCAD_VERSION).src.rpm: $(PACKAGE)-$(BRLCAD_VERSION).tar.gz
	@echo "Building source RPM for RedHat Linux"
	@CP@ $(PACKAGE)-$(BRLCAD_VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(top_builddir)/misc/brlcad.spec
	@CP@ ${HOME}/rpm/SRPMS/$(PACKAGE)-$(BRLCAD_VERSION)-*.src.rpm .
	@touch $(PACKAGE)-$(BRLCAD_VERSION).src.rpm
	@echo "WARNING: Fake make placeholder created as $(PACKAGE)-$(BRLCAD_VERSION).src.rpm"
	@echo

$(PACKAGE_NAME)_$(BRLCAD_VERSION).deb:
	@echo "Building a package for Debian Linux"
	@echo
	@${SH} $(top_srcdir)/sh/make_deb.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"

$(PACKAGE_NAME)_$(BRLCAD_VERSION).zip:
	@if ! test -f "install.$(host_triplet).log" ; then \
	  echo ; \
	  echo "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  echo "  WARNING: It seems as though you might not have" ; \
	  echo "           installed BRL-CAD yet?" ; \
	  echo ; \
	  echo "  You should run 'make install' before creating a" ; \
	  echo "  binary distribution package.  Trying regardless." ; \
	  echo "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  echo ; \
	fi
	@echo "Building a zip-compressed file archive"
	@echo
	@${SH} $(top_srcdir)/sh/make_zip.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@echo
	@echo "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).zip for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@echo "Building a gzip-compressed tape archive"
	@echo
	@${SH} $(top_srcdir)/sh/make_tgz.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@echo
	@echo "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@echo "Building a bzip2-compressed tape archive"
	@echo
	@${SH} $(top_srcdir)/sh/make_bz2.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@echo
	@echo "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2 for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar:
	@if ! test -f "install.$(host_triplet).log" ; then \
	  echo ; \
	  echo "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  echo "  WARNING: It seems as though you might not have" ; \
	  echo "           installed BRL-CAD yet?" ; \
	  echo ; \
	  echo "  You should run 'make install' before creating a" ; \
	  echo "  binary distribution package.  Trying regardless." ; \
	  echo "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  echo ; \
	fi
	@echo "Building an uncompressed tape archive"
	@echo
	@${SH} $(top_srcdir)/sh/make_tar.sh "$(PACKAGE_NAME)" "$(BRLCAD_VERSION)" "$(DESTDIR)$(prefix)"
	@echo
	@echo "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar for $(host_triplet)"


#############
# the rules #
#############

.PHONY : clobber benchmark bench regression regress test pkg dmg rpm deb zip tgz tbz

clobber:
	@cd bench && $(MAKE) clobber

benchmark: all
	@cd bench && $(MAKE) run

bench: benchmark

regression: all
	@cd regress && $(MAKE) regression

dox : misc/Doxyfile misc/doxygen_structure
	doxygen $(top_srcdir)/misc/Doxyfile

regress: regression

test: regression

check: regression
	@TIMEFRAME=1 $(MAKE) benchmark

pkg: $(PACKAGE_NAME)_$(BRLCAD_VERSION).pkg
	@echo "Done."

dmg: pkg $(PACKAGE_NAME)_$(BRLCAD_VERSION).dmg
	@echo "Done."

rpm: $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm $(PACKAGE)-$(BRLCAD_VERSION).src.rpm
	@echo "Done."

deb: $(PACKAGE_NAME)_$(BRLCAD_VERSION).deb
	@echo "Done."

zip: $(PACKAGE_NAME)_$(BRLCAD_VERSION).zip
	@echo "Done."

tgz: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz
	@echo "Done."

bz2: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2
	@echo "Done."

tar: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@echo "Done."

package:
	@echo
	@echo "To build a BRL-CAD package for distribution, there are various make"
	@echo "targets available for various platforms:"
	@echo
	@echo "  make pkg	-	Builds an Installer package for Mac OS X"
	@echo "  make dmg	-	Builds a Disk Image for Mac OS X"
	@echo "  make rpm	-	Builds source and binary RPM for RedHat Linux"
	@echo "  make deb	-	Builds a package for Debian Linux"
	@echo "  make zip	-	Builds a zip-compressed file archive"
	@echo "  make tgz	-	Builds a gzip-compressed tape archive"
	@echo "  make bz2	-	Builds a bzip2-compressed tape archive"
	@echo "  make tar	-	Builds a uncompressed tape archive"
	@echo

# make sure all files are in the repository are also in the dist.
# make sure files that should not be in the distribution in fact are
# not included.
dist-hook:
	files="`find . -name entries -exec grep 'name=' {} /dev/null \; | sed 's/:[[:space:]]*name=\"\(.*\)\"/\1/g' | sed 's/\/\.svn\/entries/\//g' | grep -v 'brlcad-' | grep -v '\/$$'`" ; \
	missing=0 ; \
	for file in $$files ; do \
		if test ! -e $(distdir)/$$file ; then \
			missing=1 ; \
			echo "MISSING FROM DIST: $$file" ; \
		fi ; \
	done ; \
	if test "x$$missing" = "x1" ; then \
		exit 1 ; \
	fi
	files="`find $(distdir)/src/tclscripts \( -name pkgIndex.tcl -o -name tclIndex \)`" ; \
	empty=`for file in $$files ; do found="\`grep -v '^#' $$file | perl -0777 -pi -e 's/^\n//g'\`" ; if test "x$$found" = "x" ; then echo $$file ; fi ; done | wc | awk '{print $$1}'` ; \
	if test $$empty -gt 9 ; then \
		echo "TOO MANY EMPTY PKGINDEX.TCL/TCLINDEX FILES" ; \
		exit 1 ; \
	fi
	find $(distdir) -type f \( -name '.cvsignore' -or -name 'brlcad_config.h' \) -exec rm -f {} \;
	find $(distdir) -type d \( -name 'CVS' -or -name '.svn' \) -prune -exec rm -rf {} \;
	dir="`pwd`" ; cd $(top_srcdir) ; \
	${SH} sh/cmakecheck.sh ; \
	ret=$$? ; \
	if test "x$$ret" != "x0" ; then \
		echo "NEED TO SYNC CMAKELISTS.TXT" ; \
		exit 1 ; \
	fi ; \
	cd "$$dir"

# don't complain about the benchmark 'summary', we don't want to delete it
distcleancheck_listfiles = \
	find . -type f -not -name summary -print


##########################
# summaries and warnings #
##########################

# make sure a dangerous umask will not prevent others from using an
# installed version by checking to make sure the exec mode is set.
# We minimally check only the binary and prefix directories.
install-data-local:
	@warn_umask=no ; \
	case `ls -ld $(DESTDIR)$(prefix)` in \
	  d????????-*) warn_umask=yes ;; \
	  d?????-???*) warn_umask=yes ;; \
	  d??-??????*) warn_umask=yes ;; \
	esac ; \
	if test "x$$warn_umask" = "xno" ; then \
	  case `ls -ld $(DESTDIR)$(bindir)` in \
	    d????????-*) warn_umask=yes ;; \
	    d?????-???*) warn_umask=yes ;; \
	    d??-??????*) warn_umask=yes ;; \
	  esac ; \
	fi ; \
	files="`find $(top_builddir)/src/tclscripts \( -name pkgIndex.tcl -o -name tclIndex \)`" ; \
	empty=`for file in $$files ; do found="\`grep -v '^#' $$file | perl -0777 -pi -e 's/^\n//g'\`" ; if test "x$$found" = "x" ; then echo $$file ; fi ; done | wc | awk '{print $$1}'` ; \
	if test $$empty -gt 9 ; then \
	    echo ; \
	    echo "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	    echo "  WARNING: Too many of the pkgIndex.tcl and" ; \
	    echo "           tclIndex files are empty." ; \
	    echo ; \
	    echo "  It is likely that the indices are not being" ; \
	    echo "  generated properly which will affect run-time" ; \
	    echo "  functionality. VERIFY THE FILES." ; \
	    echo "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	    echo ; \
	fi ; \
	if test "x$$warn_umask" = "xyes" ; then \
	    echo ; \
	    echo "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	    echo "  WARNING: There is no execute permission on the" ; \
	    echo "           install directory." ; \
	    echo ; \
	    echo "  Consider running \"umask 022\" before installing" ; \
	    echo "  next time.  You should be able to repair this"; \
	    echo "  installation's directory permissions with: " ; \
	    echo ; \
	    echo "    find $(DESTDIR)$(prefix)/. -type d -exec chmod go+x {} \;" ; \
	    if test -d "$(DESTDIR)$(bindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/bin" != "x$(DESTDIR)$(bindir)" ; then \
		echo "    find $(DESTDIR)$(bindir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(sbindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/sbin" != "x$(DESTDIR)$(sbindir)" ; then \
		echo "    find $(DESTDIR)$(sbindir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libexecdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/libexec" != "x$(DESTDIR)$(libexecdir)" ; then \
		echo "    find $(DESTDIR)$(libexecdir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(datadir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/share" != "x$(DESTDIR)$(datadir)" ; then \
		echo "    find $(DESTDIR)$(prefix)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/lib" != "x$(DESTDIR)$(libdir)" ; then \
		echo "    find $(DESTDIR)$(libdir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(mandir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/man" != "x$(DESTDIR)$(mandir)" ; then \
		echo "    find $(DESTDIR)$(mandir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(includedir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/include" != "x$(DESTDIR)$(includedir)" ; then \
		echo "    find $(DESTDIR)$(includedir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    echo ; \
	    echo "  Do not presume the find example is all that is" ; \
	    echo "  needed.  VERIFY THE PERMISSIONS." ; \
	    echo "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	    echo ; \
	fi ; \
	touch install.$(host_triplet).log ; \
	if test -w install.$(host_triplet).log ; then \
	    LC_ALL=C ; \
	    if test "x$USER" = "x" ; then \
		USER="unknown" ; \
	    fi ; \
	    echo "Installed BRL-CAD Release ${BRLCAD_VERSION} Build ${CONFIG_DATE} by ${USER} to $(DESTDIR)$(prefix) on `date`" >> install.$(host_triplet).log ; \
	    chmod ugo+rw install.$(host_triplet).log ; \
	fi


# Print out an informative summary.  As just about everything seems to
# end up calling the all-am hook, which in turns calls the all-local
# hook.  The initial target goal is used to print out a custom summary
# message.  If the make being used doesn't set the MAKECMDGOALS
# variable, something generic is printed still.  For a make install,
# print out rule lines according to the size of the installation path
# to emphasize the achievement.
#
all-local:
	@echo "Done."
	@echo
	@echo "BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE}"
	@echo
	@if test "x$(MAKECMDGOALS)" = "xall-am" -o "x$(.TARGETS)" = "xall-am" -o "x$(MAKECMDGOALS)" = "xfast" -o "x$(.TARGETS)" = "xfast" ; then \
	  echo $(ECHO_N) "Elapsed compilation time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  echo $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  echo "---" ;\
	  echo "Run 'make install' to begin installation into $(prefix)" ;\
	  echo "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	elif test "x$(MAKECMDGOALS)" = "xinstall-am" -o "x$(.TARGETS)" = "xinstall-am" ; then \
	  echo $(ECHO_N) "Elapsed installation time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  echo $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  echo "---" ;\
	  echo "Run 'make test' to run the BRL-CAD Test Suite" ;\
	  echo "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	  echo ;\
	  line1="  BRL-CAD ${BRLCAD_VERSION} is now installed into $(prefix)" ;\
	  line2="  Be sure to add $(prefix)/bin to your PATH" ;\
	  separator="`echo $$line1 | tr '[[:print:]]' '*'`" ;\
	  echo "$${separator}****" ;\
	  echo "$$line1" ;\
	  echo "$$line2" ;\
	  echo "$${separator}****" ;\
	elif test "x$(MAKECMDGOALS)" = "x" -a "x$(.TARGETS)" = "x" ; then \
	  echo $(ECHO_N) "Elapsed time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  echo $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  echo "---" ;\
	  echo "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	fi
	@echo


include $(top_srcdir)/misc/Makefile.defs

# Local Variables:
# mode: Makefile
# tab-width: 8
# indent-tabs-mode: t
# End:
# ex: shiftwidth=8 tabstop=8
