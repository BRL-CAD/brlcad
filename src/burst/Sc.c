/*                            S C . C
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
 *
 */
/** @file burst/Sc.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS__IOCTL_H
#  include <sys/_ioctl.h>
#else
#  ifdef HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#    define _winsize winsize	/* For compatibility with _ioctl.h.	*/
#  endif
#endif

/* Externals from termlib(3). */
#ifdef HAVE_TERMLIB_H
#  include <termlib.h>
#else
#  ifdef HAVE_NCURSES_H
#    include <ncurses.h>
#  else
#    ifdef HAVE_CURSES_H
#      include <curses.h>
#    else
#      ifdef HAVE_TERMCAP_H
#        include <termcap.h>
#      else
#        ifdef HAVE_TERMINFO_H
#          include <terminfo.h>
#        else
#          ifdef HAVE_TINFO_H
#            include <tinfo.h>
#          endif
#        endif
#      endif
#    endif
#  endif
#  ifdef HAVE_TERM_H
#    include <term.h>
#  endif
#endif

/* termios.h might define this and conflict with vmath's */
#ifdef VMIN
#  undef VMIN
#endif

#include "bu.h"

#include "./Sc.h"


static FILE *out_fp;		/* Output stream.	*/
static char tstrings[ScTCAPSIZ];    /* Individual TCS.	*/
static char *tstr_addr = tstrings;	/* Used by tgetstr().	*/
static int fd_stdout = 1;

/* This is a global buffer for the terminal capabilities entry.	*/
char ScTermcap[ScTCAPSIZ];

/* This is a global buffer for the name of the terminal.	*/
char ScTermname[ScTERMSIZ] = "UNKNOWN";

/* Individual terminal control strings (TCS).			*/
char *ScBC, /* Backspace character.			*/
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
int ScLI, /* Number of lines on screen.		*/
    ScCO; /* Number of columns on screen.		*/


/*
  Get the terminal parameters.
*/
static void
ScLoadTP(void) {
#ifdef TIOCGWINSZ
    /* Get window size for DMD layers support.			*/
    struct _winsize window;

    if (ioctl(fd_stdout, TIOCGWINSZ, &window) == 0
	&&	window.ws_row != 0 && window.ws_col != 0
	) {
	ScLI = (int) window.ws_row;
	ScCO = (int) window.ws_col;
    } else
#endif
    {
	ScLI = tgetnum("li");
	ScCO = tgetnum("co");
    }
    return;
}


/*
  Get the terminal control strings.
*/
static void
ScLoadTCS(void) {
    ScCS = tgetstr("cs", &tstr_addr);
    ScSE = tgetstr("se", &tstr_addr);
    ScSO = tgetstr("so", &tstr_addr);
    ScCE = tgetstr("ce", &tstr_addr);
    ScCL = tgetstr("cl", &tstr_addr);
    ScHO = tgetstr("ho", &tstr_addr);
    ScCM = tgetstr("cm", &tstr_addr);
    ScBC = tgetstr("bc", &tstr_addr);
    ScPC = tgetstr("pc", &tstr_addr);
    ScUP = tgetstr("up", &tstr_addr);
    ScTI = tgetstr("ti", &tstr_addr);
    ScAL = tgetstr("al", &tstr_addr);
    ScDL = tgetstr("dl", &tstr_addr);
    ScSR = tgetstr("sr", &tstr_addr);
    ScSF = tgetstr("sf", &tstr_addr);
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
PutChr(char c) {
    return putc(c, out_fp);
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
  variables.  Returns "1" for success, "0" for failure and
  prints appropriate diagnostics on STDERR if $TERM is not set or
  there is a problem in retrieving the corresponding termcap entry.
*/
int
ScInit(FILE *fp) {
    char *term; /* Name of terminal from environment. */
    out_fp = fp;
    fd_stdout = fileno(out_fp);
    if ((term = getenv("TERM")) == NULL) {
	(void) fprintf(stderr, "TERM not set or exported!\n");
	return 0;
    }
    bu_strlcpy(ScTermname, term, ScTERMSIZ);

    /* Get terminal entry.						*/
    switch (tgetent(ScTermcap, term)) {
	case -1 :
	    (void) fprintf(stderr, "Can't open termcap file!\n");
	    return 0;
	case 0 :
	    (void) fprintf(stderr,
			   "Terminal type not in termcap file!\n"
		);
	    return 0;
    }

    /* Get individual terminal parameters and control strings.	*/
    ScLoadTP();
    ScLoadTCS();

    tputs(ScTI, 1, (int (*)(int))PutChr);	/* Initialize terminal.			*/
    return 1;		/* All is well.				*/
}


/*
  Clear from the cursor to the end of that line.
*/
int
ScClrEOL(void) {
    if (ScCE == NULL)
	return 0;
    tputs(ScCE, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Reset the scrolling region to the entire screen.
*/
int
ScClrScrlReg(void) {
    if (ScCS == NULL)
	return 0;
    tputs(tgoto(ScCS, ScLI-1, 0), 1, (int (*)(int))PutChr);
    return 1;
}


/*
  End standout mode.
*/
int
ScClrStandout(void) {
    if (ScSE == NULL)
	return 0;
    tputs(ScSE, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Clear the screen and "home" the cursor.
*/
int
ScClrText(void) {
    if (ScCL == NULL)
	return 0;
    tputs(ScCL, ScLI, (int (*)(int))PutChr);
    return 1;
}


/*
  Insert a the line under the cursor.
*/
int
ScInsertLn(void) {
    if (ScAL == NULL)
	return 0;
    tputs(ScAL, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Delete the line under the cursor.
*/
int
ScDeleteLn(void) {
    if (ScDL == NULL)
	return 0;
    tputs(ScDL, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Scroll backward 1 line.
*/
int
ScDnScroll(void) {
    if (ScSR == NULL)
	return 0;
    tputs(ScSR, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Move the cursor to the top-left corner of the screen.
*/
int
ScHmCursor(void) {
    if (ScHO == NULL)
	return 0;
    tputs(ScHO, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Move the cursor to screen coordinates x, y (for column and row,
  respectively).
*/
int
ScMvCursor(int x, int y) {
    if (ScCM == NULL)
	return 0;

    --x; --y; /* Tgoto() adds 1 to each coordinate!? */
    tputs(tgoto(ScCM, x, y), 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Set the scrolling region to be from "top" line to "btm" line,
  inclusive.
*/
int
ScSetScrlReg(int top, int btm) {
    if (ScCS == NULL)
	return 0;
    tputs(tgoto(ScCS, btm-1, top-1), 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Begin standout mode.
*/
int
ScSetStandout(void) {
    if (ScSO == NULL)
	return 0;
    tputs(ScSO, 1, (int (*)(int))PutChr);
    return 1;
}


/*
  Scroll text forward 1 line.
*/
int
ScUpScroll(void) {
    if (ScSF == NULL)
	return 0;
    tputs(ScSF, 1, (int (*)(int))PutChr);
    return 1;
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
