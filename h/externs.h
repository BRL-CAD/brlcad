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
#ifndef EXTERNS_H
#define EXTERNS_H

/* We need sys/select.h under Irix 5 to get fd_set. */

#ifdef HAVE_SYS_SELECT_H
#	include <sys/select.h>
#endif

/* POSIX says FD_SET() et.al. go in <sys/time.h> rather than <sys/types.h> */
#if defined(_POSIX_SOURCE) || defined(linux)
#	include <sys/time.h>
#endif

/* Some additional fd_set manipulation routines */

#ifndef FD_OR
#define FD_OR(x, a, b) { register int _i; for (_i = 0; _i < FD_SETSIZE; _i++) \
	if (FD_ISSET(_i, a) || FD_ISSET(_i, b)) FD_SET(_i, x); \
	else FD_CLR(_i, x); }
#endif

#ifndef FD_AND
#define FD_AND(x, a, b) { register int _i; for (_i = 0; _i < FD_SETSIZE; _i++)\
	if (FD_ISSET(_i, a) && FD_ISSET(_i, b)) FD_SET(_i, x); \
	else FD_CLR(_i, x); }
#endif

#ifndef FD_MOVE
#define FD_MOVE(a, b) { register int _i; for (_i = 0; _i < FD_SETSIZE; _i++) \
	if (FD_ISSET(_i, b)) FD_SET(_i, a); else FD_CLR(_i, a); }
#endif

#ifndef FD_OREQ
#define FD_OREQ(a, b) { register int _i; for (_i = 0; _i < FD_SETSIZE; _i++) \
	if (FD_ISSET(_i, b)) FD_SET(_i, a); }
#endif

#ifndef FD_ANDEQ
#define FD_ANDEQ(a, b) { register int _i; for (_i = 0; _i < FD_SETSIZE; _i++) \
	if (!FD_ISSET(_i, a) || !FD_ISSET(_i, b)) FD_CLR(_i, a) }
#endif

/* Needed on Irix 6 to declare struct timeval for unistd.h for select() */
#ifdef IRIX
#	include <sys/time.h>
#endif

/* Here, we want to include unistd.h if we have it to get the definitions
   of things such as off_t.  If we don't have it, make some good guesses. */

#ifdef HAVE_UNISTD_H
	/* Has close(), fork(), exec(), getopt() */
	/* But *not* open()!!!! */
#	include <unistd.h>

	/* This is UNIX, pick up the header files for remaining system calls */
#	include <fcntl.h>	/* for open() */
#	include <sys/wait.h>	/* for wait() */

#else /* HAVE_UNISTD_H */

#	if !defined(OFF_T) && !defined(HAVE_OFF_T)
#		define	off_t	long
#		define	OFF_T	long
#	endif

/*
 *	System calls
 */
extern int	close();
extern int	dup();
#ifndef CRAY1		/* Horrid XMP UNICOS 4.0.7 /bin/cc bug if you define this */
extern void	exit();
#endif

extern int	execl();
extern int	fork();
extern int	getuid();

extern int	open();
extern off_t	lseek();
extern int	nice();
extern int	pipe();
extern int	read();
extern unsigned	sleep();
extern void	sync();
extern int	unlink();
extern int	wait();
extern int	write();

extern void	*shmat();
extern int	shmget();
#endif



#ifdef HAVE_STDLIB_H
	/* Has abort(), atof(), exit(), malloc(), free(), qsort(), atof() */
#	include <stdlib.h>
#else /* HAVE_STDLIB_H */

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
extern double	atof();
extern int	qsort();
extern int	strcmp();

extern long	time();

#endif /* HAVE_STDLIB_H */


#if defined(alliant) && !defined(__STDC__)
extern double   modf();
#endif


/*
 *  Now, define all the routines found in BRL-CAD's libsysv
 */
#if USE_PROTOTYPES
extern void	port_setlinebuf( FILE *fp );

#	if defined(HAVE_GETOPT_H)
#		include <getopt.h>
#	else
#		if !defined(HAVE_GETOPT_DECL)
extern int	getopt( int argc, char **argv, char *optstr );
#		endif
#	endif

#	if !defined(HAVE_REGEX_DECL)
extern char	*re_comp( char *s );
extern int	re_exec( char *s );
#	endif

#else	/* USE_PROTOTYPES */

extern void	port_setlinebuf();
#	if defined(HAVE_GETOPT_H)
#		include <getopt.h>
#	else
#		if !defined(HAVE_GETOPT_DECL)
extern int	getopt();
#		endif
#	endif
#	if !defined(HAVE_REGEX_DECL)
extern char	*re_comp();
extern int	re_exec();
#	endif
#endif	/* USE_PROTOTYPES */

#if !defined(HAVE_GETOPT_DECL)
/* getopt munchies */
extern char	*optarg;
extern int	optind;
extern int	opterr;
#endif

/* sys_errlist and errno */
extern int	errno;
#ifndef HAVE_SYS_ERRLIST_DECL
extern int	sys_nerr;
extern char *	sys_errlist[];
#endif

#ifndef HAVE_SBRK
extern char *	sbrk();
extern int	brk();
#endif

/* Neither ANSI nor POSIX define these, they are UNIX-specific.
 * But we need them, and all UNIX systems provide them.
 */
#if __STDC__
#	ifndef HAVE_POPEN_DECL
	extern FILE	*fdopen( int filedes, const char *type );
	extern FILE	*popen( const char *command, const char *type );
#	endif
#	ifndef HAVE_STRDUP_DECL
	extern char	*strdup(const char *s);
#	endif
#endif /* __STDC__ */

#endif /* EXTERNS_H */
