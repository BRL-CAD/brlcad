#!/bin/sh
#			V E R I F Y . S H
#
# This shell script is used to verify the configuration of the compile-time
# environment for the BRL CAD Package.
# It is run automatically by ./setup.sh, but when hunting problems
# it can be useful all by itself.
#
#  $Header$

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

eval `sh sh/machinetype.sh -v`
BRLCAD_ROOT=$BASEDIR
export BRLCAD_ROOT

############################################################################
#
#  Ensure that machinetype.sh and Cakefile.defs are set up the same way.
#  This is mostly a double-check on people porting to new machines.
#
#  Rather than invoke CPP here trying to guess exactly how CAKE
#  is invoking it, just run CAKE on a trivial Cakefile and compare
#  the results.
#
############################################################################
echo Comparing settings between machinetype.sh and Cakefile.defs

IN_FILE=/tmp/verify_in$$
OUT_FILE=/tmp/verify_out$$
trap '/bin/rm -f ${IN_FILE} ${OUT_FILE}; exit 1' 1 2 3 15	# Clean up temp file

rm -f ${OUT_FILE}

cat > ${IN_FILE} << EOF
#line 1 "$0"
#include "Cakefile.defs"

#ifdef BSD
#undef BSD	/* So that C_UNIXTYPE can have a value of BSD, not "43", etc. */
${OUT_FILE}:
	echo C_MACHINE=MTYPE ';' > ${OUT_FILE}
	echo C_UNIXTYPE=\'BSD\' ';' >> ${OUT_FILE}
	echo C_HAS_TCP=HAS_TCP >> ${OUT_FILE}
#else
#undef SYSV
${OUT_FILE}:
	echo C_MACHINE=MTYPE ';' > ${OUT_FILE}
	echo C_UNIXTYPE=\'SYSV\' ';' >> ${OUT_FILE}
	echo C_HAS_TCP=HAS_TCP >> ${OUT_FILE}
#endif
EOF

# Run the file through CAKE.
## echo cake -f ${IN_FILE} ${OUT_FILE}
cake -f ${IN_FILE} ${OUT_FILE} > /dev/null
if test $? -ne 0
then
	/bin/rm -f ${IN_FILE} ${OUT_FILE}
	echo "$0: cake returned error code"
	exit 1
fi

if test ! -f ${OUT_FILE}
then
	/bin/rm -f ${IN_FILE} ${OUT_FILE}
	echo "$0: cake run failed to create expected output file"
	exit 1
fi

# Source the output file as a shell script, to set C_* variables here.
## cat ${OUT_FILE}
. ${OUT_FILE}

# See if things match up
if test "x${MACHINE}" != "x${C_MACHINE}" -o \
	"x${UNIXTYPE}" != "x${C_UNIXTYPE}" -o \
	"x${HAS_TCP}" != "x${C_HAS_TCP}"
then
	echo "$0 ERROR:  Mis-match between machinetype.sh and Cakefile.defs"
	echo "$0 ERROR:  machinetype.sh claims ${MACHINE} ${UNIXTYPE} ${HAS_TCP}"
	echo "$0 ERROR:  Cakefile.defs claims ${C_MACHINE} ${C_UNIXTYPE} ${C_HAS_TCP}"
	echo "It may be useful to examine ${IN_FILE} and ${OUT_FILE}"
	exit 1		# Die
fi

echo "OK: PATH, cake, machinetype.sh, and Cakefile.defs all match."
/bin/rm -f ${IN_FILE} ${OUT_FILE}
exit 0
