/*
**	Cake configuration parameters.
**
**	$Header$
*/

/* type used when type is not known.  Needs to be as wide as a (char*) */
typedef	long	Cast;

/* location of the systemwide cake library */
/*#define	SLIB		"/u/pgrad/zs/lib/cake" **BRL*/
#define	SLIB		"/usr/brlcad/lib/cake"

/* suffix (after $HOME) of personal libraries */
#define	ULIB		"/lib/cake"

/*
 * Machine-specific commands to invoke the C preprocessor
 */
#if __MACHINETYPE__vax
	/* VAX 11/780, mircoVAX, etc. */
#	define	CPP		"cc"
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-D__CAKE__vax"
#endif

#if __MACHINETYPE__alpha
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__alpha"
#endif

#if __MACHINETYPE__mips
	/* DECStation with MIPS chip */
#	define	CPP		"cc"
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-D__CAKE__mips"
#endif

#if __MACHINETYPE__ibm
	/*
	 *  Using CPP is necessary for the IBM RS/6000, because their
	 *  cc -E will not work unless the source file name ends in .c
	 */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__ibm"
#endif

#if __MACHINETYPE__c1
	/* CPP is necessary on the Convex, cc -E needs .c suffix */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__c1"
#	define	CPP_OPTIONS2	"-pcc"
#endif

#if __MACHINETYPE__cr2
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-N"
#	define	CPP_OPTIONS2	"-D__CAKE__cr2"
#endif

#if __MACHINETYPE__xmp
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-N"
#	define	CPP_OPTIONS2	"-D__CAKE__xmp"
#endif

#if __MACHINETYPE__bsdi
	/* BSD/OS from BSDI */
#	define	CPP		"/usr/bin/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__bsdi"
#endif

#if __MACHINETYPE__fbsd
	/* FreeBSD OS from www.frebsd.org */
#	define	CPP		"/usr/bin/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__fbsd"
#endif

#if __MACHINETYPE__li
	/* Linux with GNU CPP */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-traditional"
#	define	CPP_OPTIONS2	"-D__CAKE__linux"
#endif

#if __MACHINETYPE__386
	/* 386BSD from Jolitz */
#	define	CPP		"/usr/bin/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__386"
#endif

#if __MACHINETYPE__nb
	/* NetBSD */
#	define	CPP		"/usr/bin/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__nb"
#endif

#if __MACHINETYPE__nb86
	/* NetBSD 86 */
#	define	CPP		"/usr/bin/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__nb86"
#endif

#if __MACHINETYPE__at
	/* CPP is necessary on the PC/AT with Interactive Systems Unix,
	 * cc -E needs .c suffix */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__at"
#endif

#if __MACHINETYPE__next
	/* Gnu CPP */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-traditional"
#	define	CPP_OPTIONS2	"-D__CAKE__next"
#endif

#if __MACHINETYPE__aux
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__aux"
#endif

#if __MACHINETYPE__ard
	/* Stardent 3000 Series running UNIX 4.1 */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__ard"
#endif

#if __MACHINETYPE__stad
	/* Stardent i860 machine.  cc -E puts spaces around substitutions */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__stad"
#endif

#if __MACHINETYPE__sun3
#	define	CPP		"cc"
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-D__CAKE__sun3"
#endif

#if __MACHINETYPE__sun4
#	define	CPP		"cc"
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-D__CAKE__sun4"
#endif

#if __MACHINETYPE__sun5
#if defined( __GNUC__ )
	/* GNU compiler */
#	define	CPP		"gcc"
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-traditional"
#	define	CPP_OPTIONS3	 "-D__CAKE__SunOS5"
#else
	/* SunOS 5 with unbundled compilers */
#	define	CPP		"cc"
	/*
	 * WARNING:  Make sure /opt/SUNWspro/bin is ahead of /usr/ucb
	 * in your path.
	 * /opt/SUNWspro/bin/cc -E puts unwanted spaces around
	 * substitutions so fall back to old SunC mode
	 */
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-Xs"
/* XXX This next symbol is irregular, and needs to be changed */
#	define	CPP_OPTIONS3	"-D__CAKE__SunOS5"
#endif
#endif

/*
 *  The SGI story:
 *	4d: On Irix 3, "cc main.c" does only sgi, and not __sgi.
 *	5d: On Irix 4, "cc main.c" does not have __STDC__ defined.
 *	6d: On Irix 5, "cc main.c" has sgi=1, __sgi=1, __STDC__=1.
 *  Use this fact to pass a flag on to Cakefile.defs.
 *  cc -E appends a bloody space after each substitution.
 */
#if __MACHINETYPE__4d
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__4d"
#	define	CPP_OPTIONS2	"-D__CAKE__irix3"
#endif
#if __MACHINETYPE__5d
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__5d"
#	define	CPP_OPTIONS2	"-D__CAKE__irix4"
#endif
#if __MACHINETYPE__6d
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__6d"
#	define	CPP_OPTIONS2	"-D__CAKE__irix5"
#endif
#if __MACHINETYPE__7d
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__7d"
#	define	CPP_OPTIONS2	"-D__CAKE__irix6"
#endif
#if __MACHINETYPE__8d
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__CAKE__8d"
#	define	CPP_OPTIONS2	"-D__CAKE__irix7"
#endif

#if !defined(CPP)
#    if 1
	/*
	 *  Some systems handle cc -E quite nicely, and don't
	 *  append spaces after each substitution.
	 *  Using cc gets you more configuration flags than cpp these days.
	 */
#	define	CPP		"cc"
#	define	CPP_OPTIONS	"-E"
#    else
#	define	CPP		"/lib/cpp"
	/* If C preprocessor needs options, define them here.  For example, GNU CPP */
#	if 0
#		define	CPP_OPTIONS	"-traditional"
#	endif
#    endif
#endif

/* location of the statistics file - if not defined, no stats kept */
/*#define	STATS_FILE	"/u/pgrad/zs/lib/cake_stats" **BRL*/
/* #define	STATS_FILE	"/usr/brlcad/lib/cake_stats" **BRL*/

/* characters always requiring shell attention */
#define	METACHARS	"*?!&|;<>()[]{}'`\"%$~#"

/* default command to execute non-script shell actions */
#define	SYSTEM_CMD	"/bin/csh -cf"

/* default command to execute shell scripts */
#define	SCRIPT_CMD	"/bin/csh -f"

/*	System V compatibility			*/
#ifdef	ATT
/* the resolution of the times(2) system call */
#ifdef	exlsi
#define	TIMERES		100
#else
#define	TIMERES		60
#endif

#include		"port.h"

#define	index(s, c)	strchr(s, c)
#define	rindex(s, c)	strrchr(s, c)
#endif

#define	MAXARGS		128		/* BRL, was 64 */
#define	MAXGSTACK	128
#define	MAXARGSIZE	128
#define	MAXPATSIZE	(8*1024)	/* BRL, was 512 */
#define	MAXLEXBUF	(8*1024)	/* BRL, was 2048 */
#define	MAXSIZE		(16*1024)	/* BRL, was 2048 */
#define	MAXSCRIPT	16384
#define	MAXNEST		8
#define	SIZE		97		/* size of tables */
#define	CHARSETSIZE	256
#define	GENESIS		(time_t) 42	/* something distinctive */
#define	CHASEROOT	"!MAINCAKE!"
