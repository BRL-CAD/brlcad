/*
	Author:	Gary S. Moss
		U. S. Army Ballistic Research Laboratory
		Aberdeen Proving Ground
		Maryland 21005-5066

	$Header$ (BRL)

*/
/*LINTLIBRARY*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include "common.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifndef cray
#if defined( VLDSYSV )
#include <sys/_ioctl.h>
#else
#include <sys/ioctl.h>
#define _winsize winsize	/* For compatibility with _ioctl.h.	*/
#endif
#endif
#include "./Sc.h"

/* Externals from termlib(3). */
#if __STDC__
extern char	*tgoto( char *cm, int destcol, int destline );
extern char	*tgetstr( char *id, char **area );
extern int	tgetent( char *bp, char *name );
extern int	tgetnum( char *id );
#else
extern char	*tgoto();
extern char	*tgetstr();
extern int	tgetent();
extern int	tgetnum();
#endif
extern int	tputs();

/* Externals from the C library. */
#if __STDC__
extern char	*getenv( char *s );
#else
extern char	*getenv();
extern char	*strncpy();
#endif

static FILE	*out_fp;		/* Output stream.	*/
static char	tstrings[ScTCAPSIZ];    /* Individual TCS.	*/
static char	*tstr_addr = tstrings;	/* Used by tgetstr().	*/
static int	fd_stdout = 1;

/* This is a global buffer for the terminal capabilities entry.	*/
char		ScTermcap[ScTCAPSIZ];

/* This is a global buffer for the name of the terminal.	*/
char		ScTermname[ScTERMSIZ] = "UNKNOWN";

/* Individual terminal control strings (TCS).			*/
char		*ScBC, /* Backspace character.			*/
		*ScPC, /* Padding character.			*/
		*ScUP, /* Cursor up one line.			*/
		*ScCS, /* Change scrolling region.		*/
		*ScSO, /* Begin standout mode.			*/
		*ScSE, /* End standout mode.			*/
		*ScCE, /* Clear to end of line.			*/
		*ScCL, /* Clear display and home cursor.	*/
		*ScHO, /* Home cursor.				*/
		*ScCM, /* Screen-relative cursor motion.	*/
		*ScTI, /* Initialize terminal.			*/
		*ScAL, /* Insert line.				*/
		*ScDL, /* Delete line.				*/
		*ScSR, /* Scroll text down.			*/
		*ScSF; /* Scroll text up.			*/

/* Individual terminal parameters.				*/
int		ScLI, /* Number of lines on screen.		*/
		ScCO; /* Number of columns on screen.		*/

/*
	static void ScLoadTP( void )

	Get the terminal parameters.

 */
static void
#if __STDC__
ScLoadTP( void )
#else
ScLoadTP()
#endif
	{
#ifdef TIOCGWINSZ
	/* Get window size for DMD layers support.			*/
	struct _winsize		window;

	if(	ioctl( fd_stdout, TIOCGWINSZ, &window ) == 0
	    &&	window.ws_row != 0 && window.ws_col != 0
		)
		{
		ScLI = (int) window.ws_row;
		ScCO = (int) window.ws_col;
		}
	else
#endif
		{
		ScLI = tgetnum( "li" );
		ScCO = tgetnum( "co" );
		}
	return;
	}

/*
	static void ScLoadTCS( void )

	Get the terminal control strings.
 */
static void
#if __STDC__
ScLoadTCS( void )
#else
ScLoadTCS()
#endif
	{;
	ScCS = tgetstr( "cs", &tstr_addr );
	ScSE = tgetstr( "se", &tstr_addr );
	ScSO = tgetstr( "so", &tstr_addr );
	ScCE = tgetstr( "ce", &tstr_addr );
	ScCL = tgetstr( "cl", &tstr_addr );
	ScHO = tgetstr( "ho", &tstr_addr );
	ScCM = tgetstr( "cm", &tstr_addr );
	ScBC = tgetstr( "bc", &tstr_addr );
	ScPC = tgetstr( "pc", &tstr_addr );
	ScUP = tgetstr( "up", &tstr_addr );
	ScTI = tgetstr( "ti", &tstr_addr );
	ScAL = tgetstr( "al", &tstr_addr );
	ScDL = tgetstr( "dl", &tstr_addr );
	ScSR = tgetstr( "sr", &tstr_addr );
	ScSF = tgetstr( "sf", &tstr_addr );
	return;
	}

/*
	int	PutChr( char c )

	This function prevents the default "PutChr" from being pulled in
	from the termcap library (-ltermlib).  DO NOT change its name or
	STDOUT will be assumed as the output stream for terminal control.
	Some applications might want to open "/dev/tty" so that they can
	use STDOUT for something else.
	
 */
int
#if __STDC__
PutChr( char c )
#else
PutChr( c )
char	c;
#endif
	{
	return	putc( c, out_fp );
	}

/*
	bool	ScInit( FILE *fp )

	ScInit() must be invoked before any other function in the Sc package.
	Stream fp must be open for writing and all terminal control sequences
	will be sent to fp, giving the application the option of using STDOUT
	for other things.  Besides setting the output stream, ScInit() does
	the following:

	Initializes the terminal.  Fills terminal name and capabilities
	into external buffers.  Gets terminal control strings into external
	variables.  Gets individual terminal parameters into external
	variables.  Returns "true" for success, "false" for failure and
	prints appropriate diagnostics on STDERR if $TERM is not set or
	there is a problem in retrieving the corresponding termcap entry.
 */
