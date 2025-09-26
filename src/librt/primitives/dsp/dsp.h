/*                           D S P . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2025 United States Government as represented by
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
/*
 * Private header, only for dsp implementation use.
 *
 */

#ifndef LIBRT_PRIMITIVES_DSP_DSP_H
#define LIBRT_PRIMITIVES_DSP_DSP_H

#include "common.h"
#include "raytrace.h"
#include "rt/geom.h"

__BEGIN_DECLS

/* access to the DSP data array */
# define DSP(_p, _x, _y) (						\
	((_p) && (_p)->dsp_buf) ?					\
	((unsigned short *)((_p)->dsp_buf))[				\
	    (_y) * ((struct rt_dsp_internal *)_p)->dsp_xcnt + (_x)	\
	    ] : 0)

extern int
rt_dsp_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *to);

extern int
rt_dsp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

__END_DECLS

#endif /* LIBRT_PRIMITIVES_DSP_DSP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
