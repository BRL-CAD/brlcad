#!/bin/sh
#################################################################
#								#
#			gen.sh					#
#								#
#  The real code behind the Master Makefile for			#
#  the BRL CAD Software Distribution				#
#								#
#  Author -							#
#	Michael John Muuss					#
#								#
# Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005-5066		#
#								#
#  $Header$		#
#								#
#################################################################

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

#
# Sanity check -- make sure that all the necessary programs have
# made it into the search path.  Otherwise, nothing will work.
# For this purpose, specifically exclude "dot" from the check.
#
NECESSARY_CMDS="cake cakesub machinetype.sh cakeinclude.sh"
PATH_ELEMENTS=`echo $PATH | sed 's/^://
				s/:://g
				s/:$//
				s/:\\.://g
				s/:/ /g'`
for CMD in ${NECESSARY_CMDS}
do
	not_found=1		# Assume cmd not found
	for PREFIX in ${PATH_ELEMENTS}
	do
		if test -f ${PREFIX}/${CMD}
		then
			# This was -x, but older BSD systems don't do -x.
			if test -s ${PREFIX}/${CMD}
			then
				# all is well
				not_found=0
				break
			fi
			echo "$0 WARNING:  ${PREFIX}/${CMD} exists, but is not executable."
		fi
	done
	if test ${not_found} -ne 0
	then
		echo "$0 ERROR:  ${CMD} is not in your Shell search path!"
		echo "$0 ERROR:  Software installation can not proceed until this has been fixed."
		echo "$0 ERROR:  Consult installation directions for more information."
		exit 1		# Die
	fi
done

# This will set Shell variables MACHINE, UNIXTYPE, and HAS_TCP
eval `machinetype.sh -b`

# Label number for this CAD Release,
# RCS main Revision number, and date.
RELEASE=2.5;	RCS_REVISION=8;		REL_DATE=dd-mmm-yy
#RELEASE=2.4;	RCS_REVISION=8;		REL_DATE=10-Jun-88	# internal
#RELEASE=2.3;	RCS_REVISION=7;		REL_DATE=02-Nov-87
#RELEASE=2.0;	RCS_REVISION=6;		REL_DATE=11-Jul-87
#RELEASE=1.24;	RCS_REVISION=5;		REL_DATE=11-Jun-87
#RELEASE=1.20;	RCS_REVISION=1;		REL_DATE=12-Feb-87

DISTDIR=/m/dist/.
ARCHDIR=/m/.
ARCHIVE=${ARCHDIR}/cad${RELEASE}.tar

TOP_FILES="README Cakefile* \
		cray.sh cray-ar.sh mtype.sh"

# No compilation or tools needed, not machine specific
ADIRS="h doc papers cake cakeaux pix bench contributed"
# Machine specific directories
BDIRS="libsysv \
	libmalloc \
	conv \
	db \
	libpkg \
	libfb \
	rfbd \
	libtermio \
	libcursor \
	libplot3 \
	libtig \
	libwdb \
	libfont \
	librle \
	liburt \
	libspl librt rt \
	mged \
	proc-db \
	util \
	fbed \
	lgt \
	vas4 \
	vdeck \
	tools \
	whetstone dhrystone"

# If there is no TCP networking, eliminate network-only directories.
if test HAS_TCP = 0
then
	BDIRS=`echo ${BDIRS} | sed -e  's/libpkg//
					s/rfbd//'`
fi

if test x$1 = x
then	TARGET=all
else	TARGET=$1
fi

echo
echo "This Release = ${RELEASE} of ${REL_DATE}      Making Target: ${TARGET}"
echo "   UNIX Type = ${UNIXTYPE}"
echo "     Has TCP = ${HAS_TCP}"
echo "     Machine = ${MACHINE}"
echo

# Now, actually work on making the target

case ${TARGET} in

benchmark)
	sh $0 relink
	(cd libsysv.${MACHINE};  cake -k)
	(cd libmalloc.${MACHINE};  cake -k)
	(cd conv.${MACHINE}; cake -k)
	(cd db.${MACHINE}; cake -k)
	if test ${HAS_TCP} = 1
	then
		(cd libpkg.${MACHINE};  cake -k)  # needed for IF_REMOTE
	fi
	(cd libfb.${MACHINE};  cake -k)
	(cd libplot3.${MACHINE};  cake -k)
	(cd libspl.${MACHINE};  cake -k)
	(cd librt.${MACHINE};  cake -k)
	(cd rt.${MACHINE};  cake -k)
	;;

