# $Id$

bin_PROGRAMS = \
	all_sf \
	display \
	firpass \
	ir-X \
	ir-sgi \
	irdisp \
	secpass \
	shapefact

all_sf_LDADD = @RT@ @BN@
all_sf_SOURCES = all_sf.c

display_LDADD = @RT@ @BN@
display_SOURCES = display.c

firpass_LDADD = @RT@ @BN@
firpass_SOURCES = firpass.c subroutines.c

irdisp_LDADD = @FB@
irdisp_SOURCES = irdisp.c
irdisp_CFLAGS = @X_CFLAGS@

ir_sgi_LDADD = @FB@
ir_sgi_SOURCES = ir-sgi.c

ir_X_LDADD = @X_LIBS@
ir_X_SOURCES = ir-X.c
ir_X_CFLAGS = @X_CFLAGS@

secpass_LDADD = @RT@ @BN@
secpass_SOURCES = secpass.c subroutines.c

shapefact_LDADD = @RT@ @BN@
shapefact_SOURCES = shapefact.c


man_MANS = \
	all_sf.1 \
	display.1 \
	firpass.1 \
	irdisp.1 \
	ir-sgi.1 \
	ir-X.1 \
	pictsgi.1 \
	pictx.1 \
	secpass.1 \
	see.1 \
	see2.1 \
	shapefact.1

EXTRA_DIST = $(man_MANS)
