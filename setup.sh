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

# identify ourself
if [ x${SILENT} = x ] ; then
    echo "B R L - C A D   S E T U P"
    echo "========================="
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

BRLCAD_BINDIR=$BRLCAD_ROOT/bin
BRLCAD_MANDIR=$BRLCAD_ROOT/man/man1

if [ x${SILENT} = x ] ; then
    echo "BRLCAD_ROOT=${BRLCAD_ROOT}"
    echo "BRLCAD_BINDIR=${BRLCAD_BINDIR}"
    echo "BRLCAD_MANDIR=${BRLCAD_MANDIR}"
fi

############################################################################
#
# Sanity check
# Make sure that BRLCAD_BINDIR is in the current user's search path
# For this purpose, specifically exclude "dot" from the check.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo
	echo Verifying that ${BRLCAD_BINDIR} is in your search path.
fi
PATH_ELEMENTS=`echo $PATH | sed 's/^://
				s/:://g
				s/:$//
				s/:\\.:/:/g
				s/:/ /g'`

not_found=1		# Assume cmd not found
for PREFIX in ${PATH_ELEMENTS}
do
	if test ${PREFIX} = ${BRLCAD_BINDIR}
	then
		# This was -x, but older BSD systems don't do -x.
		if test -d ${PREFIX}
		then
			# all is well
			not_found=0
			break
		else
			mkdir -p ${BRLCAD_BINDIR}
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
	# These might be due to a previous verion of BRL-CAD being
	# in the search path, or a user with old copies of these
	# programs in a personal "bin" directory.
	# Either way, we can't depend on it having the proper definitions
	# suitable for the latest release
	for i in machinetype.sh cake brlcad-check.sh
	do
		if test -f ${PREFIX}/$i
		then
			echo " "
			echo "$0 ERROR: detected presence of ${PREFIX}/$i"
			echo " "
			echo " Different version of BRL-CAD detected in ${PREFIX},"
			echo " which is earlier in your search path than ${BRLCAD_BINDIR}."
			echo " "
			echo " Please place ${BRLCAD_BINDIR} earlier in your PATH"
			echo " at least while you are installing BRL-CAD."
			exit 2
		fi
	done
done
if test ${not_found} -ne 0
then
	echo "$0 ERROR:  ${BRLCAD_BINDIR} (BRLCAD_BINDIR) is not in your Shell search path!"
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

# First, make sure our current directory is at the top of the source tree
# Check for presence of a few representative files.
for i in Cakefile.defs sh/machinetype.sh librt/prep.c
do
	if test ! -f $i
	then
		echo "ERROR: Current directory for setup.sh isn't top of source tree."
		echo "Can't locate file $i"
		exit 1
	fi
done

# Next, make sure they don't have the sources also in BRLCAD_ROOT.
set -- `/bin/ls -laid .`
CWD_INUM=$1
set -- `/bin/ls -laid ${BRLCAD_ROOT}`
BRLCAD_ROOT_INUM=$1
if test "${CWD_INUM}" -eq "${BRLCAD_ROOT_INUM}"
then
	echo
	echo "ERROR: BRLCAD_ROOT can't point to the directory containing the source code."
	echo "Please make the executable tree either be a proper subdirectory"
	echo "of the source tree, such as ${BRLCAD_ROOT}/exec,"
	echo "or point it at a completely separate tree."
	exit 1
fi

# Now ask for permission to go blasting.
if [ X${SILENT} = X ] ; then
	echo
	echo "Cleaning out ${BRLCAD_ROOT}."
fi

inst_dirs=" bin etc html include itcl3.2 itk3.2 lib man pro-engineer \
	sample_applications tcl8.3 tclscripts tk8.3 vfont"

echo "Is it OK to delete the following directories:"
for dir in $inst_dirs ; do
    echo "   ${BRLCAD_ROOT}/$dir"
done
echo "(yes|no)[no]"

read ANS
if test "$ANS" != "yes"
then
	echo "You did not answer 'yes', aborting."
	exit 1
fi

for dir in $inst_dirs ; do
    if [ X${SILENT} = X ] ; then
	echo "rm -fr ${BRLCAD_ROOT}/$dir"
    fi
    rm -fr ${BRLCAD_ROOT}/$dir
