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

if [ $# -gt 0 -a X$1 = X-s ] ; then
	SILENT=-s
	shift
else
	SILENT=""
fi

############################################################################
#
# Acquire current machine type, BASEDIR, etc.
#
# newbindir.sh can be run to edit all relevant files (esp. machinetype.sh).
# Or, just set environment variable $BRLCAD_ROOT before running this script.
#
############################################################################
eval `sh sh/machinetype.sh -v`

BRLCAD_ROOT=$BASEDIR
export BRLCAD_ROOT

BINDIR=$BASEDIR/bin
MANDIR=$BASEDIR/man/man1

if [ X${SILENT} = X ] ; then
	echo "  BINDIR = ${BINDIR},  BASEDIR = ${BASEDIR}"
fi

############################################################################
#
# Sanity check
# Make sure that BINDIR is in the current user's search path
# For this purpose, specifically exclude "dot" from the check.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo
	echo Verifying that ${BINDIR} is in your search path.
fi
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
		else
			mkdir ${BINDIR}
			if test -d ${PREFIX}
			then
				# all is well
				not_found=0
				break
			fi
		fi
		echo "$0 WARNING:  ${PREFIX} is in the search path, but is not a directory."
	fi

	# Make sure that there are no conflicting files earlier in path.
	if test -f ${PREFIX}/machinetype.sh -o -f ${PREFIX}/cake
	then
		echo " "
		echo "$0 ERROR: Different version of BRL-CAD detected in ${PREFIX},"
		echo " which is earlier in your search path than ${BINDIR}."
		echo " Please place ${BINDIR} earlier in your PATH."
		exit 2
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

if [ X${SILENT} = X ] ; then
	echo "OK"
fi
############################################################################
#
# Ensure that destination directory is clean.  No stale cakes, etc.
# Then create desired directory structure.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo
	echo "Cleaning out ${BASEDIR}."
fi
echo "OK to run  \"rm -fr ${BASEDIR}/*\"  ? (yes|no)[no]"
read ANS
if test "$ANS" != "yes"
then
	echo "You did not answer 'yes', aborting."
	exit 1
fi

if [ X${SILENT} = X ] ; then
	echo "rm -fr ${BASEDIR}/*"
fi
rm -fr ${BASEDIR}/*

if [ X${SILENT} = X ] ; then
	echo
	echo Creating the necessary directories
fi
for LAST in \
	bin include include/brlcad html lib vfont \
	man man/man1 man/man3 man/man5 etc tcl tk \
	tclscripts tclscripts/mged tclscripts/nirt \
	tclscripts/pl-dm
do
	if test ! -d $BASEDIR/$LAST
	then
		mkdir $BASEDIR/$LAST
	fi
done

############################################################################
#
# Install the entire set of shell scripts from "sh/" into BINDIR
#
# Includes machinetype.sh and many others,
# but does NOT include gen.sh, setup.sh, or newbindir.sh -- those
# pertain to the installation process only, and don't get installed.
#
# Note that the installation directory is "burned in" as they are copied.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo Installing shell scripts
fi
cd sh
for i in *.sh
do
	if test -f ${BINDIR}/${i}
	then
		mv -f ${BINDIR}/${i} ${BINDIR}/`basename ${i} .sh`.bak
	fi
	sed -e 's,=/usr/brlcad$,='${BASEDIR}, < ${i} > ${BINDIR}/${i}
	chmod 555 ${BINDIR}/${i}
done
cd ..

############################################################################
#
# Make and install "cake" and "cakeaux"
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo Compiling cake and cakeaux
	echo "   " `machinetype.sh -d`
fi
if test x$1 != x-f
then
	cd cake
	make ${SILENT} clobber
	make ${SILENT} install
	make ${SILENT} clobber
	if test ! -f ${BINDIR}/cake
	then
		echo "***ERROR:  cake not installed"
	fi

	cd ../cakeaux
	make ${SILENT} clobber
	for i in cakesub cakeinclude
	do
		make ${SILENT} $i			# XXX, should do "install"
		if test -f ${BINDIR}/$i
		then
			mv -f ${BINDIR}/$i ${BINDIR}/$i.bak
		fi
		cp $i ${BINDIR}/.
	done
	make ${SILENT} clobber
	cp *.1 ${MANDIR}/.
	cd ..
fi

############################################################################
#
#  Finally, after potentially having edited Cakefile.defs, 
#  ensure that machinetype.sh and Cakefile.defs are set up the same way.
#  This is mostly a double-check on people porting to new machines.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo "Running brlcad-check.sh"
fi
if sh/brlcad-check.sh ${SILENT}
then
	if [ X${SILENT} = X ] ; then
		echo OK
	fi
else
	echo "brlcad-check.sh failed, aborting setup.sh"
	exit 1
fi

############################################################################
#
#  Make final preparations to ready things for compilation.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo "make mkdir"
fi
sh gen.sh ${SILENT} mkdir		# Won't have any effect unless NFS is set.

# Congratulations.  Everything is fine.
if [ X${SILENT} = X ] ; then
	echo "BRL-CAD initial setup is complete."
# Just doing "make install" isn't good enough, it doesn't
# compile things in bench, db, or proc-db.  You may want db.
echo "Next, run:"
echo "	make; make install"
fi
