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

SHELL=/bin/sh
export SHELL

#  Local machine types to be dealt with on this machine.
#  Should just be string from mtype.sh unless this is an NFS or RFS server
MACHINES="sel"
MTYPE=`sh mtype.sh`

# Label number for this CAD Release
RELEASE=2.4

# RCS main Revision number
RCS_REVISION=8

# Release 1.20 was Revision 1
# Release 1.24 was Revision 5
# Release 2.0 was Revision 6
# Release 2.3 was Revision 7

DISTDIR=/m/dist/.
ARCHDIR=/m/.
ARCHIVE=${ARCHDIR}/cad${RELEASE}.tar

TOP_FILES="README Makefile Makefile.defs Makefile.rules \
		cray.sh cray-ar.sh mtype.sh"

# NOTE:  The following directories contain code which may not compile
# on all systems.  In that case, they should be removed from $DIRS:
#
#	libpkg		BSD or SGI networking, remove if SYSV
#	rfbd		BSD or SGI networking, remove if SYSV
#
ADIRS="h doc bench"	# No compilation or tools needed
PDIRS="pix"		# Do after BDIRS compilations done
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
	librle \
	libspl librt rt \
	mged \
	proc-db \
	util \
	fbed \
	lgt \
	vas4 \
	vdeck \
	whetstone dhrystone"

if test x$1 = x
then	TARGET=all
else	TARGET=$1
fi

echo; echo "Current Release = ${RELEASE}.  Making ${TARGET}"

case ${TARGET} in

benchmark)
	sh $0 depend
	sh $0 relink
	cd libsysv.${MTYPE}; make depend; make -k
	cd libmalloc.${MTYPE}; make depend; make -k
	cd conv.${MTYPE}; make -k
	cd db.${MTYPE}; make -k
	cd pix.${MTYPE}; make -k
	cd libpkg.${MTYPE}; make depend; make -k	  # needed for IF_REMOTE, rm if SYSV
	cd libfb.${MTYPE}; make depend; make -k
	cd libplot3.${MTYPE}; make depend; make -k
	cd libspl.${MTYPE}; make depend; make -k
	cd librt.${MTYPE}; make depend; make -k
	cd rt.${MTYPE}; make depend; make -k
	;;

#  These directives operate in the machine-specific directories
#
#  all		Build all products
#  clean	Remove all .o files, leave products
#  noprod	Remove all products, leave .o files
#  clobber	clean + noprod
#  lint
all|clean|noprod|clobber|lint)
	for dir in ${BDIRS}; do
		echo ---------- ${dir}.${MTYPE};
		( cd ${dir}.${MTYPE}; make -k \
			-f ../Makefile.${MTYPE} \
			-f Makefile.loc \
			-f ../Makefile.rules ${TARGET} )
	done
	for dir in ${PDIRS}; do
		echo ---------- ${dir};
		(cd ${dir}; make -k \
			-f ../Makefile.${MTYPE} \
			-f Makefile.loc \
			-f ../Makefile.rules ${TARGET} )
	done;;

# These operate in a mixture of places, treating both source and binary
install|uninstall)
	for dir in ${ADIRS} ${PDIRS}; do
		echo ---------- ${dir};
		(cd ${dir}; make -k \
			-f ../Makefile.${MTYPE} \
			-f Makefile.loc \
			-f ../Makefile.rules ${TARGET} )
	done
	for dir in ${BDIRS}; do
		echo ---------- ${dir}.${MTYPE};
		(cd ${dir}.${MTYPE}; make -k \
			-f ../Makefile.${MTYPE} \
			-f Makefile.loc \
			-f ../Makefile.rules ${TARGET} )
	done;;

#  These directives operate in the source directory
#  depend	Re-build Makefile.loc dependencies (WILL need "make relink")
#  inst-man
#  inst-dist	BRL-only:  inst sources in dist tree without inst products
depend|inst-man|inst-dist|print|typeset)
	for dir in ${ADIRS} ${PDIRS} ${BDIRS}; do
		echo ---------- ${dir};
		(cd $dir; make -k \
			-f ../Makefile.${MTYPE} \
			-f Makefile.loc \
			-f ../Makefile.rules ${TARGET} )

	done;;

mkdir)
	for dir in ${BDIRS}; do
		for mach in ${MACHINES}; do
			mkdir ${dir}.${mach}
			ln Makefile-subd ${dir}.${mach}/Makefile
			ln ${dir}/Makefile.loc ${dir}.${mach}
		done
	done;;

relink)
	for dir in ${BDIRS}; do
		for mach in ${MACHINES}; do
			rm -f ${dir}.${mach}/Makefile ${dir}.${mach}/Makefile.loc
			ln Makefile-subd ${dir}.${mach}/Makefile
			ln ${dir}/Makefile.loc ${dir}.${mach}
		done
	done;;

rmdir)
	set -x
	for dir in ${BDIRS}; do
		for mach in ${MACHINES}; do
			rm -r ${dir}.${mach}
		done
	done;;

shell)
	for dir in ${BDIRS}; do
		( cd ${dir}; echo ${dir}; /bin/sh )
	done;;

checkin)
	echo " RCS_Revision ${RCS_REVISION}"
	rcs -l ${TOP_FILES}
	ci -u -f -r${RCS_REVISION} -sRel "-MRelease ${RELEASE}" ${TOP_FILES}
	for dir in ${ADIRS} ${BDIRS} ${PDIRS}; do
		echo ---------- $dir;
		(cd $dir; rm -f Makefile.bak; \
		rcs -l *.[cshf1-9] Makefile*; \
		ci -u -f -r${RCS_REVISION} -sRel "-MRelease ${RELEASE}" *.[cshf1-9] Makefile* )
	done;;

# Do a "make install" as step 1 of making a distribution.
#
# Use "make dist" as step 2 of making a distribution.
dist)
	cp Copyright* README Makefile* cray.sh cray-ar.sh MTYPE.sh ${DISTDIR}
	(cd bench; make clobber; make install)
	echo "End of BRL-CAD Release $RELEASE tape, `date`" > ${DISTDIR}/zzzEND
	cd ${DISTDIR}; du -a > Contents
	;;
# Use as step 3 of making a distribution -- write the archive
# Use as final step of making a distribution -- write the archive
arch)
	# $4 will be file size in bytes, pad to 200K byte boundary for SGI
	set -- `ls -l ${ARCHIVE}` ; \
		PADBYTES=`echo "204800 $4 204800 %-p" | dc` ; \
		if test ${PADBYTES} -lt 204800 ; then \
			gencolor -r${PADBYTES} 0 >> ${ARCHIVE} ; \
		fi
	fi
	rm -f ${EXCLUDE}
	;;

	echo "$0: No code to make ${TARGET}"
	echo "$0: No code to make ${TARGET}, use 'help' for help"
	exit 1;;

esac

exit 0
