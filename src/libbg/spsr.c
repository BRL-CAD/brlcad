/*                         S P S R . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file spsr.c
 *
 * Brief description
 *
 */
#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bg/spsr.h"

#include "bg/spsr.h"

#ifdef ENABLE_SPR

#include "bu/malloc.h"
#include "../other/PoissonRecon/Src/SPR.h"

struct c_vert {
    point_t p;
    vect_t n;
    int is_set;
};

int
bg_3d_spsr(int **faces, int *num_faces, point_t **points, int *num_pnts,
	const point_t *input_points_3d, const vect_t *input_normals_3d,
	int num_input_pnts, struct bg_3d_spsr_opts *spsr_opts)
{
    int i = 0;
    struct spr_options opts = SPR_OPTIONS_DEFAULT_INIT;
    struct c_vert **c_verts = (struct c_vert **)bu_calloc(num_input_pnts, sizeof(struct c_vert *), "output array");
    if (spsr_opts) {
	opts.depth = spsr_opts->depth;
	opts.pointweight = spsr_opts->point_weight;
    }
    for (i = 0; i < num_input_pnts; i++) {
	struct c_vert *v;
	BU_GET(v, struct c_vert);
	VMOVE(v->p, input_points_3d[i]);
	VMOVE(v->n, input_normals_3d[i]);
	v->is_set = 1;
	c_verts[i] = v;
    }
    (void)spr_surface_build(faces, num_faces, (double **)points, num_pnts, (const struct cvertex **)c_verts, num_input_pnts, &opts);
    for (i = 0; i < num_input_pnts; i++) {
	BU_PUT(c_verts[i], struct c_vert);
    }
    bu_free((void *)c_verts, "vert array");
    return 0;
}


#else /* ENABLE_SPR */

int
bg_3d_spsr(int **UNUSED(faces), int *UNUSED(num_faces), point_t **UNUSED(points), int *UNUSED(num_pnts),
	const point_t *UNUSED(input_points_3d), const vect_t *UNUSED(input_normals_3d),
	int UNUSED(num_input_pnts), struct bg_3d_spsr_opts *UNUSED(spsr_opts))
{
    bu_log("Screened Poisson Reconstruction was not enabled for this compilation.\n");
    return 0;
}

#endif /* ENABLE_SPR */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

