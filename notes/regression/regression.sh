#!/bin/sh
#			R E G R E S S I O N . S H		#
#  Author -						 	#
#	Michael John Muuss					#
#								#
# Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005-5066		#
#								#
#################################################################

if [ $# -gt 0 -a X$1 = X-s ] ; then
	SILENT=-s
	shift
else
	SILENT=""
fi


# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

#
# Existence check -- make sure that all the necessary programs
# have made it into the search path (including "dot").  Otherwise, 
# they can't be checked for validity.
#
SCRIPTS="\
any-png.sh
brlcad-check.sh
cadbug.sh
cakeinclude.sh
cray.sh
font.c
machinetype.sh
morphedit.tcl
newvers.sh
pixinfo.sh
pixread.sh
pixwrite.sh
ranlib5.sh
sgisnap.sh
sharedliblink.sh
sharedlibvers.sh
show.sh
"

FRAMEBUFFER_COMMANDS="
fbanim
fbcbars
fbclear
fbcmap
fbcmrot
fbcolor
fbed
fbfade
fbframe
fbfree
fbgamma
fbgammamod
fbgrid
fbhelp
fblabel
fbline
fbpoint
fbscanplot
fbserv
fbstretch
fbzoom
"

DATACONVERSION_COMMANDS="
a-d
alias-pix
ap-pix
asc-nmg
asc-pl
asc2g
asc2pix
bw-a
bw-d
bw-fb
bw-imp
bw-pix
bw-png
bw-ps
bw-rle
bw3-pix
c-d
cat-fb
cell-fb
cmap-fb
comgeom-g
conv-vg2g
cubitorle
cy-g
d-a 
d-bw
d-f
d-i
double-asc
dpix-pix
dxf-g
euclid-g
f-d
f-i
fast4-g
fb-bw
fb-cmap
fb-fb
fb-orle
fb-pix
fb-png 
fb-rle
g-acad
g-euclid
g-euclid1
g-iges
g-jack
g-nmg
g-off
g-shell.rect
g-tankill
g-vrml
g-wave
g2asc
g4-g5
gif-fb
gif2fb
giftorle
graytorle
i-a
i-d
i-f
iges-g
ir-X
ir-sgi
jack-g
mac-pix
nastran-g
nmg-polysolid
nmg-rib
off-g
orle-fb
op-bw
orle-pix
painttorle
patch-g
pcd-pix
pix-alias
pix-bw
pix-bw3
pix-fb
pix-orle
pix-png
pix-ppm
pix-ps
pix-rle
pix-spm
pix-sun
pix-yuv
pix2asc
pixflip-fb
pl-asc
pl-fb
pl-hpgl
pl-pl
pl-ps
pl-sgi
pl-tek
png-bw
png-fb
png-pix
polar-fb
poly-bot
pp-fb
proe-g
rle-fb
rle-pix
script-tab
sgi-pix
spm-fb
stl-g
sun-pix
tankill-g
txyz-pl
u-a
u-bw
u-d
u-f
viewpoint-g
xyz-pl
yuv-pix
rletoabA60
rletoabA62
rletoascii
rletogif
rletogray
rletopaint
rletops
rletoraw
"

ANIMATION_COMMANDS="
anim_cascade
anim_fly
anim_hardtrack
anim_keyread
anim_lookat
anim_offset
anim_orient
anim_script
anim_sort
anim_time
anim_track
anim_turn
"

PIX_COMMANDS="
pix3filter
pixautosize
pixbackgnd
pixbgstrip
pixblend
pixborder
pixbustup
pixclump
pixcolors
pixcount
pixcut
pixdiff
pixdsplit
pixelswap
pixembed
pixfade
pixfields
pixfieldsep
pixfilter
pixhalve
pixhist
pixhist3d
pixhist3d-pl
pixinterp2x
pixmatte
pixmerge
pixmorph
pixpaste
pixrect
pixrot
pixsaturate
pixscale
pixshrink
pixstat
pixsubst
pixtile
pixuntile
"

RLE_COMMANDS="
rleClock
rleaddcom
rlebg
rlebox
rlecat
rlecomp
rledither
rleflip
rlehdr
rlehisto
rleldmap
rlemandl
rlenoise
rlepatch
rleprint
rlequant
rlescale
rleselect
rlesetbg
rlespiff
rlesplice
rlesplit
rlestereo
rleswap
rlezoom
"

COMMANDS="

FRAMEBUFFER_COMMANDS
DATACONVERSION_COMMANDS
PIX_COMMANDS
RLE_COMMANDS

all_sf
applymap
avg4
awf
azel
bary
brlman
buffer
burst
bwcrop
bwdiff
bwfilter
bwhist
bwhisteq
bwish
bwmod
bwrect
bwrot
bwscale
bwshrink
bwstat
bwthresh
cad_boundp
cad_parea
cake
cakeinclude
cakesub
chan_add
chan_mult
chan_permute
crop
cv
damdf
dauto
dauto2
dbcp
dconv
ddisp
decimate
dfft
display
dmod
dpeak
dsel
dsp_add
dstat
dunncolor
dunnsnap
dwin
euclid_format
euclid_unformat
fake
fant
fhor
files-tape
findcom
firpass
g_diff
g_lint
gencolor
halftone
hd
ihist
imgdims
imod
into
irdisp
istat
jove
lgt
loop
mcut
mergechan
mged
mk_bolt
mk_gastank
mk_handle
mk_window
mk_winfrm
mk_wire
msrandom
mst
nirt
plcolor
pldebug
plgetframe
plline2
plrot
plstat
png_info
pyrmask
query
rawtorle
remapid
remrt
repos
rpatch
rtcell
rtcheck
rtfrac
rtg3
rthide
rtpp
rtrad
rtrange
rtray
rtregis
rtscale
rtshot
rtsil
rtwalk
rtweight
rtxray
scriptsort
secpass
shapefact
smush
syn
tabinterp
tabsub
targatorle
tclsh
teach-jove
terrain
texturescale
to8
tobw
ttcp
umod
unexp
unslice
ustat
vas4
vdeck
wasatchrle
wavelet
wish
"


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

# This will set Shell variables MACHINE, UNIXTYPE, HAS_TCP, and BASEDIR
eval `machinetype.sh -b`
BRLCAD_ROOT=${BASEDIR}
export BRLCAD_ROOT

CAKE=../cake.$MACHINE/cake

if [ ! -f Cakefile.defs -a X$1 != Xdist ] ; then
	echo "WARNING: $0 should be run from the root of the brlcad source tree."
	echo "WARNING: Many operations won't work (but some will)"
	echo ""
fi
DISTDIR=dist
ARCHDIR=arch
ARCHIVE=${ARCHDIR}/cad${RELEASE}.tar

# Every shell script to be distributed must be listed here.
# Scripts to be installed in $BINDIR need to be listed in setup.sh $SCRIPTS also.
TOP_FILES="Copyright* README Cakefile* Makefile Acknowledgements \
		gen.sh setup.sh newbindir.sh"

# Has Cakefile, but no compilation or tools needed, not machine specific
ADIRS="h doc pix vfont awf brlman"

# Has no Cakefile, just copy it (and all sub-directories!) verbatim.
# Only used in "dist" command.
CDIRS="sh cake cakeaux html"

# Source directories that will have Machine specific binary directories
# These will be built in the order listed.
# db depends on conv, conv depends on libwdb, libwdb depends on librt
BDIRS="bench \
	libsysv \
	libbu libbn \
	libwdb \
	libpkg \
	libfb \
	libtcl8.3 \
	libtk8.3 \
	libitcl3.2 \
	libdm \
	libz \
	libpng \
	fbserv \
	librt \
	liboptical \
	libmultispectral \
	libtermio \
	libtclcad \
	conv \
	db \
	rt \
	mged \
	remrt \
	libcursor \
	liborle \
	libutahrle \
	libfft \
	proc-db \
	jack \
	mk \
	comgeom-g \
	iges \
	fb \
	util \
	fbed \
	lgt \
	patch \
	vas4 \
	vdeck \
	sig \
	tab \
	tools \
	anim \
	off \
	halftone \
	nirt \
	irprep \
	jove \
	canon \
	burst \
	gtools \
	bwish \
"			# This ends the list.

TSDIRS=". mged nirt pl-dm lib util"
HTML_DIRS="html/manuals html/manuals/shaders html/manuals/Anim_Tutorial html/manuals/libdm html/manuals/mged html/manuals/mged/animmate html/manuals/librt html/manuals/libbu html/manuals/cadwidgets html/ReleaseNotes html/ReleaseNotes/Rel5.0 html/ReleaseNotes/Rel5.0/Summary"
INSTALL_ONLY_DIRS="sample_applications $HTML_DIRS"
PROE_DIRS=". sun4_solaris sgi_elf2 text text/fullhelp text/menus"

# If there is no TCP networking, eliminate network-only directories.
if test "${HAS_TCP}" = "0"
then
	BDIRS=`echo ${BDIRS} | sed -e  's/libpkg//
					s/remrt//
					s/fbserv//'`
fi

# If this is not an SGI, eliminate SGI direct-SCSI-library-specific directories
case "${MACHINE}" in
	4d|5d|6d|7d|m3i6*|m4i6*)
		# These platforms all have libds.a
		;;
	*)
		BDIRS=`echo ${BDIRS} | sed -e  's/canon//' `
		;;
