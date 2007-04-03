# $Id$

all-am:
	cd unix && $(MAKE) $(AM_MAKEFLAGS) all

clean-am:
	cd unix && $(MAKE) $(AM_MAKEFLAGS) clean

distclean-am:
	cd unix && $(MAKE) $(AM_MAKEFLAGS) distclean

install-am: all-am
	cd unix && $(MAKE) $(AM_MAKEFLAGS) install

INSTALLED_BINARIES = \
	tclsh8.5

INSTALLED_INCLUDES = \
	tcl.h \
	tclDecls.h \
	tclPlatDecls.h \
	tclTomMath.h \
	tclTomMathDecls.h

INSTALLED_ARCHIVES = \
	libtcl8.5.a \
	libtclstub8.5.a \
	tcl8/8.4/http-2.5.3.tm \
	tcl8/8.5/msgcat-1.4.2.tm \
	tcl8/8.5/tcltest-2.3a1.tm \
	tcl8.5 \
	tclConfig.sh

INSTALLED_MANPAGES = \
	TCL_MEM_DEBUG.3 \
	tclsh.1 \
	attemptckalloc.3 \
	attemptckrealloc.3 \
	ckalloc.3 \
	ckfree.3 \
	ckrealloc.3 \
	Tcl_*.3 \
	after.n \
	append.n \
	apply.n \
	array.n \
	auto_execok.n \
	auto_import.n \
	auto_load.n \
	auto_mkindex.n \
	auto_mkindex_old.n \
	auto_qualify.n \
	auto_reset.n \
	bell.n \
	bgerror.n \
	binary.n \
	bind.n \
	bindtags.n \
	bitmap.n \
	break.n \
	button.n \
	canvas.n \
	case.n \
	catch.n \
	cd.n \
	chan.n \
	checkbutton.n \
	clipboard.n \
	clock.n \
	close.n \
	colors.n \
	concat.n \
	console.n \
	continue.n \
	cursors.n \
	dde.n \
	destroy.n \
	dict.n \
	encoding.n \
	entry.n \
	eof.n \
	error.n \
	eval.n \
	event.n \
	exec.n \
	exit.n \
	expr.n \
	fblocked.n \
	fconfigure.n \
	fcopy.n \
	file.n \
	fileevent.n \
	filename.n \
	flush.n \
	focus.n \
	font.n \
	for.n \
	foreach.n \
	format.n \
	frame.n \
	gets.n \
	glob.n \
	global.n \
	grab.n \
	grid.n \
	history.n \
	http.n \
	if.n \
	image.n \
	incr.n \
	info.n \
	interp.n \
	join.n \
	keysyms.n \
	label.n \
	labelframe.n \
	lappend.n \
	lassign.n \
	lindex.n \
	linsert.n \
	list.n \
	listbox.n \
	llength.n \
	load.n \
	lower.n \
	lrange.n \
	lrepeat.n \
	lreplace.n \
	lsearch.n \
	lset.n \
	lsort.n \
	mathfunc.n \
	memory.n \
	menu.n \
	menubutton.n \
	message.n \
	msgcat.n \
	namespace.n \
	open.n \
	option.n \
	options.n \
	pack-old.n \
	pack.n \
	package.n \
	panedwindow.n \
	parray.n \
	photo.n \
	pid.n \
	pkg::create.n \
	pkg_mkIndex.n \
	place.n \
	proc.n \
	puts.n \
	pwd.n \
	radiobutton.n \
	raise.n \
	re_syntax.n \
	read.n \
	reflectedchan.n \
	regexp.n \
	registry.n \
	regsub.n \
	rename.n \
	return.n \
	SafeBase.n \
	scale.n \
	scan.n \
	scrollbar.n \
	seek.n \
	selection.n \
	send.n \
	set.n \
	socket.n \
	source.n \
	spinbox.n \
	split.n \
	string.n \
	subst.n \
	switch.n \
	Tcl.n \
	tcl_*.n \
	tcltest.n \
	tclvars.n \
	tell.n \
	text.n \
	time.n \
	tm.n \
	trace.n \
	unknown.n \
	unload.n \
	unset.n \
	update.n \
	uplevel.n \
	upvar.n \
	variable.n \
	vwait.n \
	while.n \
	winfo.n \
	wm.n

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
	cd unix && rm -f libtcl* tclsh tcltest Tcl
	cd unix/dltest && $(MAKE) $(AM_MAKEFLAGS) clean

fast: all-am

depends: all-am

EXTRA_DIST = \
	ChangeLog \
	ChangeLog.1999 \
	ChangeLog.2000 \
	ChangeLog.2001 \
	ChangeLog.2002 \
	ChangeLog.2003 \
	ChangeLog.2004 \
	README \
	changes \
	compat \
	doc \
	generic \
	library \
	libtommath \
	license.terms \
	macosx \
	tests \
	tools \
	unix/Makefile \
	unix/Makefile.in \
	unix/README \
	unix/aclocal.m4 \
	unix/config.status \
	unix/configure \
	unix/configure.in \
	unix/dltest/Makefile \
	unix/dltest/Makefile.in \
	unix/dltest/README \
	unix/dltest/pkga.c \
	unix/dltest/pkgb.c \
	unix/dltest/pkgc.c \
	unix/dltest/pkgd.c \
	unix/dltest/pkge.c \
	unix/dltest/pkgf.c \
	unix/dltest/pkgua.c \
	unix/install-sh \
	unix/installManPage \
	unix/ldAix \
	unix/tcl.m4 \
	unix/tcl.spec \
	unix/tclAppInit.c \
	unix/tclConfig.h.in \
	unix/tclConfig.sh \
	unix/tclConfig.sh.in \
	unix/tclLoadAix.c \
	unix/tclLoadDl.c \
	unix/tclLoadDyld.c \
	unix/tclLoadNext.c \
	unix/tclLoadOSF.c \
	unix/tclLoadShl.c \
	unix/tclUnixChan.c \
	unix/tclUnixCompat.c \
	unix/tclUnixEvent.c \
	unix/tclUnixFCmd.c \
	unix/tclUnixFile.c \
	unix/tclUnixInit.c \
	unix/tclUnixNotfy.c \
	unix/tclUnixPipe.c \
	unix/tclUnixPort.h \
	unix/tclUnixSock.c \
	unix/tclUnixTest.c \
	unix/tclUnixThrd.c \
	unix/tclUnixThrd.h \
	unix/tclUnixTime.c \
	unix/tclXtNotify.c \
	unix/tclXtTest.c \
	win

DISTCLEANFILES = \
	unix/autom4te.cache \
	unix/config.log \
	unix/config.status \
	unix/configure \
	unix/tclConfig.sh \
	unix/tclsh

MAINTAINERCLEANFILES = Makefile.in

