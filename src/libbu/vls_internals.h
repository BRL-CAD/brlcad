/*                  V L S _ I N T E R N A L S . H
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
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

#ifndef LIBBU_VLS_INTERNALS_H
#define LIBBU_VLS_INTERNALS_H seen

#include "bu.h"


/* private constants */

/* minimum vls allocation increment size */
static const size_t _VLS_ALLOC_STEP = 128;

/* constant bit flags  for var 'vp_part' */
#define VP_UNKNOWN          0x0001
#define VP_FLAG             0x0002
#define VP_LENGTH_MOD       0x0004
#define VP_CONVERSION_SPEC  0x0008
#define VP_MISC             0x0010
#define VP_VALID            0x0100
#define VP_OBSOLETE         0x1000
#define VP_PARTS            (VP_FLAG | VP_LENGTH_MOD | VP_CONVERSION_SPEC | VP_MISC)

/* other flags */
static const int VP_NOPRINT = 0;
static const int VP_PRINT   = 1;

/* private structs */
typedef struct
vprintf_flags
{
    int fieldlen;
    int flags;
    int have_digit;
    int have_dot;
    int left_justify;
    int precision;
} vflags_t;

/* private shared function decls */
BU_EXPORT extern int format_part_status(const char c);
BU_EXPORT extern int handle_format_part(const int vp_part, vflags_t *f, const char c, const int print);
BU_EXPORT extern int handle_obsolete_format_char(const char c, const int print);

#endif /* LIBBU_VLS_INTERNALS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
