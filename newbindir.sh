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
#  kept in "machinetype.sh"
#
#  Changed since Release 4.5:
#  Nearly everything responds to the environment variable BRLCAD_ROOT,
#  and either obtains it in C by calling bu_brlcad_path(),
#  or in scripts via "machinetype.sh -v".
#  Cake has been modified to pluck BRLCAD_ROOT out of it's envinronment
#  and send it through to CPP, so even the Cakefiles can see it.
#
#  $Header$

eval `grep "^BASEDIR=" sh/machinetype.sh`		# sets BASEDIR

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
	libbu/brlcad_path.c \
	canon/canonserver.c
do
	if [ -f $i ] ; then

		mv -f $i ${i}.oldbindir
		cp ${i}.oldbindir ${i}
		chmod 775 $i
		ed - $i << EOF
f
g,$BASEDIR,s,,$NEW,gp
w
q
EOF
		chmod 555 $i
	fi

done
