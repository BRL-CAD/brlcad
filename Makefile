#################################################################
#								#
#			Makefile				#
#								#
#  The top-level Makefile for the BRL CAD Software Distribution	#
#								#
#  This exists mostly as a convienience, so folks can		#
#  type "make install" and such like, and get the expected	#
#  results, even in the face of the rather complex mechanism.	#
#  Thus, we make Make into a simple front-end for gen.sh	#
#								#
#  Author -							#
#	Michael John Muuss					#
#								#
# Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005-5066		#
#								#
#								#
#################################################################

# SHELL		= /bin/sh

# Main rule (default):
all:
	-@sh gen.sh $@
help:
	-@sh gen.sh $@
benchmark:
	-@sh gen.sh $@
fast:
	-@sh gen.sh $@
clean:
	-@sh gen.sh $@
noprod:
	-@sh gen.sh $@
clobber:
	-@sh gen.sh $@
lint:
	-@sh gen.sh $@
ls:
	-@sh gen.sh $@
ls-bin:
	-@sh gen.sh $@
depend:
	-@sh gen.sh $@
install:
	-@sh gen.sh $@
inst-nobak:
	-@sh gen.sh $@
uninstall:
	-@sh gen.sh $@
perms:
	-@sh gen.sh $@
inst-man:
	-@sh gen.sh $@
inst-dist:
	-@sh gen.sh $@
print:
	-@sh gen.sh $@
typeset:
	-@sh gen.sh $@
nroff:
	-@sh gen.sh $@
mkdir:
	-@sh gen.sh $@
relink:
	-@sh gen.sh $@
rmdir:
	-@sh gen.sh $@
wc:
	-@sh gen.sh $@
tclIndex:
	-@sh gen.sh $@
tags:
	-@sh gen.sh $@
TAGS:
	-@sh gen.sh $@
etags:
	-@sh gen.sh $@
command:
	-@sh gen.sh $@
rcs-lock:
	-@sh gen.sh $@
checkin:
	-@sh gen.sh $@
dist:
	-@sh gen.sh $@
arch:
	-@sh gen.sh $@
pkg:
	-@sh gen.sh $@
rpm:
	-@sh gen.sh $@
sunpkg:
	-@sh gen.sh $@
tcl:
	-@sh gen.sh $@
