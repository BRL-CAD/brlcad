/*LINTLIBRARY*/
/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) libcursor.c	1.13
		Last edit: 	2/4/87 at 12:42:12
		Retrieved: 	2/4/87 at 12:42:39
		SCCS archive:	/v/src/libcursor/s.libcursor.c
 */

#include "conf.h"

#include <stdio.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifndef CRAY
#include <sys/ioctl.h>
#define _winsize winsize	/* For compat with _ioctl.h. */
#endif

#define TBUFSIZ		1024
#define MAX_TERM_LEN	80

extern char	*getenv(), *tgetstr();
extern int	tgetent();
#if ! defined( BSD )
extern void	clr_Tabs(), save_Tty();
#endif

static FILE	*out_fp;		/* Output stream.	*/
static char	termCapBuf[TBUFSIZ];  	/* Termcap entry.	*/
static char	tstrings[TBUFSIZ];    	/* Individual TCS.	*/
static char	*tstr_addr = tstrings;	/* Used by tgetstr().	*/
#ifdef TIOCGWINSZ
static int	fd_stdout = 1;
#endif

static void	LoadTP(), LoadTCS();


/* This is a global buffer for the name of the terminal.	*/
char		termName[MAX_TERM_LEN] = "UNKNOWN";

/* Individual terminal control strings (TCS).			*/
char		*BC, /* Backspace.				*/
		*UP, /* Cursor up one line.			*/
		*CS, /* Change scrolling region.		*/
		*SO, /* Begin standout mode.			*/
		*SE, /* End standout mode.			*/
		*CE, /* Clear to end of line.			*/
		*CL, /* Clear display and home cursor.		*/
		*HO, /* Home cursor.				*/
		*CM, /* Screen-relative cursor motion.		*/
		*TI, /* Initialize terminal.			*/
		*DL, /* Delete line.				*/
		*SR, /* Scroll text down.			*/
		*SF; /* Scroll text up.				*/

/* Individual terminal parameters.				*/
int		LI, /* Number of lines on screen.		*/
		CO; /* Number of columns on screen.		*/

/* This function must be called first to read the termcap database and
	to specify the output stream.
 */
int		InitTermCap();

/* This function must be accessible to the termcap library, but will
	not necessarily be needed by the application.
 */
int		PutChr();

/* These functions output terminal control strings to the file stream
	specified by the InitTermCap() call which must precede them.
	They return 0 if the capability is not described in the termcap
	database and 1 otherwise.  Of course if the database entry is
	wrong, the command will not do its job.
 */
int		ClrStandout(), ClrEOL(), ClrText(),
		DeleteLn(),
		HmCursor(),
		MvCursor(),
		ResetScrlReg(),
		ScrollDn(), ScrollUp(),
		SetScrlReg(), SetStandout();


/*	I n i t T e r m C a p ( )
	Get terminal name from environent and leave in 'termName' for
	external reference.
	Read termcap entry into 'termCapBuf'.
	Get individual parameters and control strings.
	Initialize the terminal.
	Returns 1 for success, 0 for failure and prints
	appropriate diagnostic.
	Use 'fp' as output stream.
 */
InitTermCap( fp )
FILE	*fp;
	{	char	*term; /* Name of terminal from environment ($TERM).*/
	out_fp = fp;
#ifdef TIOCGWINSZ
	fd_stdout = fileno( out_fp );
#endif
	if( (term = getenv( "TERM" )) == NULL )
		{
		(void) fprintf( stderr, "TERM not set or exported!\n" );
		return	0;
		}
	else
		{
		(void) strncpy( termName, term, MAX_TERM_LEN-1 );
		}

	/* Get terminal entry.						*/
	switch( tgetent( termCapBuf, term ) )
		{
	case -1 :
		(void) fprintf( stderr, "Can't open termcap file!\n" );
		return	0;
	case  0 :
		(void) fprintf( stderr,
				"Terminal type not in termcap file!\n"
				);
		return	0;
		}

	/* Get individual terminal parameters and control strings.	*/
	LoadTP();
	LoadTCS();

	tputs( TI, 1, PutChr );	/* Initialize terminal.			*/
	return	1;		/* All is well.				*/
	}

