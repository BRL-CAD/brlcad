/*
**	Cake configuration parameters.
**
**	$Header$
*/

/* type used when type is not known */
typedef	int	Cast;

/* location of the systemwide cake library */
/*#define	SLIB		"/u/pgrad/zs/lib/cake" **BRL*/
#define	SLIB		"/usr/brlcad/lib/cake"

/* suffix (after $HOME) of personal libraries */
#define	ULIB		"/lib/cake"

/* command to invoke the C preprocessor */
#if _AIX
	/*
	 *  Using CPP is necessary for the IBM RS/6000, because their
	 *  cc -E will not work unless the source file name ends in .c
	 */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D_AIX=1"
#endif
#if defined(convex) || defined(__convex__)
	/* CPP is necessary on the Convex, cc -E needs .c suffix */
#	define	CPP		"/lib/cpp"
#endif
#if defined(i386) && defined(__bsdi__)
#	define	CPP	"/usr/bin/cpp"
#endif
#if defined(__386BSD__)
#	define	CPP	"/usr/bin/cpp"
#endif
#if defined(__NetBSD__)
#	define	CPP	"/usr/bin/cpp"
#endif
#if defined(i386) && !defined(__bsdi__) && !defined(__386BSD__) && !defined(__NetBSD__)
	/* CPP is necessary on the PC/AT, cc -E needs .c suffix */
#	define	CPP		"/lib/cpp"
#endif
#if defined(NeXT)
	/* Gnu CPP */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-traditional"
#endif
#if defined(ardent)
	/* Stardent 3000 Series running UNIX 4.1 */
#	define	CPP		"/lib/cpp"
#endif
#if !defined(alliant) && defined(i860) && defined(unix) && __STDC__ == 0
	/* Stardent i860 machine.  cc -E puts spaces around substitutions */
#	define	CPP		"/lib/cpp"
#	define	CPP_OPTIONS	"-D__stardent=1"
#endif

#if ((defined(sun) && defined(sparc)) || defined(__sparc)) && defined(ATT)
	/* SunOS 5 with unbundled compilers */
#	undef __STDC__
#	define __STDC__ 1	/* hack! */
#	define	CPP		"cc"
	/*
	 * WARNING:  Make sure /opt/SUNWspro/bin is ahead of /usr/ucb
	 * in your path.
	 * /opt/SUNWspro/bin/cc -E puts unwanted spaces around
	 * substitutions so fall back to old SunC mode
	 */
#	define	CPP_OPTIONS	"-E"
#	define	CPP_OPTIONS2	"-Xs"
#	define	CPP_OPTIONS3	"-D__CAKE__SunOS5=1"
#endif

/*
 *  The SGI story:
 *	On Irix 3, "cc main.c" does only sgi, and not __sgi.
 *	On Irix 4, "cc main.c" does not have __STDC__ defined.
 *	On Irix 5, "cc main.c" has sgi=1, __sgi=1, __STDC__=1.
 *  Use this fact to pass a flag on to Cakefile.defs.
 *  cc -E appends a bloody space after each substitution.
 */
#if defined(sgi) || defined(__sgi)
#	define	CPP		"/lib/cpp"
#	if __STDC__
#		define	CPP_OPTIONS	"-D__CAKE__irix5"
#	else
#		if defined(__sgi)
#			define	CPP_OPTIONS	"-D__CAKE__irix4"
#		else
#			define	CPP_OPTIONS	"-D__CAKE__irix3"
#		endif
#	endif
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
#define	STATS_FILE	"/usr/brlcad/lib/cake_stats"

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
