# $Id$

AUTOMAKE_OPTIONS = 1.6 dist-zip dist-bzip2
ACLOCAL_AMFLAGS = -I m4

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
	brlcad.spec \
	config.log \
	config.status \
	configure \
	install.$(host_triplet) \
	libtool \
	so_locations

MAINTAINERCLEANFILES = Makefile.in


##########################
# packaged distributions #
##########################

$(PACKAGE)-$(VERSION).tar.gz: dist

$(PACKAGE_NAME)-$(VERSION).pkg:
	@if ! test -f "install.$(host_triplet)" ; then \
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
	@@SH@ sh/make_pkg.sh "$(PACKAGE_NAME)" "$(MAJOR_VERSION)" "$(MINOR_VERSION)" "$(PATCH_VERSION)" "$(DESTDIR)$(prefix)" "misc/macosx/Resources"

$(PACKAGE_NAME)-$(VERSION).dmg: $(PACKAGE_NAME)-$(VERSION).pkg
	@$(ECHO) "Building a Disk Image for Mac OS X"
	@$(ECHO)
	@@SH@ sh/make_dmg.sh "$(PACKAGE_NAME)-$(VERSION)"

# using home is probably not a great choice..
$(PACKAGE)-$(VERSION).rpm: $(PACKAGE)-$(VERSION).tar.gz
	@$(ECHO) "Building binary RPM for RedHat Linux"
	@$(ECHO)
	@CP@ $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	@CP@ ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm .
	@touch $(PACKAGE)-$(VERSION).rpm
	@$(ECHO) "WARNING: Fake make placeholder created as $(PACKAGE)-$(VERSION).rpm"
	@$(ECHO)

# using home is probably not a great choice..
$(PACKAGE)-$(VERSION).src.rpm: $(PACKAGE)-$(VERSION).tar.gz
	@$(ECHO) "Building source RPM for RedHat Linux"
	@CP@ $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	@CP@ ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .
	@touch $(PACKAGE)-$(VERSION).src.rpm
	@$(ECHO) "WARNING: Fake make placeholder created as $(PACKAGE)-$(VERSION).src.rpm"
	@$(ECHO)

$(PACKAGE)-$(VERSION).deb:
	@$(ECHO) "Building a package for Debian Linux"
	@$(ECHO)
	@@SH@ sh/make_deb.sh "$(PACKAGE_NAME)-$(VERSION)"

$(PACKAGE)-$(VERSION).bin.zip:
	@if ! test -f "install.$(host_triplet)" ; then \
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
	@@SH@ sh/make_zip.sh "$(PACKAGE_NAME)-$(VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)-$(VERSION).bin.zip for $(host_triplet)"

 $(PACKAGE)-$(VERSION).bin.tar.gz: $(PACKAGE)-$(VERSION).bin.tar
	@$(ECHO) "Building a gzip-compressed tape archive"
	@$(ECHO)
	@@SH@ sh/make_tgz.sh "$(PACKAGE_NAME)-$(VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)-$(VERSION).bin.tar.gz for $(host_triplet)"

 $(PACKAGE)-$(VERSION).bin.tar.bz2: $(PACKAGE)-$(VERSION).bin.tar
	@$(ECHO) "Building a bzip2-compressed tape archive"
	@$(ECHO)
	@@SH@ sh/make_bz2.sh "$(PACKAGE_NAME)-$(VERSION)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)-$(VERSION).bin.tar.bz2 for $(host_triplet)"

 $(PACKAGE)-$(VERSION).bin.tar:
	@if ! test -f "install.$(host_triplet)" ; then \
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
	@@SH@ sh/make_tar.sh "$(PACKAGE_NAME)" "$(MAJOR_VERSION)" "$(MINOR_VERSION)" "$(PATCH_VERSION)" "$(DESTDIR)$(prefix)"
	@$(ECHO)
	@$(ECHO) "Built $(PACKAGE_NAME)-$(VERSION).bin.tar for $(host_triplet)"


#############
# the rules #
#############

.PHONY : benchmark bench pkg dmg rpm deb zip tgz tbz

benchmark: all
	@SH@ $(top_srcdir)/bench/run.sh

bench: benchmark

pkg: $(PACKAGE_NAME)-$(VERSION).pkg
	@$(ECHO) "Done."

dmg: pkg $(PACKAGE_NAME)-$(VERSION).dmg
	@$(ECHO) "Done."

rpm: $(PACKAGE)-$(VERSION).rpm $(PACKAGE)-$(VERSION).src.rpm
	@$(ECHO) "Done."