bool
#if __STDC__
ScInit( FILE *fp )
#else
ScInit( fp )
FILE	*fp;
#endif
	{	char	*term; /* Name of terminal from environment. */
	out_fp = fp;
	fd_stdout = fileno( out_fp );
	if( (term = getenv( "TERM" )) == NULL )
		{
		(void) fprintf( stderr, "TERM not set or exported!\n" );
		return	false;
		}
	(void) strncpy( ScTermname, term, ScTERMSIZ-1 );

	/* Get terminal entry.						*/
	switch( tgetent( ScTermcap, term ) )
		{
	case -1 :
		(void) fprintf( stderr, "Can't open termcap file!\n" );
		return	false;
	case  0 :
		(void) fprintf( stderr,
				"Terminal type not in termcap file!\n"
				);
		return	false;
		}

	/* Get individual terminal parameters and control strings.	*/
	ScLoadTP();
	ScLoadTCS();

	tputs( ScTI, 1, PutChr );	/* Initialize terminal.			*/
	return	true;		/* All is well.				*/
	}

/*
	bool	ScClrEOL( void )

	Clear from the cursor to the end of that line.

 */
bool
#if __STDC__
ScClrEOL( void )
#else
ScClrEOL()
#endif
	{
	if( ScCE == NULL )
		return	false;
	tputs( ScCE, 1, PutChr );
	return	true;
	}

/*
	bool	ScClrScrlReg( void )

	Reset the scrolling region to the entire screen.

 */
bool
#if __STDC__
ScClrScrlReg( void )
#else
ScClrScrlReg()
#endif
	{
	if( ScCS == NULL )
		return	false;
	tputs( tgoto( ScCS, ScLI-1, 0 ), 1, PutChr );
	return	true;
	}

/*
	bool	ScClrStandout( void )

	End standout mode.

 */
bool
#if __STDC__
ScClrStandout( void )
#else
ScClrStandout()
#endif
	{
	if( ScSE == NULL )
		return	false;
	tputs( ScSE, 1, PutChr );
	return	true;
	}

/*	bool	ScClrText( void )

	Clear the screen and "home" the cursor.

 */
bool
#if __STDC__
ScClrText( void )
#else
ScClrText()
#endif
	{
	if( ScCL == NULL )
		return	false;
	tputs( ScCL, ScLI, PutChr );
	return	true;
	}

/*
	bool	ScInsertLn( void )

	Insert a the line under the cursor.

 */
bool
#if __STDC__
ScInsertLn( void )
#else
ScInsertLn()
#endif
	{
	if( ScAL == NULL )
		return	false;
	tputs( ScAL, 1, PutChr );
	return	true;
	}

/*
	bool	ScDeleteLn( void )

	Delete the line under the cursor.

 */
bool
#if __STDC__
ScDeleteLn( void )
#else
ScDeleteLn()
#endif
	{
	if( ScDL == NULL )
		return	false;
	tputs( ScDL, 1, PutChr );
	return	true;
	}

/*
	bool	ScDnScroll( void )

	Scroll backward 1 line.

 */
bool
#if __STDC__
ScDnScroll( void )
#else
ScDnScroll()
#endif
	{
	if( ScSR == NULL )
		return	false;
	tputs( ScSR, 1, PutChr );
	return	true;
	}

/*
	bool	ScHmCursor( void )

	Move the cursor to the top-left corner of the screen.

 */
bool
#if __STDC__
ScHmCursor( void )
#else
ScHmCursor()
#endif
	{
	if( ScHO == NULL )
		return	false;
	tputs( ScHO, 1, PutChr );
	return	true;
	}

/*
	bool	ScMvCursor( int x, int y )

	Move the cursor to screen coordinates x, y (for column and row,
		respectively).

 */
bool
#if __STDC__
ScMvCursor( int x, int y )
#else
ScMvCursor( x, y )
int	x, y;
#endif
	{
	if( ScCM == NULL )
		return	false;

	--x; --y; /* Tgoto() adds 1 to each coordinate!? */
	tputs( tgoto( ScCM, x, y ), 1, PutChr );
	return	true;
	}

/*
	bool	ScSetScrlReg( int top, int btm )

	Set the scrolling region to be from "top" line to "btm" line,
		inclusive.

 */
bool
#if __STDC__
ScSetScrlReg( int top, int btm )
#else
ScSetScrlReg( top, btm )
int	top, btm;
#endif
	{
	if( ScCS == NULL )
		return	false;
	tputs( tgoto( ScCS, btm-1, top-1 ), 1, PutChr );
	return	true;
	}

/*
	bool	ScSetStandout( void )

	Begin standout mode.

 */
bool
#if __STDC__
ScSetStandout( void )
#else
ScSetStandout()
#endif
	{
	if( ScSO == NULL )
		return	false;
	tputs( ScSO, 1, PutChr );
	return	true;
	}

/*
	bool	ScUpScroll( void )

	Scroll text forward 1 line.
	
 */
bool
#if __STDC__
ScUpScroll( void )
#else
ScUpScroll()
#endif
	{
	if( ScSF == NULL )
		return	false;
	tputs( ScSF, 1, PutChr );
	return	true;
	}
