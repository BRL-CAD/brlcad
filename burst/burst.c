/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifndef DEBUG
#define NDEBUG
#define STATIC static
#else
#define STATIC
#endif

#include <assert.h>

#include <stdio.h>
#include <signal.h>

#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"

#define DEBUG_BURST	0	/* 1 enables debugging for this module */

/*
	bool getCommand( char *name, char *buf, int len, FILE *fp )

	Read next command line into buf and stuff the command name into name
	from input stream fp.  buf must be at least len bytes long.

	RETURN:	true for success

		false for end of file 
 */
STATIC bool
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
				return	true;
				}
			else /* Skip over blank lines. */
				continue;
			}
		else
			{ /* Generate comment command. */
			(void) strcpy( name, CMD_COMMENT );
			return	true;
			}
		}
	return	false; /* EOF */
	}

/*
	void setupSigs( void )

	Initialize all signal handlers.
 */
STATIC void
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
#if defined( BSD )
		case SIGCHLD :
#else
		case SIGCLD :
#endif
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
STATIC int
#if STD_C
parsArgv( int argc, char **argv )
#else
parsArgv( argc, argv )
int argc;
char **argv;
#endif
	{	register int c;
		extern int optind;
		extern char *optarg;
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
	batchmode = true;
	while( getCommand( cmdname, cmdbuf, LNBUFSZ, fp ) )
		{	Func	*cmdfunc;
		if( (cmdfunc = getTrie( cmdname, cmdtrie )) == NULL )
			{	register int i, len = strlen( cmdname );
			bu_log( "ERROR -- command syntax:\n" );
			bu_log( "\t%s\n", cmdbuf );
			bu_log( "\t" );
			for( i = 0; i < len; i++ )
				bu_log( " " );
			bu_log( "^\n" );
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
	batchmode = false;
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
	exitCleanly( EXIT_SUCCESS );
clean:	(void) unlink( tmpfname );
death:	return	EXIT_FAILURE;
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
