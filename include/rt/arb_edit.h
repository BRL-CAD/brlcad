/*                     A R B _ E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @addtogroup arb_edit */
/** @{ */
/** @file arb_edit.h
 *
 * @brief
 * Editing operations for arb primitives.
 *
 */

#ifndef ARB_EDIT_H
#define ARB_EDIT_H

#include "common.h"
#include "bn.h"
#include "db.h"
#include "rtgeom.h"

__BEGIN_DECLS

#ifndef RT_EXPORT
#  if defined(RT_DLL_EXPORTS) && defined(RT_DLL_IMPORTS)
#    error "Only RT_DLL_EXPORTS or RT_DLL_IMPORTS can be defined, not both."
#  elif defined(RT_DLL_EXPORTS)
#    define RT_EXPORT __declspec(dllexport)
#  elif defined(RT_DLL_IMPORTS)
#    define RT_EXPORT __declspec(dllimport)
#  else
#    define RT_EXPORT
#  endif
#endif

/**
 * face definitions for each arb type
 *
 * row 1: ARB4
 * row 2: ARB5
 * row 3: ARB6
 * row 4: ARB7
 * row 5: ARB8
 *
 * To use this array, define it as follows:
 *
 * const int arb_faces[5][24] = rt_arb_faces;
 *
 */
#define rt_arb_faces { \
    {0,1,2,3, 0,1,4,5, 1,2,4,5, 0,2,4,5, -1,-1,-1,-1, -1,-1,-1,-1}, \
    {0,1,2,3, 4,0,1,5, 4,1,2,5, 4,2,3,5, 4,3,0,5, -1,-1,-1,-1}, \
    {0,1,2,3, 1,2,6,4, 0,4,6,3, 4,1,0,5, 6,2,3,7, -1,-1,-1,-1}, \
    {0,1,2,3, 4,5,6,7, 0,3,4,7, 1,2,6,5, 0,1,5,4, 3,2,6,4}, \
    {0,1,2,3, 4,5,6,7, 0,4,7,3, 1,2,6,5, 0,1,5,4, 3,2,6,7}, \
}

/* The following arb editing arrays generally contain the following:
 *
 *	location 	comments
 *-------------------------------------------------------------------
 *	0,1		edge end points
 * 	2,3		bounding planes 1 and 2
 *	4, 5,6,7	plane 1 to recalculate, using next 3 points
 *	8, 9,10,11	plane 2 to recalculate, using next 3 points
 *	12, 13,14,15	plane 3 to recalculate, using next 3 points
 *	16,17		points (vertices) to recalculate
 *
 * Each line is repeated for each edge (or point) to move.
 * See g_arb.c for more details.
 */

/**
 * edit array for arb8's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 26
 * row  7: edge 56
 * row  8: edge 67
 * row  9: edge 78
 * row 10: edge 58
 * row 11: edge 37
 * row 12: edge 48
 *
 * To use this array, define it as follows:
 *
 * const short earb8[12][18] = earb8_edit_array;
 */
#define earb8_edit_array { \
    {0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5}, \
    {1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6}, \
    {2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,7}, \
    {0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,7}, \
    {0,4, 0,1, 2,0,4,3, 4,0,1,4, -1,0,0,0, 7,5}, \
    {1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6}, \
    {4,5, 2,3, 4,0,5,4, 1,4,5,6, -1,0,0,0, 1,7}, \
    {5,6, 4,5, 3,1,5,6, 1,4,5,6, -1,0,0,0, 2,7}, \
    {6,7, 3,2, 5,2,7,6, 1,4,6,7, -1,0,0,0, 3,4}, \
    {4,7, 4,5, 2,0,7,4, 1,4,5,7, -1,0,0,0, 3,6}, \
    {2,6, 0,1, 3,1,2,6, 5,2,3,6, -1,0,0,0, 5,7}, \
    {3,7, 0,1, 2,0,3,7, 5,2,3,7, -1,0,0,0, 4,6}, \
}

/**
 * edge/vertex mapping for arb8's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 26
 * row  7: edge 56
 * row  8: edge 67
 * row  9: edge 78
 * row 10: edge 58
 * row 11: edge 37
 * row 12: edge 48
 *
 * To use this mapping , define it as follows:
 * const short arb8_evm[12][2] = arb8_edge_vertex_mapping;
 */
#define arb8_edge_vertex_mapping { \
    {0,1}, \
    {1,2}, \
    {2,3}, \
    {0,3}, \
    {0,4}, \
    {1,5}, \
    {4,5}, \
    {5,6}, \
    {6,7}, \
    {4,7}, \
    {2,6}, \
    {3,7}, \
}

