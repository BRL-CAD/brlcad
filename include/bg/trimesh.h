/*                      T R I M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup bg_trimesh */
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
 * Check if a mesh is topologically solid. True if the mesh is closed,
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

/**
 * @brief
 * Calculate an axis aligned bounding box (RPP) for a triangle mesh.
 *
 * NOTE:  This routine bounds only those points that are active in the triangle
 * mesh, not all points present in the supplied points array.
 *
 * @param[out] min XYZ coordinate defining the minimum bbox point
 * @param[out] max XYZ coordinate defining the maximum bbox point
 * @param[in] faces array of trimesh faces
 * @param[in] num_faces size of faces array
 * @param[in] p array that holds the points defining the trimesh
 * @param[in] num_pnts size of pnts array
 */
BG_EXPORT extern int
bg_trimesh_aabb(point_t *min, point_t *max, const int *faces, size_t num_faces, const point_t *p, size_t num_pnts);



/* Structure holding user-adjustable decimation settings */
struct bg_trimesh_decimation_settings {
    int method;            // Select decimation method to use
    fastf_t feature_size;  // Smallest feature size (mm) to leave undecimated
    fastf_t max_runtime;   // If the decimation takes more than max_runtime seconds, abort
    size_t max_threads;    // Don't use more than max_threads when processing.
};
#define BG_TRIMESH_DECIMATION_METHOD_DEFAULT 0
#define BG_TRIMESH_DECIMATION_SETTINGS_INIT {BG_TRIMESH_DECIMATION_METHOD_DEFAULT, 0.0, 0.0, 0}

/**
 * @brief
 * Decimate a mesh and return the decimated faces.
 *
 * @param[out] ofaces faces array for the new output mesh
 * @param[out] n_ofaces length of ofaces array
 * @param[in] ifaces array of input trimesh
 * @param[in] n_ifaces size of input faces array
 * @param[in] p array that holds the points defining the trimesh
 * @param[in] n_p size of points array
 * @param[in] s decimation settings
 *
 * NOTE: This routine will not produce a points array that includes only the
 * points used in the decimated mesh - to generate that output, use the
 * bg_trimesh_3d_gc routine with the ofaces set produced by this function.
 *
 * @return -1 if error, 0 if successful */
BG_EXPORT extern int bg_trimesh_decimate(int **ofaces, int *n_ofaces,
    int *ifaces, int n_ifaces, point_t *p, int n_p, struct bg_trimesh_decimation_settings *s);


/* Make an attempt at a trimesh intersection calculator that returns the sets
 * of faces intersecting and inside the other for each mesh. Doesn't attempt
 * a boolean evaluation, just characterizes faces */
BG_EXPORT extern int
bg_trimesh_isect(
    int **faces_inside_1, int *num_faces_inside_1, int **faces_inside_2, int *num_faces_inside_2,
    int **faces_isect_1, int *num_faces_isect_1, int **faces_isect_2, int *num_faces_isect_2,
    int *faces_1, int num_faces_1, point_t *vertices_1, int num_vertices_1,
    int *faces_2, int num_faces_2, point_t *vertices_2, int num_vertices_2);

/**
 * @brief
 * Compute vertex normals for a mesh based on the connected faces.
 *
 * @param[out] onorms array of normals - will have the same length as the input points array
 * @param[in] ifaces array of input trimesh
 * @param[in] n_ifaces size of input faces array
 * @param[in] p array that holds the points defining the trimesh
 * @param[in] n_p size of points array
 *
 * NOTE: Any vertex point not used by the triangles in the trimesh will have a
 * zero normal in the onorms array.  This routine does not repack the data to
 * eliminate unused vertices - for that use bg_trimesh_optimize
 *
 * @return -1 if error, 0 if successful */
BG_EXPORT extern int bg_trimesh_normals(vect_t **onorms, int *ifaces, int n_ifaces, point_t *p, int n_p);


/* Various additional mesh optimization steps that can be enabled
 * NOTE:  If we want to look at exposing the capabilities of something like
 * https://github.com/zeux/meshoptimizer this would be the place to start... */
struct bg_trimesh_optimization_settings {
    int collapse_degenerate;  // Remove degenerate faces
    fastf_t degenerate_edge_length;  // If near zero and collapse_degenerate is set, only collapse triangles with two or more uses of the exact same vertex
    fastf_t max_runtime;   // If the optimization takes more than max_runtime seconds, abort
    size_t max_threads;    // Don't use more than max_threads when processing.
};

