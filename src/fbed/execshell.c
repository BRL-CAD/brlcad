/*                     E X E C S H E L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file execshell.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

#define DFL_SHELL	"/bin/sh"

#if !defined(SIGCLD) && defined(SIGCHLD)
#  define SIGCLD SIGCHLD  /* POSIX name */
#endif


/*	e x e c _ S h e l l ( )
	If args[0] is NULL, spawn a shell, otherwise execute the specified
	command line.
	Return the exit status of the program, or -1 if wait() or fork()
	return an error.
 */
int
exec_Shell(char **args)
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
				register void (*istat)(), (*qstat)(), (*cstat)();
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
