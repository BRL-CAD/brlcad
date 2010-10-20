#                     M A K E F I L E . A M
# BRL-CAD
#
# Copyright (c) 2005-2010 United States Government as represented by
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
	ChangeLog \
	TODO \
	autogen.sh \
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
	  $(ECHO) ; \
	  $(ECHO) "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  $(ECHO) "  WARNING: It seems as though you might not have" ; \
	  $(ECHO) "           installed BRL-CAD yet?" ; \
	  $(ECHO) ; \
	  $(ECHO) "  You should run 'make install' before creating a" ; \
	  $(ECHO) "  binary distribution package.  Trying regardless." ; \
	  $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  $(ECHO) ; \
	fi
	@$(ECHO) "Building an Installer package for Mac OS X"
	@$(ECHO)
	@$(ECHO) "Warning: this package target presently assumes that the install directory"
	@$(ECHO) "         does not contain extra files and comprises a full clean install."
	@$(ECHO)
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
	@$(ECHO) "Building a Disk Image for Mac OS X"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_dmg.sh "$(PACKAGE_NAME)" "$(BRLCAD_VERSION)" "$(top_srcdir)/misc/macosx/background.png"

# using home is probably not a great choice..
$(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm: $(PACKAGE)-$(BRLCAD_VERSION).tar.gz
	@$(ECHO) "Building binary RPM for RedHat Linux"
	@$(ECHO)
	@CP@ $(PACKAGE)-$(BRLCAD_VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(top_builddir)/misc/brlcad.spec
	@CP@ ${HOME}/rpm/RPMS/i386/$(PACKAGE_NAME)_$(BRLCAD_VERSION)_*.i386.rpm .
	@touch $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm
	@$(ECHO) "WARNING: Fake make placeholder created as $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm"
	@$(ECHO)

# using home is probably not a great choice..
# use PACKAGE instead of PACKAGE_NAME because it's a source release
$(PACKAGE)-$(BRLCAD_VERSION).src.rpm: $(PACKAGE)-$(BRLCAD_VERSION).tar.gz
	@$(ECHO) "Building source RPM for RedHat Linux"
	@CP@ $(PACKAGE)-$(BRLCAD_VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(top_builddir)/misc/brlcad.spec
	@CP@ ${HOME}/rpm/SRPMS/$(PACKAGE)-$(BRLCAD_VERSION)-*.src.rpm .
	@touch $(PACKAGE)-$(BRLCAD_VERSION).src.rpm
	@$(ECHO) "WARNING: Fake make placeholder created as $(PACKAGE)-$(BRLCAD_VERSION).src.rpm"
	@$(ECHO)

$(PACKAGE_NAME)_$(BRLCAD_VERSION).deb:
	@$(ECHO) "Building a package for Debian Linux"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_deb.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"

$(PACKAGE_NAME)_$(BRLCAD_VERSION).zip:
	@if ! test -f "install.$(host_triplet).log" ; then \
	  $(ECHO) ; \
	  $(ECHO) "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  $(ECHO) "  WARNING: It seems as though you might not have" ; \
	  $(ECHO) "           installed BRL-CAD yet?" ; \
	  $(ECHO) ; \
	  $(ECHO) "  You should run 'make install' before creating a" ; \
	  $(ECHO) "  binary distribution package.  Trying regardless." ; \
	  $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  $(ECHO) ; \
	fi
	@$(ECHO) "Building a zip-compressed file archive"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_zip.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).zip for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@$(ECHO) "Building a gzip-compressed tape archive"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_tgz.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@$(ECHO) "Building a bzip2-compressed tape archive"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_bz2.sh "$(PACKAGE_NAME)_$(BRLCAD_VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2 for $(host_triplet)"

 $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar:
	@if ! test -f "install.$(host_triplet).log" ; then \
	  $(ECHO) ; \
	  $(ECHO) "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	  $(ECHO) "  WARNING: It seems as though you might not have" ; \
	  $(ECHO) "           installed BRL-CAD yet?" ; \
	  $(ECHO) ; \
	  $(ECHO) "  You should run 'make install' before creating a" ; \
	  $(ECHO) "  binary distribution package.  Trying regardless." ; \
	  $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	  $(ECHO) ; \
	fi
	@$(ECHO) "Building an uncompressed tape archive"
	@$(ECHO)
	@${SH} $(top_srcdir)/sh/make_tar.sh "$(PACKAGE_NAME)" "$(BRLCAD_VERSION)" "$(DESTDIR)$(prefix)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar for $(host_triplet)"


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
	@$(ECHO) "Done."

dmg: pkg $(PACKAGE_NAME)_$(BRLCAD_VERSION).dmg
	@$(ECHO) "Done."

rpm: $(PACKAGE_NAME)_$(BRLCAD_VERSION).rpm $(PACKAGE)-$(BRLCAD_VERSION).src.rpm
	@$(ECHO) "Done."

deb: $(PACKAGE_NAME)_$(BRLCAD_VERSION).deb
	@$(ECHO) "Done."

zip: $(PACKAGE_NAME)_$(BRLCAD_VERSION).zip
	@$(ECHO) "Done."

tgz: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.gz
	@$(ECHO) "Done."

bz2: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar.bz2
	@$(ECHO) "Done."

tar: $(PACKAGE_NAME)_$(BRLCAD_VERSION).tar
	@$(ECHO) "Done."

package:
	@$(ECHO)
	@$(ECHO) "To build a BRL-CAD package for distribution, there are various make"
	@$(ECHO) "targets available for various platforms:"
	@$(ECHO)
	@$(ECHO) "  make pkg	-	Builds an Installer package for Mac OS X"
	@$(ECHO) "  make dmg	-	Builds a Disk Image for Mac OS X"
	@$(ECHO) "  make rpm	-	Builds source and binary RPM for RedHat Linux"
	@$(ECHO) "  make deb	-	Builds a package for Debian Linux"
	@$(ECHO) "  make zip	-	Builds a zip-compressed file archive"
	@$(ECHO) "  make tgz	-	Builds a gzip-compressed tape archive"
	@$(ECHO) "  make bz2	-	Builds a bzip2-compressed tape archive"
	@$(ECHO) "  make tar	-	Builds a uncompressed tape archive"
	@$(ECHO)

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
	    $(ECHO) ; \
	    $(ECHO) "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	    $(ECHO) "  WARNING: Too many of the pkgIndex.tcl and" ; \
	    $(ECHO) "           tclIndex files are empty." ; \
	    $(ECHO) ; \
	    $(ECHO) "  It is likely that the indices are not being" ; \
	    $(ECHO) "  generated properly which will affect run-time" ; \
	    $(ECHO) "  functionality. VERIFY THE FILES." ; \
	    $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	    $(ECHO) ; \
	fi ; \
	if test "x$$warn_umask" = "xyes" ; then \
	    $(ECHO) ; \
	    $(ECHO) "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}" ; \
	    $(ECHO) "  WARNING: There is no execute permission on the" ; \
	    $(ECHO) "           install directory." ; \
	    $(ECHO) ; \
	    $(ECHO) "  Consider running \"umask 022\" before installing" ; \
	    $(ECHO) "  next time.  You should be able to repair this"; \
	    $(ECHO) "  installation's directory permissions with: " ; \
	    $(ECHO) ; \
	    $(ECHO) "    find $(DESTDIR)$(prefix)/. -type d -exec chmod go+x {} \;" ; \
	    if test -d "$(DESTDIR)$(bindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/bin" != "x$(DESTDIR)$(bindir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(bindir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(sbindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/sbin" != "x$(DESTDIR)$(sbindir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(sbindir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libexecdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/libexec" != "x$(DESTDIR)$(libexecdir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(libexecdir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(datadir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/share" != "x$(DESTDIR)$(datadir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(prefix)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/lib" != "x$(DESTDIR)$(libdir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(libdir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(mandir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/man" != "x$(DESTDIR)$(mandir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(mandir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(includedir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/include" != "x$(DESTDIR)$(includedir)" ; then \
		$(ECHO) "    find $(DESTDIR)$(includedir)/. -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    $(ECHO) ; \
	    $(ECHO) "  Do not presume the find example is all that is" ; \
	    $(ECHO) "  needed.  VERIFY THE PERMISSIONS." ; \
	    $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	    $(ECHO) ; \
	fi ; \
	touch install.$(host_triplet).log ; \
	if test -w install.$(host_triplet).log ; then \
	    LC_ALL=C ; \
	    if test "x$USER" = "x" ; then \
		USER="unknown" ; \
	    fi ; \
	    $(ECHO) "Installed BRL-CAD Release ${BRLCAD_VERSION} Build ${CONFIG_DATE} by ${USER} to $(DESTDIR)$(prefix) on `date`" >> install.$(host_triplet).log ; \
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
	@$(ECHO) "Done."
	@$(ECHO)
	@$(ECHO) "BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE}"
	@$(ECHO)
	@if test "x$(MAKECMDGOALS)" = "xall-am" -o "x$(.TARGETS)" = "xall-am" -o "x$(MAKECMDGOALS)" = "xfast" -o "x$(.TARGETS)" = "xfast" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed compilation time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'make install' to begin installation into $(prefix)" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	elif test "x$(MAKECMDGOALS)" = "xinstall-am" -o "x$(.TARGETS)" = "xinstall-am" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed installation time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'make test' to run the BRL-CAD Test Suite" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	  $(ECHO) ;\
	  line1="  BRL-CAD ${BRLCAD_VERSION} is now installed into $(prefix)" ;\
	  line2="  Be sure to add $(prefix)/bin to your PATH" ;\
	  separator="`$(ECHO) $$line1 | tr '[[:print:]]' '*'`" ;\
	  $(ECHO) "$${separator}****" ;\
	  $(ECHO) "$$line1" ;\
	  $(ECHO) "$$line2" ;\
	  $(ECHO) "$${separator}****" ;\
	elif test "x$(MAKECMDGOALS)" = "x" -a "x$(.TARGETS)" = "x" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed time: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/conf/TS` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  ${SH} $(top_srcdir)/sh/elapsed.sh ${CONFIG_TS} ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	fi
	@$(ECHO)


include $(top_srcdir)/misc/Makefile.defs

# Local Variables:
# mode: Makefile
# tab-width: 8
# indent-tabs-mode: t
# End:
# ex: shiftwidth=8 tabstop=8
