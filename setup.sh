#!/bin/sh
#			S E T U P . S H
#
# This shell script is to be run as the very first step in installing
# the BRL CAD Package
# Secret option:  -f, (fast) to skip re-compiling Cake.
#
#  $Header$

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

# Confirm that the installation directory is correct,
# or change it in:
# Cakefile.defs, cake/Makefile, cakeaux/Makefile, setup.sh, cray.sh
# This is unfortunate, and needs to come from some file, somehow.

BINDIR=/usr/brlcad/bin

echo "  BINDIR = ${BINDIR}"

############################################################################
#
# Sanity check
# Make sure that BINDIR is in the current user's search path
# For this purpose, specifically exclude "dot" from the check.
#
############################################################################
PATH_ELEMENTS=`echo $PATH | sed 's/^://
				s/:://g
				s/:$//
				s/:\\.:/:/g
				s/:/ /g'`

not_found=1		# Assume cmd not found
for PREFIX in ${PATH_ELEMENTS}
do
	if test ${PREFIX} = ${BINDIR}
	then
		# This was -x, but older BSD systems don't do -x.
		if test -d ${PREFIX}
		then
			# all is well
			not_found=0
			break
		fi
		echo "$0 WARNING:  ${PREFIX} is in the search path, but is not a directory."
	fi
done
if test ${not_found} -ne 0
then
	echo "$0 ERROR:  ${BINDIR} (BINDIR) is not in your Shell search path!"
	echo "$0 ERROR:  Software setup can not proceed until this has been fixed."
	echo "$0 ERROR:  Consult installation directions for more information,"
	echo "$0 ERROR:  file: install.doc, section INSTALLATION DIRECTORIES."
	exit 1		# Die
fi

############################################################################
#
# Acquire current machine type, etc.
#
############################################################################
eval `sh machinetype.sh -b`

############################################################################
#
# Install the necessary shell scripts in BINDIR
#
############################################################################
SCRIPTS="machinetype.sh cakeinclude.sh cray.sh ranlib5.sh sgisnap.sh"

for i in ${SCRIPTS}
do
	if test -f ${BINDIR}/${i}
	then
		mv -f ${BINDIR}/${i} ${BINDIR}/`basename ${i} .sh`.bak
	fi
	cp ${i} ${BINDIR}/.
done

############################################################################
#
# Handle any vendor-specific configurations and setups.
# This is messy, and should be avoided where possible.
#
############################################################################
chmod 664 Cakefile.defs

############################################################################
#
#  Finally, after potentially having edited Cakefile.defs, 
#  ensure that machinetype.sh and Cakefile.defs are set up the same way.
#  This is mostly a double-check on people porting to new machines.
#
#  Note that the `echo MTYPE` is necessary because some newer AT&T cpp's
#  add a space around each macro substitution.
#
############################################################################
IN_FILE=/tmp/setup$$.c
OUT_FILE=/tmp/setup$$
trap '/bin/rm -f ${IN_FILE} ${OUT_FILE}; exit 1' 1 2 3 15	# Clean up temp file

cat << EOF > ${IN_FILE}
#line 1 "$0"
#include "Cakefile.defs"

C_MACHINE=\`echo MTYPE\`;
#ifdef BSD
	C_UNIXTYPE="BSD";
#else
	C_UNIXTYPE="SYSV";
#endif
C_HAS_TCP=\`echo HAS_TCP\`;
EOF
# Run the file through the macro preprocessor.
# Many systems don't provide many built-in symbols with bare CPP,
# so try to run through the compiler.
# Using cc is essential for the IBM RS/6000, and helpful on the SGI.
cc -E -I. -DDEFINES_ONLY ${IN_FILE} > ${OUT_FILE}
if test $? -ne 0
then
	# Must be an old C compiler without -E, fall back to /lib/cpp
	/lib/cpp -DDEFINES_ONLY < ${IN_FILE} > ${OUT_FILE}
fi

# Note that we depend on CPP's "#line" messages to be ignored as comments
# when sourced by the "." command here:
. ${OUT_FILE}
/bin/rm -f ${IN_FILE} ${OUT_FILE}

# See if things match up
if test x${MACHINE} != x${C_MACHINE} -o \
	x${UNIXTYPE} != x${C_UNIXTYPE} -o \
	x${HAS_TCP} != x${C_HAS_TCP}
then
	echo "$0 ERROR:  Mis-match between machinetype.sh and Cakefile.defs"
	echo "$0 ERROR:  machinetype.sh claims ${MACHINE} ${UNIXTYPE} ${HAS_TCP}"
	echo "$0 ERROR:  Cakefile.defs claims ${C_MACHINE} ${C_UNIXTYPE} ${C_HAS_TCP}"
	exit 1		# Die
fi


############################################################################
#
# Make and install "cake" and "cakeaux"
#
############################################################################
if test x$1 != x-f
then
	cd cake
	make clobber
	make install
	make clobber

	cd ../cakeaux
	make clobber
	for i in cakesub cakeinclude
	do
		make $i			# XXX, should do "install"
		if test -f ${BINDIR}/$i
		then
			mv -f ${BINDIR}/$i ${BINDIR}/$i.bak
		fi
		cp $i ${BINDIR}/.
	done
	make clobber
	cd ..
fi