#define BG_TRIMESH_OPTIMIZATION_SETTINGS_INIT {0, 0.0, 0.0, 0}

/**
 * @brief
 * Return trimesh information for a 3D mesh that contains just the date needed
 * to represent in the mesh.  Used to finalize intermediate processing meshes
 * to generate a compact mesh for export or storage.
 *
 * @param[out] ofaces faces array for the new output mesh with new indices based on opnts array.
 * @param[out] n_ofaces length of ofaces array
 * @param[out] opnts compact points array for the new output mesh.
 * @param[out] onorms (optional) compact normals array for the output mesh's points.
 * @param[out] n_opnts length of opnts array.
 * @param[in] ifaces array of input trimesh
 * @param[in] n_ifaces size of input faces array
 * @param[in] ipnts array that holds the points defining the original trimesh
 * @param[in] inorms (optional) array that holds the normals for the mesh vertices
 * @param[in] s (optional) settings to enable various additional processing steps
 *
 * @return -1 if error, number of faces in new trimesh if successful
 */
BG_EXPORT extern int bg_trimesh_optimize(
	int **ofaces, int *n_ofaces,
	point_t **opnts, vect_t **onorms, int *n_opnts,
	const int *ifaces, int n_ifaces,
	const point_t *ipnts, const vect_t *inorms,
	struct bg_trimesh_optimization_settings *s);


/**
 * @brief
 * Return trimesh information for a planar (2D) mesh that contains just the set
 * of points active in the mesh.
 *
 * @param[out] ofaces faces array for the new output mesh
 * @param[out] n_ofaces length of ofaces array
 * @param[out] opnts points array for the new output mesh
 * @param[out] n_opnts length of opnts array
 * @param[in] ifaces array of input trimesh
 * @param[in] n_ifaces size of input faces array
 * @param[in] ipnts array that holds the points defining the original trimesh
 *
 * @return -1 if error, number of faces in new trimesh if successful (should
 * match the original face count)
 */
BG_EXPORT extern int bg_trimesh_2d_gc(int **ofaces, int *n_ofaces, point2d_t **opnts, int *n_opnts,
	const int *ifaces, int n_ifaces, const point2d_t *ipnts);

/**
 * @brief
 * Return trimesh information for a 3D mesh that contains just the set
 * of points active in the mesh.
 *
 * @param[out] ofaces   faces array for the new output mesh.
 * @param[out] opnts    points array for the new output mesh.
 * @param[out] n_opnts  length of opnts array.
 * @param[in] faces     array of input trimesh
 * @param[in] num_faces size of input faces array
 * @param[in] in_pts    holds the points defining the original trimesh
 *
 * @return -1 if error, number of faces in new trimesh if successful (should
 * match the original face count)
 */
BG_EXPORT extern int bg_trimesh_3d_gc(int **ofaces, point_t **opnts, int *n_opnts,
	const int *faces, int num_faces, const point_t *in_pts);

/**
 * @brief
 * Return a face set where all topologically connected faces are oriented
 * consistently relative to their neighbors.
 *
 * @param[out] of   faces array for the new output mesh (of==f is valid).
 * @param[in]  f    input set of faces.
 * @param[in]  fcnt input face count
 *
 * @return -1 if error, otherwise return the number of times a face flipping
 * operation was performed
 */
BG_EXPORT extern int
bg_trimesh_sync(int *of, int *f, int fcnt);

/**
 * @brief
 * Return a set of face sets where all topologically connected faces are
 * grouped into common sets.
 *
 * @param[out] ofs  array of faces arrays containing the new output face sets.
 * @param[out] ofc  array of face counts for the new output face sets.
 * @param[in]  f    input set of faces.
 * @param[in]  fcnt input face count
 *
 * @return -1 if error, otherwise return the number of face sets created
 */
BG_EXPORT extern int
bg_trimesh_split(int ***ofs, int **ofc, int *f, int fcnt);

/**
 * @brief
 * Return a set of face sets where all topologically connected faces are
 * grouped into common sets.
 *
 * @param[in]  fname     plot file name
 * @param[in]  faces     face index array
 * @param[in]  num_faces number of faces
 * @param[in]  pnts      points array
 * @param[in]  num_pnts  number of points
 *
 * @return BRLCAD_ERROR if error, otherwise return BRLCAD_OK
 */
BG_EXPORT extern int
bg_trimesh_2d_plot3(const char *fname, const int *faces, size_t num_faces, const point2d_t *pnts, size_t num_pnts);

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
