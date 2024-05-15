/*                    B T G . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2024 United States Government as represented by
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

#ifdef TIE_PRECISION
#  undef TIE_PRECISION
#endif
#define TIE_PRECISION 1

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "rt/tie.h"

#include "btg.h"
#include "../../librt_private.h"

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
    tie_init_double(tie, ntri, TIE_KDTREE_FAST);
    return tie;
}

void
bottie_push_double(void *vtie, TIE_3 **tri, unsigned int ntri, void *u, unsigned int pstride)
{
    struct tie_s *tie = (struct tie_s *)vtie;

    tie_push_double(tie, tri, ntri, u, pstride);
}

int
bottie_prep_double(struct soltab *stp, struct rt_bot_internal *bot_ip, struct rt_i *rtip)
{
    struct tie_s *tie;
    struct bot_specific *bot;
    size_t tri_index, i;
    TIE_3 *tribuf = NULL, **tribufp = NULL;

    RT_BOT_CK_MAGIC(bot_ip);

    BU_GET(bot, struct bot_specific);
    stp->st_specific = (void *)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;
    if (bot_ip->thickness) {
	bot->bot_thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "bot_thickness");
	for (tri_index = 0; tri_index < bot_ip->num_faces; tri_index++)
	    bot->bot_thickness[tri_index] = bot_ip->thickness[tri_index];
    } else {
	bot->bot_thickness = NULL;
    }

    if (bot_ip->face_mode) {
	bot->bot_facemode = bu_bitv_dup(bot_ip->face_mode);
    } else {
	bot->bot_facemode = BU_BITV_NULL;
    }
    bot->bot_facelist = NULL;

    tie = (struct tie_s *)bottie_allocn_double(bot_ip->num_faces);
    if (tie != NULL) {
	bot_ip->tie = bot->tie = tie;
    } else {
	return -1;
    }

    if ((tribuf = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3 * bot_ip->num_faces, "triangle tribuffer")) == NULL) {
	tie_free_double(tie);
	return -1;
    }
    if ((tribufp = (TIE_3 **)bu_malloc(sizeof(TIE_3*) * 3 * bot_ip->num_faces, "triangle tribuffer pointer")) == NULL) {
	tie_free_double(tie);
	bu_free(tribuf, "tribuf");
	return -1;
    }

    for (i = 0; i < bot_ip->num_faces*3; i++) {
	tribufp[i] = &tribuf[i];
	VMOVE(tribuf[i].v, (bot_ip->vertices+3*bot_ip->faces[i]));
    }

    /* tie_pushX sig: (struct tie_s *,
     *                 TIE_3 **,
     *                 unsigned int,
     *                 void *,
     *                 unsigned int);
     */
    tie_push_double((struct tie_s *)bot_ip->tie, tribufp, bot_ip->num_faces, bot, 0);

    bu_free(tribuf, "tribuffer");
    bu_free(tribufp, "tribufp");

    tie_prep_double((struct tie_s *)bot->tie);

    VMOVE(stp->st_min, tie->amin);
    VMOVE(stp->st_max, tie->amax);

    /* zero thickness will get missed by the raytracer */
    BBOX_NONDEGEN(stp->st_min, stp->st_max, rtip->rti_tol.dist);

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

    /* hit_vpriv is used later to clean up odd hits, exit before entrance, or
     * dangling entrance in bot_makesegs_(). When TIE was leaving this
     * unset, BOT hits were disappearing from the segment depending on the
     * random hit_vpriv[X] uninitialized values - set it using id->norm and the
     * ray direction. */
    VMOVE(tsp->tri_N, id->norm);
    hp->hit_vpriv[X] = VDOT(tsp->tri_N, ray->dir);

    /* Of the hit_vpriv assignments added in commit 50164, only hit_vpriv[X]
     * was based on initialized calculations.  Rather than leaving the other
     * assignments (which were based on calculations using uninitialized values
     * in the tsp struct) we simply assign 0 values.
     *
     * NOTE: bot_norm_ does use Y and Z in the normal calculations, so those
     * still won't work.  Likewise, bot_plate_segs_ uses hit_surfno which is
     * also not set correctly.  Perhaps the currently unused tri would have
     * the needed info? */
    hp->hit_vpriv[Y] = 0.0;
    hp->hit_vpriv[Z] = 0.0;
    hp->hit_surfno = 0;

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

    tie_work_double(tie, &ray, &id, hitfunc, &hitdata);

    /* use hitfunc to build the hit list */
    if (hitdata.nhits == 0)
	return 0;

    /* adjust hit distances to initial ray origin */
    for (i = 0; i < hitdata.nhits; i++)
	hitdata.hits[i].hit_dist = hitdata.hits[i].hit_dist - dirlen;

    /* FIXME: we don't have the hit_surfno but at least initialize it */
    for (i = 0; i < hitdata.nhits; i++)
        hitdata.hits[i].hit_surfno = 0;

    return rt_bot_makesegs(hitdata.hits, hitdata.nhits, stp, rp, ap, seghead, NULL);
}

void
bottie_free_double(void *vtie)
{
    tie_free_double((struct tie_s *)vtie);
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
