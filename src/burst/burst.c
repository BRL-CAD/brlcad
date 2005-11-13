/*                         B U R S T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file burst.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"

#define SIGCLD SIGCHLD

#define DEBUG_BURST	0	/* 1 enables debugging for this module */

/*
	boolean getCommand( char *name, char *buf, int len, FILE *fp )

	Read next command line into buf and stuff the command name into name
	from input stream fp.  buf must be at least len bytes long.

	RETURN:	1 for success

		0 for end of file
 */
static boolean
#if STD_C
getCommand( char *name, char *buf, int len, FILE *fp )
#else
getCommand( name, buf, len, fp )
char *name;
char *buf;
int len;
FILE *fp;
#endif
	{
	assert( name != NULL );
	assert( buf != NULL );
	assert( fp != NULL );
	while( fgets( buf, len, fp ) != NULL )
		{
		if( buf[0] != CHAR_COMMENT )
			{
			if( sscanf( buf, "%s", name ) == 1 )
				{
				buf[strlen(buf)-1] = NUL; /* clobber newline */
				return	1;
				}
			else /* Skip over blank lines. */
				continue;
			}
		else
			{ /* Generate comment command. */
			(void) strcpy( name, CMD_COMMENT );
			return	1;
			}
		}
	return	0; /* EOF */
	}

/*
	void setupSigs( void )

	Initialize all signal handlers.
 */
static void
#if STD_C
setupSigs( void )
#else
setupSigs()
#endif
	{	register int i;
	for( i = 0; i < NSIG; i++ )
		switch( i )
			{
		case SIGINT :
			if( (norml_sig = signal( i, SIG_IGN )) == SIG_IGN )
				abort_sig = SIG_IGN;
			else
				{
				norml_sig = intr_sig;
				abort_sig = abort_RT;
				(void) signal( i,  norml_sig );
				}
			break;
		case SIGCHLD :
			break; /* leave SIGCLD alone */
		case SIGPIPE :
			(void) signal( i, SIG_IGN );
			break;
		case SIGQUIT :
			break;
#if ! defined( SYSV )
#if ! defined( SIGTSTP )
#define SIGTSTP	18
#endif
		case SIGTSTP :
			break;
#endif
			}
	return;
	}

/*
	int parsArgv( int argc, char **argv )

	Parse program command line.
 */
static int
#if STD_C
parsArgv( int argc, char **argv )
#else
parsArgv( argc, argv )
int argc;
char **argv;
#endif
	{	register int c;
	/* Parse options.						*/
	while( (c = getopt( argc, argv, "b" )) != EOF )
		{
		switch( c )
			{
		case 'b' :
			tty = 0;
			break;
		case '?' :
			return	0;
			}
		}
	return	1;
	}

/*
	void readBatchInput( FILE *fp )

	Read and execute commands from input stream fp.
 */
void
#if STD_C
readBatchInput( FILE *fp )
#else
readBatchInput( fp )
FILE *fp;
#endif
	{
	assert( fp != (FILE *) NULL );
	batchmode = 1;
	while( getCommand( cmdname, cmdbuf, LNBUFSZ, fp ) )
		{	Func	*cmdfunc;
		if( (cmdfunc = getTrie( cmdname, cmdtrie )) == NULL )
			{	register int i, len = strlen( cmdname );
			brst_log( "ERROR -- command syntax:\n" );
			brst_log( "\t%s\n", cmdbuf );
			brst_log( "\t" );
			for( i = 0; i < len; i++ )
				brst_log( " " );
			brst_log( "^\n" );
			}
		else
		if( strcmp( cmdname, CMD_COMMENT ) == 0 )
			{ /* special handling for comments */
			cmdptr = cmdbuf;
			cmdbuf[strlen(cmdbuf)-1] = '\0'; /* clobber newline */
			(*cmdfunc)( (HmItem *) 0 );
			}
		else
			{ /* Advance pointer past nul at end of
				command name. */
			cmdptr = cmdbuf + strlen( cmdname ) + 1;
			(*cmdfunc)( (HmItem *) 0 );
			}
		}
	batchmode = 0;
	return;
	}

/*
	int main( int argc, char *argv[] )
 */
int
#if STD_C
main( int argc, char *argv[] )
#else
main( argc, argv )
int argc;
char *argv[];
#endif
	{
#if ! defined( BSD ) && ! defined( sgi )
	setvbuf( stderr, (char *) NULL, _IOLBF, BUFSIZ );
#endif
	if(	tmpnam( tmpfname ) == NULL
	     ||	(tmpfp = fopen( tmpfname, "w" )) == (FILE *) NULL
		)
		{
		perror( tmpfname );
		(void) fprintf( stderr,
				"Write access denied for file (%s).\n",
				tmpfname );
		goto	death;
		}
	if( ! parsArgv( argc, argv ) )
		{
		prntUsage();
		goto	clean;
		}

	setupSigs();
	if( ! initUi() ) /* must be called before any output is produced */
		goto	clean;

#if DEBUG_BURST
	prntTrie( cmdtrie, 0 );
#endif
	assert( airids.i_next == NULL );
	assert( armorids.i_next == NULL );
	assert( critids.i_next == NULL );

	if( ! isatty( 0 ) || ! tty )
		readBatchInput( stdin );
	if( tty )
		(void) HmHit( mainhmenu );
	exitCleanly( BURST_EXIT_SUCCESS );
clean:	(void) unlink( tmpfname );
death:	return	BURST_EXIT_FAILURE;
	}

/*
	void exitCleanly( int code )

	Should be only exit from program after success of initUi().
 */
void
#if STD_C
exitCleanly( int code )
#else
exitCleanly( code )
int code;
#endif
	{
	if( tty )
		closeUi(); /* keep screen straight */
	(void) fclose( tmpfp );
	if( unlink( tmpfname ) == -1 )
		locPerror( tmpfname );
	exit( code );
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
