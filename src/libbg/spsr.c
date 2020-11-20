/*                         S P S R . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2020 United States Government as represented by
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
 * Interface to Screened Poisson Surface Reconstruction
 *
 */
#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#include "./spsr/SPSR.h"

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
    struct spsr_options opts = SPSR_OPTIONS_DEFAULT_INIT;
    struct c_vert **c_verts = (struct c_vert **)bu_calloc(num_input_pnts, sizeof(struct c_vert *), "output array");
    if (spsr_opts) {
	if (spsr_opts->depth) opts.depth = spsr_opts->depth;
	if (spsr_opts->point_weight > 0) opts.pointweight = spsr_opts->point_weight;
	if (spsr_opts->samples_per_node > 0) opts.samples_per_node = spsr_opts->samples_per_node;
    }
    for (i = 0; i < num_input_pnts; i++) {
	struct c_vert *v;
	BU_GET(v, struct c_vert);
	VMOVE(v->p, input_points_3d[i]);
	VMOVE(v->n, input_normals_3d[i]);
	v->is_set = 1;
	c_verts[i] = v;
    }
    (void)spsr_surface_build(faces, num_faces, (double **)points, num_pnts, (const struct cvertex **)c_verts, num_input_pnts, &opts);
    for (i = 0; i < num_input_pnts; i++) {
	BU_PUT(c_verts[i], struct c_vert);
    }
    bu_free((void *)c_verts, "vert array");
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

