#!/bin/sh
#			M A C H I N E T Y P E . S H
#
# A Shell script to determine the machine architecture type,
# operating system variant (Berkeley or SysV), and
# the presence of Berkeley-style TCP networking capability.
# The machine type must be FOUR characters or less.
# 
# This is useful to permit the separation of
# incompatible binary program files, and to drive proper tailoring
# of some of the Makefiles.
#
# Note that this Shell script uses the same mechanism (ie, CPP)
# to determine the system type as the main Cakefile (Cakefile.defs)
# uses.  To support a new type of machine, the same #ifdef construction
# will be required both here and in Cakefile.defs
#
# Command args:
#	[none]	Print only machine type
#	-m	Print only machine type
#	-s	Print only system type, BRL style: (BSD, SYSV)
#	-a	Print only system type, ATT style: (BSD, ATT)
#	-n	Print only HAS_TCP variable
#	-v	Print all, in verbose human form
#	-b	Print all, in Bourne-Shell legible form
#
# Info note:  On a VAX-11/780, this script takes about 1.3 CPU seconds to run
#
# Mike Muuss, BRL, 10-May-1988
# With thanks to Terry Slattery and Bob Reschly for assistance
# $Revision$

FILE=/tmp/machtype$$
trap '/bin/rm -f ${FILE}; exit 1' 1 2 3 15	# Clean up temp file

/lib/cpp << EOF > ${FILE}
#line 1 "$0"

#ifdef vax
#	undef	vax
	MACHINE=vax;
	UNIXTYPE=BSD;
	HAS_TCP=1;
#endif

#ifdef alliant
#	undef	fx
	MACHINE=fx;
	UNIXTYPE=BSD;
	HAS_TCP=1;
#endif

#ifdef gould
#	undef	sel
	MACHINE=sel;
	UNIXTYPE=BSD;
	HAS_TCP=1;
#endif

#if defined(sgi) && !defined(mips)
/*	Silicon Graphics 3D */
#	undef	sgi
	MACHINE=3d;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
#endif

#if defined(sgi) && defined(mips)
/*	Silicon Graphics 4D, which uses the MIPS chip */
#	undef	sgi
	MACHINE=4d;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
#endif

#ifdef sun
/*	Need some way to discriminate sun3 and sun4/sparc */
#	undef	sun
#	undef	sun3
	MACHINE=sun3;
	UNIXTYPE=BSD;
	HAS_TCP=1;
#endif

#if defined(CRAY) && !defined(CRAY2)
/*	Cray X-M/P running UNICOS. */
#	undef	xmp
	MACHINE=xmp;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
#endif

#ifdef CRAY2
#	undef	cr2
	MACHINE=cr2;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
#endif

#ifdef pyramid
#	undef	pyr
	MACHINE=pyr;
	UNIXTYPE=BSD;	# Pyramid can be dual-environment, assume BSD
	HAS_TCP=1;
#endif

EOF

# Note that we depend on CPP's "#line" messages to be ignored as comments
# when sourced by the "." command here:
. ${FILE}
/bin/rm -f ${FILE}

# See if we learned anything by all this
if test x${MACHINE} = x
then
	echo "$0: ERROR, unable to determine machine type." 1>&2
	echo "$0: Consult installation instructions for details." 1>&2
	MACHINE=//error//
	UNIXTYPE=--error--
	HAS_TCP=0
	# Performing an "exit 1" here does not help any if this script
	# is being invoked by, eg, grave accents (which is a typical use).
	# So, simply return the error strings invented above,
	# which should cause more sensible errors downstream than
	# having Shell variables competely unset.
fi

# Now, look at first arg to determine output behavior
case x$1 in

x|x-m)
	echo ${MACHINE}; exit 0;;
x-s)
	echo ${SYSTEM}; exit 0;;
x-n)
	echo ${HAS_TCP}; exit 0;;
x-a)
	if test ${SYSTEM} = BSD
	then	echo BSD
	else	echo ATT
	fi
	exit 0;;
x-v)
	echo MACHINE = ${MACHINE}, UNIXTYPE = ${UNIXTYPE}, HAS_TCP = ${HAS_TCP}
	exit 0;;
x-b)
	echo "MACHINE=${MACHINE}; UNIXTYPE=${UNIXTYPE}; HAS_TCP=${HAS_TCP}"
	exit 0;;
*)
	echo "$0:  Unknown argument $1" 1>&2; break;;
esac
exit 1
