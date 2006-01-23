/*                     E X E C S H E L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"


#define DFL_SHELL	"/bin/sh"
#define CSH		"/bin/csh"
#define TCSH		"/usr/brl/bin/tcsh"
#define	PERROR_RET() \
	(void) sprintf( error_buf, "\"%s\" (%d)", __FILE__, __LINE__ ); \
	loc_Perror( error_buf ); \
	return	-1;

void
loc_Perror(char *msg)
{
#ifdef HAVE_STRERROR
	if( errno >= 0 )
		bu_log( "%s: %s\n", msg, strerror(errno) );
#else
	if( errno >= 0 && errno < sys_nerr )
		bu_log( "%s: %s\n", msg, sys_errlist[errno] );
#endif
	else
		bu_log( "\"%s\" (%d), errno not set, shouldn't call perror.\n",
			__FILE__, __LINE__
			);
	return;
	}

/*	e x e c _ S h e l l ( )
	If args[0] is NULL, spawn a shell, otherwise execute the specified
	command line.
	Return the exit status of the program, or -1 if wait() or fork()
	return an error.
 */
int
exec_Shell(char **args)
{	register int child_pid;
		static char error_buf[32];
		void (*intr_sig)(), (*quit_sig)();
	if( args[0] == NULL )
		{ char	*arg_sh = getenv( "SHELL" );
		/* $SHELL, if set, DFL_SHELL otherwise.			*/
		if(	arg_sh == NULL
		/* Work around for process group problem.		*/
		    ||	strcmp( arg_sh, TCSH ) == 0
		    ||	strcmp( arg_sh, CSH ) == 0
			)
			arg_sh = DFL_SHELL;
		args[0] = arg_sh;
		args[1] = NULL;
		}
	intr_sig = signal( SIGINT,  SIG_IGN );
	quit_sig = signal( SIGQUIT, SIG_IGN );
	switch( child_pid = fork() )
		{
		case -1 :
			PERROR_RET();
		case  0 : /* Child process - execute.		*/
			{ int	tmp_fd;
			if(	(tmp_fd =
				open( "/dev/tty", O_WRONLY )) == -1
				)
				{
				PERROR_RET();
				}
			(void) close( 2 );
			if( fcntl( tmp_fd, F_DUPFD, 2 ) == -1 )
				{
				PERROR_RET();
				}
			(void) execvp( args[0], args );
			loc_Perror( args[0] );
			exit( errno );
			}
		default :
			{	register int	pid;
				int		stat_loc;
			while(	(pid = wait( &stat_loc )) != -1
			     && pid != child_pid
				)
				;
			prnt_Event( "\n" );
			(void) signal( SIGINT,  intr_sig );
			(void) signal( SIGQUIT, quit_sig );
			if( pid == -1 )
				{ /* No children.			*/
				loc_Perror( "wait" );
				return	errno;
				}
			switch( stat_loc & 0377 )
				{
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
