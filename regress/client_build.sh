#!/bin/sh
#
#  This script is intended to be run only by the client_build.sh script.
#  That is where the necessary environment variables are set.

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