/**
 * edit array for arb7's

 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 41
 * row  5: edge 15
 * row  6: edge 26
 * row  7: edge 56
 * row  8: edge 67
 * row  9: edge 37
 * row 10: edge 57
 * row 11: edge 45
 * row 12: point 5
 *
 * To use this array, define it as follows:
 *
 * const short earb7[12][18] = earb7_edit_array;
 */
#define earb7_edit_array { \
    {0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5}, \
    {1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6}, \
    {2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,4}, \
    {0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,-1}, \
    {0,4, 0,5, 4,0,5,4, 2,0,3,4, 1,4,5,6, 1,-1}, \
    {1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6}, \
    {4,5, 5,3, 2,0,3,4, 4,0,5,4, 1,4,5,6, 1,-1}, \
    {5,6, 4,5, 3,1,6,5, 1,4,5,6, -1,0,0,0, 2, -1}, \
    {2,6, 0,1, 5,2,3,6, 3,1,2,6, -1,0,0,0, 4,5}, \
    {4,6, 4,3, 2,0,3,4, 5,3,4,6, 1,4,5,6, 2,-1}, \
    {3,4, 0,1, 4,0,1,4, 2,0,3,4, 5,2,3,4, 5,6}, \
    {-1,-1, -1,-1, 5,2,3,4, 4,0,1,4, 8,2,1,-1, 6,5}, \
}

/**
 * edge/vertex mapping for arb7's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 41
 * row  5: edge 15
 * row  6: edge 26
 * row  7: edge 56
 * row  8: edge 67
 * row  9: edge 37
 * row 10: edge 57
 * row 11: edge 45
 * row 12: point 5
 *
 * To use this mapping , define it as follows:
 * const short arb7_evm[12][2] = arb7_edge_vertex_mapping;
 */
#define arb7_edge_vertex_mapping { \
    {0,1}, \
    {1,2}, \
    {2,3}, \
    {0,3}, \
    {0,4}, \
    {1,5}, \
    {4,5}, \
    {5,6}, \
    {2,6}, \
    {4,6}, \
    {3,4}, \
    {4,4}, \
}

/**
 * edit array for arb6's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 25
 * row  7: edge 36
 * row  8: edge 46
 * row  9: point 5
 * row 10: point 6
 *
 * To use this array, define it as follows:
 *
 * const short earb6[10][18] = earb6_edit_array;
 */
#define earb6_edit_array { \
    {0,1, 2,1, 3,0,1,4, 0,0,1,2, -1,0,0,0, 3,-1}, \
    {1,2, 3,4, 1,1,2,5, 0,0,1,2, -1,0,0,0, 3,4}, \
    {2,3, 1,2, 4,2,3,5, 0,0,2,3, -1,0,0,0, 1,-1}, \
    {0,3, 3,4, 2,0,3,5, 0,0,1,3, -1,0,0,0, 4,2}, \
    {0,4, 0,1, 3,0,1,4, 2,0,3,4, -1,0,0,0, 6,-1}, \
    {1,4, 0,2, 3,0,1,4, 1,1,2,4, -1,0,0,0, 6,-1}, \
    {2,6, 0,2, 4,6,2,3, 1,1,2,6, -1,0,0,0, 4,-1}, \
    {3,6, 0,1, 4,6,2,3, 2,0,3,6, -1,0,0,0, 4,-1}, \
    {-1,-1, -1,-1, 2,0,3,4, 1,1,2,4, 3,0,1,4, 6,-1}, \
    {-1,-1, -1,-1, 2,0,3,6, 1,1,2,6, 4,2,3,6, 4,-1}, \
}

/**
 * edge/vertex mapping for arb6's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 25
 * row  7: edge 36
 * row  8: edge 46
 * row  9: point 5
 * row 10: point 6
 *
 * To use this mapping , define it as follows:
 *
 * const short arb6_evm[10][2] = arb6_edge_vertex_mapping;
 */
#define arb6_edge_vertex_mapping { \
    {0,1}, \
    {1,2}, \
    {2,3}, \
    {0,3}, \
    {0,4}, \
    {1,4}, \
    {2,5}, \
    {3,5}, \
    {4,4}, \
    {7,7}, \
}

/**
 * edit array for arb5's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 25
 * row  7: edge 35
 * row  8: edge 45
 * row  9: point 5
 *
 * To use this array, define it as follows:
 *
 * const short earb5[9][18] = earb5_edit_array;
 */