esac

# If this system has good vendor-provided libraries, use them.
# Remove them from the list of targets to be compiled.
# Two common sets:
#	libtcl, libtk
#	libpng, libz
# Needs to be coordinated with setting of LIBTCL_DIR LIBTK_DIR LIBZ_DIR
# in architecture-specific entry in Cakefile.defs
case "${MACHINE}" in
	li|fbsd)
		BDIRS=`echo ${BDIRS} | \
			sed -e 's/libz//' -e 's/libpng//'`
		;;
	7d|m4i65)
		# Be sure to look in /usr/lib64, not /usr/lib!
		BDIRS=`echo ${BDIRS} | \
		    sed -e 's/libz//' `
		;;
esac


if test X"$1" = X""
then	TARGET=all
else	TARGET=$1; shift
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


if test "${SILENT}" = ""
then
	echo
	echo "This Release = ${RELEASE} of ${REL_DATE}      Making Target: ${TARGET}"
	echo " BRLCAD_ROOT = ${BRLCAD_ROOT}"
	echo "Has Symlinks = ${HAS_SYMLINKS}"
	echo "   UNIX Type = ${UNIXTYPE}"
	echo "     Has TCP = ${HAS_TCP}"
	echo "     Machine = ${MACHINE}"
	echo "         NFS = ${NFS}"
	echo
