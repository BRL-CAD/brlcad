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
/** @file lgt/execshell.c
 *	Author:		Gary S. Moss
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"


#define DFL_SHELL	"/bin/sh"
#define CSH		"/bin/csh"
#define TCSH		"/usr/brl/bin/tcsh"

void
loc_Perror(char *msg)
{
    if ( errno >= 0 )
	bu_log( "%s: %s\n", msg, strerror(errno) );
    else
	bu_log( "\"%s\" (%d), errno not set, shouldn't call perror.\n",
		__FILE__, __LINE__
	    );
    return;
}


#define	PERROR_RET() \
	(void) snprintf( error_buf, 32, "\"%s\" (%d)", __FILE__, __LINE__ ); \
	loc_Perror( error_buf ); \
	return	-1;

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
    static char error_buf[32];
    void (*intr_sig)(), (*quit_sig)();
    if ( args[0] == NULL ) {
	char	*arg_sh = getenv( "SHELL" );
	/* $SHELL, if set, DFL_SHELL otherwise.			*/
	if (	arg_sh == NULL
		/* Work around for process group problem.		*/
		||	BU_STR_EQUAL( arg_sh, TCSH )
		||	BU_STR_EQUAL( arg_sh, CSH )
	    )
	    arg_sh = DFL_SHELL;
	args[0] = arg_sh;
	args[1] = NULL;
    }

    intr_sig = signal( SIGINT,  SIG_IGN );
    quit_sig = signal( SIGQUIT, SIG_IGN );
    switch ( child_pid = fork() ) {
	case -1 :
	    PERROR_RET();
	case  0 : /* Child process - execute.		*/
	{
	    int	tmp_fd;
	    if ((tmp_fd = open( "/dev/tty", O_WRONLY )) == -1) {
		PERROR_RET();
	    }
	    (void) close( 2 );
	    if ( fcntl( tmp_fd, F_DUPFD, 2 ) == -1 ) {
		PERROR_RET();
	    }
	    (void) execvp( args[0], args );
	    loc_Perror( args[0] );
	    bu_exit( errno, NULL );
	}
	default :
	{
	    int	pid;
	    int		stat_loc;
	    while ((pid = wait( &stat_loc )) != -1 && pid != child_pid)
		;
	    prnt_Event( "\n" );
	    (void) signal( SIGINT,  intr_sig );
	    (void) signal( SIGQUIT, quit_sig );
	    if ( pid == -1 ) {
		/* No children. */
		loc_Perror( "wait" );
		return errno;
	    }
	    switch ( stat_loc & 0377 ) {
		case 0177 : /* Child stopped.		*/
		    bu_log(	"\"%s\" (%d) Child stopped.\n",
				__FILE__,
				__LINE__
			);
		    return	(stat_loc >> 8) & 0377;
		case 0 :    /* Child exited.		*/
		    return	(stat_loc >> 8) & 0377;
		default :   /* Child terminated.	*/
		    bu_log(	"\"%s\" (%d) Child terminated, signal %d, status=0x%x.\n",
				__FILE__,
				__LINE__,
				stat_loc&0177,
				stat_loc
			);
		    return	stat_loc&0377;
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
