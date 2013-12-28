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
 */
CURSOR_EXPORT extern int InitTermCap(FILE *fp);

/**
 * These below functions output terminal control strings to the file
 * stream specified by the InitTermCap() call which must precede them.
 * They return 0 if the capability is not described in the termcap
 * database and 1 otherwise.  Of course if the database entry is
 * wrong, the command will not do its job.
 */

int ClrEOL(void);
int ClrStandout(void);
int ClrText(void);
int DeleteLn(void);
int HmCursor(void);
int MvCursor(int x, int y);
int ResetScrlReg(void);
int ScrollDn(void);
int ScrollUp(void);
int SetScrlReg(int top, int btm);
int SetStandout(void);

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