fi

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
	echo '  command		run a command in each dir'
	;;

benchmark)
	if test x$NFS = x1
	then	sh $0 relink
	fi
	(T=libsysv; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT} )
	(T=bench; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT} )
	(T=libwdb; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=libpkg; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})  # needed for IF_REMOTE
	(T=libfb; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=libbu; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=libbn; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=librt; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=liboptical; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=libtcl8.3; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=libitcl3.2; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=rt; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=conv; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	(T=db; echo ${T}; cd ${DIRPRE}${T}${DIRSUF} && cake -k ${SILENT})
	;;

#  These directives operate in the machine-specific directories
#
#  all		Build all products
#  clean	Remove all .o files, leave products
#  noprod	Remove all products, leave .o files
#  clobber	clean + noprod
#  lint
#  ls		ls -al of all subdirectories
#
#  If operations are enclosed in sub-shells "(...)",
#  then on newer systems, ^C will only terminate the sub-shell
all)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		if test ! -d ${DIRPRE}${dir}${DIRSUF}
		then	continue;
		fi
		cd ${DIRPRE}${dir}${DIRSUF}
		cake -k ${SILENT}
		cd ..
	done;;

# Just like "all", but will use the "fast.sh" scripts when available.
fast)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		if test ! -d ${DIRPRE}${dir}${DIRSUF}
		then	continue;
		fi
		cd ${DIRPRE}${dir}${DIRSUF}
		if test -f ../${dir}/fast.sh
		then
			../${dir}/fast.sh ${SILENT}
		else
			cake -k ${SILENT}
		fi
		cd ..
	done;;

