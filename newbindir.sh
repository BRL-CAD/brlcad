#!/bin/sh
#			N E W B I N D I R . S H
#
#  Script to permanantly modify your copy of the BRL-CAD distribution to
#  place the installed programs in some place other than BRL's usual place.
#
#  Changed from Release 4.0
#  Formerly, this script dumped everything into one place.  Messy.
#  Now, the whole "/usr/brlcad/" tree can be re-vectored anywhere you want,
#  while preserving the tree structure below there.
#
#  Once this script is run, the "master" definition of BASEDIR is
#  kept in "setup.sh".
#  XXX It would be smarter to put it in machinetype.sh
#
#  $Header$

eval `grep "^BASEDIR=" setup.sh`		# sets BASEDIR

echo "Current BASEDIR is $BASEDIR"
echo

if test x$1 = x
then	echo "Usage: $0 new_BASEDIR"
	exit 1
fi

NEW="$1"

if test ! -d $NEW
then	echo "Ahem, $NEW is not an existing directory.  Aborting."
	exit 1
fi

echo
echo "BASEDIR was $BASEDIR, will be $NEW"
echo

#  Replace one path with another, in all the files that matter.

for i in \
	Cakefile.defs setup.sh gen.sh \
	sh/machinetype.sh \
	cake/Makefile cakeaux/Makefile \
	remrt/remrt.c libbu/vfont.c \
	fb/cat-fb.c canon/canonserver.c \
	h/tcl.h libtk/tkInt.h \
	mged/cmd.c \
	brlman/awf brlman/brlman
do
	chmod 775 $i
	ed - $i << EOF
f
g,$BASEDIR,s,,$NEW,p
w
q
EOF
	chmod 555 $i
done
