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
#  $Header$	#
#								#
#################################################################

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

# Set to 0 for non-NFS environment (default), 1 for NFS configuration.
NFS=1

# Label number for this CAD Release,
# RCS main Revision number, and date.
#RELEASE=M.N;	RCS_REVISION=X;		REL=DATE=dd-mmm-yy
#RELEASE=3.16;	RCS_REVISION=9;		REL_DATE=Today
RELEASE=3.15;	RCS_REVISION=9;		REL_DATE=17-Jul-91	# Beta+3
#RELEASE=3.14;	RCS_REVISION=9;		REL_DATE=15-July-91	# Beta+2
#RELEASE=3.13;	RCS_REVISION=9;		REL_DATE=9-Jul-91	# Beta+1
#RELEASE=3.12;	RCS_REVISION=9;		REL_DATE=8-Jul-91	# Beta
#RELEASE=3.11;	RCS_REVISION=9;		REL_DATE=2-Jul-91	# alpha+1
#RELEASE=3.10;	RCS_REVISION=9;		REL_DATE=1-Jul-91	# alpha
#RELEASE=3.9;	RCS_REVISION=9;		REL_DATE=28-Jan-91	# internal
#RELEASE=3.8;	RCS_REVISION=9;		REL_DATE=3-Jan-91	# internal
#RELEASE=3.7;	RCS_REVISION=9;		REL_DATE=19-June-89
#RELEASE=3.6;	RCS_REVISION=9;		REL_DATE=25-May-89	# internal
#RELEASE=3.5;	RCS_REVISION=9;		REL_DATE=23-May-89	# internal
#RELEASE=3.4;	RCS_REVISION=8;		REL_DATE=18-May-89	# internal
#RELEASE=3.3;	RCS_REVISION=8;		REL_DATE=17-May-89	# internal
#RELEASE=3.2;	RCS_REVISION=8;		REL_DATE=05-May-89	# internal
#RELEASE=3.1;	RCS_REVISION=8;		REL_DATE=27-Apr-89	# internal
#RELEASE=3.0;	RCS_REVISION=8;		REL_DATE=10-Oct-88
#RELEASE=2.10;	RCS_REVISION=7;		REL_DATE=04-Oct-88	# internal
#RELEASE=2.9;	RCS_REVISION=7;		REL_DATE=31-Sep-88	# internal
#RELEASE=2.8;	RCS_REVISION=7;		REL_DATE=21-Sep-88	# internal
#RELEASE=2.7;	RCS_REVISION=7;		REL_DATE=12-Sep-88	# internal
#RELEASE=2.6;	RCS_REVISION=7;		REL_DATE=09-Sep-88	# internal
#RELEASE=2.5;	RCS_REVISION=7;		REL_DATE=08-Sep-88	# internal
#RELEASE=2.4;	RCS_REVISION=7;		REL_DATE=10-Jun-88	# internal
#RELEASE=2.3;	RCS_REVISION=7;		REL_DATE=02-Nov-87
#RELEASE=2.0;	RCS_REVISION=6;		REL_DATE=11-Jul-87
#RELEASE=1.24;	RCS_REVISION=5;		REL_DATE=11-Jun-87
#RELEASE=1.20;	RCS_REVISION=1;		REL_DATE=12-Feb-87

#
# Sanity check -- make sure that all the necessary programs have
# made it into the search path.  Otherwise, nothing will work.
# For this purpose, specifically exclude "dot" from the check.
#
NECESSARY_CMDS="cake cakesub cakeinclude machinetype.sh ranlib5.sh"
PATH_ELEMENTS=`echo $PATH | sed 's/^://
				s/:://g
				s/:$//
				s/:\\.:/:/g
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

DISTDIR=/m/dist/.
ARCHDIR=/m/.
ARCHIVE=${ARCHDIR}/cad${RELEASE}.tar

TOP_FILES="Copyright* README Cakefile* Makefile \
		cray.sh cray-ar.sh ranlib5.sh sgisnap.sh \
		machinetype.sh gen.sh setup.sh newvers.sh \
		cakeinclude.sh newbindir.sh"

# Has Cakefile, but no compilation or tools needed, not machine specific
ADIRS="h doc pix vfont whetstone awf brlman"

# Has no Cakefile, just copy it verbatim.  Only used in "dist" command.
CDIRS="cake cakeaux papers contributed patch"

# Source directories that will have Machine specific binary directories
# These will be built in the order listed.
# db depends on conv, conv depends on libwdb, libwdb depends on librt
BDIRS="bench \
	libsysv \
	libmalloc \
	libplot3 \
	libwdb \
	libpkg \
	libfb \
	rfbd \
	fbserv \
	libtermio \
	libcursor \
	libfont \
	liborle \
	librle \
	libfft \
	libnurb librt \
	conv \
	db \
	rt \
	remrt \
	mged \
	proc-db \
	comgeom-g \
	iges-g \
	fb \
	util \
	fbed \
	lgt \
	vas4 \
	vdeck \
	sig \
	tab \
	tools \
	halftone \
	edpix \
	nirt irprep \
	dhrystone"

# If there is no TCP networking, eliminate network-only directories.
if test "${HAS_TCP}" = "0"
then
	BDIRS=`echo ${BDIRS} | sed -e  's/libpkg//
					s/remrt//
					s/fbserv//
					s/rfbd//'`
