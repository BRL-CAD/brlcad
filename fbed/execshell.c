/*
	SCCS id:	@(#) execshell.c	2.1
	Modified: 	12/9/86 at 15:55:07
	Retrieved: 	12/26/86 at 21:54:03
	SCCS archive:	/vld/moss/src/fbed/s.execshell.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) execshell.c 2.1, modified 12/9/86 at 15:55:07, archive /vld/moss/src/fbed/s.execshell.c";
#endif

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "./extern.h"

#define DFL_SHELL	"/bin/sh"

extern void		perror(), exit();
extern char		*getenv();
extern int		errno;

/*	e x e c _ S h e l l ( )
	If args[0] is NULL, spawn a shell, otherwise execute the specified
	command line.
	Return the exit status of the program, or -1 if wait() or fork()
	return an error.
 */
int
exec_Shell( args )
char	*args[];
	{
	register int	child_pid;

	if( args[0] == NULL )
		{ char	*arg_sh = getenv( "SHELL" );
		/* $SHELL, if set, DFL_SHELL otherwise.			*/
		if( arg_sh == NULL )
			arg_sh = DFL_SHELL;
		args[0] = arg_sh;
		args[1] = NULL;
		}
	switch( child_pid = fork() )
		{
		case -1 :
			fb_log( "\"%s\" (%d) could not fork.\n",
				__FILE__, __LINE__
				);
			return	-1;
		case  0 : /* Child process - execute.		*/
			sleep( 2 );
			(void) execvp( args[0], args );
			fb_log( "%s : could not execute.\n", args[0] );
			exit( errno );
		default :
			{	register int	pid;
				int		stat_loc;
#if defined( BSD ) || defined( sgi ) || defined( CRAY )
#ifndef SIGCLD
#define SIGCLD	SIGCHLD
#endif
				register int	(*istat)(), (*qstat)(), (*cstat)();
#else
				register void	(*istat)(), (*qstat)(), (*cstat)();
#endif
			istat = signal(SIGINT, SIG_IGN);
			qstat = signal(SIGQUIT, SIG_IGN);
			cstat = signal(SIGCLD, SIG_DFL);
			while(	(pid = wait( &stat_loc )) != -1
			     && pid != child_pid
				)
				;
			(void) signal(SIGINT, istat);
			(void) signal(SIGQUIT, qstat);
			(void) signal(SIGCLD, cstat);
			if( pid == -1 )
				{
				fb_log( "\"%s\" (%d) wait failed : no children.\n",
					__FILE__, __LINE__
					);
				return	-1;
				}
			switch( stat_loc & 0377 )
				{
				case 0177 : /* Child stopped.		*/
					fb_log( "Child stopped.\n" );
					return	(stat_loc >> 8) & 0377;
				case 0 :    /* Child exited.		*/
					return	(stat_loc >> 8) & 0377;
				default :   /* Child terminated.	*/
					fb_log( "Child terminated.\n" );
					return	1;
				}
			}
		}
	}
