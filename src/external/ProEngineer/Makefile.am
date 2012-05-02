
lib_LTLIBRARIES = libproe2brl.la

# XXX this is different based on the platform
PRO_MACHINE_TYPE = i486_linux

# XXX this depends on local system configuration
PRO_HOME = /usr/local/ptc/proeWildfire3.0
PROTOOL_SRC = $(PRO_HOME)/protoolkit
PROTOOL_SYS = $(PROTOOL_SRC)/$(PRO_MACHINE_TYPE)
PRODEV_SRC  = $(PRO_HOME)/prodevelop
PRODEV_SYS  = $(PRODEV_SRC)/$(PRO_MACHINE_TYPE)
PROTK_UNLOCK = $(PRO_HOME)/bin/protk_unlock

PDLIB       = $(PRODEV_SYS)/obj/prodevelop.a
PDLIB_DLL   = $(PRODEV_SYS)/obj/prodev_dll.a
PTLIB       = $(PROTOOL_SYS)/obj/protoolkit.a
PTLIB_DLL   = $(PROTOOL_SYS)/obj/protk_dll.a
PTCLIBS     = $(PTLIB) $(PDLIB)
PTCLIBS_DLL = $(PTLIB_DLL) $(PDLIB_DLL)
BRLLIBS     = ../../libbu/.libs/libbu.a ../../libbn/.libs/libbn.a

# XXX this may be different based on the platform
SYSLIBS     = -lc -lm -ldl

# XXX this may be different based on the platform
AM_CFLAGS = -m32 -fpic
AM_LDFLAGS = -shared -Wl,-Bsymbolic -Wl,-noinhibit-exec -Wl,-E $(SYSLIBS)
# XXX the PRO_MACHINE/PRO_OS depends on system/os (obviously). values defined by PTC, see their example makefiles for target system.
AM_CPPFLAGS = \
	-DPRO_MACHINE=34 -DPRO_OS=3 \
	-I$(PROTOOL_SRC)/protk_appls/includes \
	-I$(PROTOOL_SRC)/includes \
	-I$(PRODEV_SRC)/includes

libproe2brl_ladir = $(BRLCAD_DATA)/pro-engineer/$(PRO_MACHINE_TYPE)
libproe2brl_la_CFLAGS = $(AM_CFLAGS)
libproe2brl_la_LIBADD = $(PDLIB_DLL) $(PTLIB_DLL) $(BRLLIBS) $(SYSLIBS)
libproe2brl_la_SOURCES = proe-brl.c

all-am: libproe2brl.la Makefile
	@if test "x$(MAKECMDGOALS)" = "xall-am" -o "x$(.TARGETS)" = "xall-am" -o "x$(MAKECMDGOALS)" = "x" -a "x$(.TARGETS)" = "x" ; then \
	  @ECHO@ "---" ; \
	  @ECHO@ "Run 'make plugin' from misc/pro-engineer before 'make install' to build the Pro-Engineer plugin." ; \
	  @ECHO@ "Run 'make plugin2' from misc/pro-engineer before 'make install' to build a .so Pro-Engineer plugin." ; \
	  @ECHO@ ; \
	fi

install-data-local:
	@if test -f .libs/proe-brl ; then \
		@ECHO@ "Installing proe-brl" ; \
		if ! test -d $(DESTDIR)$(proe_brldir) ; then \
			mkdir -p $(DESTDIR)$(proe_brldir) ; \
		fi ; \
		$(INSTALL) .libs/proe-brl $(DESTDIR)$(proe_brldir)/proe-brl ; \
	fi
	@if test -f libproe2brl.la ; then \
		@ECHO@ "Installing libproe2brl.la" ; \
		if ! test -d $(DESTDIR)$(libproe2brl_ladir) ; then \
			mkdir -p $(DESTDIR)$(libproe2brl_ladir) ; \
		fi ; \
		if test -x $(PROTK_UNLOCK) ; then \
			$(PROTK_UNLOCK) $(prefix)/lib/libproe2brl.so ; \
		fi ; \
		cp $(prefix)/lib/libproe2brl.so $(libproe2brl_ladir)/ ; \
	fi

unlock:
	@if test -x $(PROTK_UNLOCK) ; then \
		if test -f .libs/proe-brl ; then \
			$(PROTK_UNLOCK) .libs/proe-brl ; \
		else \
			@ECHO@ "Run 'make plugin' before running 'make unlock'" ; \
		fi ; \
	else \
		@ECHO@ "The ProE unlock tool does not exist as $(PROTK_UNLOCK)" ; \
		@ECHO@ "Set and verify PROTK_UNLOCK in misc/pro-engineer/Makefile" ; \
	fi

unlock2:
	@if test -x $(PROTK_UNLOCK) ; then \
		if test -f libproe2brl.so ; then \
			$(PROTK_UNLOCK) libproe2brl.so ; \
		else \
			@ECHO@ "Run 'make plugin2' before running 'make unlock2'" ; \
		fi ; \
	else \
		@ECHO@ "The ProE unlock tool does not exist as $(PROTK_UNLOCK)" ; \
		@ECHO@ "Set and verify PROTK_UNLOCK in misc/pro-engineer/Makefile" ; \
	fi

plugin: proe-brl
	@@ECHO@ "---"
	@@ECHO@ "Run 'make unlock' to unlock the Pro-Engineer plugin for distribution"
	@@ECHO@ "Run 'make install' to install the Pro-Engineer plugin"
	@@ECHO@ "You will need to edit protk.dat to use this plugin instead of libproe2brl.so"
	@@ECHO@

plugin2: libproe2brl.la
	@@ECHO@ "---"
	@@ECHO@ "Run 'make unlock2' to unlock the Pro-Engineer .so plugin for distribution"
	@@ECHO@ "Run 'make install' to install the Pro-Engineer .so plugin"
	@@ECHO@

proedir = $(BRLCAD_DATA)/pro-engineer
tmsgdir = $(BRLCAD_DATA)/pro-engineer/text/usascii/
rsrcdir = $(BRLCAD_DATA)/pro-engineer/text/usascii/resource/

proe_DATA = \
	INSTALL.txt \
	protk.dat

tmsg_DATA = usermsg.txt

rsrc_DATA = \
	proe_brl.res \
	proe_brl_error.res \
	proe_brl_gen_error.res

EXTRA_DIST = \
	$(proe_DATA) \
	$(rsrc_DATA) \
	$(tmsg_DATA) \
	mk.in \
	proe-brl-notes.txt \
	proe-brl.dll.config \
	protk.dat.in \
	win32-msvc8/proe \
	win32-msvc8/proe-brl-3.0/proe-brl-3.0.vcproj \
	win32-msvc8/proe-brl-4.0/proe-brl-4.0.vcproj \
	win32-msvc8/proe-brl-5.0/proe-brl-5.0.vcproj \
	win32-msvc8/proe/proe.sln

CLEANFILES = \
	libproe2brl.so \
	proe-brl

DISTCLEANFILES = \
	mk \
	protk.dat


include $(top_srcdir)/misc/Makefile.defs
