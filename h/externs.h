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

#if __STDC__ && !apollo
#	include <stdlib.h>
#else

/*
 *	System calls
 */
extern int	close();
extern int	dup();
extern int	execl();
#ifndef CRAY1		/* Horrid XMP UNICOS 4.0.7 /bin/cc bug if you define this */
extern void	exit();
#endif
extern int	fork();
extern int	getuid();
#ifdef BSD
extern int	lseek();
#else
extern long	lseek();
#endif
extern int	nice();
extern int	open();
extern int	pipe();
extern int	read();
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

extern int	atoi();
extern int	qsort();
extern int	strcmp();

extern long	time();

#endif /* __STDC__ */
#endif /* EXTERNS_H_SEENYET */
