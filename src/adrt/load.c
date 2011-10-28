/*                         L O A D . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file load.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tie.h"
#include "load.h"

uint32_t slave_load_mesh_num;
adrt_mesh_t *slave_load_mesh_list;

void
slave_load_free()
{
#if 0
    int i;

    /* Free texture data */
    for (i = 0; i < texture_num; i++)
	texture_list[i].texture->free (texture_list[i].texture);
    free (texture_list);

    /* Free mesh data */
    for (i = 0; i < db->mesh_num; i++) {
	/* Free triangle data */
	free (db->mesh_list[i]->tri_list);
	free (db->mesh_list[i]);
    }
    free (db->mesh_list);
#endif
}

int
slave_load_region(struct tie_s *UNUSED(tie), char *UNUSED(data))
{
    /*
     * data contains a region name and the triangle soup.
     * Meant to be called several times, with slave_load_kdtree called at the
     * end to finally prep it up.
     */
    return -1;
}

int
slave_load_kdtree(struct tie_s *UNUSED(tie), char *UNUSED(data))
{
    /* after slave_load_region calls have filled in all the geometry, this loads
     * a tree or requests a tree generation if data is NULL
     */
    return -1;
}

int
slave_load(struct tie_s *tie, void *data)
{
    char *meh = (char *)data;

    tie_check_degenerate = 0;

    meh += 3;	/* advance to the opcode */

    switch ( *meh ) {
	case ADRT_LOAD_FORMAT_G:	/* given a filename and 1 toplevel region, recursively load from a .g file */
	    {
		const char *db = NULL; /* FIXME */
		const char *ugh[2];
		ugh[0] = (char *)(meh + 1 + sizeof(int));
		ugh[1] = NULL;
		return load_g ( tie, db, *(int *)(meh + 1), ugh, NULL);
	    }
	case ADRT_LOAD_FORMAT_REG:	/* special magic for catching data on the pipe */
	    return slave_load_region (tie, meh + 1);
	case ADRT_LOAD_FORMAT_KDTREE:	/* more special magic */
	    return slave_load_kdtree (tie, meh + 1);
	default:
	    fprintf(stderr, "Unknown load format\n");
	    return 1;
    }

    return -1;
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
