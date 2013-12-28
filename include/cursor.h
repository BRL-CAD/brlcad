/*                        C U R S O R . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file cursor.h
 *
 * These are declarations of functions provided by libcursor, a simple
 * wrapper library to terminal capabilities (termcap).
 *
 */

#ifndef CURSOR_H
#define CURSOR_H

#include "common.h"

#include <stdio.h>

__BEGIN_DECLS

#ifndef CURSOR_EXPORT
#  if defined(CURSOR_DLL_EXPORTS) && defined(CURSOR_DLL_IMPORTS)
#    error "Only CURSOR_DLL_EXPORTS or CURSOR_DLL_IMPORTS can be defined, not both."
#  elif defined(CURSOR_DLL_EXPORTS)
#    define CURSOR_EXPORT __declspec(dllexport)
#  elif defined(CURSOR_DLL_IMPORTS)
#    define CURSOR_EXPORT __declspec(dllimport)
#  else
#    define CURSOR_EXPORT
#  endif
#endif


/**
 * Initialize termcap.
 *
 * This function must be called first to read the termcap database and
 * to specify the output stream.
 *
 * Get individual parameters and control strings.  Initialize the
 * terminal.  Use 'fp' as output stream.
 *
 * Returns 1 for success, 0 for failure and prints appropriate
 * diagnostic.
 */
CURSOR_EXPORT extern int InitTermCap(FILE *fp);

/**
 * These below functions output terminal control strings to the file
 * stream specified by the InitTermCap() call which must precede them.
 * They return 0 if the capability is not described in the termcap
 * database and 1 otherwise.  Of course if the database entry is
 * wrong, the command will not do its job.
 */

/**
 * Begin standout mode.
 */
CURSOR_EXPORT extern int SetStandout(void);

/**
 * End standout mode.
 */
CURSOR_EXPORT extern int ClrStandout(void);

/**
 * Clear from the cursor to end of line.
 */
CURSOR_EXPORT extern int ClrEOL(void);

/**
 * Clear screen and home cursor.
 */
CURSOR_EXPORT extern int ClrText(void);

/**
 * Delete the current line.
 */
CURSOR_EXPORT extern int DeleteLn(void);

/**
 * Home the cursor.
 */
CURSOR_EXPORT extern int HmCursor(void);

/**
 * Move the cursor to screen coordinates x, y.
 */
CURSOR_EXPORT extern int MvCursor(int x, int y);

/**
 * Reverse scroll 1 line.
 */
CURSOR_EXPORT extern int ScrollDn(void);

/**
 * Forward scroll 1 line.
 */
CURSOR_EXPORT extern int ScrollUp(void);

/**
 * Set the scrolling region to be from "top" to "btm".
 */
CURSOR_EXPORT extern int SetScrlReg(int top, int btm);

/**
 * Reset the scrolling region to the entire screen.
 */
CURSOR_EXPORT extern int ResetScrlReg(void);


__END_DECLS

#endif /* CURSOR_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
