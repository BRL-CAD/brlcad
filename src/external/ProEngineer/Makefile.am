# $Id$#

EXTRA_PROGRAMS = proe-brl
# EXTRA_LTLIBRARIES = libproe2brl.la

PRO_MACHINE_TYPE = sgi_mips4
PROTOOL_SRC = /opt/ptc/proe2001/protoolkit
PROTOOL_SYS = $(PROTOOL_SRC)/$(PRO_MACHINE_TYPE)
PRODEV_SRC  = /opt/ptc/proe2001/prodevelop
PRODEV_SYS  = $(PRODEV_SRC)/$(PRO_MACHINE_TYPE)
PROTK_UNLOCK = /usr/proe/bin/protk_unlock

PDLIB       = $(PRODEV_SYS)/obj/prodevelop.a
PDLIB_DLL   = $(PRODEV_SYS)/obj/prodev_dll.a
PTLIB       = $(PROTOOL_SYS)/obj/protoolkit.a
PTLIB_DLL   = $(PROTOOL_SYS)/obj/protk_dll.a
PTCLIBS     = $(PTLIB) $(PDLIB)
PTCLIBS_DLL = $(PTLIB_DLL) $(PDLIB_DLL)
BRLLIBS     = $(BU) $(BN) $(TCL)
SYSLIBS     = -lc -lm -lw

AM_CFLAGS = -Wl,-rpath -Wl,$(libdir) -Wl,-multigot -Wl,-T10000000 -Wl,-B,symbolic
AM_LDFLAGS = $(BRLLIBS) $(SYSLIBS)
AM_CPPFLAGS = \
	-DPRO_MACHINE=23 -DPRO_OS=3 \
	-I$(PROTOOL_SRC)/protk_appls/includes \
	-I$(PROTOOL_SRC)/includes \
	-I$(PRODEV_SRC)/includes

proe_brldir = $(BRLCAD_DATA)/pro-engineer/$(PRO_MACHINE_TYPE)
proe_brl_CFLAGS = $(AM_CFLAGS)
proe_brl_LDADD = $(PTLIB) $(PDLIB) $(SYSLIBS)
proe_brl_SOURCES = proe-brl.c

#cc -n32 -mips4 -woff 1521 -multigot -Wl,-wall  -rpath /usr/brlcad/lib -Wl,-hiddens_file,/dev/null -shared -T10000000 -B symbolic -o proe-brl.dll proe-brl.o /opt/ptc/proe2001/protoolkit/sgi_mips4/obj/protk_dll.a /opt/ptc/proe2001/prodevelop/sgi_mips4/obj/prodev_dll.a -L/usr/brlcad/lib -lbu -lbn -ltcl -lc -lm -lw

#gcc -shared  libproe2brl_la-proe-brl.o   -L/vld/morrison/brlcad/src/libbu/.libs -L/vld/morrison/brlcad/src/other/libtcl/.libs /opt/ptc/proe2001/protoolkit/sgi_mips4/obj/protk_dll.a /opt/ptc/proe2001/prodevelop/sgi_mips4/obj/prodev_dll.a ../../src/libbu/.libs/libbu.so ../../src/libbn/.libs/libbn.so ../../src/other/libtcl/.libs/libtcl8.4.so -lc -lm -lw -lmx -lmalloc    -lc  -Wl,-soname -Wl,libproe2brl.so.1 `test -n "sgi1.0" && echo -Wl,-set_version -Wl,sgi1.0` -Wl,-update_registry -Wl,.libs/so_locations -o .libs/libproe2brl.so.1.0

#/usr/bin/ld -n32 -shared  libproe2brl_la-proe-brl.o   -L/vld/morrison/brlcad/src/libbu/.libs -L/vld/morrison/brlcad/src/other/libtcl/.libs /opt/ptc/proe2001/protoolkit/sgi_mips4/obj/protk_dll.a /opt/ptc/proe2001/prodevelop/sgi_mips4/obj/prodev_dll.a ../../src/libbu/.libs/libbu.so ../../src/libbn/.libs/libbn.so ../../src/other/libtcl/.libs/libtcl8.4.so -lc -lm -lw -lmx -lmalloc    -lc  -soname libproe2brl.so.1 `test -n "sgi1.0" && echo -set_version sgi1.0` -update_registry .libs/so_locations -o .libs/libproe2brl.so.1.0

# libproe2brl_la_SOURCES = proe-brl.c
# libproe2brl_la_CFLAGS = $(AM_CFLAGS)
# libproe2brl_la_LDFLAGS = -multigot -rpath $(libdir) -T10000000 -B symbolic
# libproe2brl_la_LIBADD = $(PTLIB_DLL) $(PDLIB_DLL) $(BRLLIBS) $(SYSLIBS)
# libproe2brl_sodir = $(BRLCAD_DATA)/pro-engineer/$(PRO_MACHINE_TYPE)

libproe2brl.so: mk
	make -f mk dll

all-am: Makefile
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
	@if test -f libproe2brl.so ; then \
		@ECHO@ "Installing libproe2brl.so" ; \
		if ! test -d $(DESTDIR)$(proe_brldir) ; then \
			mkdir -p $(DESTDIR)$(proe_brldir) ; \
		fi ; \
		$(INSTALL) libproe2brl.so $(DESTDIR)$(proe_brldir)/libproe2brl.so ; \
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

plugin2: libproe2brl.so
	@@ECHO@ "---"
	@@ECHO@ "Run 'make unlock2' to unlock the Pro-Engineer .so plugin for distribution"
	@@ECHO@ "Run 'make install' to install the Pro-Engineer .so plugin"
	@@ECHO@

proedir = $(BRLCAD_DATA)/pro-engineer
tmsgdir = $(BRLCAD_DATA)/pro-engineer/text/usascii/
rsrcdir = $(BRLCAD_DATA)/pro-engineer/text/usascii/resource/

proe_DATA = \
	install.doc \
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
	protk.dat.in

CLEANFILES = libproe2brl.so proe-brl

DISTCLEANFILES = mk protk.dat

MAINTAINERCLEANFILES = Makefile.in


include $(top_srcdir)/misc/Makefile.defs

