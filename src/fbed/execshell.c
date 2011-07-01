/*                     E X E C S H E L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fbed/execshell.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "fb.h"

#include "./std.h"
#include "./ascii.h"
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
    int child_pid;

    if ( args[0] == NULL )
    {
	char *arg_sh = getenv( "SHELL" );
	/* $SHELL, if set, DFL_SHELL otherwise. */
	if ( arg_sh == NULL )
	    arg_sh = DFL_SHELL;
	args[0] = arg_sh;
	args[1] = NULL;
    }
    switch ( child_pid = fork() )
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
	    bu_exit( 1, NULL );
	default :
	{
	    int pid;
	    int stat_loc;
	    void (*istat)(), (*qstat)(), (*cstat)();
	    istat = signal(SIGINT, SIG_IGN);
	    qstat = signal(SIGQUIT, SIG_IGN);
	    cstat = signal(SIGCLD, SIG_DFL);
	    while (	(pid = wait( &stat_loc )) != -1
			&& pid != child_pid
		)
		;
	    (void) signal(SIGINT, istat);
	    (void) signal(SIGQUIT, qstat);
	    (void) signal(SIGCLD, cstat);
	    if ( pid == -1 )
	    {
		fb_log( "\"%s\" (%d) wait failed : no children.\n",
			__FILE__, __LINE__
		    );
		return -1;
	    }
	    switch ( stat_loc & 0377 )
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
