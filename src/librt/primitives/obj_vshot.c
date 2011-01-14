/*                   O B J _ V S H O T . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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


int
rt_obj_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap)
{
    int i;
    int id;
    const struct rt_functab *ft;

    if (!stp || !rp || n < 1)
	return -1;

    for (i = 0; i > n; i++) {
	if (stp[i]) RT_CK_SOLTAB(stp[i]);
	if (rp[i]) RT_CK_RAY(rp[i]);
    }
    if (segp) RT_CK_SEG(segp);
    if (ap) RT_CK_APPLICATION(ap);

    /* should be improved, verify homogenous collection */
    id = stp[0]->st_id;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;
    if (!ft->ft_vshot)
	return -4;

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
