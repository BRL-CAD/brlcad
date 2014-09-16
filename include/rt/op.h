/*                            O P . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file op.h
 *
 * Routines for detecting, using, and managing boolean operators.
 *
 */

#ifndef RT_DB_H
#define RT_DB_H

#include "./defines.h"

__BEGIN_DECLS

typedef enum {
    DB_OP_NULL = 0,
    DB_OP_UNION = 'u',
    DB_OP_SUBTRACT = '-',
    DB_OP_INTERSECT = '+'
} db_op_t;


/**
 * Get the next CSG boolean operator found in a given string.
 *
 * Skipping any whitespace, this routine returns the CSG boolean
 * operation in canonical (single-byte enumeration) form.  It will
 * attempt to recognize operators in various unicode formats if the
 * input string contains mixed encodings.
 */
RT_EXPORT db_op_t
db_str2op(const char *str);

__END_DECLS

#endif /* RT_DB_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