#  These directives operate in the machine-specific directories
#
#  all		Build all products
#  clean	Remove all .o files, leave products
#  noprod	Remove all products, leave .o files
#  clobber	clean + noprod
#  lint
all)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir}.${MACHINE};
		( cd ${dir}.${MACHINE}; cake -k )
	done;;

clean|noprod|clobber|lint)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir}.${MACHINE};
		( cd ${dir}.${MACHINE}; cake -k ${TARGET} )
	done;;

# These operate in a mixture of places, treating both source and binary
install|uninstall)
	for dir in ${ADIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir}; cake -k ${TARGET} )
	done
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir}.${MACHINE};
		( cd ${dir}.${MACHINE}; cake -k ${TARGET} )
	done;;

#  These directives operate in the source directory
#
#  inst-dist	BRL-only:  inst sources in dist tree without inst products
#
inst-man|inst-dist|print|typeset|nroff)
	for dir in ${ADIRS} ${BDIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir}; cake -k ${TARGET} )
	done;;

#  These directives are for managing the multi-machine objects.
#  These commands will need to be run once for each type of
#  machine to be supported, **while logged on to that type of machine**.
#	mkdir	create binary directories for current machine type
#	rmdir	remove binary directories for current machine type
#	relink	recreate links to SRCDIR/Cakefile for current mach type
mkdir|relink)
	if test ${UNIXTYPE} = BSD
	then	lnarg="-s"
	else	lnarg=""
	fi
	for dir in ${BDIRS}; do
		if test -d ${dir}.${MACHINE}
		then
			rm -f ${dir}.${MACHINE}/Cakefile
		else
			mkdir ${dir}.${MACHINE}
		fi
		(cd ${dir}.${MACHINE}; ln ${lnarg} ../${dir}/Cakefile .)
	done;;

rmdir)
	set -x
	for dir in ${BDIRS}; do
		rm -fr ${dir}.${MACHINE}
	done;;

shell)
	for dir in ${BDIRS}; do
		( cd ${dir}; echo ${dir}; /bin/sh )
	done;;

checkin)
	echo " RCS_Revision ${RCS_REVISION}"
	CI_ARGS='-f -r${RCS_REVISION} -sRel${RELEASE} -mRelease_${RELEASE}'
	rcs -l ${TOP_FILES}
	ci -u ${CI_ARGS} ${TOP_FILES}
	for dir in ${ADIRS} ${BDIRS}; do
		echo -------------------------------- $dir;
		(cd $dir; \
		rcs -l *.[cshf1-9] Cakefile; \
		ci -u ${CI_ARGS} *.[cshf1-9] Cakefile )
	done;;

#
# Steps in creating a distribution:
#	"make checkin"	to mark the RCS archives
#	"make install"	to install released binaries, copy over source tree.
#			(or "make inst-dist" for trial attempts)
#	"make dist"	to polish up distribution tree
#	"make arch"	to create TAR archive
#
dist)
	cp Copyright* README Cakefile* *.sh ${DISTDIR}
	echo "Preparing the 'bench' directory"
	echo "End of BRL-CAD Release $RELEASE tape, `date`" > ${DISTDIR}/zzzEND
	cd ${DISTDIR}; du -a > Contents
	;;
# Use as step 3 of making a distribution -- write the archive
# Use as final step of making a distribution -- write the archive
arch)
	# $4 will be file size in bytes, pad to 200K byte boundary for SGI
	# pad to 200K byte boundary for SGI cartridge tapes.
	PADBYTES=`echo "204800 $4 204800 %-p" | dc`
	PADBYTES=`echo "204800 $5 204800 %-p" | dc`
	then 
	then
		gencolor -r${PADBYTES} 0 >> ${ARCHIVE}
	fi
	rm -f ${EXCLUDE}
	;;

	echo "$0: No code to make ${TARGET}"
	echo "$0: No code to make ${TARGET}, use 'help' for help"
	exit 1;;

esac

exit 0
