#!/bin/sh
#			B U I L D _ M A K E F I L E . S H
#
# This shell script is used to build a makefile configured
# for a particular platform using Cakefile.defs.
#
#  $Header$

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell

SHELL=/bin/sh
export SHELL

IN_FILE=Cakefile.tmp
OUT_FILE=makefile

cat > ${IN_FILE} << EOF
#include "../Cakefile.defs"

#ifndef LIBTCL_USE_SYSTEM
#undef LIBTCL
#define LIBTCL -L\$"(LIBDIR)" -ltcl
#undef LIBTK
#define LIBTK -L\$"(LIBDIR)" -ltk
#undef LIBTCL_DEP
#define LIBTCL_DEP \$"(LIBDIR)"/libtcl.\$"(ARCH_SUF)"
#undef LIBTK_DEP
#define LIBTK_DEP  \$"(LIBDIR)"/libtk.\$"(ARCH_SUF)"
#endif

${OUT_FILE}:
    echo \PRODUCTS = g-xxx rtexample pl-dm > ${OUT_FILE}
    echo \INCLUDE_DIR = INCLUDE_DIR >> ${OUT_FILE}
    echo \LIBDM_CONFIG = LIBDM_CONFIG >> ${OUT_FILE}
    echo \LIBDIR = LIBDIR >> ${OUT_FILE}
    echo \X\LIBDIR = -L''XLIBDIR >> ${OUT_FILE}
    echo \X\INCDIR = XINCDIR >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \LDFLAGS = LDFLAGS >> ${OUT_FILE}
    echo \CC = CC >> ${OUT_FILE}
    echo \CFLAGS = CC_OPTS \$"(LIBDM_CONFIG)"  -I\$"(INCLUDE_DIR)" >> ${OUT_FILE}
    echo \ARCH_SUF = ARCH_SUF >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \STANDARD_LIBES = STANDARD_LIBES >> ${OUT_FILE}
    echo \LIBTCL_LIBES = LIBTCL_LIBES >> ${OUT_FILE}
    echo \LIBBU_LIBES = LIBBU_LIBES >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \LIBWDB = -L\$"(LIBDIR)" -lwdb >> ${OUT_FILE}
    echo \LIBRT = -L\$"(LIBDIR)" -lrt >> ${OUT_FILE}
    echo \LIBBN = -L\$"(LIBDIR)" -lbn >> ${OUT_FILE}
    echo \LIBBU = -L\$"(LIBDIR)" -lbu >> ${OUT_FILE}
    echo \LIBSYSV = -L\$"(LIBDIR)" -lsysv >> ${OUT_FILE}
    echo \LIBDM = -L\$"(LIBDIR)" -ldm >> ${OUT_FILE}
    echo \LIBTCL = LIBTCL >> ${OUT_FILE}
    echo \LIBTK = LIBTK >> ${OUT_FILE}
    echo \LIBES = \$"(LIBSYSV)" \$"(STANDARD_LIBES)" >> ${OUT_FILE}
    echo \LIBDM_LIBES = LIBDM_SAMPLE_LIBES >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \LIBWDB_DEP = \$"(LIBDIR)"/libwdb.\$"(ARCH_SUF)" >> ${OUT_FILE}
    echo \LIBRT_DEP = \$"(LIBDIR)"/librt.\$"(ARCH_SUF)" >> ${OUT_FILE}
    echo \LIBBN_DEP = \$"(LIBDIR)"/libbn.\$"(ARCH_SUF)" >> ${OUT_FILE}
    echo \LIBBU_DEP = \$"(LIBDIR)"/libbu.\$"(ARCH_SUF)" >> ${OUT_FILE}
    echo \LIBTCL_DEP = LIBTCL_DEP >> ${OUT_FILE}
    echo \LIBTK_DEP = LIBTK_DEP >> ${OUT_FILE}
    echo \LIBDM_DEP = \$"(LIBDIR)"/libdm.\$"(ARCH_SUF)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \G_XXX_LIBES = \$"(LIBWDB)" \$"(LIBRT)" \$"(LIBBN)" \$"(LIBBU)" \$"(LIBBU_LIBES)" \$"(LIBTCL)" \$"(LIBTCL_LIBES)" \$"(LIBES)" >> ${OUT_FILE}
    echo \G_XXX_DEP_LIBES = \$"(LIBRT_DEP)" \$"(LIBBN_DEP)" \$"(LIBBU_DEP)" \$"(LIBTCL_DEP)" \$"(LIBWDB_DEP)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \RTEXAMPLE_LIBES = \$"(LIBRT)" \$"(LIBBN)" \$"(LIBBU)" \$"(LIBBU_LIBES)" \$"(LIBFB)" \$"(LIBFB_LIBES)" \$"(LIBES)" >> ${OUT_FILE}
    echo \RTEXAMPLE_DEP_LIBES = \$"(LIBRT_DEP)" \$"(LIBBN_DEP)" \$"(LIBBU_DEP)" \$"(LIBFB_DEP)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo \PL_DM_LIBES = \$"(LIBWDB)" \$"(LIBRT)" \$"(LIBBN)" \$"(LIBBU)" \$"(LIBTK)" \$"(LIBTCL)" \$"(LIBDM_LIBES)" \$"(LIBES)" >> ${OUT_FILE}
    echo \PL_DM_DEP_LIBES = \$"(LIBRT_DEP)" \$"(LIBBN_DEP)" \$"(LIBBU_DEP)" \$"(LIBWDB_DEP)" \$"(LIBTCL_DEP)" \$"(LIBTK_DEP)" \$"(LIBDM_DEP)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo all: g-xxx rtexample pl-dm >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo g-xxx:	g-xxx.o \$"(G_XXX_DEP_LIBES)" >> ${OUT_FILE}
    echo "	" \$"(CC)" \$"(LDFLAGS)" g-xxx.o \$"(G_XXX_LIBES)" -o g-xxx >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo rtexample:	rtexample.o \$"(RTEXAMPLE_DEP_LIBES)" >> ${OUT_FILE}
    echo "	" \$"(CC)" \$"(LDFLAGS)" rtexample.o \$"(RTEXAMPLE_LIBES)" -o rtexample >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo pl-dm:	pl-dm.o \$"(PL_DM_DEP_LIBES)" >> ${OUT_FILE}
    echo "	" \$"(CC)" \$"(LDFLAGS)" pl-dm.o \$"(PL_DM_LIBES)" -o pl-dm >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo clean: >> ${OUT_FILE}
    echo "	" rm -f *.o *.bak >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo clobber: >> ${OUT_FILE}
    echo "	" rm -f *.o *.bak \$"(PRODUCTS)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
    echo noprod: >> ${OUT_FILE}
    echo "	" rm -f \$"(PRODUCTS)" >> ${OUT_FILE}
    echo "" >> ${OUT_FILE}
EOF

# Run the file through cake to create a configured makefile
cake -f ${IN_FILE} > /dev/null

# remove tmp file
/bin/rm -f ${IN_FILE}
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
