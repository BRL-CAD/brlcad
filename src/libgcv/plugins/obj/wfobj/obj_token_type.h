/*                O B J _ T O K E N _ T Y P E . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2016 United States Government as represented by
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
/** @file obj_token_type.h
 *
 * Token object used by scanner and parser.
 *
 */

#ifndef LIBGCV_WFOBJ_OBJ_TOKEN_TYPE_H
#define LIBGCV_WFOBJ_OBJ_TOKEN_TYPE_H

#include "obj_util.h"

const int TOKEN_STRING_LEN = 80;

typedef union YYSTYPE
{
    float real;
    int integer;
    int reference[3];
    unsigned char toggle;
    size_t index;
    char string[TOKEN_STRING_LEN];
} YYSTYPE;
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
