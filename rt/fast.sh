#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling RT using multiple processors.
#
#  $Header$

cake \
 do.o \
 material.o \
 opt.o \
 refract.o &

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
 bomb.o \
 rtshot.o \
 rtwalk.o &

wait
echo --- Collecting any stragglers.
cake rt
