# $Id$

         
A2G=../conv/asc2g
G2A=../conv/g2asc

#SUFFIXES: .g .asc

.asc.g: ; $(A2G) $< $@

UH = Makefile.dist db.php
MODELS = axis.asc bldg391.asc boolean-ops.asc castle.asc cornell.asc cray.asc crod.asc cube.asc demo.asc galileo.asc havoc.asc kman.asc ktank.asc lgt-test.asc m35.asc moss.asc pic.asc prim.asc sphflake.asc star.asc tank_car.asc terra.asc terra.dsp truck.asc wave.asc woodsman.asc world.asc xmp.asc

pkgdata_DATA = ${MODELS:asc=g}

EXTRA_DIST = ${UH} ${MODELS}
