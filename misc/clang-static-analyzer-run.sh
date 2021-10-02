#!/bin/bash

# We want the script to fail hard and immediately if anything goes wrong, in
# order to avoid masking failures (e.g. build failures, which will leave
# empty scan reports but certainly don't constitute successes. See (for example)
# https://stackoverflow.com/q/821396
set -e
set -o pipefail

# Note: we set these variables to tell the ccc-analyzer script which compilers
# to use, but it is not always enough - when some of the sub-builds are
# triggered, they do not seem to inherit these settings and revert to the
# default compiler.  The only way so far to avoid this issue reliably seems to
# be to edit the ccc-analyzer script to use the compiler we want as the default
# compiler.
export CCC_CC=clang
export CCC_CXX=clang++

# This appears to be a workable way to enable the new Z3 static analyzer
# support, but at least as of 2017-12 it greatly slows the testing (by orders
# of magnitude).  Might be viable (or at least more useful even if it can't
# quickly complete) if we completely pre-build src/other
#export CCC_ANALYZER_CONSTRAINTS_MODEL=z3

expected_success=()
declare -A expected_failure
new_success=()
new_failure=()

# function for testing targets that are expected to be clean
function cleantest {
	echo $1
	scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-$1 make -j12 $1
	if [ "$(ls -A ./scan-reports-$1)" ]; then
		new_failure+=("$1")
		((failures ++))
	else
		expected_success+=("$1")
	fi
}
function failingtest {
	echo "$1"
	scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-$1 make -j12 $1
	if [ "$(ls -A ./scan-reports-$1)" ]; then
		#TODO - find some way to tell if new failures have been introduced...
		report_cnt=$(ls -l ./scan-reports-$1/*/report* | wc -l)
		expected_failure[$1]="$report_cnt"
	else
		new_success+=("$1")
	fi
}

function notest {
	echo "$1"
	make -j12 $1
}

# configure using the correct compiler and values.  We don't
# need this report, so clear it after configure is done
scan-build --use-analyzer=/usr/bin/$CCC_CC -o ./scan-reports-config cmake .. -DBRLCAD_EXTRADOCS=OFF -DCMAKE_C_COMPILER=ccc-analyzer -DCMAKE_CXX_COMPILER=c++-analyzer

# clear out any old reports
rm -rfv ./scan-reports-*

# we don't care about reports for src/other or misc tools
# we don't maintain ourselves - build those without recording
# the analyzer output
cd src/other
make -j12
cd ../../
cd src/other/ext
make -j12
cd ../../../
cd misc/tools
make -j12
cd ../../


# The following targets should generate empty directories (i.e. their report
# directory should not be present at the end of the test.

failures="0"

rm -rf ./scan-reports*

# Libraries - order is important here.  We want to build all of
# a library's dependencies before we build the library so the
# problem reports (if any) are specific to that library

cleantest libbu
cleantest libpkg
cleantest libbn
cleantest libbv
failingtest libbg
failingtest libnmg
failingtest libbrep
failingtest librt
cleantest libwdb
cleantest libgcv
failingtest gcv_plugins
cleantest libanalyze
cleantest liboptical
cleantest libicv
cleantest libged
cleantest libdm
cleantest dm_plugins
cleantest libfft
failingtest ged_plugins
failingtest libtclcad
cleantest libtermio
cleantest librender

# Executables

if [ "0" -eq "1" ]; then
 cleantest 3dm-g
 cleantest a-d
 failingtest admin-db
 cleantest alias-pix
 cleantest all_sf
 cleantest anim_cascade
 cleantest anim_fly
 failingtest anim_hardtrack
 cleantest anim_keyread
 cleantest anim_lookat
 cleantest anim_offset
 cleantest anim_orient
 failingtest anim_script
 failingtest anim_sort
 failingtest anim_time
 failingtest anim_track
 failingtest anim_turn
 cleantest ap-pix
 failingtest asc-nmg
 cleantest asc-plot3
 cleantest asc2dsp
 cleantest asc2g
 cleantest asc2pix
 cleantest azel
 cleantest bary
 failingtest beset
 failingtest bolt
 cleantest bombardier
 cleantest bot-bldxf
 cleantest bot_dump
 cleantest bot_shell-vtk
 cleantest brep_cobb
 cleantest brep_cube
 cleantest brep_simple
 cleantest brepintersect
 cleantest breplicator
 cleantest brickwall
 failingtest btclsh
 cleantest buffer
 failingtest burst
 cleantest bw-a
 cleantest bw-d
 cleantest bw-fb
 cleantest bw-imp
 cleantest bw-pix
 cleantest bw-png
 cleantest bw-ps
 failingtest bw-rle
 cleantest bw3-pix
 cleantest bwcrop
 cleantest bwdiff
 cleantest bwfilter
 cleantest bwhist
 failingtest bwhisteq
 failingtest bwish
 cleantest bwmod
 cleantest bwrect
 cleantest bwrot
 cleantest bwscale
 cleantest bwshrink
 cleantest bwstat
 cleantest bwthresh
 cleantest c-d
 failingtest cad_boundp
 cleantest cad_parea
 cleantest cchannel
 failingtest cell-fb
 cleantest chan_add
 cleantest chan_mult
 failingtest chan_permute
 cleantest clutter
 cleantest cmap-fb
 cleantest coil
 failingtest comgeom-g
 cleantest contours
 cleantest conv-vg2g
 cleantest csgbrep
 cleantest cv
 cleantest cy-g
 cleantest d-a
 cleantest d-bw
 cleantest d-f
 cleantest d-i
 cleantest d-u
 cleantest d2-c
 cleantest damdf
 cleantest dauto
 cleantest dauto2
 cleantest dbcp
 cleantest dbupgrade
 failingtest dconv
 cleantest ddisp
 failingtest decimate
 failingtest dem-g
 cleantest dfft
 cleantest dmod
 cleantest double-asc
 cleantest dpeak
 cleantest dpix-pix
 cleantest dsel
 cleantest dstats
 cleantest dwin
 failingtest dxf-g
 cleantest enf-g
 cleantest euclid-g
 cleantest euclid_format
 cleantest euclid_unformat
 cleantest f-d
 cleantest f-i
 failingtest fast4-g
 cleantest fb-bw
 cleantest fb-cmap
 failingtest fb-fb
 failingtest fb-pix
 failingtest fb-png
 failingtest fb-rle
 cleantest fbanim
 cleantest fbcbars
 cleantest fbclear
 cleantest fbcmap
 cleantest fbcmrot
 cleantest fbcolor
 failingtest fbed
 cleantest fbfade
 failingtest fbframe
 cleantest fbfree
 cleantest fbgamma
 cleantest fbgammamod
 cleantest fbgrid
 cleantest fbhelp
 cleantest fblabel
 cleantest fbline
 cleantest fbpoint
 cleantest fbscanplot
 cleantest fbserv
 cleantest fbstretch
 cleantest fbzoom
 cleantest fence
 cleantest fftc
 cleantest fhor
 cleantest firpass
 cleantest fix_polysolids
 failingtest g-acad
 cleantest g-dot
 failingtest g-dxf
 cleantest g-egg
 cleantest g-euclid
 cleantest g-euclid1
 failingtest g-iges
 failingtest g-jack
 failingtest g-nff
 cleantest g-nmg
 failingtest g-obj
 failingtest g-off
 failingtest g-ply
 failingtest g-raw
 failingtest g-shell-rect
 failingtest g-step
 failingtest g-stl
 failingtest g-tankill
 cleantest g-var
 cleantest g-voxel
 cleantest g-vrml
 cleantest g-x3d
 cleantest g-xxx
 failingtest g-xxx_facets
 cleantest g2asc
 cleantest g4-g5
 cleantest g5-g4
 cleantest gastank
 cleantest gcv
 cleantest gdiff
 cleantest gen-attributes-file
 cleantest gencolor
 cleantest gif-fb
 failingtest gif2fb
 cleantest glint
 cleantest globe
 cleantest gqa
 cleantest gtransfer
 failingtest halftone
 cleantest handle
 cleantest hex
 cleantest human
 cleantest i-a
 cleantest i-d
 cleantest i-f
 cleantest icv
 cleantest ifftc
 failingtest iges-g
 cleantest ihist
 cleantest imgdims
 cleantest imod
 cleantest ir-X
 cleantest irdisp
 cleantest issttcltk
 cleantest istats
 cleantest jack-g
 cleantest kurt
 cleantest lens
 failingtest lgt
 cleantest loop
 cleantest lowp
 cleantest mac-pix
 failingtest masonry
 failingtest menger
 cleantest metaball
 failingtest mged
 cleantest mkbuilding
 cleantest molecule
 failingtest naca456
 failingtest nastran-g
 cleantest nirt
 failingtest nmg-bot
 cleantest nmg-rib
 cleantest nmg-sgp
 cleantest nmgmodel
 cleantest off-g
 failingtest patch-g
 cleantest pdb-g
 cleantest picket_fence
 cleantest pictx
 cleantest pipe
 failingtest pix-alias
 cleantest pix-bw
 cleantest pix-bw3
 cleantest pix-fb
 cleantest pix-png
 cleantest pix-ppm
 cleantest pix-ps
 failingtest pix-rle
 cleantest pix-spm
 failingtest pix-sun
 cleantest pix-yuv
 cleantest pix2asc
 cleantest pix2g
 cleantest pix3filter
 cleantest pixautosize
 failingtest pixbackgnd
 cleantest pixbgstrip
 cleantest pixblend
 cleantest pixborder
 cleantest pixbustup
 cleantest pixclump
 failingtest pixcmp
 cleantest pixcolors
 cleantest pixcount
 failingtest pixcrop
 cleantest pixcut
 cleantest pixdiff
 cleantest pixelswap
 cleantest pixembed
 cleantest pixfade
 failingtest pixfields
 cleantest pixfieldsep
 cleantest pixfilter
 cleantest pixflip-fb
 cleantest pixhalve
 cleantest pixhist
 cleantest pixhist3d
 cleantest pixhist3d-plot3
 cleantest pixinterp2x
 cleantest pixmatte
 failingtest pixmerge
 cleantest pixmorph
 cleantest pixpaste
 cleantest pixrect
 cleantest pixrot
 cleantest pixsaturate
 cleantest pixscale
 failingtest pixshrink
 cleantest pixstat
 cleantest pixsubst
 cleantest pixtile
 cleantest pixuntile
 cleantest plot3-X
 cleantest plot3-asc
 cleantest plot3-dm
 failingtest plot3-fb
 cleantest plot3-hpgl
 cleantest plot3-plot3
 cleantest plot3-ps
 cleantest plot3-tek
 cleantest plot3color
 cleantest plot3debug
 cleantest plot3getframe
 cleantest plot3line2
 cleantest plot3rot
 cleantest plot3stat
 cleantest ply-g
 cleantest png-bw
 cleantest png-fb
 cleantest png-pix
 cleantest png_info
 failingtest polar-fb
 cleantest poly-bot
 cleantest pp-fb
 cleantest proe-g
 cleantest pyramid
 failingtest random
 cleantest raw-g
 cleantest rawbot
 cleantest remapid
 failingtest remrt
 cleantest reshoot
 cleantest ringworld
 cleantest rle-fb
 cleantest rle-pix
 cleantest room
 cleantest roots_example
 failingtest rpatch
 failingtest rt
 failingtest rt_bot_faces
 failingtest rtarea
 failingtest rtcell
 failingtest rtcheck
 failingtest rtedge
 cleantest rtexample
 failingtest rtfrac
 failingtest rtg3
 failingtest rthide
 failingtest rtpp
 failingtest rtrad
 failingtest rtrange
 failingtest rtray
 cleantest rtregis
 cleantest rtscale
 failingtest rtserver
 cleantest rtserverTest
 cleantest rtshot
 failingtest rtsil
 cleantest rtsrv
 cleantest rtwalk
 failingtest rtweight
 failingtest rtxray
 cleantest script-tab
 failingtest scriptsort
 failingtest secpass
 cleantest shapefact
 cleantest showshot
 cleantest showtherm
 failingtest shp-g
 cleantest sketch
 failingtest smod
 failingtest sphflake
 cleantest spm-fb
 cleantest stl-g
 cleantest sun-pix
 failingtest surfaceintersect
 failingtest tabinterp
 cleantest tabsub
 cleantest tankill-g
 cleantest tea
 failingtest tea_nmg
 cleantest terrain
 cleantest texturescale
 failingtest tgf-g
 cleantest tire
 cleantest torii
 cleantest ttcp
 cleantest tube
 cleantest u-a
 cleantest u-bw
 cleantest u-d
 cleantest u-f
 cleantest umod
 cleantest ustats
 failingtest vdeck
 failingtest vegetation
 cleantest viewpoint-g
 cleantest walk_example
 cleantest wavelet
 cleantest wavy
 cleantest wdb_example
 failingtest window
 cleantest window_frame
 failingtest wire
 cleantest xyz-plot3
 cleantest yuv-pix
fi

# The targets listed below have known issues that will be difficult
# to resolve

# libgcv (uses perplex/re2c/lemon outputs)
# step-g (uses exp2cxx outputs)
# obj-g  (uses generated parser outputs)
# csg    (uses generated parser outputs)
# list_elements (uses exp2cxx outputs, test program)

echo "Summary:"
if [ "${#new_failure[@]}" -ne "0" ]; then
	echo "   new failures: ${new_failure[@]}"
fi
if [ "${#new_success[@]}" -ne "0" ]; then
	echo "   new successes: ${new_success[@]}"
fi
if [ "${#expected_success[@]}" -ne "0" ]; then
	echo "   successes(expected): ${expected_success[@]}"
fi
if [ "${#expected_failure[@]}" -ne "0" ]; then
	echo "   failures(expected):"
for f in "${!expected_failure[@]}"; do
	echo "                       $f(${expected_failure["$f"]} bugs)";
done
fi

# Clean up empty scan-* output dirs
find . -type d -empty -name scan-\* |xargs rmdir

if [ "$failures" -ne "0" ]; then
	exit 1
fi

