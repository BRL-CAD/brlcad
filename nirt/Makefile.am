# $Id$

bin_PROGRAMS = nirt

nirt_SOURCES = nirt.c command.c conversion.c if.c interact.c bsphere.c read_mat.c str_manip.c parse_fmt.c
noinst_HEADERS = nirt.h usrfmt.h
nirt_LDADD = @RT@ @BN@ @FB@
nirt_CFLAGS = -DDFLT_SHELL=\"/bin/sh\"
man_MANS = nirt.1 showshot.1
EXTRA_DIST = $(man_MANS)
