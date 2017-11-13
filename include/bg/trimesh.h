/*                      T R I M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file trimesh.h */
/** @addtogroup trimesh */
/** @{ */

/**
 * @brief
 * Algorithms related to 3D meshes built from triangles.
 */

#ifndef BG_TRIMESH_H
#define BG_TRIMESH_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* every pair of contiguous elements is the start and end vertex index of an edge */
struct bg_trimesh_edges {
    int count;
    int *edges;
};

struct bg_trimesh_faces {
    int count;
    int *faces;
};

struct bg_trimesh_solid_errors {
    struct bg_trimesh_faces degenerate;
    struct bg_trimesh_edges unmatched;
    struct bg_trimesh_edges excess;
    struct bg_trimesh_edges misoriented;
};

#define BG_TRIMESH_EDGES_INIT_NULL {0, NULL}
#define BG_TRIMESH_FACES_INIT_NULL {0, NULL}
#define BG_TRIMESH_SOLDID_ERRORS_INIT_NULL {BG_TRIMESH_FACES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL}

BG_EXPORT extern void bg_free_trimesh_edges(struct bg_trimesh_edges *edges);
BG_EXPORT extern void bg_free_trimesh_faces(struct bg_trimesh_faces *faces);
BG_EXPORT extern void bg_free_trimesh_solid_errors(struct bg_trimesh_solid_errors *errors);

BG_EXPORT extern int bg_trimesh_closed_fan(size_t vcnt, size_t fcnt, fastf_t *v, int *f);
BG_EXPORT extern int bg_trimesh_orientable(size_t vcnt, size_t fcnt, fastf_t *v, int *f);
BG_EXPORT extern int bg_trimesh_solid(size_t vcnt, size_t fcnt, fastf_t *v, int *f, int **bedges);
BG_EXPORT extern int bg_trimesh_solid2(size_t vcnt, size_t fcnt, fastf_t *v, int *f, struct bg_trimesh_solid_errors *errors);

__END_DECLS

#endif  /* BG_TRIMESH_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
