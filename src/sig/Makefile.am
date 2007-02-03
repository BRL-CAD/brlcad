# $Id$

bin_PROGRAMS = \
	a-d \
	bw-d \
	c-d \
	d-a \
	d-bw \
	d-f \
	d-i \
	d-u \
	damdf \
	dauto \
	dauto2 \
	dconv \
	ddisp \
	dfft \
	dmod \
	dpeak \
	dsel \
	dstat \
	dwin \
	f-d \
	f-i \
	fhor \
	i-a \
	i-d \
	i-f \
	ihist \
	imod \
	istat \
	syn \
	u-a \
	u-bw \
	u-d \
	u-f \
	umod \
	ustat

AM_CPPFLAGS = ${TCL_CPPFLAGS}

noinst_HEADERS = complex.h

a_d_SOURCES = a-d.c
a_d_LDADD = ${BU}

d_a_SOURCES = d-a.c
i_d_SOURCES = i-d.c
d_i_SOURCES = d-i.c
d_u_SOURCES = d-u.c
bw_d_SOURCES = bw-d.c
d_bw_SOURCES = d-bw.c
d_f_SOURCES = d-f.c
f_d_SOURCES = f-d.c
f_i_SOURCES = f-i.c
i_f_SOURCES = i-f.c

c_d_SOURCES = c-d.c
c_d_LDADD = ${LIBM}

i_a_SOURCES = i-a.c
u_a_SOURCES = u-a.c
u_d_SOURCES = u-d.c
u_f_SOURCES = u-f.c
u_bw_SOURCES = u-bw.c

#  Processors

dwin_SOURCES = dwin.c coswin.c hamwin.c
dwin_LDADD = ${LIBM}

dauto_SOURCES = dauto.c
dauto_LDADD = ${BU}

dmod_SOURCES = dmod.c
dmod_LDADD = ${BU} ${LIBM}

dfft_SOURCES = dfft.c interp.c butter.c
dfft_LDADD = ${FFT}

dconv_SOURCES = dconv.c
dconv_LDADD = ${FFT}

ddisp_SOURCES = ddisp.c
ddisp_LDADD = ${FB}

fhor_SOURCES = fhor.c
fhor_LDADD = ${FB}

imod_SOURCES = imod.c
imod_LDADD = ${BU} ${LIBM}

umod_SOURCES = umod.c
umod_LDADD = ${BU} ${LIBM}


#  Misc

dstat_SOURCES = dstat.c
dstat_LDADD = ${LIBM}

dsel_SOURCES = dsel.c

istat_SOURCES = istat.c
istat_LDADD = ${BU} ${LIBM}

ustat_SOURCES = ustat.c
ustat_LDADD = ${BU} ${LIBM}


#  Experimental

dpeak_SOURCES = dpeak.c

syn_SOURCES = syn.c
syn_LDADD = ${LIBM}

damdf_SOURCES = damdf.c
dauto2_SOURCES = dauto2.c
ihist_SOURCES = ihist.c

# XXX need to review why these are not built
EXTRA_DIST = \
	d2-c.c \
	fhor2.c \
	fhor3.c \
	smod.c

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(a_d_OBJECTS) \
	$(bw_d_OBJECTS) \
	$(c_d_OBJECTS) \
	$(d_a_OBJECTS) \
	$(d_bw_OBJECTS) \
	$(d_f_OBJECTS) \
	$(d_i_OBJECTS) \
	$(d_u_OBJECTS) \
	$(damdf_OBJECTS) \
	$(dauto_OBJECTS) \
	$(dauto2_OBJECTS) \
	$(dconv_OBJECTS) \
	$(ddisp_OBJECTS) \
	$(dfft_OBJECTS) \
	$(dmod_OBJECTS) \
	$(dpeak_OBJECTS) \
	$(dsel_OBJECTS) \
	$(dstat_OBJECTS) \
	$(dwin_OBJECTS) \
	$(f_d_OBJECTS) \
	$(f_i_OBJECTS) \
	$(fhor_OBJECTS) \
	$(i_a_OBJECTS) \
	$(i_d_OBJECTS) \
	$(i_f_OBJECTS) \
	$(ihist_OBJECTS) \
	$(imod_OBJECTS) \
	$(istat_OBJECTS) \
	$(syn_OBJECTS) \
	$(u_a_OBJECTS) \
	$(u_bw_OBJECTS) \
	$(u_d_OBJECTS) \
	$(u_f_OBJECTS) \
	$(umod_OBJECTS) \
	$(ustat_OBJECTS) \
	$(bin_PROGRAMS)
