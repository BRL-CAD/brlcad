#!/bin/sh
#			R E G R E S S I O N . S H		#
#  Author -						 	#
#	Christopher Sean Morrison      				#
#								#
# Source -							#
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
# SCRIPTS
# FRAMEBUFFER_COMMANDS
# DATACONVERSION_COMMANDS
# ANIMATION_COMMANDS
# PIX_COMMANDS
# RLE_COMMANDS
# KITCHEN_SINK
#
# COMMANDS includes everything
#

SCRIPTS="\
any-png.sh \
brlcad-check.sh \
cadbug.sh \
cakeinclude.sh \
cray.sh \
font.c \
machinetype.sh \
morphedit.tcl \
newvers.sh \
pixinfo.sh \
pixread.sh \
pixwrite.sh \
ranlib5.sh \
sgisnap.sh \
sharedliblink.sh \
sharedlibvers.sh \
show.sh \
"

FRAMEBUFFER_COMMANDS="\
fbanim \
fbcbars \
fbclear \
fbcmap \
fbcmrot \
fbcolor \
fbed \
fbfade \
fbframe \
fbfree \
fbgamma \
fbgammamod \
fbgrid \
fbhelp \
fblabel \
fbline \
fbpoint \
fbscanplot \
fbserv \
fbstretch \
fbzoom \
"

DATACONVERSION_COMMANDS="
a-d \
alias-pix \
ap-pix \
asc-nmg \
asc-pl \
asc2g \
asc2pix \
bw-a \
bw-d \
bw-fb \
bw-imp \
bw-pix \
bw-png \
bw-ps \
bw-rle \
bw3-pix \
c-d \
cat-fb \
cell-fb \
cmap-fb \
comgeom-g \
conv-vg2g \
cubitorle \
cy-g \
d-a \
d-bw \
d-f \
d-i \
double-asc \
dpix-pix \
dxf-g \
euclid-g \
f-d \
f-i \
fast4-g \
fb-bw \
fb-cmap \
fb-fb \
fb-orle \
fb-pix \
fb-png \
fb-rle \
g-acad \
g-euclid \
g-euclid1 \
g-iges \
g-jack \
g-nmg \
g-off \
g-shell.rect \
g-tankill \
g-vrml \
g-wave \
g2asc \
g4-g5 \
gif-fb \
gif2fb \
giftorle \
graytorle \
i-a \
i-d \
i-f \
iges-g \
ir-X \
ir-sgi \
jack-g \
mac-pix \
nastran-g \
nmg-polysolid \
nmg-rib \
off-g \
orle-fb \
op-bw \
orle-pix \
painttorle \
patch-g \
pcd-pix \
pix-alias \
pix-bw \
pix-bw3 \
pix-fb \
pix-orle \
pix-png \
pix-ppm \
pix-ps \
pix-rle \
pix-spm \
pix-sun \
pix-yuv \
pix2asc \
pixflip-fb \
pl-asc \
pl-fb \
pl-hpgl \
pl-pl \
pl-ps \
pl-sgi \
pl-tek \
png-bw \
png-fb \
png-pix \
polar-fb \
poly-bot \
pp-fb \
proe-g \
rle-fb \
rle-pix \
script-tab \
sgi-pix \
spm-fb \
stl-g \
sun-pix \
tankill-g \
txyz-pl \
u-a \
u-bw \
u-d \
u-f \
viewpoint-g \
xyz-pl \
yuv-pix \
rletoabA60 \
rletoabA62 \
rletoascii \
rletogif \
rletogray \
rletopaint \
rletops \
rletoraw \
"

ANIMATION_COMMANDS="
anim_cascade \
anim_fly \
anim_hardtrack \
anim_keyread \
anim_lookat \
anim_offset \
anim_orient \
anim_script \
anim_sort \
anim_time \
anim_track \
anim_turn \
"

PIX_COMMANDS="
pix3filter \
pixautosize \
pixbackgnd \
pixbgstrip \
pixblend \
pixborder \
pixbustup \
pixclump \
pixcolors \
pixcount \
pixcut \
pixdiff \
pixdsplit \
pixelswap \
pixembed \
pixfade \
pixfields \
pixfieldsep \
pixfilter \
pixhalve \
pixhist \
pixhist3d \
pixhist3d-pl \
pixinterp2x \
pixmatte \
pixmerge \
pixmorph \
pixpaste \
pixrect \
pixrot \
pixsaturate \
pixscale \
pixshrink \
pixstat \
pixsubst \
pixtile \
pixuntile \
"

RLE_COMMANDS="
rleClock \
rleaddcom \
rlebg \
rlebox \
rlecat \
rlecomp \
rledither \
rleflip \
rlehdr \
rlehisto \
rleldmap \
rlemandl \
rlenoise \
rlepatch \
rleprint \
rlequant \
rlescale \
rleselect \
rlesetbg \
rlespiff \
rlesplice \
rlesplit \
rlestereo \
rleswap \
rlezoom \
"

