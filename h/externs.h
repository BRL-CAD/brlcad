/*
 *  			E X T E R N S . H
 *
 *  Declarations for C library routines and UNIX system calls.
 *  Inspired by the ANSI C header file <stdlib.h>
 *  Not claimed to be complete (yet)
 *  
 *  Authors -
 *	Michael John Muuss
 *	Charles M. Kennedy
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */
#ifndef EXTERNS_H_SEENYET
#define EXTERNS_H_SEENYET

/*
 *  First, define all the routines found in BRL-CAD's libsysv
 */
#if USE_PROTOTYPES
extern int	bsdselect( long readfds, int sec, int us );
#if !defined(__stardent) && !defined(__bsdi__)
extern int	getopt( int argc, char **argv, char *optstr );
#endif
extern char	*re_comp( char *s );
extern int	re_exec( char *s );

#else
extern int	bsdselect();
extern int	getopt();
extern char	*re_comp();
extern int	re_exec();
#endif

extern char	*optarg;
extern int	optind;
extern int	opterr;

#if (__STDC__ && !apollo) || (sgi && mips)
	/* SGI has this, even though it isn't ANSI */
#	include <stdlib.h>

#	if defined(__stardent)
		extern FILE	*popen( const char *, const char * );
		extern FILE	*fdopen( int, const char * );
#	endif
#else

/*
 *	System calls
 */
extern int	close();
extern int	dup();
#ifndef CRAY1		/* Horrid XMP UNICOS 4.0.7 /bin/cc bug if you define this */
extern void	exit();
#endif

#if !defined(__convexc__)
extern int	execl();
extern int	fork();
extern int	getuid();

#ifdef BSD
extern int	lseek();
#else
extern long	lseek();
#endif

extern int	open();

#endif /* !defined(__convexc__) */

extern int	nice();
extern int	pipe();
extern int	read();
extern char	*sbrk();
extern unsigned	sleep();
extern void	sync();
extern int	unlink();
extern int	wait();
extern int	write();

/*
 *	C Library Routines
 */
extern void	perror();
extern void	free();

extern char	*malloc();
extern char	*calloc();
extern char	*getenv();
extern char	*realloc();
extern char	*tempnam();
extern char	*strcpy();
extern char	*strcat();
extern char	*strncat();
extern char	*mktemp();

extern int	atoi();
extern int	qsort();
extern int	strcmp();

extern long	time();

/*
 *	STDIO Library Routine supplements
 */
#if defined(alliant) ||  defined(__stardent)
	extern FILE	*popen(); /* Not declared in stdio.h */
#endif
#if defined(__stardent)
	extern FILE	*fdopen();
#endif

/*
 *	Math Library Routines
 */
extern double	atof();

#endif /* __STDC__ */
#endif /* EXTERNS_H_SEENYET */