fi

# If this is not an SGI 4D, eliminate SGI-specific directories
if test "${MACHINE}" != "4d"
then
	BDIRS=`echo ${BDIRS} | sed -e  's/edpix//'`
fi

if test "$1" = ""
then	TARGET=all
else	TARGET=$1
fi

# For handline multiple machines in an NFS environment
if test "${NFS}" = "1"
then
	DIRPRE=.
	DIRSUF=.${MACHINE}
else
	DIRPRE=
	DIRSUF=
fi


echo
echo "This Release = ${RELEASE} of ${REL_DATE}      Making Target: ${TARGET}"
echo "Has Symlinks = ${HAS_SYMLINKS}"
echo "   UNIX Type = ${UNIXTYPE}"
echo "     Has TCP = ${HAS_TCP}"
echo "     Machine = ${MACHINE}"
echo "         NFS = ${NFS}"
echo

# Now, actually work on making the target

case "${TARGET}" in

help)
	echo '	all		Makes all software'
	echo '	benchmark	Special:  Make only benchmark'
	echo '	clean		rm *.o, leave products'
	echo '	noprod		rm products, leave *.o'
	echo '	clobber		rm products and *.o'
	echo '	lint		run lint'
	echo '	install		install all products, with backups'
	echo '	install-nobak	install all products, without backups'
	echo '	uninstall	remove all products'
	echo '	print		print all sources to stdout'
	echo '	typeset		troff all manual pages'
	echo '	nroff		nroff all manual pages'
	echo '	mkdir		NFS: create binary dirs'
	echo '	relink		NFS: relink Cakefile'
	echo '	rmdir		NFS: remove binary dirs'
	;;

benchmark)
	if test x$NFS = x1
	then	sh $0 relink
	fi
	(cd ${DIRPRE}libsysv${DIRSUF};  cake -k)
	(cd ${DIRPRE}libmalloc${DIRSUF};  cake -k)
	(cd ${DIRPRE}bench${DIRSUF};  cake -k)
	(cd ${DIRPRE}libwdb${DIRSUF};  cake -k)
	(cd ${DIRPRE}libplot3${DIRSUF};  cake -k)
	if test ${HAS_TCP} = 1
	then
		(cd ${DIRPRE}libpkg${DIRSUF};  cake -k)  # needed for IF_REMOTE
	fi
	(cd ${DIRPRE}libfb${DIRSUF};  cake -k)
	(cd ${DIRPRE}libnurb${DIRSUF};  cake -k)
	(cd ${DIRPRE}librt${DIRSUF};  cake -k)
	(cd ${DIRPRE}conv${DIRSUF}; cake -k)
	(cd ${DIRPRE}db${DIRSUF}; cake -k)
	(cd ${DIRPRE}rt${DIRSUF};  cake -k)
	;;

#  These directives operate in the machine-specific directories
#
#  all		Build all products
#  clean	Remove all .o files, leave products
#  noprod	Remove all products, leave .o files
#  clobber	clean + noprod
#  lint
#  ls		ls -al of all subdirectories
all)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF}; cake -k )
	done;;

clean|noprod|clobber|lint)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF}; cake -k ${TARGET} )
	done;;

# Listing of source directories
ls)
	for dir in ${ADIRS} ${BDIRS} ${CDIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir}; ls -al )
	done;;

# Listing of binary directories
ls-bin)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF}; ls -al )
	done;;

# These operate in a mixture of places, treating both source and binary
install|install-nobak|uninstall)
	for dir in ${ADIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir}; cake -k ${TARGET} )
	done
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF}; cake -k ${TARGET} )
	done;;

#  These directives operate in the source directory
#
#  inst-dist	BRL-only:  inst sources in dist tree without inst products
#
install-man|inst-dist|print|typeset|nroff)
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
	if test x${DIRSUF} = x
	then
		echo "${TARGET}:  unnecessary in non-NFS environment"
		exit 0;		# Nothing to do
	fi
	if test x${HAS_SYMLINKS} = x1
	then	lnarg="-s"
	else	lnarg=""
	fi
	for dir in ${BDIRS}; do
		if test -d ${DIRPRE}${dir}${DIRSUF}
		then
			rm -f ${DIRPRE}${dir}${DIRSUF}/Cakefile
		else
			mkdir ${DIRPRE}${dir}${DIRSUF}
		fi
		(cd ${DIRPRE}${dir}${DIRSUF}; ln ${lnarg} ../${dir}/Cakefile .)
	done;;

