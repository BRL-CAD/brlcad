/*                        P O L Y G O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/* @file polygon.h */
/** @addtogroup bg_polygon */
/** @{ */

/**
 *  @brief Functions for working with polygons
 */

#ifndef BG_POLYGON_H
#define BG_POLYGON_H

#include "common.h"
#include "vmath.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "bg/polygon_types.h"

__BEGIN_DECLS

/* TODO - the following are operations originally from libged - ultimately need
 * to better integrate these and the other polygon routines.  For now, trying
 * to get all the related logic in the same place so it is clearer what we do
 * and don't have, and make what we do have easier to reuse. */

/*
 * Weird upper limit from clipper ---> sqrt(2^63 -1)/2
 * Probably should be sqrt(2^63 -1)
 */
#define CLIPPER_MAX 1518500249

BG_EXPORT fastf_t
bg_find_polygon_area(
	struct bg_polygon *gpoly,
	fastf_t sf,
	matp_t model2view,
	fastf_t size
	);

BG_EXPORT int
bg_polygons_overlap(
	struct bg_polygon *polyA,
	struct bg_polygon *polyB,
	matp_t model2view,
	struct bn_tol *tol,
	fastf_t iscale
	);

/* model2view and view2model may be NULL, if the polygons are coplanar */
BG_EXPORT struct bg_polygon *
bg_clip_polygon(
	bg_clip_t op,
       	struct bg_polygon *subj,
       	struct bg_polygon *clip,
       	fastf_t sf,
       	matp_t model2view,
       	matp_t view2model
	);

/* model2view and view2model may be NULL, if the polygons are coplanar */
BG_EXPORT struct bg_polygon *
bg_clip_polygons(
	bg_clip_t op,
       	struct bg_polygons *subj,
       	struct bg_polygons *clip,
       	fastf_t sf,
       	matp_t model2view,
       	matp_t view2model
	);


BG_EXPORT void bg_polygon_free(struct bg_polygon *gpp);
BG_EXPORT void bg_polygons_free(struct bg_polygons *gpp);


/********************************
 * Operations on 2D point types *
 ********************************/

/**
 * @brief
 * Test whether a polygon is clockwise (CW) or counter clockwise (CCW)
 *
 * Determine if a set of points forming a polygon are in clockwise
 * or counter-clockwise order (see http://stackoverflow.com/a/1165943)
 *
 * @param[in] npts number of points in polygon
 * @param[in] pts array of points
 * @param[in] pt_indices index values into pts array building a convex polygon. duplicated points
 * aren't allowed.
 *
 * If pt_indices is NULL, the first npts points in pts will be checked in array order.
 *
 * @return BG_CCW if polygon is counter-clockwise
 * @return BG_CW if polygon is clockwise
 * @return 0 if the test failed
 */
BG_EXPORT extern int bg_polygon_direction(size_t npts, const point2d_t *pts, const int *pt_indices);


/**
 * @brief
 * test whether a point is inside a 2d polygon
 *
 * franklin's test for point inclusion within a polygon - see
 * https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
 * for more details and the implementation file polygon.c for license info.
 *
 * @param[in] npts number of points pts contains
 * @param[in] pts array of points, building a convex polygon. duplicated points
 * aren't allowed. the points in the array will be sorted counter-clockwise.
 * @param[in] test_pt point to test.
 *
 * @return 0 if point is outside polygon
 * @return 1 if point is inside polygon
 */
BG_EXPORT extern int bg_pnt_in_polygon(size_t npts, const point2d_t *pts, const point2d_t *test_pt);

/**
 * Triangulation is the process of finding a set of triangles that as a set cover
 * the same total surface area as a polygon.  There are many algorithms for this
 * operation, which have various trade-offs in speed and output quality.
 */
typedef enum {
    TRI_ANY = 0,
    TRI_EAR_CLIPPING,
    TRI_CONSTRAINED_DELAUNAY,
    TRI_MONOTONE,
    TRI_HERTEL_MEHLHORN,
    TRI_KEIL_SNOEYINK,
    TRI_DELAUNAY
} triangulation_t;

/**
 * @brief
 * Triangulate a 2D polygon with holes.
 *
 * The primary polygon definition must be provided as an array of
 * counter-clockwise indices to 2D points.  If interior "hole" polygons are
 * present, they must be passed in via the holes_array and their indices be
 * ordered clockwise.
 *
 * If no holes are present, caller should pass NULL for holes_array and holes_npts,
 * and 0 for nholes, or use bg_polygon_triangulate instead.
 *
 * @param[out] faces Set of faces in the triangulation, stored as integer indices to the pts.  The first three indices are the vertices of the first face, the second three define the second face, and so forth.
 * @param[out] num_faces Number of faces created
 * @param[out] out_pts  output points used by faces set. If an algorithm was selected that generates new points, this will be a new array.
 * @param[out] num_outpts number of output points, if an algorithm was selected that generates new points
 * @param[in] poly Non-hole polygon, defined as a CCW array of indices into the pts array.
 * @param[in] poly_npts Number of points in non-hole polygon
 * @param[in] holes_array Array of hole polygons, each defined as a CW array of indices into the pts array.
 * @param[in] holes_npts Array of counts of points in hole polygons
 * @param[in] nholes Number of hole polygons contained in holes_array
 * @param[in] steiner Array of Steiner points
 * @param[in] steiner_npts Number of Steiner points
 * @param[in] pts Array of points defining a polygon. Duplicated points
 * @param[in] npts Number of points pts contains
 * @param[in] type Type of triangulation 
 *
 * @return 0 if triangulation is successful
 * @return 1 if triangulation is unsuccessful
 */