clean|noprod|clobber|lint)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF} && cake -k ${SILENT} ${TARGET} )
	done
	( cd ${DIRPRE}cake${DIRSUF} && make -k ${TARGET} )
	( cd ${DIRPRE}cakeaux${DIRSUF} && make -k ${TARGET} )
	;;
	

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
		( cd ${dir} && cake -k ${SILENT} ${TARGET} )
	done
	for dir in ${BDIRS}; do
		echo -------------------------------- ${DIRPRE}${dir}${DIRSUF};
		( cd ${DIRPRE}${dir}${DIRSUF} && cake -k ${SILENT} ${TARGET} )
	done
	for dir in ${TSDIRS}; do
		echo -------------------------------- tclscripts/${dir};
		( cd tclscripts/${dir} && cake -k ${SILENT} ${TARGET} )
	done
	for dir in ${INSTALL_ONLY_DIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir} && cake -k ${SILENT} ${TARGET} )
	done
	for dir in ${PROE_DIRS}; do
		echo -------------------------------- pro-engineer/${dir};
		( cd pro-engineer/${dir} && cake -k ${SILENT} ${TARGET} )
	done;;

perms)
	#########################################
	#
	#Set File/directory ownership/permissions
	#
	#########################################

	chown -R bin /usr/brlcad
	chgrp -R bin /usr/brlcad

	for dir in /usr/brlcad/etc /usr/brlcad/html /usr/brlcad/include \
		/usr/brlcad/man /usr/brlcad/sample_applications \
		/usr/brlcad/tclscripts /usr/brlcad/vfont; do

		find ${dir} -type f -exec chmod 664 {} \;
	done

	#
	# We leave tcl and tk alone since they are well set already
	#

	chmod 775 /usr/brlcad/bin/*
	find /usr/brlcad/lib -type f -exec chmod 664 {} \;
	find /usr/brlcad/lib -type l -exec chmod 775 {} \;
	
	find /usr/brlcad -type d -exec chmod 775 {} \;

	
	;;
#  These directives operate in the source directory
#
#  inst-dist	install sources in /dist tree without installing any products
#
install-man|inst-dist|print|typeset|nroff)
	for dir in ${ADIRS} ${BDIRS}; do
		echo -------------------------------- ${dir};
		( cd ${dir} && cake -k ${SILENT} ${TARGET} )
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
		rm -fr ${DIRPRE}${dir}${DIRSUF}
	done;;

wc)
	rm -f /tmp/cad-lines
	for dir in ${ADIRS} ${BDIRS} ${CDIRS}; do
		( cd ${dir}; wc *.[chsly] | grep total >> /tmp/cad-lines )
	done
	awk '{tot += $1;}; END{print "Total lines of source = ", tot;}' < /tmp/cad-lines
	rm -f /tmp/cad-lines
	;;

tclIndex)
	( cd tclscripts && cake -k ${SILENT} ${TARGET} )

	for dir in ${TSDIRS}; do
	    ( cd tclscripts/$dir && cake -k ${SILENT} ${TARGET} )
	done;;

tags)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir};
		( cd $dir && cake -k ${SILENT} ${TARGET} )
	done;;

TAGS)
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir};
		( cd $dir && cake -k ${SILENT} ${TARGET} )
	done;;

etags)
	/bin/rm -f etags;
	for dir in ${BDIRS}; do
		echo -------------------------------- ${dir};
#		etags -a -o etags ${dir}/*.c
		find ${dir} -name \*.c -exec etags -a -o etags {} \;
	done;;
shell)
	for dir in ${BDIRS}; do
		( cd ${dir}; echo ${dir}; /bin/sh )
	done;;

command)
	# Particularly useful for finding things, like this:
	# ./gen.sh command grep some_variable '*.c'
	for dir in ${BDIRS}; do
		( cd ${dir}; echo ${dir}; eval $* )
	done;;

rcs-lock)
	rcs -l ${TOP_FILES}
	rcs -u ${TOP_FILES}
	for dir in ${ADIRS} ${BDIRS}; do
		echo -------------------------------- $dir;
		(cd $dir; \
		rcs -l *.[cshf1-9] Cakefile; \
		rcs -u *.[cshf1-9] Cakefile )
	done;;

checkin)
	echo " RCS_Revision ${RCS_REVISION}"
	REL_NODOT=`echo ${RELEASE}|tr . _`
	CI_ARGS="-f -r${RCS_REVISION} -sRel${REL_NODOT} -mRelease_${RELEASE}"
	rcs -l ${TOP_FILES}
	ci -u ${CI_ARGS} ${TOP_FILES}
	for dir in ${ADIRS} ${BDIRS}; do
		echo -------------------------------- $dir;
		(cd $dir; rm -f vers.c version; \
		co RCS/*; \
		rcs -l *.[cshf1-9] Cakefile; \
		ci -u ${CI_ARGS} *.[cshf1-9] Cakefile )
	done;;

#
# Steps in creating a distribution:
#	Make sure the CVS repository is up-to-date and in the state for distribution
#	"cvs rtag REL_TAG brlcad" to mark the CVS archives with the release version
#
#	Start in a blank directory with only gen.sh.
#	"gen.sh dist" to create the distribution in "dist" and the TAR archive in "arch"
#	Note that "gen.sh dist -r REL_TAG" may be used later to recreate the same distribution
#	Any additional arguments to "gen.sh" after the "dist" will be passed to "cvs"
#
dist)
#	/usr/gnu/bin/tar doesn't work correctly on CAD
#	make sure we are not running under IRIX
	OS=`uname`
	if test "$OS" != Linux -a "$OS" != FreeBSD
	then
		echo "Source distribution should be made using Linux or FreeBSD"
		echo "/usr/gnu/bin/tar is broken under IRIX"
		exit
	fi
#	if this is a tty, get the encryption key
	if tty -s
	then
		DO_ENCRYPTION=1
		echo "Enter encryption key:"
		read KEY
		echo "encryption key is /$KEY/"
	else
		DO_ENCRYPTION=0
	fi

#	create fresh distribution and archive directories
	rm -rf ${DISTDIR}
	mkdir ${DISTDIR}
	rm -rf ${ARCHDIR}
	mkdir ${ARCHDIR}

#	note that date and time
	date > ${DISTDIR}/Date_of_distribution

	if test "$CVSROOT" = ""
	then
		CVSROOT=cad.arl.mil:/c/CVS
		export CVSROOT
	fi

#	create the args for the "cvs export"
	if test $# -eq 0
	then
		CVS_ARGS="-D now"
	else
		CVS_ARGS=$*
	fi

#	get the distribution
	cvs export -d ${DISTDIR} ${CVS_ARGS} brlcad

#	fix "gen.sh" to set NFS=0
	sed -e 's/^NFS=1/NFS=0/' < ${DISTDIR}/gen.sh > ${DISTDIR}/tmp
	mv ${DISTDIR}/tmp ${DISTDIR}/gen.sh

#	fix "Cakefile.defs" to production values
	sed -e '/^#define[ 	]*NFS/d' < ${DISTDIR}/Cakefile.defs > ${DISTDIR}/tmp
	sed -e '/PRODUCTION/s/0/1/' < ${DISTDIR}/tmp > ${DISTDIR}/Cakefile.defs
	rm -f ${DISTDIR}/tmp

	if test `grep '^#define[ 	]*NFS' ${DISTDIR}/Cakefile.defs|wc -l` -eq 0
	then 	echo "Shipping non-NFS version of Cakefile.defs (this is good)";
	else
		echo "ERROR: Update Cakefile.defs for non-NFS before proceeding!"
		exit 1;
	fi
	if test `grep "^NFS=1" ${DISTDIR}/gen.sh|wc -l` -eq 0
	then 	echo "Shipping non-NFS version of gen.sh (this is good)";
	else
		echo "ERROR: Update gen.sh for non-NFS before proceeding!"
		exit 1;
	fi
	echo
	echo "I hope you have made Cakefile.defs tidy!"
	echo "(Check for PRODUCTION values properly set)"
	echo
	grep PRODUCTION ${DISTDIR}/Cakefile.defs
	echo

	echo "Formatting the INSTALL.TXT file"
	rm -f ${DISTDIR}/INSTALL.ps
	gtbl ${DISTDIR}/doc/install.doc | groff  > ${DISTDIR}/INSTALL.ps

	echo "Preparing the 'bench' directory"
	(cd ${DISTDIR}/bench; cake clobber; cake install)
	echo "End of BRL-CAD Release $RELEASE archive, `date`" > ${DISTDIR}/zzzEND
	(cd ${DISTDIR}; du -a > Contents)

#	making archive
	cd ${DISTDIR}; tar cfv ../${ARCHIVE} *
	# $4 will be file size in bytes (BSD machine)
	# $5 will be file size in bytes (SYSV machine)
	# pad to 200K byte boundary for SGI cartridge tapes.
	set -- `ls -l ../${ARCHIVE}`
	PADBYTES=`echo "204800 $5 204800 %-p" | dc`
	if test ${PADBYTES} -lt 204800
	then
		gencolor -r${PADBYTES} 0 >> ../${ARCHIVE}
	fi
	chmod 444 ../${ARCHIVE}
	echo "${ARCHIVE} created"

	# The FTP images:
	FTP_ARCHIVE=../${ARCHDIR}/cad${RELEASE}.tar
	EXCLUDE=/tmp/cad-exclude
	rm -f ${EXCLUDE}
	echo 'vfont/*' >> ${EXCLUDE}
	echo 'doc/*' >> ${EXCLUDE}
	echo 'html/*' >> ${EXCLUDE}
	echo 'pix/*' >> ${EXCLUDE}
	echo 'pro-engineer/*' >> ${EXCLUDE}
	echo 'libtcl8.3/*' >> ${EXCLUDE}
	echo 'libtk8.3/*' >> ${EXCLUDE}
	echo 'libitcl3.2/*' >> ${EXCLUDE}

	tar cfv - Copy* README doc html \
	    zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-a.gz
	chmod 444 ${FTP_ARCHIVE}-a.gz
	echo "${FTP_ARCHIVE}-a.gz created (doc)"

	tar -cvf - -X ${EXCLUDE} [A-Z]* [a-k]* zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-b.gz
	chmod 444 ${FTP_ARCHIVE}-b.gz
	echo "${FTP_ARCHIVE}-b.gz created (core 1)"

	tar -cvf - -X ${EXCLUDE} Copy* README l[a-g]* lia* lib[a-s]* zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-c.gz
	chmod 444 ${FTP_ARCHIVE}-c.gz
	echo "${FTP_ARCHIVE}-c.gz created (core 2)"

	tar -cvf - -X ${EXCLUDE} Copy* README lib[t-z]* li[c-z]* l[j-z]* [m-p]* zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-d.gz
	chmod 444 ${FTP_ARCHIVE}-d.gz
	echo "${FTP_ARCHIVE}-d.gz created (core 3)"

	tar -cvf - -X ${EXCLUDE} Copy* README [q-z]* zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-e.gz
	chmod 444 ${FTP_ARCHIVE}-e.gz
	echo "${FTP_ARCHIVE}-e.gz created (core 4)"

	tar cfv - Copy* README pix zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-f.gz
	chmod 444 ${FTP_ARCHIVE}-f.gz
	echo "${FTP_ARCHIVE}-f.gz created (pix)"

	tar cfv - Copy* README vfont zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-g.gz
	chmod 444 ${FTP_ARCHIVE}-g.gz
	echo "${FTP_ARCHIVE}-g.gz created (vfont)"

	tar cfv - Copy* README pro-engineer zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-h.gz
	chmod 444 ${FTP_ARCHIVE}-h.gz
	echo "${FTP_ARCHIVE}-h.gz created (pro-engineer)"

	tar cfv - Copy* README libtcl8.3 libtk8.3 libitcl3.2 zzzEND |\
		gzip -9 > ${FTP_ARCHIVE}-i.gz
	chmod 444 ${FTP_ARCHIVE}-i.gz
	echo "${FTP_ARCHIVE}-i.gz created (libtcl8.3 libtk8.3 libitcl3.2)"

	rm -f ${EXCLUDE}

#	Select available encryption utility
	echo "Hello" | crypt key > /dev/null 2>&1
	if test $? -gt 0
	then
		COMMAND=enigma
	else
		COMMAND=crypt
	fi

	if test ${DO_ENCRYPTION} -eq 1
	then
		for FILE in ${FTP_ARCHIVE}-*.gz ; do
			$COMMAND ${KEY} < ${FILE} > ${FILE}.crypt
			rm -f ${FILE}
		done
	else
		echo "not encrypted" > ../${ARCHDIR}/NOT_ENCRYPTED
	fi	
	;;

# on a FreeBSD system, create the install package
pkg)

	#########################################
	#
	#  Create the various input files
	#
	#########################################

	cat > comment << EOF
Geometric Modeling and rendering package
EOF

	cat > desc << EOF
The BRL-CAD package is a suite of tools for gemetric modeling, analysis
and rendering.  It consists of tons of code, including embeddable libraries.
The GUI is in Tcl/Tk.
EOF

	cat > postinst.sh << EndOfFile
#!/bin/sh

more << EOF
BRL-CAD is copyrighted software.  It is distributed under a license agreement.
Please do not redistribute this software outside your organization.
EOF
EndOfFile


	#########################################
	#
	#  Create the packing list
	#
	#  The "png" package includes both libpng and libz
	#
	#  Note that the FreeBSD pkg system does not properly handle symbolic links
	#    so "brlcad/lib/iwidgets" must be created and detroyed using @exec and @unexec
	#########################################

cat > contents << EOF
@name brlcad-$RELEASE
@pkgdep png-1.0.9
@cwd /usr
@owner bin
@group bin
@unexec /bin/rm -f %D/brlcad/lib/iwidgets
EOF


find /usr/brlcad \! -type d -print | sed 's,/usr/,,' | grep -v 'iwidgets$' >> contents

cat >> contents << EOF
@exec /usr/bin/env OBJFORMAT=elf /sbin/ldconfig -m /usr/brlcad/lib
@exec /bin/ln -s %D/brlcad/lib/iwidgets3.0.1 %D/brlcad/lib/iwidgets
@unexec /usr/bin/env OBJFORMAT=elf /sbin/ldconfig -R
EOF

find -d /usr/brlcad -type d -print | sed "s,/usr/,@dirrm ," >> contents

	##############################
	#
	#  Create the package
	#
	##############################

pkg_create -I postinst.sh -d desc -c comment -f contents brlcad-$RELEASE
rm ./comment ./contents ./desc ./postinst.sh

;;
# On a Linux system, bundle up /usr/brlcad binary tree as an RPM.
rpm)
	REV=`date '+%m%d' `
	RPM_BASE=brlcad-${RELEASE}-${REV}.i386
	SPEC=/tmp/${RPM_BASE}.spec
	rm -f $SPEC
	cat > $SPEC << EOF
Summary:  BRL-CAD(tm) Solid Modeling System with ray-tracer and geometry editor
Name: brlcad
Version: ${RELEASE}
Release: ${REV}
Copyright:  Copyright 2000 by U.S.Army in all countires except the USA.  See distribution restrictions in your license agreement or ftp.arl.mil:/pub/brl-cad/agreement
Group: Applications/Graphics
Source:  http://ftp.arl.mil:/brl-cad/Downloads/Rel${RELEASE}/
URL:  http://ftp.arl.mil/brlcad/
Vendor: The U. S. Army Research Laboratory, Aberdeen Proving Ground, MD  USA  21005-5068
Packager: Mike Muuss <Mike@arl.mil>
%description
The BRL-CAD(tm) Package is a powerful Constructive Solid Geometry (CSG)
solid modeling system.  BRL-CAD includes an interactive geometry
editor, a ray tracing library, two ray-tracing based lighting models,
a generic framebuffer library, a network-distributed image-processing
and signal-processing capability, and a large collection of related
tools and utilities.

This version was compiled on RedHat Linux 6.2

%prep
        exit 0
%files
%docdir /usr/brlcad/man
%docdir /usr/brlcad/html
/usr/brlcad
EOF
	rpm -bb $SPEC
	# Oddly, this produces /usr/src/redhat/RPMS/i386/${RPM_BASE}.rpm
	# No advantage to gzip'ing, according to Lee.
	##gzip -9 < /usr/src/redhat/RPMS/i386/${RPM_BASE}.rpm > ./${RPM_BASE}.rpm.gz
	cp /usr/src/redhat/RPMS/i386/${RPM_BASE}.rpm ./${RPM_BASE}.rpm
	# Privacy step still needed to be run by hand.
	## enigma $KEY < ./${RPM_BASE}.rpm.gz > ./${RPM_BASE}.rpm.gz.crypt
	# enigma $KEY < ./${RPM_BASE}.rpm > ./${RPM_BASE}.rpm.crypt
	;;

sunpkg)
	# Build Solaris pkg.  Bundle up /usr/brlcad
	rm -f prototype pkginfo
	REV=`date '+%Y.%m.%d' `
	PKG_BASE=brlcad-${RELEASE}-${REV}-solaris
	cat > pkginfo << EOF
PKG=brlcad
NAME=BRL-CAD(tm) Solid Modeling System with ray-tracer and geometry editor
ARCH=sparc
VERSION=${RELEASE},REV=${REV}
CATEGORY=application
CLASSES=none
BASEDIR=/usr/brlcad
PRODNAME=brlcad
PRODVERS=${RELEASE}
DESC=The BRL-CAD(tm) Package is a powerful Constructive Solid Geometry (CSG) \
solid modeling system.  BRL-CAD includes an interactive geometry \
editor, a ray tracing library, two ray-tracing based lighting models, \
a generic framebuffer library, a network-distributed image-processing \
and signal-processing capability, and a large collection of related \
tools and utilities. \
Copyright 2000 by U.S.Army in all countires except the USA.  See distribution restrictions in your license agreement or ftp.arl.mil:/pub/brl-cad/agreement
VENDOR=The U. S. Army Research Laboratory, Aberdeen Proving Ground, MD  USA  21005-5068
HOTLINE=http://ftp.arl.mil/brlcad/
EMAIL=acst@arl.mil
EOF
	echo "i pkginfo=./pkginfo" >> ./prototype
	pkgproto /usr/brlcad/.=/usr/brlcad >> ./prototype
	rm -fr /tmp/brlcad
	# This creates /tmp/brlcad directory
	pkgmk -o -a sun4 -d /tmp -f ./prototype brlcad
	pkgtrans -s /tmp /tmp/$PKG_BASE brlcad
	##pkgchk -d /tmp/$PKG_BASE all
	rm -fr /tmp/brlcad
	echo "Wrote /tmp/$PKG_BASE"
	# Privacy step sitll needed to be run by hand.
	;;

# For SGI IRIX, use the "swpkg" GUI tool.
	# swpkg -nofork
	# Critical note: Product Name may *not* have a "-" in it!
	# DON'T SAY "BRL-CAD"!!!
	# Lee established the convention of using "brlcad".
	#
	# On first tab (Create Product Hierarchy):
	# Product Name = brlcad
	# Description = "The BRL-CAD Solid Modeling System"
	# click "< Assign"
	# click books, click delete
	# click relnotes, click delete
	# click optional, click delete
	# click "sw".
	# Version = 5.1
	# click "< Assign"
	# click "man"
	# Version = 5.1
	# click "< Assign"
	# Select second tab:  "Tag Files"
	# In File Broswer box, delete "/tmp", enter "/usr/brlcad/" return.
	# Click "All".
	# Control-left-click on "html" and "man" to un-highlight them.
	# click "> Add"  (watch a bazillion files go by)
	# In "Tags" box, click on "brlcad.man.manpages".
	# In "File Browser", click on "html", control-left-click on "man".
	# click "> Add" (watch the manual stuff go by)
	# Select 5th tab, "Build Product"
	# Change "Distribution Directory" to "/var/tmp"
	# Check off "Verbose" checkbox
	# Click on "Build All"
	# Click OK on "save spec"
	# Click OK on "Save Idb"
	# Watch files scroll by in uppermost scrolling window
	# When done, select menu "File > Exit"
	# Four files should have been created.  Bundle them up with:
	# tar cvf brlcad51.tardist brlcad brlcad.idb brlcad.man brlcad.sw
	# gzip -9 < brlcad51.tardist > brlcad51.tardist.gz
	# crypt xyzzy < brlcad51.tardist.gz > brlcad51.tardist.gz.crypt

*)
	echo "$0: No code to make ${TARGET}, use 'help' for help"
	exit 1;;

esac

exit 0
