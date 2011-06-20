/*                            S C . H
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
/** @file burst/Sc.h
 *
 */

/**
   <Sc.h> -- MUVES "Sc" (Screen manager) package definitions
**/

/**

The Sc package provides an interface to the termcap library
(termlib(3)) for doing cursor movement, character attribute,
and other low-level screen management operations.  It is
intended for use in building terminal-independent full-screen
user interfaces.  Do to its use of the tgoto() termlib(3)
routine, programs that use this package should turn off TAB3
in the terminal handler.

**/
#ifndef Sc_H_INCLUDE
#define Sc_H_INCLUDE
#include "./burst.h"

/**
   int ScInit(FILE *fp)

   ScInit() must be invoked before any other function in the Sc
   package.  Stream fp must be open for writing and all terminal
   control sequences will be sent to fp, giving the application
   the option of using STDOUT for other things.  Besides setting
   the output stream, ScInit() does the following:

   Initializes the terminal.

   Fills terminal name and capabilities into these externals:

   char ScTermname[ScTERMSIZ]	(terminal name from $TERM)
   char ScTermcap[ScTCAPSIZ]	(terminal capabilities entry)

   Gets terminal control strings into external variables (here
   are some that don't have individual functions to output them,
   the ones that DO are below):

   char *ScBC (backspace character)
   char *ScPC (padding character)
   char *ScUP (move the cursor up one line)
   char *ScTI (initialize the terminal)

   Gets individual terminal parameters into these externals:

   int ScLI (number of lines on screen)
   int ScCO (number of columns on screen)

   Returns "1" for success, "0" for failure and prints
   appropriate diagnostics on STDERR if $TERM is not set or
   there is a problem in retrieving the corresponding termcap
   entry.
**/
    extern int ScInit(FILE *fp);

/**

Below are functions paired with terminal control strings that
they output to the stream specified with ScInit().  It is not
recommended that the control strings be used directly, but it
may be useful in certain applications to check their value;
if ScInit() has not been invoked or the corresponding terminal
capability does not exist, its control string will be NULL,
AND the function will return "false".  Otherwise, they will
return "true" (assuming that it worked).  There is no way to
be sure of this.

char *ScCE (clear from under the cursor to end of line)
int ScClrEOL(void)

char *ScCS (change scrolling region)
int ScClrScrlReg(void)

char *ScSE (end standout mode)
int ScClrStandout(void)

char *ScCL (clear screen, and home cursor)
int ScClrText(void)

char *ScAL (insert a line under the cursor)
int ScInsertLn(void)

char *ScDL (delete the line under the cursor)
int ScDeleteLn(void)

char *ScSR (scroll text backwards 1 line)
int ScDnScroll(void)

char *ScHO (move cursor to top-left corner of screen)
int ScHmCursor(void)

char *ScCM (move cursor to column and row <x, y>)
int ScMvCursor(x, y)

char *ScCS (set scrolling region from top to btm incl.)
int ScSetScrlReg(top, btm)

char *ScSO (begin standout mode)
int ScSetStandout(void)

char *ScSF (scroll text forwards 1 line)
int ScUpScroll(void)

**/
extern char *ScBC;
extern char *ScPC;
extern char *ScUP;
extern char *ScCS;
extern char *ScSO;
extern char *ScSE;
extern char *ScCE;
extern char *ScCL;
extern char *ScHO;
extern char *ScCM;
extern char *ScTI;
extern char *ScDL;
extern char *ScSR;
extern char *ScSF;

extern int ScLI;
extern int ScCO;

extern int ScClrEOL(void);
extern int ScClrScrlReg(void);
extern int ScClrStandout(void);
extern int ScClrText(void);
extern int ScDeleteLn(void);
extern int ScDnScroll(void);
extern int ScHmCursor(void);
extern int ScInsertLn(void);
extern int ScMvCursor(int x, int y);
extern int ScSetScrlReg(int top, int btm);
extern int ScSetStandout(void);
extern int ScUpScroll(void);

#define ScTCAPSIZ 1024
#define ScTERMSIZ 80

extern char ScTermcap[];
extern char ScTermname[];

#endif		/* Sc_H_INCLUDE */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