BG_EXPORT extern int bg_nested_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
						   const int *poly, const size_t poly_npts,
						   const int **holes_array, const size_t *holes_npts, const size_t nholes,
						   const int *steiner, const size_t steiner_npts,
						   const point2d_t *pts, const size_t npts, triangulation_t type);

/**
 * @brief
 * Triangulate a 2D polygon without holes.
 *
 * The polygon cannot have holes and must be provided as an array of
 * counter-clockwise 2D points.
 *
 * No points are added as part of this triangulation process - the result uses
 * only those points in the original polygon, and hence only the face
 * information is created as output.
 *
 * The same fundamental routines are used here as in the bg_nested_polygon_triangulate
 * logic - this is a convenience function to simplify calling the routine when
 * specification of hole polygons is not needed.
 *
 * @param[out] faces Set of faces in the triangulation, stored as integer indices to the pts.  The first three indices are the vertices of the first face, the second three define the second face, and so forth.
 * @param[out] num_faces Number of faces created
 * @param[out] out_pts output points used by faces set, if an algorithm was selected that generates new points
 * @param[out] num_outpts number of output points, if an algorithm was selected that generates new points
 * @param[in] steiner Array of Steiner points
 * @param[in] steiner_npts Number of Steiner points
 * @param[in] pts Array of points defining a polygon. Duplicated points
 * @param[in] npts Number of points pts contains
 * @param[in] type Triangulation type
 *
 * @return 0 if triangulation is successful
 * @return 1 if triangulation is unsuccessful
 */
BG_EXPORT extern int bg_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
				   	    const int *steiner, const size_t steiner_npts,
					    const point2d_t *pts, const size_t npts, triangulation_t type);



/*********************************************************
  Operations on 3D point types - these are assumed to be
  polygons embedded in 3D planes in space
*********************************************************/

/**
 * @brief
 * Calculate the interior area of a polygon in a 3D plane in space.
 *
 * If npts > 4, Greens Theorem is used. The polygon mustn't
 * be self-intersecting.
 *
 * @param[out] area The interior area of the polygon
 * @param[in] npts Number of point_ts, stored in pts
 * @param[in] pts All points of the polygon, sorted counter-clockwise.
 * The array mustn't contain duplicated or non-coplanar points.
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one parameter is a NULL-pointer
 */
BG_EXPORT extern int bg_3d_polygon_area(fastf_t *area, size_t npts, const point_t *pts);


/**
 * @brief
 * Calculate the centroid of a non self-intersecting polygon in a 3D plane in space.
 *
 * @param[out] cent The centroid of the polygon
 * @param[in] npts Number of point_ts, stored in pts
 * @param[in] pts all points of the polygon, sorted counter-clockwise.
 * The array mustn't contain duplicated points or non-coplanar points.
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one in-parameter is a NULL-pointer
 */
BG_EXPORT extern int bg_3d_polygon_centroid(point_t *cent, size_t npts, const point_t *pts);


/**
 * @brief
 * Sort an array of point_ts, building a convex polygon, counter-clockwise
 *
 *@param[in] npts Number of points, pts contains
 *@param pts Array of point_ts, building a convex polygon. Duplicated points
 *aren't allowed. The points in the array will be sorted counter-clockwise.
 *@param[in] cmp Plane equation of the polygon
 *
 *@return 0 if calculation was successful
 *@return 1 if calculation failed, e.g. because pts is a NULL-pointer
 */
BG_EXPORT extern int bg_3d_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp);


/**
 * @brief
 * Calculate for an array of plane_eqs, which build a polyhedron, the
 * point_t's for each face.
 *
 * @param[out] npts Array, which stores for every face the number of
 * point_ts, added to pts. Needs to be allocated with npts[neqs] already.
 * @param[out] pts 2D-array which stores the point_ts for every
 * face. The array needs to be allocated with pts[neqs][neqs-1] already.
 * @param[in] neqs Number of plane_ts, stored in eqs
 * @param[in] eqs Array, that contains the plane equations, which
 * build the polyhedron
 *
 * @return 0 if calculation was successful
 * @return 1 if calculation failed, e.g. because one parameter is a NULL-Pointer
 */
BG_EXPORT extern int bg_3d_polygon_make_pnts_planes(size_t *npts, point_t **pts, size_t neqs, const plane_t *eqs);



/* Debugging functions - do not use */
BG_EXPORT extern void bg_polygon_plot_2d(const char *filename, const point2d_t *pnts, int npnts, int r, int g, int b);
BG_EXPORT extern void bg_polygon_plot(const char *filename, const point_t *pnts, int npnts, int r, int g, int b);
BG_EXPORT extern void bg_tri_plot_2d(const char *filename, const int *faces, int num_faces, const point2d_t *pnts, int r, int g, int b);

__END_DECLS

#endif  /* BG_POLYGON_H */
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
