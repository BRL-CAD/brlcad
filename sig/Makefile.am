# $Id$

bin_PROGRAMS = a-d d-a i-d d-i bw-d d-bw f-d d-f f-i i-f c-d i-a u-a u-d u-f u-bw d-u dwin dauto dmod dfft dconv imod umod ddisp fhor dstat dsel dpeak syn damdf dauto2 ihist istat ustat


BU=$(top_builddir)/libbu/libbu.la @TCL_LIBS@ @PTHREAD_LIBS@ -lm 
FB=$(top_builddir)/libfb/libfb.la @X_LIBS@ $(PKG) $(BU)
FFT=$(top_builddir)/libfft/libfft.la -lm
PKG=$(top_builddir)/libpkg/libpkg.la

a_d_SOURCES = a-d.c
a_d_LDADD = $(BU)

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
i_a_SOURCES = i-a.c
u_a_SOURCES = u-a.c
u_d_SOURCES = u-d.c
u_f_SOURCES = u-f.c
u_bw_SOURCES = u-bw.c

#  Processors

dwin_SOURCES = dwin.c coswin.c hamwin.c
dwin_LDADD = -lm
dauto_SOURCES = dauto.c
dmod_SOURCES = dmod.c
dmod_LDADD = -lm
dfft_SOURCES = dfft.c interp.c butter.c
dfft_LDADD = $(FFT)
dconv_SOURCES = dconv.c
dconv_LDADD = $(FFT)


ddisp_SOURCES = ddisp.c 
ddisp_LDADD = $(FB)

fhor_SOURCES = fhor.c 
fhor_LDADD = $(FB)

imod_SOURCES = imod.c
imod_LDADD = -lm
umod_SOURCES = umod.c
umod_LDADD = -lm


#  Misc

dstat_SOURCES = dstat.c
dsel_SOURCES = dsel.c
istat_SOURCES = istat.c
istat_LDADD = -lm
ustat_SOURCES = ustat.c
ustat_LDADD = -lm


#  Experimental

dpeak_SOURCES = dpeak.c
syn_SOURCES = syn.c
syn_LDADD = -lm
damdf_SOURCES = damdf.c
dauto2_SOURCES = dauto2.c
ihist_SOURCES = ihist.c

#include "../Cakefile.rules"