rmdir)
	set -x
	for dir in ${BDIRS}; do
		rm -fr ${dir}${DIRSUF}
	done;;

shell)
	for dir in ${BDIRS}; do
		( cd ${dir}; echo ${dir}; /bin/sh )
	done;;

checkin)
	echo " RCS_Revision ${RCS_REVISION}"
	REL_NODOT=`echo ${RELEASE}|tr . _`
	CI_ARGS="-f -r${RCS_REVISION} -sRel${REL_NODOT} -mRelease_${RELEASE}"
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
	if test `grep '#define[ 	]*NFS' Cakefile.defs|wc -l` -eq 0
	then 	echo "Shipping non-NFS version of Cakefile.defs (this is good)";
	else
		echo "ERROR: Update Cakefile.defs for non-NFS before proceeding!"
		exit 1;
	fi
	if test `grep "^NFS=1" gen.sh|wc -l` -eq 0
	then 	echo "Shipping non-NFS version of gen.sh (this is good)";
	else
		echo "ERROR: Update gen.sh for non-NFS before proceeding!"
		exit 1;
	fi
	echo
	echo "I hope you have made Cakefile.defs tidy!"
	echo

	for i in ${CDIRS}
	do
		rm -fr ${DISTDIR}/$i
		mkdir ${DISTDIR}/$i
		# Get everything except the RCS subdirs
		cp $i/[!R]* ${DISTDIR}/$i/.
	done
	for i in ${TOP_FILES}
	do
		rm -f ${DISTDIR}/$i
	done
	cp ${TOP_FILES} ${DISTDIR}/.
	echo "Preparing the 'bench' directory"
	(cd bench; cake clobber; cake install)
	echo "End of BRL-CAD Release $RELEASE tape, `date`" > ${DISTDIR}/zzzEND
	cd ${DISTDIR}; du -a > Contents
	;;

# Use as final step of making a distribution -- write the archive
arch)
	cd ${DISTDIR}; tar cfv ${ARCHIVE} *
	# $4 will be file size in bytes (BSD machine)
	# $5 will be file size in bytes (SYSV machine)
	# pad to 200K byte boundary for SGI cartridge tapes.
	set -- `ls -l ${ARCHIVE}`
	PADBYTES=`echo "204800 $5 204800 %-p" | dc`
	if test ${PADBYTES} -lt 204800
	then
		gencolor -r${PADBYTES} 0 >> ${ARCHIVE}
	fi
	chmod 444 ${ARCHIVE}
	echo "${ARCHIVE} created"

	FTP_ARCHIVE=/usr/spool/ftp/tmp/cad${RELEASE}.tar
	rm -f /tmp/cad-exclude
	echo 'papers/*' >> /tmp/cad-exclude
	echo 'vfont/*' >> /tmp/cad-exclude
	echo 'contributed/*' >> /tmp/cad-exclude
	echo 'dmdfb/*' >> /tmp/cad-exclude
	echo 'doc/*' >> /tmp/cad-exclud
	echo 'pix/*' >> /tmp/cad-exclude
	/usr/gnu/bin/tar -cvf - -X /tmp/cad-exclude * |\
		crypt alphabeta | compress > ${FTP_ARCHIVE}-a.Z
		compress | crypt ${KEY} > ${FTP_ARCHIVE}-a.Z
	echo "${FTP_ARCHIVE}-a.Z created"
	echo "${FTP_ARCHIVE}-a.Z created (doc)"
	/usr/gnu/bin/tar cfv - Copy* README doc pix zzzEND |\
		crypt alphabeta | compress > ${FTP_ARCHIVE}-b.Z
		compress | crypt ${KEY} > ${FTP_ARCHIVE}-b.Z
	echo "${FTP_ARCHIVE}-b.Z created"
	echo "${FTP_ARCHIVE}-b.Z created (core 1)"
	/usr/gnu/bin/tar cfv - Copy* README papers dmdfb \
	    vfont contributed zzzEND |\
		crypt alphabeta | compress > ${FTP_ARCHIVE}-c.Z
		compress | crypt ${KEY} > ${FTP_ARCHIVE}-c.Z
	echo "${FTP_ARCHIVE}-c.Z created"
	rm -f ${EXCLUDE}
	;;

*)
	echo "$0: No code to make ${TARGET}, use 'help' for help"
	exit 1;;

esac

exit 0
