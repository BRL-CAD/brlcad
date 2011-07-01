/*                         D M - P S . C
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
/** @file mged/dm-ps.c
 *
 * Routines specific to MGED's use of LIBDM's Postscript display manager.
 *
 */

#include "common.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* for struct timeval */
#endif
#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "dm-ps.h"


extern void dm_var_init(struct dm_list *initial_dm_list);

int
PS_dm_init(struct dm_list *o_dm_list, int argc, const char *argv[])
{
    dm_var_init(o_dm_list);

    dmp = dm_open(INTERP, DM_TYPE_PS, argc, argv);
    if (dmp == DM_NULL)
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
