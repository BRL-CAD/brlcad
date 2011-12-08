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
/** @file primitives/bot/btg.c
 *
 * the bot/tie glue.
 *
 */

#define TIE_PRECISION 1

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "bot.h"
#include "tie.h"

#include "btg.h"

#include "tie.c"
#include "tie_kdtree.c"

int tie_check_degenerate = 0;
fastf_t TIE_PREC = 0.1;

int rt_bot_makesegs(struct hit *hits, size_t nhits, struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead, struct rt_piecestate *psp);

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
bottie_push_double(void *vtie, TIE_3 **tri, unsigned int ntri, void *u, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;

    tie_push1(tie, tri, ntri, u, pstride);
}

int
bottie_prep_double(struct soltab *stp, struct rt_bot_internal *bot_ip, struct rt_i *UNUSED(rtip))
{
    struct tie_s *tie;
    struct bot_specific *bot;
    unsigned int i;
    TIE_3 *tribuf = NULL, **tribufp = NULL;

    RT_BOT_CK_MAGIC(bot_ip);

    BU_GET(bot, struct bot_specific);
    stp->st_specific = (genptr_t)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;

    if((tie = bot_ip->tie = bot->tie = bottie_allocn_double(bot_ip->num_faces)) == NULL)
	return -1;
    if((tribuf = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3 * bot_ip->num_faces, "triangle tribuffer")) == NULL) {
	tie_free(tie);
	return -1;
    }
    if((tribufp = (TIE_3 **)bu_malloc(sizeof(TIE_3*) * 3 * bot_ip->num_faces, "triangle tribuffer pointer")) == NULL) {
	tie_free(tie);
	bu_free(tribuf, "tribuf");
	return -1;
    }

    for(i=0;i<bot_ip->num_faces*3;i++) {   
	tribufp[i] = &tribuf[i];
	VMOVE(tribuf[i].v, (bot_ip->vertices+3*bot_ip->faces[i]));
    }
    tie_push1(bot_ip->tie, tribufp, bot_ip->num_faces, bot, 0);
    bu_free(tribuf, "tribuffer");
    bu_free(tribufp, "tribufp");

    tie_prep1((struct tie_s *)bot->tie);

    VMOVE(stp->st_min, tie->amin);
    VMOVE(stp->st_max, tie->amax);
    VMOVE(stp->st_center, tie->mid);
    stp->st_aradius = tie->radius;
    stp->st_bradius = tie->radius;

    return 0;
}

#define MAXHITS 128

struct hitdata_s {
    int nhits;
    struct hit hits[MAXHITS];
    struct tri_specific ts[MAXHITS];
    struct xray *rp;
};

static void *
hitfunc(struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, struct tie_tri_s *UNUSED(tri), void *ptr)
{
    struct hitdata_s *h = (struct hitdata_s *)ptr;
    struct tri_specific *tsp;
    struct hit *hp;

    if(h->nhits > (MAXHITS-1)) {
	bu_log("Too many hits!\n");
	return (void *)1;
    }

    hp = &h->hits[h->nhits];
    tsp = hp->hit_private = &h->ts[h->nhits];
    h->nhits++;


    hp->hit_magic = RT_HIT_MAGIC;
    hp->hit_dist = id->dist;
    VMOVE(tsp->tri_N, id->norm);

    /* add hitdist into array and add one to nhits */
    return NULL;	/* continue firing */
}

int
bottie_shot_double(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct bot_specific *bot;
    struct tie_s *tie;
    struct hitdata_s hitdata;
    struct tie_id_s id;
    struct tie_ray_s ray;

    bot = (struct bot_specific *)stp->st_specific;
    tie = (struct tie_s *)bot->tie;

    hitdata.nhits = 0;
    hitdata.rp = &ap->a_ray;

    VCOMB2(ray.pos, 1.0, rp->r_pt, -1.01, rp->r_dir);
    VMOVE(ray.dir, rp->r_dir);
    ray.depth = ray.kdtree_depth = 0;

    tie_work1(tie, &ray, &id, hitfunc, &hitdata);

    /* use hitfunc to build the hit list */
    if(hitdata.nhits == 0)
	return 0;

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