/*	L o a d T P ( )
	Get the terminal parameters.
 */
static void
LoadTP()
	{
#ifdef TIOCGWINSZ
	/* Get window size for DMD layers support.			*/
	struct _winsize		window;

	if(	ioctl( fd_stdout, TIOCGWINSZ, &window ) == 0
	    &&	window.ws_row != 0 && window.ws_col != 0
		)
		{
		LI = (int) window.ws_row;
		CO = (int) window.ws_col;
		}
	else
#endif
		{
		LI = tgetnum( "li" );
		CO = tgetnum( "co" );
		}
	return;
	}

/*	L o a d T C S ( )
	Get the terminal control strings.
 */
static void
LoadTCS()
	{
	CS = tgetstr( "cs", &tstr_addr );
	SE = tgetstr( "se", &tstr_addr );
	SO = tgetstr( "so", &tstr_addr );
	CE = tgetstr( "ce", &tstr_addr );
	CL = tgetstr( "cl", &tstr_addr );
	HO = tgetstr( "ho", &tstr_addr );
	CM = tgetstr( "cm", &tstr_addr );
	BC = tgetstr( "bc", &tstr_addr );
	UP = tgetstr( "up", &tstr_addr );
	TI = tgetstr( "ti", &tstr_addr );
	DL = tgetstr( "dl", &tstr_addr );
	SR = tgetstr( "sr", &tstr_addr );
	SF = tgetstr( "sf", &tstr_addr );
	return;
	}

/*	H m C u r s o r ( )
	Home the cursor.
 */
int
HmCursor()
	{
	if( HO != NULL )
		{
		tputs( HO, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	S c r o l l U p ( )
	Forward scroll 1 line.
 */
int
ScrollUp()
	{
	if( SF != NULL )
		{
		tputs( SF, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	S c r o l l D n ( )
	Reverse scroll 1 line.
 */
int
ScrollDn()
	{
	if( SR != NULL )
		{
		tputs( SR, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	D e l e t e L n ( )
	Delete the current line.
 */
int
DeleteLn()
	{
	if( DL != NULL )
		{
		tputs( DL, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	M v C u r s o r ( )
	Move the cursor to screen coordinates x, y.
 */
int
MvCursor( x, y )
int	x, y;
	{
	--x; --y; /* Tgoto() adds 1 to each coordinate!?		*/
	if( CM != NULL )
		{
		tputs( tgoto( CM, x, y ), 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	C l r E O L ( )
	Clear from the cursor to end of line.
 */
int
ClrEOL()
	{
	if( CE != NULL )
		{
		tputs( CE, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	C l r T e x t ( )
	Clear screen and home cursor.
 */
int
ClrText()
	{
	if( CL != NULL )
		{
		tputs( CL, LI, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	S e t S c r l R e g ( )
	Set the scrolling region to be from "top" to "btm".
 */
int
SetScrlReg( top, btm )
int	top, btm;
	{
	if( CS != NULL )
		{
		tputs( tgoto( CS, btm-1, top-1 ), 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	R e s e t S c r l R e g ( )
	Reset the scrolling region to the entire screen.
 */
int
ResetScrlReg()
	{
	if( CS != NULL )
		{
		tputs( tgoto( CS, LI-1, 0 ), 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	C l r S t a n d o u t ( )
	End standout mode.
 */
int
ClrStandout()
	{
	if( SE != NULL )
		{
		tputs( SE, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	S e t S t a n d o u t ( )
	Begin standout mode.
 */
int
SetStandout()
	{
	if( SO != NULL )
		{
		tputs( SO, 1, PutChr );
		return	1;
		}
	else
		return	0;
	}

/*	P u t C h r ( )							*/
PutChr( c )
char	c;
	{
	return	putc( c, out_fp );
	}
