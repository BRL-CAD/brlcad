/*                           T R Y . H
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
 */
/** @file try.h
 *	Author:		Gary S. Moss
 */
#define INCL_TRY
typedef struct
{
    int (*f_func)();		/* Function pointer. */
    char *f_buff;		/* Macro key-stroke buffer. */
    char *f_name;		/* Function/macro name. */
}
Func_Tab;
#define FT_NULL	(Func_Tab *) NULL

typedef union try
{
    struct
    {
	int t_curr;  /* Current letter. */
	union try	*t_altr; /* Alternate letter node link. */
	union try	*t_next; /* Next letter node link. */
    }
    n;
    struct
    {
	Func_Tab	*t_ftbl; /* Function table pointer. */
	union try	*t_altr; /* Alternate letter node link. */
	union try	*t_next; /* Next letter node link. */
    }
    l;
}
Try;
#define TRY_NULL	(Try *) NULL

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
