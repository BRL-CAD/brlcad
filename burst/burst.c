/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"
#define DEBUG_BURST	false

/*
	bool getCommand( char *name, char *buf, int len, FILE *fp )

	Read next command line into 'buf' and stuff the command name
	into 'name' from input stream 'fp'.
	RETURN CODES: false for end-of-file, true for success.
 */
_LOCAL_ bool
getCommand( name, buf, len, fp )
char *name;
char *buf;
int len;
FILE *fp;
	{
	assert( fp != (FILE *) NULL );
	while( fgets( buf, len, fp ) != NULL )
		if(	buf[0] != CHAR_COMMENT
		     &&	sscanf( buf, "%s", name ) == 1
			)
			{
			buf[strlen(buf)-1] = NUL; /* clobber newline */
			return	true;
			}
		else /* skip over comments and blank lines */
			continue;
	return	false; /* EOF */
	}

/*
	void setupSigs( void )

	Initialize all signal handlers.
 */
_LOCAL_ void
setupSigs()
	{	register int	i;
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
#if defined( BSD )
		case SIGCHLD :
#else
		case SIGCLD :
#endif
			break; /* Leave SIGCLD alone.			*/
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
			(void) signal( i, stop_sig );
			break;
#endif
			}
	return;
	}

/*
	int parsArgv( int argc, char **argv )

	Parse program command line.
 */
_LOCAL_ int
parsArgv( argc, argv )
int argc;
char **argv;
	{	register int	c;
		extern int	optind;
		extern char	*optarg;
	/* Parse options.						*/
	while( (c = getopt( argc, argv, "b" )) != EOF )
		{
		switch( c )
			{
		case 'b' :
			tty = false;
			break;
		case '?' :
			return	0;
			}
		}
	return	true;
	}

/*
	void readBatchInput( FILE *fp )

	Read and execute commands from input stream 'fp'.
 */
void
readBatchInput( fp )
FILE	*fp;
	{
	assert( fp != (FILE *) NULL );
	batchmode = true;
	while( getCommand( cmdname, cmdbuf, LNBUFSZ, fp ) )
		{	Func	*cmdfunc;
		if( (cmdfunc = getTrie( cmdname, cmdtrie )) == NULL )
			{	register int	i, len = strlen( cmdname );
			rt_log( "ERROR -- command syntax:\n" );
			rt_log( "\t%s\n", cmdbuf );
			rt_log( "\t" );
			for( i = 0; i < len; i++ )
				rt_log( " " );
			rt_log( "^\n" );
			}
		else
			{ /* Advance pointer past nul at end of
				command name. */
			cmdptr = cmdbuf + strlen( cmdname ) + 1;
			(*cmdfunc)( (HmItem *) 0 );
			}
		}
	batchmode = false;
	return;
	}

/*
	int main( int argc, char *argv[] )
 */
_LOCAL_ int
main( argc, argv )
int argc;
char *argv[];
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
		return	failure;
		}
	if( ! parsArgv( argc, argv ) )
		{
		prntUsage();
		(void) unlink( tmpfname );
		return	failure;
		}

	setupSigs();
	if( ! initUi() ) /* must be called before any output is produced */
		{
		(void) unlink( tmpfname );
		return	failure;
		}
#if DEBUG_BURST
	prntTrie( cmdtrie, 0 );
#endif
	if( ! isatty( 0 ) || ! tty )
		readBatchInput( stdin );
	if( tty )
		(void) HmHit( mainhmenu );
	exitCleanly( 0 );
	return	success;
	}

/*
	void exitCleanly( int sig )

	Should be only exit from program after success of 'initUi()'.
 */
void
exitCleanly( sig )
int	sig;
	{
	if( tty )
		closeUi(); /* keep screen straight */
	(void) fclose( tmpfp );
	if( unlink( tmpfname ) == -1 )
		locPerror( tmpfname );
	exit( sig );
	}