done

if [ X${SILENT} = X ] ; then
	echo
	echo Creating the necessary directories
fi
for LAST in \
	bin include include/brlcad lib vfont etc \
	man man/man1 man/man3 man/man5 \
	tclscripts tclscripts/mged tclscripts/nirt \
		tclscripts/lib tclscripts/util tclscripts/pl-dm \
	html html/manuals html/manuals/mged \
		html/manuals/mged/animmate html/manuals/libdm \
		html/manuals/shaders html/manuals/Anim_Tutorial \
		html/manuals/librt html/manuals/libbu html/manuals/cadwidgets \
		html/ReleaseNotes html/ReleaseNotes/Rel5.0 \
		html/ReleaseNotes/Rel5.0/Summary \
	pro-engineer pro-engineer/text pro-engineer/sgi_elf2 \
		pro-engineer/sun4_solaris pro-engineer/text/fullhelp \
		pro-engineer/text/menus sample_applications
do
	if test ! -d $BRLCAD_ROOT/$LAST
	then
		mkdir -p $BRLCAD_ROOT/$LAST
	fi
done

# Make our permissions track those in Cakefile.defs
BINPERM=`grep BINPERM Cakefile.defs | head -1| awk '{print $3}' `
MANPERM=`grep MANPERM Cakefile.defs | head -1| awk '{print $3}' `
echo BINPERM=${BINPERM}, MANPERM=${MANPERM}


############################################################################
#
# Install the entire set of shell scripts from "sh/" into BRLCAD_BINDIR
#
# Includes machinetype.sh and many others,
# but does NOT include gen.sh, setup.sh, or newbindir.sh -- those
# pertain to the installation process only, and don't get installed.
#
# Note that the installation directory is "burned in" as they are copied.
#
############################################################################
if [ X${SILENT} = X ] ; then
	echo -n "Installing shell scripts..."
fi
cd sh
for i in *.sh
do
	if test -f ${BRLCAD_BINDIR}/${i}
	then
		mv -f ${BRLCAD_BINDIR}/${i} ${BRLCAD_BINDIR}/`basename ${i} .sh`.bak
	fi
	sed -e 's,=/usr/brlcad$,='${BRLCAD_ROOT}, < ${i} > ${BRLCAD_BINDIR}/${i}
	chmod ${BINPERM} ${BRLCAD_BINDIR}/${i}
done
cd ..
if [ X${SILENT} = X ] ; then
	echo "done"
fi

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
	if [ ! -d .cake.$MACHINE ] ; then
		mkdir .cake.$MACHINE
	fi
	cp cake/*.[chyl1] cake/Makefile .cake.$MACHINE

	cd .cake.$MACHINE
	make ${SILENT} clobber
	make ${SILENT} install
	/usr/bin/make ${SILENT} clobber

	if test ! -f ${BRLCAD_BINDIR}/cake
	then
	    echo "***ERROR:  cake not installed"
	else
	    echo "cake installed"
	fi
	cd ../

	if [ ! -d .cakeaux.$MACHINE ] ; then
	    mkdir .cakeaux.$MACHINE
	fi
	cp cakeaux/*.[chyl1] cakeaux/Makefile .cakeaux.$MACHINE
	cd .cakeaux.$MACHINE

	make ${SILENT} clobber
	for i in cakesub cakeinclude
	do
		make ${SILENT} $i			# XXX, should do "install"
		if test -f ${BRLCAD_BINDIR}/$i
		then
			mv -f ${BRLCAD_BINDIR}/$i ${BRLCAD_BINDIR}/$i.bak
		fi
		cp $i ${BRLCAD_BINDIR}/.
		chmod ${BINPERM} ${BRLCAD_BINDIR}/$i
	done

	make ${SILENT} clobber
	cp cakesub.1 ${BRLCAD_MANDIR}/.
	chmod ${MANPERM} ${BRLCAD_MANDIR}/cakesub.1
	cd ..
fi
echo "Done making cake and cakeaux"
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
echo " "
echo "	make; make install"
echo " "
echo "(It is vitally important that you run both of these commands"
echo "as this is a two-step operation.)"
fi
