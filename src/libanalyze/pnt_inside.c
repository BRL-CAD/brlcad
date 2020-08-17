/*                    P N T _ I N S I D E . C
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file pnt_inside.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "raytrace.h"
#include "analyze.h"

static void
_tgc_hack_fix(struct partition *part, struct soltab *stp) {
    /* hack fix for bad tgc surfaces - avoids a logging crash, which is probably something else altogether... */
    if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	/* correct invalid surface number */
	if (part->pt_inhit->hit_surfno < 1 || part->pt_inhit->hit_surfno > 3) {
	    part->pt_inhit->hit_surfno = 2;
	}
	if (part->pt_outhit->hit_surfno < 1 || part->pt_outhit->hit_surfno > 3) {
	    part->pt_outhit->hit_surfno = 2;
	}
    }
}


static int
in_out_hit(struct application *ap, struct partition *partH, struct seg *UNUSED(segs))
{
    struct partition *part = partH->pt_forw;
    struct soltab *stp = part->pt_inseg->seg_stp;

    int *ret = (int *)(ap->a_uptr);

    RT_CK_APPLICATION(ap);

    _tgc_hack_fix(part, stp);

    if (part->pt_inhit->hit_dist < 0) {
	(*ret) = -1;
    }

    if (NEAR_ZERO(part->pt_inhit->hit_dist, VUNITIZE_TOL)) {
	(*ret) = 1;
    }

    return 0;
}

static int
in_out_miss(struct application *UNUSED(ap))
{
    return 0;
}

int
analyze_pnt_in_vol(point_t *p, struct application *ap, int on_is_in)
{
    int i;
    int fret = 1;
    int dir_results[6] = {0, 0, 0, 0, 0, 0};
    int (*a_hit)(struct application *, struct partition *, struct seg *);
    int (*a_miss)(struct application *);
    void *uptr_stash;

    vect_t mx, my, mz, px, py, pz;
    VSET(mx, -1,  0,  0);
    VSET(my,  0, -1,  0);
    VSET(mz,  0,  0, -1);
    VSET(px,  1,  0,  0);
    VSET(py,  0,  1,  0);
    VSET(pz,  0,  0,  1);

    /* reuse existing application, just cache pre-existing hit routines and
     * substitute our own */
    a_hit = ap->a_hit;
    a_miss = ap->a_miss;
    uptr_stash = ap->a_uptr;

    ap->a_hit = in_out_hit;
    ap->a_miss = in_out_miss;

    VMOVE(ap->a_ray.r_pt, *p);

    /* Check the six directions to see if any of them indicate we are inside */

    /* -x */
    ap->a_uptr = &(dir_results[0]);
    VMOVE(ap->a_ray.r_dir, mx);
    (void)rt_shootray(ap);
    /* -y */
    ap->a_uptr = &(dir_results[1]);
    VMOVE(ap->a_ray.r_dir, my);
    (void)rt_shootray(ap);
    /* -z */
    ap->a_uptr = &(dir_results[2]);
    VMOVE(ap->a_ray.r_dir, mz);
    (void)rt_shootray(ap);
    /* x */
    ap->a_uptr = &(dir_results[3]);
    VMOVE(ap->a_ray.r_dir, px);
    (void)rt_shootray(ap);
    /* y */
    ap->a_uptr = &(dir_results[4]);
    VMOVE(ap->a_ray.r_dir, py);
    (void)rt_shootray(ap);
    /* z */
    ap->a_uptr = &(dir_results[5]);
    VMOVE(ap->a_ray.r_dir, pz);
    (void)rt_shootray(ap);

    for (i = 0; i < 6; i++) {
	if (on_is_in) {
	    if (dir_results[i] != 0)
	       	fret = -1;
	} else {
	    if (dir_results[i] < 0)
	       	fret = -1;
	}
    }

    /* restore application */
    ap->a_hit = a_hit;
    ap->a_miss = a_miss;
    ap->a_uptr = uptr_stash;

    return fret;
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

