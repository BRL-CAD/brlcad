/*
	SCCS id:	%Z% %M%	%I%
	Last edit: 	%G% at %U%	G S M
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#ifndef lint
static
char	sccsTag[] = "%Z% %M%	%I%	last edit %G% at %U%";
#endif lint
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
			prnt_Debug( "exec_Shell() could not fork" );
			return	-1;
		case  0 : /* Child process - execute.		*/
			sleep( 2 );
			(void) execvp( args[0], args );
			prnt_Debug( "%s : could not execute", args[0] );
			exit( errno );
		default :
			{	register int	pid;
				int		stat_loc;
#if defined( BSD )
#define SIGCLD	SIGCHLD
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
				prnt_Debug( "wait failed : no children" );
				return	-1;
				}
			switch( stat_loc & 0377 )
				{
				case 0177 : /* Child stopped.		*/
					prnt_Debug( "Child stopped" );
					return	(stat_loc >> 8) & 0377;
				case 0 :    /* Child exited.		*/
					return	(stat_loc >> 8) & 0377;
				default :   /* Child terminated.	*/
					prnt_Debug( "Child terminated" );
					return	1;
				}
			}
		}
	}
