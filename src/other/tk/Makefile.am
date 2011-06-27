
all: all-am

all-am:
	if test -d unix ; then cd unix && $(MAKE) $(AM_MAKEFLAGS) all ; fi

clean-am:
	if test -d unix ; then cd unix && $(MAKE) $(AM_MAKEFLAGS) clean ; fi

distclean-am:
	if test -d unix ; then cd unix && $(MAKE) $(AM_MAKEFLAGS) distclean ; fi

install-am: all-am
	if test -d unix ; then cd unix && $(MAKE) $(AM_MAKEFLAGS) install ; fi

INSTALLED_BINARIES = \
	wish \
	wish8.5

INSTALLED_INCLUDES = \
	tk.h \
	tkDecls.h \
	tkPlatDecls.h

INSTALLED_ARCHIVES = \
	libtk.* \
	libtk8.5.* \
	libtkstub.* \
	libtkstub8.5.* \
	tk8.5 \
	tkConfig.sh

INSTALLED_MANPAGES = \
	wish.1 \
	Tk_*.3 \
	Ttk_*.3 \
	loadTk.n \
	tk.n \
	tk_*.n \
	ttk_*.n \
	tkerror.n \
	tkvars.n \
	tkwait.n \
	toplevel.n

uninstall-bin:
	@$(NORMAL_UNINSTALL)
	@list='$(INSTALLED_BINARIES)'; for p in $$list; do \
	  f=`echo "$$p" | sed 's,^.*/,,;s/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  echo " rm -f $(DESTDIR)$(bindir)/$$f"; \
	  rm -f $(DESTDIR)$(bindir)/$$f; \
	done

uninstall-includes:
	@$(NORMAL_UNINSTALL)
	@list='$(INSTALLED_INCLUDES)'; for p in $$list; do \
	  echo " rm -f $(DESTDIR)$(includedir)/$$p"; \
	  rm -f $(DESTDIR)$(includedir)/$$p; \
	done

uninstall-libs:
	@$(NORMAL_UNINSTALL)
	@list='$(INSTALLED_ARCHIVES)'; for p in $$list; do \
	  echo " rm -rf $(DESTDIR)$(libdir)/$$p"; \
	  rm -rf $(DESTDIR)$(libdir)/$$p; \
	done

man1dir = $(mandir)/man1
uninstall-man1:
	@$(NORMAL_UNINSTALL)
	@list=''; \
	l2='$(INSTALLED_MANPAGES)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.1*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man1dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man1dir)/$$inst; \
	done

man3dir = $(mandir)/man3
uninstall-man3:
	@$(NORMAL_UNINSTALL)
	@list=''; \
	l2='$(INSTALLED_MANPAGES)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.3*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man3dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man3dir)/$$inst; \
	done

manndir = $(mandir)/mann
uninstall-mann:
	@$(NORMAL_UNINSTALL)
	@list=''; \
	l2='$(INSTALLED_MANPAGES)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.n*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(manndir)/$$inst"; \
	  rm -f $(DESTDIR)$(manndir)/$$inst; \
	done

uninstall-mans: uninstall-man1 uninstall-man3 uninstall-mann

uninstall-am: uninstall-bin uninstall-includes uninstall-libs uninstall-mans

noprod: prodclean

prodclean-recursive:

prodclean: prodclean-recursive
	cd unix && rm -f libtk* wish tktest Tk

fast: all-am

depends: all-am

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.2002 \
	ChangeLog.2004 \
	README \
	bitmaps \
	changes \
	compat \
	doc \
	generic \
	library \
	license.terms \
	macosx \
	tests \
	unix/Makefile.in \
	unix/README \
	unix/aclocal.m4 \
	unix/configure \
	unix/configure.in \
	unix/install-sh \
	unix/installManPage \
	unix/license.terms \
	unix/tcl.m4 \
	unix/tkAppInit.c \
	unix/tkConfig.h.in \
	unix/tkConfig.sh.in \
	unix/tkUnix.c \
	unix/tkUnix3d.c \
	unix/tkUnixButton.c \
	unix/tkUnixColor.c \
	unix/tkUnixConfig.c \
	unix/tkUnixCursor.c \
	unix/tkUnixDefault.h \
	unix/tkUnixDialog.c \
	unix/tkUnixDraw.c \
	unix/tkUnixEmbed.c \
	unix/tkUnixEvent.c \
	unix/tkUnixFocus.c \
	unix/tkUnixFont.c \
	unix/tkUnixInit.c \
	unix/tkUnixInt.h \
	unix/tkUnixKey.c \
	unix/tkUnixMenu.c \
	unix/tkUnixMenubu.c \
	unix/tkUnixPort.h \
	unix/tkUnixRFont.c \
	unix/tkUnixScale.c \
	unix/tkUnixScrlbr.c \
	unix/tkUnixSelect.c \
	unix/tkUnixSend.c \
	unix/tkUnixWm.c \
	unix/tkUnixXId.c \
	win \
	xlib \
	CMakeLists.txt \
	CMake/CheckCFileRuns.cmake \
	CMake/CheckPrototypeExists.cmake \
	CMake/CheckSystemFunctionality.cmake \
	CMake/FindX11.cmake \
	CMake/ResolveCompilerPaths.cmake \
	CMake/ac_std_funcs.cmake \
	CMake/tcl.cmake

DISTCLEANFILES = \
	unix/autom4te.cache \
	unix/config.log \
	unix/config.status \
	unix/configure \
	unix/tkConfig.sh \
	unix/wish

# INTENTIONALLY NOT INCLUDED
# include $(top_srcdir)/misc/Makefile.defs
