/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

#define DFL_SHELL	"/bin/sh"

/*	e x e c _ S h e l l ( )
	If args[0] is NULL, spawn a shell, otherwise execute the specified
	command line.
	Return the exit status of the program, or -1 if wait() or fork()
	return an error.
 */
int
exec_Shell( args )
char *args[];
	{
	register int child_pid;

	if( args[0] == NULL )
		{ char *arg_sh = getenv( "SHELL" );
		/* $SHELL, if set, DFL_SHELL otherwise. */
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
			return -1;
		case  0 : /* Child process - execute. */
			sleep( 2 );
			(void) execvp( args[0], args );
			fb_log( "%s : could not execute.\n", args[0] );
			exit( 1 );
		default :
			{	register int pid;
				int stat_loc;
#if STD_SIGNAL_DECLS
				register void (*istat)(), (*qstat)(), (*cstat)();
#else
				register int (*istat)(), (*qstat)(), (*cstat)();
#endif
#if !defined(SIGCLD) && defined(SIGCHLD)
#	define SIGCLD	SIGCHLD		/* BSD and POSIX name */
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
				return -1;
				}
			switch( stat_loc & 0377 )
				{
				case 0177 : /* Child stopped. */
					fb_log( "Child stopped.\n" );
					return (stat_loc >> 8) & 0377;
				case 0 :    /* Child exited. */
					return (stat_loc >> 8) & 0377;
				default :   /* Child terminated. */
					fb_log( "Child terminated.\n" );
					return 1;
				}
			}
		}
	}
