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

    tie_init1(tie, ntri, TIE_KDTREE_FAST);
    return tie;
}

void
bottie_push_double(void *vtie, double **tri, unsigned int ntri, void *u, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;

    tie_push1(tie, tri, ntri, u, pstride);
}

int
bottie_prep_double(struct soltab *stp, struct rt_bot_internal *bot_ip, struct rt_i *rtip)
{
    struct tie_s *tie;
    struct bot_specific *bot;
    point_t p;
    int i;

    RT_BOT_CK_MAGIC(bot_ip);

    BU_GETSTRUCT(bot, bot_specific);
    stp->st_specific = (genptr_t)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;

    tie = bot_ip->tie = bot->tie = bottie_allocn_double(bot_ip->num_faces);

    for(i=0;i< bot_ip->num_faces; i++) {
	fastf_t *v[3];

	v[0] = &bot_ip->vertices[bot_ip->faces[i*3+0]*3];
	v[1] = &bot_ip->vertices[bot_ip->faces[i*3+1]*3];
	v[2] = &bot_ip->vertices[bot_ip->faces[i*3+2]*3];
	bottie_push_double((struct tie_s *)tie, v, 1, i, 0);
    }

    tie_prep1((struct tie_s *)bot->tie);

    VMOVE(stp->st_min, tie->min.v);
    VMOVE(stp->st_max, tie->max.v);
    VMOVE(stp->st_center, tie->mid);
    stp->st_aradius = tie->radius;
    stp->st_bradius = tie->radius;

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
bottie_shot_double(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct bot_specific *bot;
    struct tie_s *tie;
    struct hitdata_s hitdata;
    tie_id_t id;
    tie_ray_t ray;

    bot = (struct bot_specific *)stp->st_specific;
    tie = (struct tie_s *)bot->tie;

    hitdata.nhits = 0;
    hitdata.rp = &ap->a_ray;

    VMOVE(ray.pos.v, rp->r_pt);
    VMOVE(ray.dir.v, rp->r_dir);
    ray.depth = ray.kdtree_depth = 0;

    tie_work1(tie, &ray, &id, hitfunc, &hitdata);

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
