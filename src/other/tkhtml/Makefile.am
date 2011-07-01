# Makefile.am --
#
#	This file is a Makefile for Tkhtml3, based off of the Sample TEA 
#	Extension Makefile.  If it has the name	"Makefile.am" then it is 
#	a template for a Makefile;  to generate the actual Makefile, 
#	run "./autogen.sh", which triggers the correct autotools steps to
#	generate a ready-to-run build.
#
# Copyright (c) 1999 Scriptics Corporation.
# Copyright (c) 2002-2005 ActiveState Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#========================================================================
# The names of the source files is defined in the configure script.
# The object files are used for linking into the final library.
# This will be used when a dist target is added to the Makefile.
# It is not important to specify the directory, as long as it is the
# $(srcdir) or in the generic, win or unix subdirectory.
#========================================================================

PKG_OBJECTS	= @PKG_OBJECTS@ cssprop.$(OBJEXT)

PKG_STUB_OBJECTS = @PKG_STUB_OBJECTS@

#========================================================================
# "PKG_LIB_FILE" refers to the library (dynamic or static as per
# configuration options) composed of the named objects.
#========================================================================

PKG_LIB_FILE	= @PKG_LIB_FILE@
PKG_STUB_LIB_FILE = @PKG_STUB_LIB_FILE@

# lib_BINARIES	= $(PKG_LIB_FILE)
# BINARIES	= $(lib_BINARIES)

SHELL		= @SHELL@

srcdir		= @srcdir@
prefix		= @prefix@
exec_prefix	= @exec_prefix@

bindir		= @bindir@
libdir		= @libdir@
datadir		= @datadir@
mandir		= @mandir@
includedir	= @includedir@

PKG_DIR		= $(PACKAGE_NAME)$(PACKAGE_VERSION)
pkgdatadir	= $(datadir)/$(PKG_DIR)
pkglibdir	= $(libdir)/$(PKG_DIR)
pkgincludedir	= $(includedir)/$(PKG_DIR)

top_builddir	= .

INSTALL		= @INSTALL@
INSTALL_PROGRAM	= @INSTALL_PROGRAM@
#INSTALL_DATA	= @INSTALL_DATA@
INSTALL_SCRIPT	= @INSTALL_SCRIPT@

PACKAGE_NAME	= @PACKAGE_NAME@
PACKAGE_VERSION	= @PACKAGE_VERSION@
CC		= @CC@
CFLAGS_DEFAULT	= @CFLAGS_DEFAULT@
CFLAGS_WARNING	= @CFLAGS_WARNING@
CLEANFILES	= @CLEANFILES@ $(BUILT_SOURCES) $(BUILT_EXTRA)
EXEEXT		= @EXEEXT@
LDFLAGS_DEFAULT	= @LDFLAGS_DEFAULT@ ${LDFLAGS}
MAKE_LIB	= @MAKE_LIB@
MAKE_SHARED_LIB	= @MAKE_SHARED_LIB@
MAKE_STATIC_LIB	= @MAKE_STATIC_LIB@
MAKE_STUB_LIB	= @MAKE_STUB_LIB@
OBJEXT		= @OBJEXT@
RANLIB		= @RANLIB@
RANLIB_STUB	= @RANLIB_STUB@
SHLIB_CFLAGS	= @SHLIB_CFLAGS@
SHLIB_LD	= @SHLIB_LD@
SHLIB_LD_LIBS	= @SHLIB_LD_LIBS@
STLIB_LD	= @STLIB_LD@
#TCL_DEFS	= @TCL_DEFS@
TCL_BIN_DIR	= @TCL_BIN_DIR@
TCL_SRC_DIR	= @TCL_SRC_DIR@
TK_BIN_DIR	= @TK_BIN_DIR@
TK_SRC_DIR	= @TK_SRC_DIR@

# Not used, but retained for reference of what libs Tcl required
#TCL_LIBS	= @TCL_LIBS@

#========================================================================
# TCLLIBPATH seeds the auto_path in Tcl's init.tcl so we can test our
# package without installing.  The other environment variables allow us
# to test against an uninstalled Tcl.  Add special env vars that you
# require for testing here (like TCLX_LIBRARY).
#========================================================================

