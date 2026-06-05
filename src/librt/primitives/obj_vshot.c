/*                   O B J _ V S H O T . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2026 United States Government as represented by
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

#include "common.h"

#include "raytrace.h"

#include "../librt_private.h"


int
rt_obj_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap)
{
    int i;
    int id;
    const struct rt_functab *ft;
    int use_scalar = 0;

    if (!stp || !rp || n < 1)
	return 0;
    if (!stp[0])
	return 0;

    for (i = 0; i < n; i++) {
	RT_CK_SOLTAB(stp[i]);
	if (rp[i])
	    RT_CK_RAY(rp[i]);
	if (stp[i]->st_nu_inv_matp)
	    use_scalar = 1;
    }
    if (segp)
	RT_CK_SEG(segp);
    if (ap)
	RT_CK_APPLICATION(ap);

    /* should be improved, verify homogeneous collection */
    id = stp[0]->st_id;
    if (id < 0)
	return -2;

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!use_scalar && !ft->ft_vshot)
	return -4;

    if (use_scalar || !ft->ft_vshot) {
	for (i = 0; i < n; i++) {
	    struct seg seghead;
	    struct seg *tmp_seg;
	    int ret;

	    BU_LIST_INIT(&seghead.l);
	    ret = _rt_nonuniform_shot(stp[i], rp[i], ap, &seghead);
	    if (ret <= 0) {
		segp[i].seg_stp = (struct soltab *)0;
		continue;
	    }
	    tmp_seg = BU_LIST_FIRST(seg, &seghead.l);
	    BU_LIST_DEQUEUE(&tmp_seg->l);
	    segp[i] = *tmp_seg;
	    RT_FREE_SEG(tmp_seg, ap->a_resource);
	    while (BU_LIST_WHILE(tmp_seg, seg, &seghead.l)) {
		BU_LIST_DEQUEUE(&tmp_seg->l);
		RT_FREE_SEG(tmp_seg, ap->a_resource);
	    }
	}
	return 0;
    }

    ft->ft_vshot(stp, rp, segp, n, ap);
    return 0;
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
