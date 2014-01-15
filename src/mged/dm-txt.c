/*                          D M - T X T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file mged/dm-txt.c
 *
 * Routines specific to MGED's use of LIBDM's X display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern void dm_var_init(struct dm_list *initial_dm_list);		/* defined in attach.c */

int
Txt_dm_init(struct dm_list *o_dm_list, int argc, const char *argv[])
{
    dm_var_init(o_dm_list);
    if ((dmp = dm_open(INTERP, DM_TYPE_TXT, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;
    return TCL_OK;
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
