/*                    B T G . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2010 United States Government as represented by
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
/** @file btg.c
 *
 * the bot/tie glue.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "btg.h"

#define TIE_PRECISION 1
#include "tie.c"
#include "tie_kdtree.c"

void
*bottie_allocn_double(unsigned long long ntri)
{
    struct tie_s *tie;
    tie = bu_malloc(sizeof(struct tie_s), "TIE");
    if(tie == NULL)
	return NULL;

    tie_init(tie, ntri, TIE_KDTREE_FAST);
}

void
bottie_push_double(void *vtie, double *tri, unsigned int ntri, void *plist, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;
}

void
bottie_prep_double(void *vtie)
{
    tie_prep((struct tie_s *)vtie);
}

static int
hitfunc()
{
}

int
bottie_shot_double(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    /* use hitfunc to build the hit list */
    return -1;
}

void
bottie_free_double(void *vtie)
{
    tie_free0((struct tie_s *)vtie);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
