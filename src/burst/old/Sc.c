/*                            S C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include "bu/str.h"

#include "./Sc.h"


static FILE *out_fp;		/* Output stream.	*/
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
  This function prevents the default "PutChr" from being pulled in
  from the termcap library (-ltermlib).  DO NOT change its name or
  STDOUT will be assumed as the output stream for terminal control.
  Some applications might want to open "/dev/tty" so that they can
  use STDOUT for something else.
*/
int
PutChr(int c) {
    return putc((char)c, out_fp);
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

    return 1;		/* All is well.				*/
}


/*
  Clear from the cursor to the end of that line.
*/
int
ScClrEOL(void) {
    if (ScCE == NULL)
	return 0;
    return 1;
}


/*
  Reset the scrolling region to the entire screen.
*/
int
ScClrScrlReg(void) {
    if (ScCS == NULL)
	return 0;
    return 1;
}


/*
  End standout mode.
*/
int
ScClrStandout(void) {
    if (ScSE == NULL)
	return 0;
    return 1;
}


/*
  Clear the screen and "home" the cursor.
*/
int
ScClrText(void) {
    if (ScCL == NULL)
	return 0;
    return 1;
}


/*
  Insert a the line under the cursor.
*/
int
ScInsertLn(void) {
    if (ScAL == NULL)
	return 0;
    return 1;
}


/*
  Delete the line under the cursor.
*/
int
ScDeleteLn(void) {
    if (ScDL == NULL)
	return 0;
    return 1;
}


/*
  Scroll backward 1 line.
*/
int
ScDnScroll(void) {
    if (ScSR == NULL)
	return 0;
    return 1;
}


/*
  Move the cursor to the top-left corner of the screen.
*/
int
ScHmCursor(void) {
    if (ScHO == NULL)
	return 0;
    return 1;
}


/*
  Move the cursor to screen coordinates x, y (for column and row,
  respectively).
*/
int
ScMvCursor(int UNUSED(x), int UNUSED(y)) {
    if (ScCM == NULL)
	return 0;
    return 1;
}


/*
  Set the scrolling region to be from "top" line to "btm" line,
  inclusive.
*/
int
ScSetScrlReg(int UNUSED(top), int UNUSED(btm)) {
    if (ScCS == NULL)
	return 0;
    return 1;
}


/*
  Begin standout mode.
*/
int
ScSetStandout(void) {
    if (ScSO == NULL)
	return 0;
    return 1;
}


/*
  Scroll text forward 1 line.
*/
int
ScUpScroll(void) {
    if (ScSF == NULL)
	return 0;
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
