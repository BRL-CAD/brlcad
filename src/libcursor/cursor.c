/*                        C U R S O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

/* interface header */
#include "cursor.h"

/* system headers */

#include <stdlib.h>
#include <string.h>

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

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#define _winsize winsize		/* For compat with _ioctl.h. */

#define TBUFSIZ 1024
#define MAX_TERM_LEN 80

static FILE *out_fp;			/* Output stream.	*/
static char termCapBuf[TBUFSIZ];  	/* Termcap entry.	*/
static char tstrings[TBUFSIZ];    	/* Individual TCS.	*/
static char *tstr_addr = tstrings;	/* Used by tgetstr().	*/
#ifdef TIOCGWINSZ
static int fd_stdout = 1;
#endif

static void LoadTP(void);
static void LoadTCS(void);


/* This is a global buffer for the name of the terminal.	*/
static char termName[MAX_TERM_LEN] = "UNKNOWN";

/* from termcap/termlib library */
extern char *BC, /* Backspace.			*/
    *UP; /* Cursor up one line.			*/

/* Individual terminal control strings (TCS).	*/
char *CS, /* Change scrolling region.		*/
    *SO, /* Begin standout mode.		*/
    *SE, /* End standout mode.			*/
    *CE, /* Clear to end of line.		*/
    *CL, /* Clear display and home cursor.	*/
    *HO, /* Home cursor.			*/
    *CM, /* Screen-relative cursor motion.	*/
    *TI, /* Initialize terminal.		*/
    *DL, /* Delete line.			*/
    *SR, /* Scroll text down.			*/
    *SF; /* Scroll text up.			*/

/* Individual terminal parameters.		*/
int LI, /* Number of lines on screen.		*/
    CO; /* Number of columns on screen.		*/


/* This function must be accessible to the termcap library, but will
 * not necessarily be needed by the application.
 */
int PutChr(int c);


/*
 * Get terminal name from environment and leave in 'termName' for
 * external reference.
 * Read termcap entry into 'termCapBuf'.
 */
int
InitTermCap(FILE *fp)
{
    char *term; /* Name of terminal from environment ($TERM).*/
    out_fp = fp;
#ifdef TIOCGWINSZ
    fd_stdout = fileno(out_fp);
#endif
    if ((term = getenv("TERM")) == NULL) {
	(void) fprintf(stderr, "TERM not set or exported!\n");
	return 0;
    } else {
	(void) strncpy(termName, term, MAX_TERM_LEN); /* intentionally not bu_strlcpy to not add libbu dep */
	termName[sizeof(termName) - 1] = '\0';
    }

    /* Get terminal entry.						*/
    switch (tgetent(termCapBuf, term)) {
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
    LoadTP();
    LoadTCS();

    tputs(TI, 1, PutChr);	/* Initialize terminal.			*/
    return 1;		/* All is well.				*/
}


/*
 * Get the terminal parameters.
 */
static void
LoadTP(void)
{
#ifdef TIOCGWINSZ
    /* Get window size for DMD layers support.			*/
    struct _winsize window;

    if (ioctl(fd_stdout, TIOCGWINSZ, &window) == 0
	&& window.ws_row != 0 && window.ws_col != 0
	)
    {
	LI = (int) window.ws_row;
	CO = (int) window.ws_col;
	return;
    }
#endif

    LI = tgetnum("li");
    CO = tgetnum("co");
    return;
}


/* L o a d T C S ()
   Get the terminal control strings.
*/
static void
LoadTCS(void)
{
    CS = tgetstr("cs", &tstr_addr);
    SE = tgetstr("se", &tstr_addr);
    SO = tgetstr("so", &tstr_addr);
    CE = tgetstr("ce", &tstr_addr);
    CL = tgetstr("cl", &tstr_addr);
    HO = tgetstr("ho", &tstr_addr);
    CM = tgetstr("cm", &tstr_addr);
    BC = tgetstr("bc", &tstr_addr);
    UP = tgetstr("up", &tstr_addr);
    TI = tgetstr("ti", &tstr_addr);
    DL = tgetstr("dl", &tstr_addr);
    SR = tgetstr("sr", &tstr_addr);
    SF = tgetstr("sf", &tstr_addr);
    return;
}


int
HmCursor(void)
{
    if (HO != NULL) {
	tputs(HO, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ScrollUp(void)
{
    if (SF != NULL) {
	tputs(SF, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ScrollDn(void)
{
    if (SR != NULL) {
	tputs(SR, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
DeleteLn(void)
{
    if (DL != NULL) {
	tputs(DL, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
MvCursor(int x, int y)
{
    /* Tgoto() adds 1 to each coordinate!? */
    --x;
    --y;

    if (CM != NULL) {
	tputs(tgoto(CM, x, y), 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ClrEOL(void)
{
    if (CE != NULL) {
	tputs(CE, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ClrText(void)
{
    if (CL != NULL) {
	tputs(CL, LI, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
SetScrlReg(int top, int btm)
{
    if (CS != NULL) {
	tputs(tgoto(CS, btm-1, top-1), 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ResetScrlReg(void)
{
    if (CS != NULL) {
	tputs(tgoto(CS, LI-1, 0), 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
ClrStandout(void)
{
    if (SE != NULL) {
	tputs(SE, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
SetStandout(void)
{
    if (SO != NULL) {
	tputs(SO, 1, PutChr);
	return 1;
    } else {
	return 0;
    }
}


int
PutChr(int c)
{
    return putc(c, out_fp);
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