#EXTRA_PATH	= $(top_builddir):$(TCL_BIN_DIR)
EXTRA_PATH	= $(top_builddir):$(TCL_BIN_DIR):$(TK_BIN_DIR)
TCLLIBPATH	= $(top_builddir)
TCLSH_ENV	= TCL_LIBRARY=`@CYGPATH@ $(TCL_SRC_DIR)/library` \
		  @LD_LIBRARY_PATH_VAR@="$(EXTRA_PATH):$(@LD_LIBRARY_PATH_VAR@)" \
		  PATH="$(EXTRA_PATH):$(PATH)" \
		  TCLLIBPATH="$(TCLLIBPATH)"
#		  TK_LIBRARY=`@CYGPATH@ $(TK_SRC_DIR)/library`
TCLSH_PROG	= @TCLSH_PROG@
TCLSH		= $(TCLSH_ENV) $(TCLSH_PROG)
WISH_PROG	= @WISH_PROG@
WISH		= $(TCLSH_ENV) $(WISH_PROG)
SHARED_BUILD	= @SHARED_BUILD@

#INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@
INCLUDES	= @PKG_INCLUDES@ @TCL_INCLUDES@ @TK_INCLUDES@ @TK_XINCLUDES@ -I$(srcdir)/src -I$(srcdir) -I$(top_builddir)

PKG_CFLAGS	= @PKG_CFLAGS@

# TCL_DEFS is not strictly need here, but if you remove it, then you
# must make sure that configure.in checks for the necessary components
# that your library may use.  TCL_DEFS can actually be a problem if
# you do not compile with a similar machine setup as the Tcl core was
# compiled with.
#DEFS		= $(TCL_DEFS) @DEFS@ $(PKG_CFLAGS)
DEFS		= @DEFS@ $(PKG_CFLAGS)

CONFIG_CLEAN_FILES = Makefile

CPPFLAGS	= @CPPFLAGS@
LIBS		= -L/usr/lib -L/usr/local/lib @PKG_LIBS@ @LIBS@
LDFLAGS		= @LDFLAGS@
AR		= @AR@
CFLAGS		= @CFLAGS@
COMPILE		= $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)

#========================================================================
# Start of user-definable TARGETS section
#========================================================================

#========================================================================
# TEA TARGETS.  Please note that the "libraries:" target refers to platform
# independent files, and the "binaries:" target inclues executable programs and
# platform-dependent libraries.  Modify these targets so that they install
# the various pieces of your package.  The make and install rules
# for the BINARIES that you specified above have already been done.
#========================================================================

all: binaries libraries # doc

#========================================================================
# The binaries target builds executable programs, Windows .dll's, unix
# shared/static libraries, and any other platform-dependent files.
# The list of targets to build for "binaries:" is specified at the top
# of the Makefile, in the "BINARIES" variable.
#========================================================================

binaries: $(BINARIES) pkgIndex.tcl

libraries:


#========================================================================
# Your doc target should differentiate from doc builds (by the developer)
# and doc installs (see install-doc), which just install the docs on the
# end user machine when building from source.
#========================================================================

doc:
	$(TCLSH) $(srcdir)/doc/macros.tcl -nroff $(srcdir)/doc/html.man>tkhtml.n

# feed extra stuff to libtool
install-data-local: all install-binaries install-libraries install-doc
# install: all install-binaries install-libraries install-doc

install-binaries: binaries install-lib-binaries install-bin-binaries

#========================================================================
# This rule installs platform-independent files, such as header files.
# The list=...; for p in $$list handles the empty list case x-platform.
#========================================================================

install-libraries: libraries
	@mkdir -p $(DESTDIR)$(includedir)
	@echo "Installing header files in $(DESTDIR)$(includedir)"
	@list='$(PKG_HEADERS)'; for i in $$list; do \
	    echo "Installing $(srcdir)/$$i" ; \
	    $(INSTALL_DATA) $(srcdir)/$$i $(DESTDIR)$(includedir) ; \
	done;

#========================================================================
# Install documentation.  Unix manpages should go in the $(mandir)
# directory.
#========================================================================

