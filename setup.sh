#!/bin/sh
#			S E T U P . S H
#
# This shell script is to be run as the very first step in installing
# the BRL CAD Package
#
# Secret option:  -f, (fast) to skip re-compiling Cake.
#
#  $Header$

# Ensure that all subordinate scripts run with the Bourne shell,
# rather than the user's shell
SHELL=/bin/sh
export SHELL

# Ensure that other users can read and execute what we install!
umask 002


############################################################################
#
# Acquire current machine type, BASEDIR, etc.
#
# newbindir.sh can be run to edit all relevant files (esp. machinetype.sh).
#
############################################################################
eval `sh sh/machinetype.sh -v`

BINDIR=$BASEDIR/bin
MANDIR=$BASEDIR/man/man1

echo "  BINDIR = ${BINDIR},  BASEDIR = ${BASEDIR}"
echo
echo Creating the necessary directories

for LAST in \
	bin include include/brlcad html lib vfont \
	man man/man1 man/man3 man/man5 etc tcl tk tclscripts
do
	if test ! -d $BASEDIR/$LAST
	then
		mkdir $BASEDIR/$LAST
	fi
done

############################################################################
#
# Sanity check
# Make sure that BINDIR is in the current user's search path
# For this purpose, specifically exclude "dot" from the check.
#
############################################################################
echo Checking search path
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
# Install the entire set of shell scripts from "sh/" into BINDIR
#
# Includes machinetype.sh and many others,
# but does NOT include gen.sh, setup.sh, or newbindir.sh -- those
# pertain to the installation process only, and don't get installed.
#
############################################################################
echo Installing shell scripts
cd sh
for i in *.sh
do
	if test -f ${BINDIR}/${i}
	then
		mv -f ${BINDIR}/${i} ${BINDIR}/`basename ${i} .sh`.bak
	fi
	cp ${i} ${BINDIR}/.
done
cd ..

############################################################################
#
# Make and install "cake" and "cakeaux"
#
############################################################################
echo Compiling cake and cakeaux
if test x$1 != x-f
then
	cd cake
	make clobber
	make install
	make clobber
	if test ! -f ${BINDIR}/cake
	then
		echo "***ERROR:  cake not installed"
	fi

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
	cp *.1 ${MANDIR}/.
	cd ..
fi

############################################################################
#
#  Finally, after potentially having edited Cakefile.defs, 
#  ensure that machinetype.sh and Cakefile.defs are set up the same way.
#  This is mostly a double-check on people porting to new machines.
#
#  Rather than invoke CPP here trying to guess exactly how CAKE
#  is invoking it, just run CAKE on a trivial Cakefile and compare
#  the results.
#
############################################################################
echo Comparing settings between machinetype.sh and Cakefile.defs
IN_FILE=/tmp/Cakefile$$
OUT_FILE=/tmp/setup$$
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

# Congratulations.  Everything is fine.
echo "BRL-CAD initial setup is complete."
