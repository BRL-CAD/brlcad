/*                            S C . C
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
/** @file Sc.c
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
#include <stdlib.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#ifdef  HAVE_SYS__IOCTL_H
#  include <sys/_ioctl.h>
#else
#  ifdef HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#    define _winsize winsize	/* For compatibility with _ioctl.h.	*/
#  endif
#endif

/* Externals from termlib(3). */
#ifdef HAVE_NCURSES_H
#  include <ncurses.h>
#else
#  ifdef HAVE_CURSES_H
#    include <curses.h>
#  endif
#endif
#ifdef HAVE_TERM_H
#  include <term.h>
#endif

#include "./Sc.h"


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
  Get the terminal parameters.
*/
static void
ScLoadTP( void ) {
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
  Get the terminal control strings.
*/
static void
ScLoadTCS( void ) {
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
  This function prevents the default "PutChr" from being pulled in
  from the termcap library (-ltermlib).  DO NOT change its name or
  STDOUT will be assumed as the output stream for terminal control.
  Some applications might want to open "/dev/tty" so that they can
  use STDOUT for something else.
*/
int
PutChr( char c ) {
  return	putc( c, out_fp );
}

/*
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
ScInit( FILE *fp ) {
  char	*term; /* Name of terminal from environment. */
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
  
  tputs( ScTI, 1, (int (*)(int))PutChr );	/* Initialize terminal.			*/
  return	true;		/* All is well.				*/
}

/*
  Clear from the cursor to the end of that line.
*/
bool
ScClrEOL( void ) {
  if( ScCE == NULL )
    return	false;
  tputs( ScCE, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Reset the scrolling region to the entire screen.
*/
bool
ScClrScrlReg( void ) {
  if( ScCS == NULL )
    return	false;
  tputs( tgoto( ScCS, ScLI-1, 0 ), 1, (int (*)(int))PutChr );
  return	true;
}

/*
  End standout mode.
*/
bool
ScClrStandout( void ) {
  if( ScSE == NULL )
    return	false;
  tputs( ScSE, 1, (int (*)(int))PutChr );
  return	true;
}

/*
Clear the screen and "home" the cursor.
*/
bool
ScClrText( void ) {
  if( ScCL == NULL )
    return	false;
  tputs( ScCL, ScLI, (int (*)(int))PutChr );
  return	true;
}

/*
  Insert a the line under the cursor.
*/
bool
ScInsertLn( void ) {
  if( ScAL == NULL )
    return	false;
  tputs( ScAL, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Delete the line under the cursor.
*/
bool
ScDeleteLn( void ) {
  if( ScDL == NULL )
    return	false;
  tputs( ScDL, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Scroll backward 1 line.
*/
bool
ScDnScroll( void ) {
  if( ScSR == NULL )
    return	false;
  tputs( ScSR, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Move the cursor to the top-left corner of the screen.
*/
bool
ScHmCursor( void ) {
  if( ScHO == NULL )
    return	false;
  tputs( ScHO, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Move the cursor to screen coordinates x, y (for column and row,
  respectively).
*/
bool
ScMvCursor( int x, int y ) {
  if( ScCM == NULL )
    return	false;
  
  --x; --y; /* Tgoto() adds 1 to each coordinate!? */
  tputs( tgoto( ScCM, x, y ), 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Set the scrolling region to be from "top" line to "btm" line,
  inclusive.
*/
bool
ScSetScrlReg( int top, int btm ) {
  if( ScCS == NULL )
    return	false;
  tputs( tgoto( ScCS, btm-1, top-1 ), 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Begin standout mode.
*/
bool
ScSetStandout( void ) {
  if( ScSO == NULL )
    return	false;
  tputs( ScSO, 1, (int (*)(int))PutChr );
  return	true;
}

/*
  Scroll text forward 1 line.
*/
bool
ScUpScroll( void ) {
  if( ScSF == NULL )
    return	false;
  tputs( ScSF, 1, (int (*)(int))PutChr );
  return	true;
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
