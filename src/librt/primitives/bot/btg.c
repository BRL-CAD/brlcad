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
#include "bot.h"
#include "btg.h"

#define TIE_PRECISION 1
#include "tie.h"
#include "tie.c"
#include "tie_kdtree.c"

void *
bottie_allocn_double(unsigned long long ntri)
{
    struct tie_s *tie;
    tie = bu_malloc(sizeof(struct tie_s), "TIE");
    if(tie == NULL)
	return NULL;

    tie_init(tie, ntri, TIE_KDTREE_FAST);
}

void
bottie_push_double(void *vtie, double **tri, unsigned int ntri, void *u, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;

    tie_push(tie, tri, ntri, u, pstride);
}

int
bottie_prep_double(void *vtie)
{
    tie_prep((struct tie_s *)vtie);
    return 0;	/* always returning OK seems... bad. */
}

#define MAXHITS 128

struct hitdata_s {
    int nhits;
    struct hit hits[MAXHITS];
    struct xray *rp;
};

static void *
hitfunc(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    struct hitdata_s *h = (struct hitdata_s *)ptr;
    struct hit *hp;

    hp = &h->hits[h->nhits];
    h->nhits++;

    if(h->nhits > MAXHITS) {
	bu_log("Too many hits!\n");
	return 1;
    }

    hp->hit_magic = RT_HIT_MAGIC;
    hp->hit_dist = id->dist;
    hp->hit_private = tri->ptr;

    /* add hitdist into array and add one to nhits */
    return NULL;	/* continue firing */
}

int
bottie_shot_double(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
    struct tie_s *tie = (struct tie_s *)bot->tie;
    struct hitdata_s hitdata;
    tie_id_t id;
    tie_ray_t ray;

    hitdata.nhits = 0;
    hitdata.rp = &ap->a_ray;

    VMOVE(ray.pos.v, rp->r_pt);
    VMOVE(ray.dir.v, rp->r_dir);
    ray.depth = ray.kdtree_depth = 0;

    bu_log("Pew\n");
    tie_work(tie, &ray, &id, hitfunc, &hitdata);

    /* use hitfunc to build the hit list */
    if(hitdata.nhits == 0)
	return 0;

    bu_log("Bang!\n");

    /* this func is in ars, for some reason. All it needs is the dist. */
    rt_hitsort(hitdata.hits, hitdata.nhits);

    return rt_bot_makesegs(hitdata.hits, hitdata.nhits, stp, rp, ap, seghead, NULL);
}

void
bottie_free_double(void *vtie)
{
    tie_free((struct tie_s *)vtie);
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
