/*                      T R I M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

struct bg_trimesh_halfedge {
    int va, vb;
    int flipped;
};

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
#define BG_TRIMESH_SOLID_ERRORS_INIT_NULL {BG_TRIMESH_FACES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL, BG_TRIMESH_EDGES_INIT_NULL}

BG_EXPORT extern void bg_free_trimesh_edges(struct bg_trimesh_edges *edges);
BG_EXPORT extern void bg_free_trimesh_faces(struct bg_trimesh_faces *faces);
BG_EXPORT extern void bg_free_trimesh_solid_errors(struct bg_trimesh_solid_errors *errors);

/**
 * Check if a mesh is topologically closed and manifold. True if for
 * every edge, there is exactly one other edge with the same two end
 * vertices.
 */
BG_EXPORT extern int bg_trimesh_manifold_closed(int vcnt, int fcnt, fastf_t *v, int *f);

/**
 * Check if a mesh is consistently oriented. True if for every edge
 * that has exactly one matching edge, the two edges have opposite
 * orientations. Note that an open mesh can be oriented, but a
 * non-manifold mesh cannot.
 */
BG_EXPORT extern int bg_trimesh_oriented(int vcnt, int fcnt, fastf_t *v, int *f);

/**
 * Check if a mesh is toplogically solid. True if the mesh is closed,
 * manifold, and oriented.
 */
BG_EXPORT extern int bg_trimesh_solid(int vcnt, int fcnt, fastf_t *v, int *f, int **bedges);

/* The below functions are for use as arguments to error tests. Given
 * a face/edge, they return true if the caller should continue
 * iterating through faces/edges, and false otherwise.
 *
 * The *_exit and *_continue functions just return false and true
 * respectively. The *_gather functions expect the data argument to
 * be a struct bg_trimesh_faces or struct bg_trimesh_edges with
 * pre-allocated members of the correct size and count members set to
 * 0, that they will populate.
 */
typedef int (*bg_face_error_func_t)(int face_idx, void *data);
typedef int (*bg_edge_error_funct_t)(struct bg_trimesh_halfedge *edge, void *data);

BG_EXPORT extern int bg_trimesh_face_exit(int face_idx, void *data);
BG_EXPORT extern int bg_trimesh_face_continue(int face_idx, void *data);
BG_EXPORT extern int bg_trimesh_face_gather(int face_idx, void *data);
BG_EXPORT extern int bg_trimesh_edge_exit(struct bg_trimesh_halfedge *edge, void *data);
BG_EXPORT extern int bg_trimesh_edge_continue(struct bg_trimesh_halfedge *edge, void *data);
BG_EXPORT extern int bg_trimesh_edge_gather(struct bg_trimesh_halfedge *edge, void *data);

/* These functions return 0 if no instances of the error are found.
 * Otherwise, they return the number of instances of the error found
 * before the error function argument returned false (at least 1).
 */
BG_EXPORT extern int bg_trimesh_degenerate_faces(int num_faces, int *fpoints, bg_face_error_func_t degenerate_func, void *data);
BG_EXPORT extern int bg_trimesh_unmatched_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data);
BG_EXPORT extern int bg_trimesh_misoriented_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data);
BG_EXPORT extern int bg_trimesh_excess_edges(int num_edges, struct bg_trimesh_halfedge *edge_list, bg_edge_error_funct_t error_edge_func, void *data);
BG_EXPORT extern int bg_trimesh_solid2(int vcnt, int fcnt, fastf_t *v, int *f, struct bg_trimesh_solid_errors *errors);
BG_EXPORT extern int bg_trimesh_hanging_nodes(int num_vertices, int num_faces, fastf_t *vertices, int *faces, struct bg_trimesh_solid_errors *errors);

BG_EXPORT extern struct bg_trimesh_halfedge * bg_trimesh_generate_edge_list(int fcnt, int *f);

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
