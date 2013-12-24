/*                    B T G . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2013 United States Government as represented by
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
    BU_ALLOC(tie, struct tie_s);
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
    size_t tri_index, i;
    TIE_3 *tribuf = NULL, **tribufp = NULL;

    RT_BOT_CK_MAGIC(bot_ip);

    BU_GET(bot, struct bot_specific);
    stp->st_specific = (genptr_t)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;
    if (bot_ip->thickness) {
	bot->bot_thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "bot_thickness");
	for (tri_index = 0; tri_index < bot_ip->num_faces; tri_index++)
	    bot->bot_thickness[tri_index] = bot_ip->thickness[tri_index];
    }
    if (bot_ip->face_mode)
	bot->bot_facemode = bu_bitv_dup(bot_ip->face_mode);
    bot->bot_facelist = NULL;

    tie = (struct tie_s *)bottie_allocn_double(bot_ip->num_faces);
    if (tie != NULL) {
	bot_ip->tie = tie;
    } else {
	return -1;
    }

    if ((tribuf = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3 * bot_ip->num_faces, "triangle tribuffer")) == NULL) {
	tie_free(tie);
	return -1;
    }
    if ((tribufp = (TIE_3 **)bu_malloc(sizeof(TIE_3*) * 3 * bot_ip->num_faces, "triangle tribuffer pointer")) == NULL) {
	tie_free(tie);
	bu_free(tribuf, "tribuf");
	return -1;
    }

    for (i = 0; i < bot_ip->num_faces*3; i++) {
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
hitfunc(struct tie_ray_s *ray, struct tie_id_s *id, struct tie_tri_s *UNUSED(tri), void *ptr)
{
    struct hitdata_s *h = (struct hitdata_s *)ptr;
    struct tri_specific *tsp;
    struct hit *hp;
    fastf_t dn;		/* Direction dot Normal */
    fastf_t abs_dn;
    fastf_t alpha, beta;
    vect_t wxb;		/* vertex - ray_start */
    vect_t xp;		/* wxb cross ray_dir */

    if (h->nhits > (MAXHITS-1)) {
	bu_log("Too many hits!\n");
	return (void *)1;
    }

    hp = &h->hits[h->nhits];
    hp->hit_private = &h->ts[h->nhits];
    tsp = (struct tri_specific *)hp->hit_private;
    h->nhits++;


    hp->hit_magic = RT_HIT_MAGIC;
    hp->hit_dist = id->dist;
    VMOVE(tsp->tri_N, id->norm);

    /* replicate hit_vpriv[] settings from original BOT code, used later to
     * clean up odd hits, exit before entrance, or dangling entrance in
     * make_bot_segment(). BOT hits were disappearing from the segment
     * depending on hit_vpriv[X] uninitialized value.
     */
    dn = VDOT(tsp->tri_wn, ray->dir);
    abs_dn = dn >= 0.0 ? dn : (-dn);
    VSUB2(wxb, tsp->tri_A, ray->pos);
    VCROSS(xp, wxb, ray->dir);
    alpha = VDOT(tsp->tri_CA, xp);
    if (dn < 0.0) alpha = -alpha;
    beta = VDOT(tsp->tri_BA, xp);
    hp->hit_vpriv[X] = VDOT(tsp->tri_N, ray->dir);
    hp->hit_vpriv[Y] = alpha / abs_dn;
    hp->hit_vpriv[Z] = beta / abs_dn;
    hp->hit_surfno = tsp->tri_surfno;


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
    int i;
    fastf_t dirlen;

    bot = (struct bot_specific *)stp->st_specific;
    tie = (struct tie_s *)bot->tie;

    hitdata.nhits = 0;
    hitdata.rp = &ap->a_ray;
    /* do not need to init 'hits' and 'ts', tracked by 'nhits' */

    /* small backout applied to ray origin */
    dirlen = MAGSQ(rp->r_dir);
    VSUB2(ray.pos, rp->r_pt, rp->r_dir);	/* step back one dirlen */
    VMOVE(ray.dir, rp->r_dir);
    ray.depth = ray.kdtree_depth = 0;

    tie_work1(tie, &ray, &id, hitfunc, &hitdata);

    /* use hitfunc to build the hit list */
    if (hitdata.nhits == 0)
	return 0;

    /* adjust hit distances to initial ray origin */
    for (i = 0; i < hitdata.nhits; i++)
	hitdata.hits[i].hit_dist = hitdata.hits[i].hit_dist - dirlen;


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