KITCHEN_SINK=" \
all_sf \
applymap \
avg4 \
awf \
azel \
bary \
brlman \
buffer \
burst \
bwcrop \
bwdiff \
bwfilter \
bwhist \
bwhisteq \
bwish \
bwmod \
bwrect \
bwrot \
bwscale \
bwshrink \
bwstat \
bwthresh \
cad_boundp \
cad_parea \
cake \
cakeinclude \
cakesub \
chan_add \
chan_mult \
chan_permute \
crop \
cv \
damdf \
dauto \
dauto2 \
dbcp \
dconv \
ddisp \
decimate \
dfft \
display \
dmod \
dpeak \
dsel \
dsp_add \
dstat \
dunncolor \
dunnsnap \
dwin \
euclid_format \
euclid_unformat \
fake \
fant \
fhor \
files-tape \
findcom \
firpass \
g_diff \
g_lint \
gencolor \
halftone \
hd \
ihist \
imgdims \
imod \
into \
irdisp \
istat \
jove \
lgt \
loop \
mcut \
mergechan \
mged \
mk_bolt \
mk_gastank \
mk_handle \
mk_window \
mk_winfrm \
mk_wire \
msrandom \
mst \
nirt \
plcolor \
pldebug \
plgetframe \
plline2 \
plrot \
plstat \
png_info \
pyrmask \
query \
rawtorle \
remapid \
remrt \
repos \
rpatch \
rtcell \
rtcheck \
rtfrac \
rtg3 \
rthide \
rtpp \
rtrad \
rtrange \
rtray \
rtregis \
rtscale \
rtshot \
rtsil \
rtwalk \
rtweight \
rtxray \
scriptsort \
secpass \
shapefact \
smush \
syn \
tabinterp \
tabsub \
targatorle \
tclsh \
teach-jove \
terrain \
texturescale \
to8 \
tobw \
ttcp \
umod \
unexp \
unslice \
ustat \
vas4 \
vdeck \
wasatchrle \
wavelet \
wish \
"

COMMANDS=" \
${FRAMEBUFFER_COMMANDS} \
${DATACONVERSION_COMMANDS} \
${PIX_COMMANDS} \
${RLE_COMMANDS} \
${KITCHEN_SINK} \
"


# search the path for the tool
# also search the current directory

PATH_ELEMENTS=`echo $PATH | sed 's/^://
				s/:://g
				s/:$//
				s/:\\.:/:/g
				s/:/ /g'`
PATH_ELEMENTS="${PATH_ELEMENTS} ."

# if non-zero, we don't perform the regression testing
showStopperFound=0

#
# Existence check -- make sure that all the necessary programs
# have made it into the search path (including "dot").  Otherwise, 
# they can't be checked for validity.
#
echo "$0 Checking for program existence"
for CMD in ${COMMANDS}
do
	commandNotFound=1		# Assume cmd not found
	for PREFIX in ${PATH_ELEMENTS}
	do
		if test -f ${PREFIX}/${CMD}
		then
			# This was -x, but older BSD systems don't do -x.
			if test -s ${PREFIX}/${CMD}
			then
				# all is well
				commandNotFound=0
				break
			fi
			echo "WARNING:  ${PREFIX}/${CMD} exists, but is not executable."
		fi
	done
	if test ${commandNotFound} -ne 0
	then
		echo "$0 ERROR:  ${CMD} is not in your search path or current directory!"
		showStopperFound=1
	fi
done


#
# Logfile check -- make sure there are no log files laying around.  
# if there are, we croak since the user may want those log(s)
#
echo "$0 Checking for stale logfiles"
for CMD in ${COMMANDS}
do
	if [ -f ${CMD}.log ]
	then
		echo "$0 ERROR: the log file ${CMD}.log must be removed before running!"
		showStopperFound=1
	fi
done

#
# Conflict check -- check and see if there are any other files that may
# cause a conflict/clobbering later on in the regression test.
# **This should actually check for any files used during the test
# (.pix,etc) and croak rather than warn.
#
echo "$0 Checking for potential conflicting files"
for CMD in ${COMMANDS}
do
	potentialConflicters=`ls ${CMD}.* 2> /dev/null`
	for FILE in ${potentialConflicters}
	do
		echo "WARNING: ${FILE} may conflict with testing and get clobbered"
	done
done


#
# croak if we ran into an error
#
if test ${showStopperFound} -ne 0
then
	echo "$0 ERRORS FOUND -- BAILING OUT"
	exit 1
fi

echo "$0 Proceeding with regression test"



# preferably, we'd run make here and have a section for each command
# the makefile (and this file) would be generated by autoconf based 
# on what components/commands were actually installed



#
# Cleanup -- under normal termination, clean up any remaining log or
# other files that we may have generated.
#
for CMD in ${COMMANDS}
do
	if [ -f ${CMD}.* ]
	then
		rm ${CMD}.*
	fi
done
