# $Id$
lib_LTLIBRARIES = liboptical.la

SRCS = init.c material.c photonmap.c refract.c shade.c sh_air.c sh_billboard.c sh_brdf.c sh_camo.c sh_cloud.c sh_cook.c sh_fbm.c sh_fire.c sh_flat.c sh_gauss.c sh_grass.c sh_light.c sh_noise.c sh_null.c sh_plastic.c sh_points.c sh_prj.c sh_rtrans.c sh_scloud.c sh_spm.c sh_stack.c sh_stxt.c sh_tcl.c sh_text.c sh_toyota.c sh_treetherm.c sh_wood.c sh_xxx.c turb.c wray.c

liboptical_la_SOURCES = $(SRCS) vers.c
CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh liboptical_version liboptical