#define earb5_edit_array { \
    {0,1, 4,2, 0,0,1,2, 1,0,1,4, -1,0,0,0, 3,-1}, \
    {1,2, 1,3, 0,0,1,2, 2,1,2,4, -1,0,0,0, 3,-1}, \
    {2,3, 2,4, 0,0,2,3, 3,2,3,4, -1,0,0,0, 1,-1}, \
    {0,3, 1,3, 0,0,1,3, 4,0,3,4, -1,0,0,0, 2,-1}, \
    {0,4, 0,2, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {1,4, 0,3, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {2,4, 0,4, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {3,4, 0,1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
}

/**
 * edge/vertex mapping for arb5's
 *
 * row  1: edge 12
 * row  2: edge 23
 * row  3: edge 34
 * row  4: edge 14
 * row  5: edge 15
 * row  6: edge 25
 * row  7: edge 35
 * row  8: edge 45
 * row  9: point 5
 *
 * To use this mapping , define it as follows:
 *
 * const short arb5_evm[9][2] = arb5_edge_vertex_mapping;
 */
#define arb5_edge_vertex_mapping { \
    {0,1}, \
    {1,2}, \
    {2,3}, \
    {0,3}, \
    {0,4}, \
    {1,4}, \
    {2,4}, \
    {3,4}, \
    {4,4}, \
}

/**
 * edit array for arb4's
 *
 * row  1: point 1
 * row  2: point 2
 * row  3: point 3
 * row  4: dummy
 * row  5: point 4
 *
 * To use this array, define it as follows:
 *
 * const short earb4[5][18] = earb4_edit_array;
 */
#define earb4_edit_array { \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, \
}

/**
 * edge/vertex mapping for arb4's
 *
 * row  1: point 1
 * row  2: point 2
 * row  3: point 3
 * row  4: dummy
 * row  5: point 4
 *
 * To use this mapping , define it as follows:
 *
 * const short arb4_evm[5][2] = arb4_edge_vertex_mapping;
 */
#define arb4_edge_vertex_mapping { \
    {0,0}, \
    {1,1}, \
    {2,2}, \
    {3,3}, \
    {4,4}, \
}

/**
 * EXT4TO6():   extrudes face pt1 pt2 pt3 of an ARB4 "distance"
 * to produce ARB6
 */
RT_EXPORT extern void
ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb, fastf_t peqn[7][4]);


/* MV_EDGE: Moves an arb edge (end1, end2) with bounding planes bp1
 * and bp2 through point "thru".  The edge has (non-unit) slope "dir".
 * Note that the fact that the normals here point in rather than out
 * makes no difference for computing the correct intercepts.  After
 * the intercepts are found, they should be checked against the other
 * faces to make sure that they are always "inside".
 */
RT_EXPORT extern int
mv_edge(struct rt_arb_internal *arb,
	const vect_t thru,
	const int bp1, const int bp2,
	const int end1, const int end2,
	const vect_t dir,
	const struct bn_tol *tol,
       	fastf_t peqn[7][4]);

/* Extrude an arb face */
RT_EXPORT extern int
arb_extrude(struct rt_arb_internal *arb,
	int face, fastf_t dist,
	const struct bn_tol *tol,
	fastf_t peqn[7][4]);


/* Permute the vertex labels of an ARB
 *
 *	     Minimum and maximum tuple lengths
 *     ------------------------------------------------
 *	Solid	# vertices needed	# vertices
 *	type	 to disambiguate	in THE face
 *     ------------------------------------------------
 *	ARB4		3		    3
 *	ARB5		2		    4
 *	ARB6		2		    4
 *	ARB7		1		    4
 *	ARB8		3		    4
 *     ------------------------------------------------
 */
RT_EXPORT extern int
arb_permute(struct rt_arb_internal *arb, const char *encoded_permutation, const struct bn_tol *tol);


/* Mirror an arb face about the x, y or z axis */
RT_EXPORT extern int
arb_mirror_face_axis(struct rt_arb_internal *arb, fastf_t peqn[7][4], const int face, const char *axis, const struct bn_tol *tol);

/* An ARB edge is moved by finding the direction of the line
 * containing the edge and the 2 "bounding" planes.  The new edge is
 * found by intersecting the new line location with the bounding
 * planes.  The two "new" planes thus defined are calculated and the
 * affected points are calculated by intersecting planes.  This keeps 
 * ALL faces planar.
 */
RT_EXPORT extern int
arb_edit(struct rt_arb_internal *arb, fastf_t peqn[7][4], int edge, int newedge, vect_t pos_model, const struct bn_tol *tol);


__END_DECLS

#endif  /* ARB_EDIT_H */

/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
