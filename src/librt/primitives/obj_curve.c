/*                   O B J _ C U R V E . C
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
rt_obj_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    int id;
    const struct rt_functab *ft;

    if (!cvp || !stp)
	return -1;

    if (hitp) RT_CK_HIT(hitp);
    RT_CK_SOLTAB(stp);

    id = stp->st_id;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;
    if (!ft->ft_curve)
	return -4;

    ft->ft_curve(cvp, hitp, stp);
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
