#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling RT using multiple processors.
#
#  $Header$

cake \
 do.o \
 material.o \
 mathtab.o \
 opt.o \
 refract.o \
 sh_air.o \
 sh_fire.o &

cake \
 sh_cloud.o \
 sh_cook.o \
 sh_light.o &

cake \
 sh_marble.o \
 sh_plastic.o \
 sh_points.o \
 sh_spm.o &

cake \
 sh_stack.o \
 sh_stxt.o \
 sh_gauss.o \
 sh_paint.o \
 sh_noise.o \
 sh_text.o &

cake \
 view.o \
 viewcheck.o \
 viewg3.o &

cake \
 viewpp.o \
 viewrad.o \
 viewray.o &

cake \
 shade.o \
 worker.o \
 wray.o &

cake \
 main.o \
 rtshot.o \
 rtwalk.o &

wait
cake rt
