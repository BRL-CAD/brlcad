#!/bin/sh
#  $Header$

eval `grep "^BINDIR=" setup.sh`		# sets BINDIR
LIBDIR=`echo $BINDIR | sed -e 's/bin$/lib/'`
ETCDIR=`echo $BINDIR | sed -e 's/bin$/etc/'`
INCLUDE_DIR=/usr/include/brlcad

echo "Current BINDIR is $BINDIR"
echo

if test x$1 = x
then	echo "Usage: $0 new_BINDIR"
	exit 1
fi

NEW="$1"

if test ! -d $NEW
then	echo "Ahem, $NEW is not an existing directory"
	exit 1
fi

echo "(For non-production installation, all dirs forced to new BINDIR)"
echo
echo "BINDIR was $BINDIR, will be $NEW"
echo "LIBDIR was $LIBDIR, will be $NEW"
echo "ETCDIR was $ETCDIR, will be $NEW"
echo "MANDIR and INCLUDE_DIR will be $NEW"
echo

for i in Cakefile.defs setup.sh cray.sh cake/Makefile cakeaux/Makefile
do
	chmod 775 $i
	ed - $i << EOF
f
g,$BINDIR,s,,$NEW,p
g,$LIBDIR,s,,$NEW,p
g,$ETCDIR,s,,$NEW,p
g,$INCLUDE_DIR,s,,$NEW,p
g,INCLUDE_DIR	.*,s,,INCLUDE_DIR	$NEW,p
g,MANDIR	.*,s,,MANDIR	$NEW,p
w
q
EOF
done