install-doc: doc
	@mkdir -p $(DESTDIR)$(mandir)/mann
	@echo "Installing documentation in $(DESTDIR)$(mandir)"
	@list='*.n'; for i in $$list; do \
	    echo "Installing $$i"; \
	    rm -f $(DESTDIR)$(mandir)/mann/`basename $$i`; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(mandir)/mann ; \
	done

test: binaries libraries
	$(WISH) `@CYGPATH@ $(srcdir)/tests/all.tcl` $(TESTFLAGS)

# "inttest" is a custom target to run the interactive tests for the widget.
inttest: binaries libraries
	$(TCLSH) `@CYGPATH@ $(srcdir)/tests/interactive.tcl` $(TESTFLAGS)

shell: binaries libraries
	@$(TCLSH) $(SCRIPT)

gdb:
	$(TCLSH_ENV) gdb $(TCLSH_PROG) $(SCRIPT)

depend:

#========================================================================
# $(PKG_LIB_FILE) should be listed as part of the BINARIES variable
# mentioned above.  That will ensure that this target is built when you
# run "make binaries".
#
# The $(PKG_OBJECTS) objects are created and linked into the final
# library.  In most cases these object files will correspond to the
# source files above.
#========================================================================

$(PKG_LIB_FILE): $(PKG_OBJECTS)
	-rm -f $(PKG_LIB_FILE)
	${MAKE_LIB}
	$(RANLIB) $(PKG_LIB_FILE)

$(PKG_STUB_LIB_FILE): $(PKG_STUB_OBJECTS)
	-rm -f $(PKG_STUB_LIB_FILE)
	${MAKE_STUB_LIB}
	$(RANLIB_STUB) $(PKG_STUB_LIB_FILE)

#========================================================================
# We need to enumerate the list of .c to .o lines here.
#
# In the following lines, $(srcdir) refers to the toplevel directory
# containing your extension.  If your sources are in a subdirectory,
# you will have to modify the paths to reflect this:
#
# sample.$(OBJEXT): $(srcdir)/generic/sample.c
# 	$(COMPILE) -c `@CYGPATH@ $(srcdir)/generic/sample.c` -o $@
#
# Setting the VPATH variable to a list of paths will cause the makefile
# to look into these paths when resolving .c to .obj dependencies.
# As necessary, add $(srcdir):$(srcdir)/compat:....
#========================================================================

# VPATH = $(srcdir):$(srcdir)/src:$(srcdir)/unix:$(srcdir)/win:.

HDR = $(GENHDR) $(SRCHDR)

lib_LTLIBRARIES = libTkhtml.la

libTkhtml_la_LDFLAGS = -module

noinst_HEADERS = \
	src/css.h \
	src/cssInt.h \
	src/html.h \
	src/htmllayout.h \
	src/htmlmacros.h \
	src/htmlprop.h \
	src/restrack.h \
	src/swproc.h

libTkhtml_la_SOURCES = \
	$(compiled_srcs) \
	src/css.c \
	src/cssdynamic.c \
	src/cssparser.c \
	src/csssearch.c \
	src/htmldecode.c \
	src/htmldraw.c \
	src/htmlfloat.c \
	src/htmlhash.c \
	src/htmlimage.c \
	src/htmlinline.c \
	src/htmllayout.c \
	src/htmlparse.c \
	src/htmlprop.c \
	src/htmlstyle.c \
	src/htmltable.c \
	src/htmltagdb.c \
	src/htmltcl.c \
	src/htmltext.c \
	src/htmltree.c \
	src/htmlutil.c \
	src/restrack.c \
	src/swproc.c

compiled_srcs = \
	cssprop.c

BUILT_SOURCES = \
	$(compiled_srcs) \
	htmldefaultstyle.c \
	htmltokens.c

BUILT_EXTRA = \
	cssprop.h \
	cssprop.tcl \
	htmltokens.h \
	tokenlist.txt

