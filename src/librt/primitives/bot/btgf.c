/*                    B T G F . C
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
/** @file btgf.c
 *
 * the bot/tie float glue.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "btg.h"

#define TIE_PRECISION 0
#include "tie.c"
#include "tie_kdtree.c"

void
*bottie_allocn_float(unsigned long long ntri)
{
    struct tie_s *tie;
    tie = bu_malloc(sizeof(struct tie_s), "TIE");
    if(tie == NULL)
	return NULL;

    tie_init(tie, ntri, TIE_KDTREE_FAST);
}

void
bottie_push_float(void *vtie, float **tri, unsigned int ntri, void *usr, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;
}

int
bottie_prep_float(struct soltab *stp,struct rt_bot_internal *bot, struct rt_i *rtip)
{
    struct tie_s *tie = (struct tie_s *)bot->tie;
    point_t p;

    tie_prep(tie);
    VMOVE(stp->st_min, tie->min.v);
    VMOVE(stp->st_max, tie->max.v);
    VMOVE(stp->st_center, tie->mid);
    stp->st_bradius = stp->st_aradius = tie->radius;
    stp->st_specific = bot;

    return 0;
}

static int
hitfunc()
{
}

int
bottie_shot_float(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    /* use hitfunc to build the hit list */
    return -1;
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
