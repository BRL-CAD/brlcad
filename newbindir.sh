#!/bin/sh

eval `grep "^BINDIR=" setup.sh`		# sets BINDIR
LIBDIR=`echo $BINDIR | sed -e 's/bin$/lib/'`
ETCDIR=`echo $BINDIR | sed -e 's/bin$/etc/'`

echo "Current BINDIR is $BINDIR"
echo

if test x$1 = x
then	echo "Usage: $0 new_BINDIR"
	exit 1
fi

NEW="$1"

echo "(For non-production installation, all dirs forced to new BINDIR)"
echo
echo "BINDIR was $BINDIR, will be $NEW"
echo "LIBDIR was $LIBDIR, will be $NEW"
echo "ETCDIR was $ETCDIR, will be $NEW"
echo

for i in Cakefile.defs setup.sh cray.sh cake/Makefile cakeaux/Makefile
do
	chmod 664 $i
	ed - $i << EOF
f
g,$BINDIR,s,,$NEW,p
g,$LIBDIR,s,,$NEW,p
g,$ETCDIR,s,,$NEW,p
w
q
EOF
done