# ensure everyone is dependent upon cssprop.h so parallel make can
# succeed.  Inclusion is: cssprop.h -> css.h -> cssInt.h|html.h -> *.c
src/css.c: cssprop.h
src/cssdynamic.c: cssprop.h
src/cssparser.c: cssprop.h
src/csssearch.c: cssprop.h
src/htmldecode.c: cssprop.h
src/htmldraw.c: cssprop.h
src/htmlfloat.c: cssprop.h
src/htmlhash.c: cssprop.h
src/htmlimage.c: cssprop.h
src/htmlinline.c: cssprop.h
src/htmllayout.c: cssprop.h
src/htmlparse.c: cssprop.h
src/htmlprop.c: cssprop.h
src/htmlstyle.c: cssprop.h
src/htmltable.c: cssprop.h
src/htmltagdb.c: cssprop.h
src/htmltcl.c: cssprop.h
src/htmltext.c: cssprop.h
src/htmltree.c: cssprop.h
src/htmlutil.c: cssprop.h
src/restrack.c: cssprop.h
src/swproc.c: cssprop.h


#========================================================================
# The special targets to generate C code from tcl and lemon files are 
# here:

htmldefaultstyle.c: $(top_srcdir)/src/mkdefaultstyle.tcl
	$(TCLSH) $(top_srcdir)/src/mkdefaultstyle.tcl > htmldefaultstyle.c || tclsh $(top_srcdir)/src/mkdefaultstyle.tcl > htmldefaultstyle.c


tokenlist.txt:
	test -f tokenlist.txt || cp $(top_srcdir)/src/tokenlist.txt .

htmltokens.h: tokenlist.txt
	$(TCLSH) tokenlist.txt || tclsh tokenlist.txt

htmltokens.c: htmltokens.h


cssprop.tcl:
	cp $(top_srcdir)/src/cssprop.tcl .

cssprop.h: cssprop.tcl
	test -f cssprop.h || $(TCLSH) cssprop.tcl || tclsh cssprop.tcl

cssprop.c: cssprop.h


#========================================================================
# Create the pkgIndex.tcl file.
# It is usually easiest to let Tcl do this for you with pkg_mkIndex, but
# you may find that you need to customize the package.  If so, either
# modify the -hand version, or create a pkgIndex.tcl.in file and have
# the configure script output the pkgIndex.tcl by editing configure.in.
#========================================================================

# pkgIndex.tcl: $(PKG_LIB_FILE)
# 	( echo pkg_mkIndex -load Tk . \; exit; ) | $(WISH)

pkgIndex.tcl:
	echo 'package ifneeded $(PACKAGE_NAME) $(PACKAGE_VERSION) [list load [file join $$dir .. libTkhtml.so]]' > pkgIndex.tcl
#	echo 'package ifneeded $(PACKAGE_NAME) $(PACKAGE_VERSION) [list load [file join $$dir $(PKG_LIB_FILE)]]' > pkgIndex.tcl


#========================================================================
# End of user-definable section
#========================================================================

#========================================================================
# Don't modify the file to clean here.  Instead, set the "CLEANFILES"
# variable in configure.in
#========================================================================

clean: clean-am
	-test -z "$(BINARIES)" || rm -f $(BINARIES)
	-rm -f *.$(OBJEXT) core *.core
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

DISTCLEANFILES = \
	$(CONFIG_CACHE) \
	aclocal.m4 \
	config.log \
	config.status \
	$(BINARIES) \
	$(PKG_OBJECTS) \
	$(CLEANFILES)

#========================================================================
# Install binary object libraries.  On Windows this includes both .dll and
# .lib files.  Because the .lib files are not explicitly listed anywhere,
# we need to deduce their existence from the .dll file of the same name.
# Library files go into the lib directory.
# In addition, this will generate the pkgIndex.tcl
# file in the install location (assuming it can find a usable tclsh shell)
#
# You should not have to modify this target.
#========================================================================

