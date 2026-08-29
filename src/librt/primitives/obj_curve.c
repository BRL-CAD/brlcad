/*                   O B J _ C U R V E . C
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


static int
obj_curve_supports_nonuniform(int id)
{
    switch (id) {
	case ID_TOR:
	case ID_PARTICLE:
	case ID_RPC:
	case ID_RHC:
	case ID_EPA:
	case ID_EHY:
	case ID_ETO:
	case ID_HYP:
	    return 1;
	default:
	    return 0;
    }
}


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

    ft = &OBJ[id];
    if (!ft)
	return -3;
    if (!ft->ft_curve)
	return -4;

    if (stp->st_nu_inv_matp) {
	if (obj_curve_supports_nonuniform(id))
	    return _rt_nonuniform_curve(cvp, hitp, stp, stp->st_nu_matp, stp->st_nu_inv_matp);

	VSETALL(cvp->crv_pdir, 0.0);
	cvp->crv_c1 = 0.0;
	cvp->crv_c2 = 0.0;
	return -5;
    }

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
