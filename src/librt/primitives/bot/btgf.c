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
/** @file primitives/bot/btgf.c
 *
 * the bot/tie float glue.
 *
 */

#include "common.h"

#define TIE_PRECISION 0

#include "raytrace.h"
#include "rtgeom.h"
#include "tie.h"
#include "btg.h"

#include "tie.c"
#include "tie_kdtree.c"

void *
bottie_allocn_float(unsigned long long ntri)
{
    struct tie_s *tie;
    tie = bu_malloc(sizeof(struct tie_s), "TIE");
    if(tie == NULL)
	return NULL;

    tie_init(tie, ntri, TIE_KDTREE_FAST);
    return (void *)tie;
}

void
bottie_push_float(void *UNUSED(vtie), float **UNUSED(tri), unsigned int UNUSED(ntri), void *UNUSED(usr), unsigned int UNUSED(pstride))
{
    struct tie_s *tie = (struct tie_s *)vtie;
    tie = NULL;
    return;
}

int
bottie_prep_float(struct soltab *stp,struct rt_bot_internal *bot, struct rt_i *UNUSED(rtip))
{
    struct tie_s *tie = (struct tie_s *)bot->tie;

    tie_prep(tie);
    VMOVE(stp->st_min, tie->min);
    VMOVE(stp->st_max, tie->max);
    VMOVE(stp->st_center, tie->mid);
    stp->st_bradius = stp->st_aradius = tie->radius;
    stp->st_specific = bot;

    return 0;
}


int
bottie_shot_float(struct soltab *UNUSED(stp), register struct xray *UNUSED(rp), struct application *UNUSED(ap), struct seg *UNUSED(seghead))
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