deb: $(PACKAGE)-$(VERSION).deb
	@$(ECHO) "Done."

zip: $(PACKAGE)-$(VERSION).bin.zip
	@$(ECHO) "Done."

tgz: $(PACKAGE)-$(VERSION).bin.tar.gz
	@$(ECHO) "Done."

bz2: $(PACKAGE)-$(VERSION).bin.tar.bz2
	@$(ECHO) "Done."

tar: $(PACKAGE)-$(VERSION).bin.tar
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
	    $(ECHO) "    find $(DESTDIR)$(prefix) -type d -exec chmod go+x {} \;" ; \
	    if test -d "$(DESTDIR)$(bindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/bin" != "x$(DESTDIR)$(bindir)" ; then \
	  	$(ECHO) "    find $(DESTDIR)$(bindir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(sbindir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/sbin" != "x$(DESTDIR)$(sbindir)" ; then \
	  	$(ECHO) "    find $(DESTDIR)$(sbindir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libexecdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/libexec" != "x$(DESTDIR)$(libexecdir)" ; then \
	  	$(ECHO) "    find $(DESTDIR)$(libexecdir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(datadir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/share" != "x$(DESTDIR)$(datadir)" ; then \
	   	$(ECHO) "    find $(DESTDIR)$(prefix) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(sysconfdir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/etc" != "x$(DESTDIR)$(sysconfdir)" ; then \
	   	$(ECHO) "    find $(DESTDIR)$(sysconfdir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(libdir)" ; then \
	      if test "x$(DESTDIR)$(exec_prefix)/lib" != "x$(DESTDIR)$(libdir)" ; then \
	   	$(ECHO) "    find $(DESTDIR)$(libdir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(mandir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/man" != "x$(DESTDIR)$(mandir)" ; then \
	   	$(ECHO) "    find $(DESTDIR)$(mandir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    if test -d "$(DESTDIR)$(includedir)" ; then \
	      if test "x$(DESTDIR)$(prefix)/include" != "x$(DESTDIR)$(includedir)" ; then \
	   	$(ECHO) "    find $(DESTDIR)$(includedir) -type d -exec chmod go+x {} \;" ; \
	      fi ; \
	    fi ; \
	    $(ECHO) ; \
	    $(ECHO) "  Do not presume the find example is all that is" ; \
	    $(ECHO) "  needed.  VERIFY THE PERMISSIONS." ; \
	    $(ECHO) "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{" ; \
	    $(ECHO) ; \
	fi ; \
	touch install.$(host_triplet)


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
	@$(ECHO) "BRL-CAD Release @BRLCAD_VERSION@, Build @CONFIG_DATE@"
	@$(ECHO)
	@if test "x$(MAKECMDGOALS)" = "xall-am" -o "x$(.TARGETS)" = "xall-am" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed compilation time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'make install' to begin installation into $(prefix)" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	elif test "x$(MAKECMDGOALS)" = "xinstall-am" -o "x$(.TARGETS)" = "xinstall-am" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed installation time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'cd regress ; make regress' to run the BRL-CAD test Suite" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	  $(ECHO) ;\
	  line1="  BRL-CAD @BRLCAD_VERSION@ is now installed into $(prefix)" ;\
	  line2="  Be sure to add $(prefix)/bin to your PATH" ;\
	  length="`$(ECHO) $(prefix)@BRLCAD_VERSION@ | wc -c`" ;\
	  length="`expr $$length + 34`" ;\
	  separator="" ;\
	  while test $$length -gt 0  ; do \
	    separator="$${separator}***" ;\
	    length="`expr $$length - 3`" ;\
          done ;\
	  $(ECHO) "$$separator" ;\
	  $(ECHO) "$$line1" ;\
	  $(ECHO) "$$line2" ;\
	  $(ECHO) "$$separator" ;\
	elif test "x$(MAKECMDGOALS)" = "x" -a "x$(.TARGETS)" = "x" ; then \
	  $(ECHO) $(ECHO_N) "Elapsed time: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh `cat $(top_builddir)/include/brlcad_version.h | grep Compilation` ;\
	  $(ECHO) $(ECHO_N) "Elapsed time since configuration: " ;\
	  sh $(top_srcdir)/sh/elapsed.sh @CONFIG_TIME@ ;\
	  $(ECHO) "---" ;\
	  $(ECHO) "Run 'make benchmark' to run the BRL-CAD Benchmark Suite" ;\
	fi
	@$(ECHO)
