# $Id$

bin_PROGRAMS = jove findcom
bin_SCRIPTS = teach-jove

jove_SOURCES = jove_buf.c jove_c.c jove_delete.c jove_disp.c jove_draw.c jove_extend.c jove_funcs.c jove_insert.c jove_io.c jove_main.c jove_marks.c jove_misc.c jove_proc.c jove_re.c jove_screen.c jove_term.c jove_wind.c tune.c version.c
jove_LDADD = @CURSES@
findcom_SOURCES = findcom.c tune.c

BUILT_SOURCES = tune.c version.c

man_MANS = jove.1 describe.com.1 teach-jove.1

tune.c:	$(top_srcdir)/jove/jove_tune.c
	rm -f tune.c
	sed -e 's;BinDir;BINDIR;' -e 's;LibDir;LIBDIR;' < $(top_srcdir)/jove/jove_tune.c > tune.c

version.c:
	sh $(top_srcdir)/jove/mkversion.sh

teach-jove:	$(top_srcdir)/jove/teach-jove.sh
	sed -e 's;BinDir;BINDIR;' -e 's;LibDir;LIBDIR;' < $(top_srcdir)/jove/teach-jove.sh > teach-jove


install-data-local:
	$(INSTALL_DATA) $(srcdir)/describe.com.1 $(libdir)/describe.com
	$(INSTALL_DATA) $(srcdir)/teach-jove.1 $(libdir)/teach-jove

EXTRA_DIST = $(man_MANS) describe.com.1 teach-jove.1 jove_tune.c
