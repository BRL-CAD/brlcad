#!/bin/sh
#
#  This script is intended to be run only by the client_build.sh script.
#  That is where the necessary environment variables are set.

. `dirname $0`/library


args=`getopt b:d:e:qr:u: $*`

if [ $? != 0 ] ; then
	echo "Usage: $0 [-r brlcad_root] [-u user] -d regress_dir "
	exit 2
fi

set -- $args

MAILUSER=lamas
QUIET=0
initializeVariable REGRESS_DIR /tmp/`whoami`

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

initializeVariable ARCH `${REGRESS_DIR}/brlcad/sh/machinetype.sh`
initializeVariable BRLCAD_ROOT $REGRESS_DIR/brlcad.$ARCH
export QUIET
export BRLCAD_ROOT
export ARCH

PATH="$BRLCAD_ROOT/bin:$PATH" ; export PATH

STARTPWD=$PWD

if [ ! -d ${REGRESS_DIR}/.regress.$ARCH ] ; then mkdir ${REGRESS_DIR}/.regress.$ARCH ; fi

if [ -f ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG ] ; then mv ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG.bak ; fi

cd ${REGRESS_DIR}/brlcad

#echo "client_build.sh" >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo "===============" >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo "$0: starting on $HOSTNAME" >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo ""				  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo "PATH = $PATH"		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo "HOME = $HOME"		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
#echo ""		  		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG


echo ./setup.sh -s		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
./setup.sh -s			  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG 2>&1 << EOF
yes
EOF

echo ./gen.sh -s all		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
./gen.sh -s all			  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG 2>&1

echo "Compilation Complete" >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG

echo ./gen.sh -s install	  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG
./gen.sh -s install		  >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG 2>&1

echo "Installation Complete" >> ${REGRESS_DIR}/.regress.${ARCH}/MAKE_LOG

#echo "RUN ${REGRESS_DIR}/brlcad/regress/client_test.sh"

cd $STARTPWD

echo "Testing package"
echo "Running [client_test.sh -d $REGRESS_DIR]"
`dirname $0`/client_test.sh -d $REGRESS_DIR

echo "Done client_build.sh"
