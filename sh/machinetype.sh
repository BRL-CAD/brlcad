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
#	-b -v	Print all, in Bourne-Shell legible form
#
# Info note:  On a VAX-11/780, this script takes about 1.3 CPU seconds to run
#
# Mike Muuss, BRL, 10-May-1988
# With thanks to Terry Slattery and Bob Reschly for assistance
# $Revision$

# Ensure /bin/sh.  Make no remarks here, just do it.
export PATH || (sh $0 $*; kill $$)

FILE=/tmp/machtype$$
trap '/bin/rm -f ${FILE}; exit 1' 1 2 3 15	# Clean up temp file

/lib/cpp << EOF > ${FILE}
#line 1 "$0"

#if defined(unix) && defined(m68k)
#	undef	aux
	MACHINE=aux;
	UNIXTYPE=SYSV;
	HAS_TCP=0;
	HAS_SYMLINKS=1;
#endif

#ifdef vax
#	undef	vax
	MACHINE=vax;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#ifdef alliant
#	undef	fx
	MACHINE=fx;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#ifdef gould
#	undef	sel
	MACHINE=sel;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#if defined(sgi) && !defined(mips)
/*	Silicon Graphics 3D */
#	undef	sgi
	MACHINE=3d;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#if defined(sgi) && defined(mips)
/*	Silicon Graphics 4D, which uses the MIPS chip */
#	undef	sgi
	MACHINE=4d;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#if defined(sun) && !defined(sparc)
#	undef	sun
#	undef	sun3
	MACHINE=sun3;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#if defined(sparc)
#	undef	sun
#	undef	sun4
	MACHINE=sun4;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#if defined(CRAY1)
/*	Cray X-M/P running UNICOS. */
#	undef	xmp
	MACHINE=xmp;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
	HAS_SYMLINKS=0;
#endif

#if defined(CRAY2)
#	undef	cr2
	MACHINE=cr2;
	UNIXTYPE=SYSV;
	HAS_TCP=1;
	HAS_SYMLINKS=0;
#endif

#ifdef convex
#	undef	c1
	MACHINE=c1;
	UNIXTYPE=BSD;
	HAS_TCP=1;
	HAS_SYMLINKS=1;
#endif

#ifdef ardent
#	undef	ard
/*	The network code is not tested yet */
	MACHINE=ard;
	UNIXTYPE=SYSV;
	HAS_TCP=0;
	HAS_SYMLINKS=1;
#endif

#ifdef stellar
#	undef	stl
/*	The network code is not tested yet */
	MACHINE=stl;
	UNIXTYPE=SYSV;
	HAS_TCP=0;
	HAS_SYMLINKS=0;
#endif

#ifdef eta10
/*	ETA-10 running UNIX System V. */
/*	The network support is different enough that is isn't supported yet */
#	undef	eta
	MACHINE=eta;
	UNIXTYPE=SYSV;
	HAS_TCP=0;
	HAS_SYMLINKS=0;
#endif

#ifdef pyr
#	undef	pyr
	MACHINE=pyr;
	UNIXTYPE=BSD;	# Pyramid can be dual-environment, assume BSD
	HAS_TCP=1;
	HAS_SYMLINKS=1;
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
	HAS_SYMLINKS=0
	# Performing an "exit 1" here does not help any if this script
	# is being invoked by, eg, grave accents (which is a typical use).
	# So, simply return the error strings invented above,
	# which should cause more sensible errors downstream than
	# having Shell variables competely unset.
fi

# Special cases for discriminating between different versions of
# systems from vendors.
# Try very hard to avoid putting stuff here, because this technique
# is not available for use in "Cakefile.defs", so special handling
# will be required.
case ${MACHINE} in

4d)
	if test -d /usr/NeWS
	then
		# This is definitely an SGI sw Release 3.? system
		if test ! -x /tmp/gt
		then
			echo 'main(){char b[50];gversion(b);printf("%2.2s\\n",b+4);exit(0);}'>/tmp/gt.c
			cc /tmp/gt.c -lgl -o /tmp/gt
		fi
		case `/tmp/gt` in
		GT)	MACHINE=4gt;;
		PI)	MACHINE=4gt;;	# Personal Iris
		*)	MACHINE=4d;;
		esac
	else
		# This is an SGI sw Release 2 system
		MACHINE=4d2
	fi;;

esac

# Now, look at first arg to determine output behavior
case x$1 in

x|x-m)
	echo ${MACHINE}; exit 0;;
x-s)
	echo ${UNIXTYPE}; exit 0;;
x-n)
	echo ${HAS_TCP}; exit 0;;
x-a)
	if test ${UNIXTYPE} = BSD
	then	echo BSD
	else	echo ATT
	fi
	exit 0;;
x-v|x-b)
	echo "MACHINE=${MACHINE}; UNIXTYPE=${UNIXTYPE}; HAS_TCP=${HAS_TCP}; HAS_SYMLINKS=${HAS_SYMLINKS}"
	exit 0;;
*)
	echo "$0:  Unknown argument $1" 1>&2; break;;
esac
exit 1
