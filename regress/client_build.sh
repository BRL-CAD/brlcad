#!/bin/sh
#
#  This script is intended to be run only by the client_build.sh script.
#  That is where the necessary environment variables are set.

args=`getopt b:d:e:qr:u: $*`

if [ $? != 0 ] ; then
	echo "Usage: $0 [-r brlcad_root] [-u user] -d regress_dir "
	exit 2
fi

set -- $args

MAILUSER=acst
QUIET=0
BRLCAD_ROOT=/usr/brlcad

for i in $* ; do
	case "$i" in
		-d)
			echo "-d $2";
			REGRESS_DIR=$2;
			export REGRESS_DIR
			shift 2;;
		-q)
			echo "-q";
			QUIET=1;
			shift;;
		-r)
			echo "-r $2";
			BRLCAD_ROOT=$2;
			shift 2;;
		-u)
			echo "-d $2";
			MAILUSER=$2;
			export MAILUSER
			shift 2;;
		--)
			shift; break;;
	esac
done


export QUIET
export BRLCAD_ROOT



PATH=$PATH:$BRLCAD_ROOT/bin
export PATH

mkdir $REGRESS_DIR/$ARCH
cd $REGRESS_DIR/brlcad

echo "$0: starting on `hostname`" >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo ""				  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo "PATH = $PATH"		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo "HOME = $HOME"		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
echo ""		  		  >> $REGRESS_DIR/$ARCH/MAKE_LOG


echo ./setup.sh -s		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./setup.sh -s			  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1 << EOF
yes
EOF

echo ./gen.sh -s all		  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./gen.sh -s all			  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1

echo "`hostname` compilation complete" >> $REGRESS_DIR/$ARCH/MAKE_LOG

echo ./gen.sh -s install	  >> $REGRESS_DIR/$ARCH/MAKE_LOG
./gen.sh -s install		  >> $REGRESS_DIR/$ARCH/MAKE_LOG 2>&1

echo "`hostname` $ARCH installation complete" >> $REGRESS_DIR/$ARCH/MAKE_LOG

$REGRESS_DIR/brlcad/regress/client_test.sh