install-lib-binaries: binaries
	@mkdir -p $(DESTDIR)$(pkglibdir)
	@list='$(lib_BINARIES)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_PROGRAM) $$p $(DESTDIR)$(pkglibdir)/$$p"; \
	    $(INSTALL_PROGRAM) $$p $(DESTDIR)$(pkglibdir)/$$p; \
	    stub=`echo $$p|sed -e "s/.*\(stub\).*/\1/"`; \
	    if test "x$$stub" = "xstub"; then \
		echo " $(RANLIB_STUB) $(DESTDIR)$(pkglibdir)/$$p"; \
		$(RANLIB_STUB) $(DESTDIR)$(pkglibdir)/$$p; \
	    else \
		echo " $(RANLIB) $(DESTDIR)$(pkglibdir)/$$p"; \
		$(RANLIB) $(DESTDIR)$(pkglibdir)/$$p; \
	    fi; \
	    ext=`echo $$p|sed -e "s/.*\.//"`; \
	    if test "x$$ext" = "xdll"; then \
		lib=`basename $$p|sed -e 's/.[^.]*$$//'`.lib; \
		if test -f $$lib; then \
		    echo " $(INSTALL_DATA) $$lib $(DESTDIR)$(pkglibdir)/$$lib"; \
	            $(INSTALL_DATA) $$lib $(DESTDIR)$(pkglibdir)/$$lib; \
		fi; \
	    fi; \
	  fi; \
	done
	@list='$(PKG_TCL_SOURCES)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    destp=`basename $$p`; \
	    echo " Install $$destp $(DESTDIR)$(pkglibdir)/$$destp"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(pkglibdir)/$$destp; \
	  fi; \
	done
	@if test "x$(SHARED_BUILD)" = "x1"; then \
	    if test -f $(srcdir)/pkgIndex.tcl ; then \
		echo " Install $(srcdir)/pkgIndex.tcl $(DESTDIR)$(pkglibdir)"; \
		$(INSTALL_DATA) $(srcdir)/pkgIndex.tcl $(DESTDIR)$(pkglibdir); \
	    else \
		if test -f pkgIndex.tcl ; then \
		    echo " Install pkgIndex.tcl $(DESTDIR)$(pkglibdir)"; \
		    $(INSTALL_DATA) pkgIndex.tcl $(DESTDIR)$(pkglibdir); \
		fi; \
	    fi; \
	fi

#appease dvi-recursive rule from the top-level Makefile so this works as a SUBDIR
dvi:

#ditto for check, installcheck, uninstall, distuninstallcheck
check:

installcheck:

uninstall: uninstall-am uninstall-binaries uninstall-doc
	rm -f $(DESTDIR)$(pkglibdir)/pkgIndex.tcl; 

distuninstallcheck: 

#========================================================================
# Install binary executables (e.g. .exe files and dependent .dll files)
# This is for files that must go in the bin directory (located next to
# wish and tclsh), like dependent .dll files on Windows.
#
# You should not have to modify this target, except to define bin_BINARIES
# above if necessary.
#========================================================================

install-bin-binaries: binaries
	@mkdir -p $(DESTDIR)$(bindir)
	@list='$(bin_BINARIES)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$p"; \
	    $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$p; \
	  fi; \
	done

#.SUFFIXES:
# .c .$(OBJEXT)

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

uninstall-binaries:
	list='$(lib_BINARIES)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(pkglibdir)/$$p; \
	done
	list='$(PKG_TCL_SOURCES)'; for p in $$list; do \
	  p=`basename $$p`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$p; \
	done
	list='$(bin_BINARIES)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(bindir)/$$p; \
	done

uninstall-doc:
	@list='*.n' ; \
	for i in $$list ; do \
		echo "Uninstalling $i" ; \
		rm -f $(DESTDIR)$(mandir)/mann/$$i ; \
	done

EXTRA_DIST = \
	doc/html.man \
	doc/macros.tcl \
	doc/tkhtml_requirements.tcl \
	doc/tree.fig \
	doc/tree.gif \
	src/cssprop.tcl \
	src/html.css \
	src/htmltcl.c \
	src/main.c \
	src/mkdefaultstyle.tcl \
	src/quirks.css \
	src/tkhtml.tcl \
	src/tokenlist.txt \
	tclconfig/README.txt \
	tclconfig/tcl.m4 \
	CMakeLists.txt \
	CMake/FindTCL.cmake \
	CMake/FindX11.cmake

.PHONY: all binaries clean depend distclean doc install libraries test

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
