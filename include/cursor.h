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
 * initialize termcap
 */
CURSOR_EXPORT extern int InitTermCap(FILE *fp);


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
