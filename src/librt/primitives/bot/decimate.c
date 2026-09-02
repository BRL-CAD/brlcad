/*                      D E C I M A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2026 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/bot/decimate.c
 *
 * Reduce the number of triangles in a BoT mesh
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>

#include "bu.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"

#include "mmesh/meshdecimation.h"
#include "./bot_edge.h"
#include "./decimate_private.h"


/* for simplicity, only consider vertices that are shared with less
 * than MAX_AFFECTED_FACES when decimating using the non gct method.
 */
#define MAX_AFFECTED_FACES 128


static void
delete_edge(int v1, int v2, struct bot_edge **edges)
{
    struct bot_edge *edg, *prev=NULL;

    if (v1 < v2) {
	edg = edges[v1];
	while (edg) {
	    if (edg->v == v2) {
		edg->use_count--;
		if (edg->use_count < 1) {
		    if (prev) {
			prev->next = edg->next;
		    } else {
			edges[v1] = edg->next;
		    }
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot_edge");
		    return;
		}
	    }
	    prev = edg;
	    edg = edg->next;
	}
    } else {
	edg = edges[v2];
	while (edg) {
	    if (edg->v == v1) {
		edg->use_count--;
		if (edg->use_count < 1) {
		    if (prev) {
			prev->next = edg->next;
		    } else {
			edges[v2] = edg->next;
		    }
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot_edge");
		    return;
		}
	    }
	    prev = edg;
	    edg = edg->next;
	}
    }
}


/**
 * Routine to perform the actual edge decimation step The edge from v1
 * to v2 is eliminated by moving v1 to v2.  Faces that used this edge
 * are eliminated.  Faces that used v1 will have that reference
 * changed to v2.
 */
static int
decimate_edge(int v1, int v2, struct bot_edge **edges, size_t num_edges, int *faces, size_t num_faces, int face_del1, int face_del2)
{
    size_t i;
    struct bot_edge *edg;

    /* first eliminate all the edges of the two deleted faces from the edge list */
    delete_edge(faces[face_del1 * 3 + 0], faces[face_del1 * 3 + 1], edges);
    delete_edge(faces[face_del1 * 3 + 1], faces[face_del1 * 3 + 2], edges);
    delete_edge(faces[face_del1 * 3 + 2], faces[face_del1 * 3 + 0], edges);
    delete_edge(faces[face_del2 * 3 + 0], faces[face_del2 * 3 + 1], edges);
    delete_edge(faces[face_del2 * 3 + 1], faces[face_del2 * 3 + 2], edges);
    delete_edge(faces[face_del2 * 3 + 2], faces[face_del2 * 3 + 0], edges);

    /* do the decimation */
    for (i = 0; i < 3; i++) {
	faces[face_del1*3 + i] = -1;
	faces[face_del2*3 + i] = -1;
    }
    for (i = 0; i < num_faces * 3; i++) {
	if (faces[i] == v1) {
	    faces[i] = v2;
	}
    }

    /* update the edge list; now move all the remaining edges at
     * edges[v1] to somewhere else.
     */
    edg = edges[v1];
    while (edg) {
	struct bot_edge *ptr;
	struct bot_edge *next;

	next = edg->next;

	if (edg->v < v2) {
	    ptr = edges[edg->v];
	    while (ptr) {
		if (ptr->v == v2) {
		    ptr->use_count++;
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		    break;
		}
		ptr = ptr->next;
	    }
	    if (!ptr) {
		edg->next = edges[edg->v];
		edges[edg->v] = edg;
		edg->v = v2;
	    }
	} else if (edg->v > v2) {
	    ptr = edges[v2];
	    while (ptr) {
		if (ptr->v == edg->v) {
		    ptr->use_count++;
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		    break;
		}
		ptr = ptr->next;
	    }
	    if (!ptr) {
		edg->next = edges[v2];
		edges[v2] = edg;
	    }
	} else {
	    edg->v = -1;
	    edg->next = NULL;
	    bu_free(edg, "bot edge");
	}

	edg = next;
    }
    edges[v1] = NULL;

    /* now change all remaining v1 references to v2 */
    for (i = 0; i < num_edges; i++) {
	struct bot_edge *next, *prev, *ptr;

	prev = NULL;
	edg = edges[i];
	/* look at edges starting from vertex #i */
	while (edg) {
	    next = edg->next;

	    if (edg->v == v1) {
		/* this one is affected */
		edg->v = v2;	/* change v1 to v2 */
		if ((size_t)v2 < i) {
		    /* disconnect this edge from list #i */
		    if (prev) {
			prev->next = next;
		    } else {
			edges[i] = next;
		    }

		    /* this edge must move to the "v2" list */
		    ptr = edges[v2];
		    while (ptr) {
			if ((size_t)ptr->v == i) {
			    /* found another occurrence of this edge
			     * increment use count
			     */
			    ptr->use_count++;

			    /* delete the original */
			    edg->v = -1;
			    edg->next = NULL;
			    bu_free(edg, "bot edge");
			    break;
			}
			ptr = ptr->next;
		    }
		    if (!ptr) {
			/* did not find another occurrence, add to list */
			edg->next = edges[v2];
			edges[v2] = edg;
		    }
		    edg = next;
		} else if ((size_t)v2 > i) {
		    /* look for other occurrences of this edge in this
		     * list if found, just increment use count
		     */
		    ptr = edges[i];
		    while (ptr) {
			if (ptr->v == v2 && ptr != edg) {
			    /* found another occurrence */
			    /* increment use count */
			    ptr->use_count++;

			    /* disconnect original from list */
			    if (prev) {
				prev->next = next;
			    } else {
				edges[i] = next;
			    }

			    /* free it */
			    edg->v = -1;
			    edg->next = NULL;
			    bu_free(edg, "bot edge");

			    break;
			}
			ptr = ptr->next;
		    }
		    if (!ptr) {
			prev = edg;
		    }
		    edg = next;
		} else {
		    /* disconnect original from list */
		    if (prev) {
			prev->next = next;
		    } else {
			edges[i] = next;
		    }

		    /* free it */
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		    // TODO - Just freed edg, so we can't use that to test in
		    // the while loop - is this our terminating case?  Not 100%
		    // sure what the correct behavior is here...
		    edg = NULL;
		}
	    } else {
		/* unaffected edge, just continue */
		edg = next;
	    }
	}
    }

    return 2;
}


/**
 * Routine to determine if the specified edge can be eliminated within
 * the given constraints:
 *
 * "faces" is the current working version of the BOT face list.
 *
 * "v1" and "v2" are the indices into the BOT vertex list, they define
 * the edge.
 *
 * "max_chord_error" is the maximum distance allowed between the old
 * surface and new.
 *
 * "max_normal_error" is actually the minimum dot product allowed
 * between old and new surface normals (cosine).
 *
 * "min_edge_length_sq" is the square of the minimum allowed edge
 * length.
 *
 * any constraint value of -1.0 means ignore this constraint
 *
 * returns 1 if edge can be eliminated without breaking constraints, 0
 * otherwise.
 */
static int
edge_can_be_decimated(struct rt_bot_internal *bot,
		      int *faces,
		      struct bot_edge **edges,
		      int v1,
		      int v2,
		      int *face_del1,
		      int *face_del2,
		      fastf_t max_chord_error,
		      fastf_t max_normal_error,
		      fastf_t min_edge_length_sq)
{
    size_t i, j, k;
    size_t num_faces = bot->num_faces;
    size_t num_edges = bot->num_vertices;
    size_t count, v1_count;
    size_t affected_count = 0;
    vect_t v01, v02, v12;
    fastf_t *vertices = bot->vertices;
    size_t faces_affected[MAX_AFFECTED_FACES];

    if (v1 == -1 || v2 == -1) {
	return 0;
    }

    /* find faces to be deleted or affected */
    *face_del1 = -1;
    *face_del2 = -1;
    for (i = 0; i < num_faces*3; i += 3) {
	count = 0;
	v1_count = 0;
	for (j = 0; j < 3; j++) {
	    k = i + j;
	    if (faces[k] == v1) {
		/* found a reference to v1, count it */
		count++;
		v1_count++;
	    } else if (faces[k] == v2) {
		/* found a reference to v2, count it */
		count++;
	    }
	}
	if (count > 1) {
	    /* this face will get deleted */
	    if (*face_del1 != -1) {
		*face_del2 = i/3;
	    } else {
		*face_del1 = i/3;
	    }
	} else if (v1_count) {
	    /* this face will be affected */
	    faces_affected[affected_count] = i;
	    affected_count++;
	    if (affected_count >= MAX_AFFECTED_FACES) {
		return 0;
	    }
	}
    }

    /* if only one face will be deleted, do not decimate this may be a
     * free edge
     */
    if (*face_del2 == -1) {
	return 0;
    }

    /* another easy test to avoid moving free edges */
    if (affected_count < 1) {
	return 0;
    }

    /* for BOTs that are expected to have free edges, do a rigorous
     * check for free edges
     */
    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_SURFACE) {
	struct bot_edge *edg;

	/* check if vertex v1 is on a free edge */
	for (i = 0; i < num_edges; i++) {
	    edg = edges[i];
	    while (edg) {
		if ((i == (size_t)v1 || edg->v == v1) && edg->use_count < 2) {
		    return 0;
		}
		edg = edg->next;
	    }
	}
    }

    /* calculate edge vector */
    VSUB2(v12, &vertices[v1*3], &vertices[v2*3]);

    if (min_edge_length_sq > SMALL_FASTF) {
	if (MAGSQ(v12) > min_edge_length_sq) {
	    return 0;
	}
    }

    if (max_chord_error + 1.0 > -SMALL_FASTF || max_normal_error + 1.0 > -SMALL_FASTF) {
	/* check if surface is within max_chord_error of vertex to be
	 * eliminated; loop through all affected faces.
	 */
	for (i = 0; i < affected_count; i++) {
	    fastf_t dist;
	    fastf_t dot;
	    plane_t pla, plb;
	    int va, vb, vc;

	    /* calculate plane of this face before and after
	     * adjustment if the normal changes too much, do not
	     * decimate
	     */

	    /* first calculate original face normal (use original BOT
	     * face list)
	     */
	    va = bot->faces[faces_affected[i]];
	    vb = bot->faces[faces_affected[i]+1];
	    vc = bot->faces[faces_affected[i]+2];
	    VSUB2(v01, &vertices[vb*3], &vertices[va*3]);
	    VSUB2(v02, &vertices[vc*3], &vertices[va*3]);
	    VCROSS(plb, v01, v02);
	    VUNITIZE(plb);
	    plb[W] = VDOT(&vertices[va*3], plb);

	    /* do the same using the working face list */
	    va = faces[faces_affected[i]];
	    vb = faces[faces_affected[i]+1];
	    vc = faces[faces_affected[i]+2];
	    /* make the proposed decimation changes */
	    if (va == v1) {
		va = v2;
	    } else if (vb == v1) {
		vb = v2;
	    } else if (vc == v1) {
		vc = v2;
	    }
	    VSUB2(v01, &vertices[vb*3], &vertices[va*3]);
	    VSUB2(v02, &vertices[vc*3], &vertices[va*3]);
	    VCROSS(pla, v01, v02);
	    VUNITIZE(pla);
	    pla[W] = VDOT(&vertices[va*3], pla);

	    /* max_normal_error is actually a minimum dot product */
	    dot = VDOT(pla, plb);
	    if (max_normal_error + 1.0 > -SMALL_FASTF && dot < max_normal_error) {
		return 0;
	    }

	    /* check the distance between this new plane and vertex
	     * v1
	     */
	    dist = fabs(DIST_PNT_PLANE(&vertices[v1*3], pla));
	    if (max_chord_error + 1.0 > -SMALL_FASTF && dist > max_chord_error) {
		return 0;
	    }
	}
    }

    return 1;
}

/**
 * decimate a BOT using the new mmesh decimator.
 * `feature_size` is the smallest feature size to keep undecimated.
 * returns the number of edges removed.
 */
struct bot_decimate_collapse_limits {
    fastf_t max_distance_sq;
};


static int
bot_decimate_adjust_collapse(void *context, double *collapse_point,
	double *vertex0, double *vertex1)
{
    /* Selecting mmesh's callback-aware solver avoids a coordinate defect in
     * its no-callback midpoint path.  Keeping each proposal near the edge
     * being collapsed also prevents an ill-conditioned quadric from creating
     * the long, narrow spikes observed in large mixed-scale meshes. */
    if (!isfinite(collapse_point[X]) || !isfinite(collapse_point[Y]) ||
	!isfinite(collapse_point[Z]))
	return 0;

    struct bot_decimate_collapse_limits *limits =
	(struct bot_decimate_collapse_limits *)context;
    vect_t edge;
    vect_t offset;
    VSUB2(edge, vertex1, vertex0);
    VSUB2(offset, collapse_point, vertex0);
    fastf_t edge_length_sq = MAGSQ(edge);
    fastf_t position = edge_length_sq > SMALL_FASTF ?
	VDOT(offset, edge) / edge_length_sq : 0.0;
    if (position < 0.0)
	position = 0.0;
    if (position > 1.0)
	position = 1.0;
    point_t closest;
    VJOIN1(closest, vertex0, position, edge);
    return DIST_PNT_PNT_SQ(collapse_point, closest) <=
	limits->max_distance_sq;
}


size_t
rt_bot_decimate_gct(struct rt_bot_internal *bot, fastf_t feature_size) {
    RT_BOT_CK_MAGIC(bot);

    if (feature_size <= 0.0 || feature_size > SQRT_MAX_FASTF ||
	!isfinite(feature_size) || !bot->num_faces || !bot->num_vertices ||
	bot->num_faces > INT_MAX ||
	bot->num_vertices > INT_MAX ||
	(bot->num_vertices && !bot->vertices) ||
	(bot->num_faces && !bot->faces)) {
	bu_log("rt_bot_decimate_gct: invalid input\n");
	return 0;
    }
    for (size_t vertex = 0; vertex < bot->num_vertices; ++vertex) {
	const fastf_t *point = &bot->vertices[vertex * 3];
	if (!isfinite(point[X]) || !isfinite(point[Y]) ||
	    !isfinite(point[Z])) {
	    bu_log("rt_bot_decimate_gct: invalid vertex %zu\n", vertex);
	    return 0;
	}
    }
    for (size_t face_vertex = 0; face_vertex < bot->num_faces * 3;
	++face_vertex) {
	int vertex = bot->faces[face_vertex];
	if (vertex < 0 || (size_t)vertex >= bot->num_vertices) {
	    bu_log("rt_bot_decimate_gct: invalid vertex index %d in face %zu\n",
		vertex, face_vertex / 3);
	    return 0;
	}
    }

    /* NOTE:  The original gct code used a feature_size -> cost threshold
     * calculation with a sensitivity of the fourth power - the new code uses a
     * sixth power calculation, which means the same feature size will produce
     * a finer mesh.  To keep closer to the original behavior, we'll try to
     * adjust feature_size accordingly.
     *
     * Implementation remark - this really should be refactored to be a libbg
     * function on trimesh data, since there is no actual dependence on
     * rt_bot_internal information beyond the raw triangle info - probably want
     * to use the new mmesh sixth power feature_size setting as-is in that
     * version, since the upstream behavior change is unlikely to be arbitrary.
     * Doing the adjustment here solely for consistency. */
    fastf_t fsize = pow(feature_size, 2.0 / 3.0) * pow(2.0, 4.0 / 3.0);

    size_t vertex_bytes = bot->num_vertices * 3 * sizeof(fastf_t);
    fastf_t *working_vertices = (fastf_t *)bu_malloc(vertex_bytes,
	"GCT working vertices");
    memcpy(working_vertices, bot->vertices, vertex_bytes);
    size_t face_bytes = bot->num_faces * 3 * sizeof(int);
    int *working_faces = (int *)bu_malloc(face_bytes, "GCT working faces");
    memcpy(working_faces, bot->faces, face_bytes);
    int *face_sources = (int *)bu_calloc(bot->num_faces, sizeof(int),
	"GCT face sources");
    for (size_t face = 0; face < bot->num_faces; ++face)
	face_sources[face] = (int)face;
    mdOperation mdop;
    mdOperationInit(&mdop);
    mdOperationData(&mdop, bot->num_vertices, working_vertices,
	MD_FORMAT_DOUBLE, 3*sizeof(double), bot->num_faces,
	working_faces, MD_FORMAT_INT, 3*sizeof(int));
    mdOperationTriData(&mdop, face_sources, sizeof(int), NULL, NULL, NULL);
    struct bot_decimate_collapse_limits collapse_limits = {
	feature_size * feature_size
    };
    mdOperationAdjustCollapse(&mdop, NULL, bot_decimate_adjust_collapse,
	&collapse_limits);
    mdOperationStrength(&mdop, fsize);
    int decimation_result = mdMeshDecimation(&mdop, (int)bu_avail_cpus(),
	MD_FLAGS_TRIANGLE_WINDING_CCW);
    if (!decimation_result) {
	bu_free(face_sources, "GCT face sources");
	bu_free(working_faces, "GCT working faces");
	bu_free(working_vertices, "GCT working vertices");
	return 0;
    }

    struct rt_bot_internal geometry = *bot;
    geometry.num_vertices = mdop.vertexcount;
    geometry.vertices = working_vertices;
    struct rt_bot_internal *decimated = rt_bot_gc(&geometry, working_faces,
	face_sources, mdop.tricount);
    bu_free(face_sources, "GCT face sources");
    bu_free(working_faces, "GCT working faces");
    bu_free(working_vertices, "GCT working vertices");
    if (!decimated)
	return 0;

    size_t offending_vertex = 0;
    int validation_result = rt_bot_decimation_is_within_distance(
	&offending_vertex, bot, decimated, feature_size);
    if (validation_result != 1) {
	if (validation_result == 0) {
	    const fastf_t *vertex = &decimated->vertices[offending_vertex * 3];
	    bu_log("rt_bot_decimate_gct: rejected output vertex %zu "
		"(%g, %g, %g): farther than %g mm from the input surface\n",
		offending_vertex, vertex[X], vertex[Y], vertex[Z], feature_size);
	} else {
	    bu_log("rt_bot_decimate_gct: unable to validate the output mesh\n");
	}
	rt_bot_internal_free(decimated);
	BU_PUT(decimated, struct rt_bot_internal);
	return 0;
    }

    rt_bot_internal_free(bot);
    *bot = *decimated;
    BU_PUT(decimated, struct rt_bot_internal);

    return mdop.decimationcount;
}

/**
 * routine to reduce the number of triangles in a BOT by edges
 * decimation.
 *
 * max_chord_error is the maximum error distance allowed
 * max_normal_error is the maximum change in surface normal allowed
 *
 * This and associated routines maintain a list of edges and their
 * "use counts" A "free edge" is one with a use count of 1, most edges
 * have a use count of 2 When a use count reaches zero, the edge is
 * removed from the list.  The list is used to direct the edge
 * decimation process and to avoid deforming the shape of a non-volume
 * enclosing BOT by keeping track of use counts (and thereby free
 * edges) If a free edge would be moved, that decimation is not
 * performed.
 */
int
rt_bot_decimate(struct rt_bot_internal *bot,	/* BOT to be decimated */
		fastf_t max_chord_error,	/* maximum allowable chord error (mm) */
		fastf_t max_normal_error,	/* maximum allowable normal error (degrees) */
		fastf_t min_edge_length)	/* minimum allowed edge length */
{
    int *faces = NULL;
    struct bot_edge **edges = NULL;
    fastf_t min_edge_length_sq = 0.0;
    size_t edges_deleted = 0;
    size_t edge_count = 0;
    size_t face_count = 0;
    size_t actual_count = 0;
    size_t deleted = 0;
    size_t i = 0;
    int done;
    int *face_sources = NULL;

    RT_BOT_CK_MAGIC(bot);

    /* convert normal error to something useful (a minimum dot product) */
    if (max_normal_error + 1.0 > -SMALL_FASTF) {
	max_normal_error = cos(max_normal_error * DEG2RAD);
    }

    if (min_edge_length > SMALL_FASTF) {
	min_edge_length_sq = min_edge_length * min_edge_length;
    } else {
	min_edge_length_sq = min_edge_length;
    }

    /* make a working copy of the face list */
    faces = (int *)bu_malloc(sizeof(int) * bot->num_faces * 3, "faces");
    for (i = 0; i < bot->num_faces * 3; i++) {
	faces[i] = bot->faces[i];
    }
    face_count = bot->num_faces;

    /* make a list of edges in the BOT; each edge will be in the list
     * for its lower numbered vertex index
     */
    edge_count = bot_edge_table(bot, &edges);

    /* the decimation loop */
    done = 0;
    while (!done) {
	done = 1;

	/* visit each edge */
	for (i = 0; i < bot->num_vertices; i++) {
	    struct bot_edge *ptr;
	    int face_del1, face_del2;

	    ptr = edges[i];
	    while (ptr) {

		/* try to avoid making 2D objects */
		if (face_count < 5)
		    break;

		/* check if this edge can be eliminated (try both directions) */
		if (edge_can_be_decimated(bot, faces, edges, i, ptr->v,
					  &face_del1, &face_del2,
					  max_chord_error,
					  max_normal_error,
					  min_edge_length_sq)) {
		    face_count -= decimate_edge(i, ptr->v, edges, bot->num_vertices,
						faces, bot->num_faces,
						face_del1, face_del2);
		    edges_deleted++;
		    done = 0;
		    break;
		} else if (edge_can_be_decimated(bot, faces, edges, ptr->v, i,
						 &face_del1, &face_del2,
						 max_chord_error,
						 max_normal_error,
						 min_edge_length_sq)) {
		    face_count -= decimate_edge(ptr->v, i, edges, bot->num_vertices,
						faces, bot->num_faces,
						face_del1, face_del2);
		    edges_deleted++;
		    done = 0;
		    break;
		} else {
		    ptr = ptr->next;
		}
	    }
	}
    }

    /* free some memory */
    for (i = 0; i < bot->num_vertices; i++) {
	struct bot_edge *ptr, *ptr2;

	ptr = edges[i];
	while (ptr) {
	    ptr2 = ptr;
	    ptr = ptr->next;
	    bu_free(ptr2, "ptr->edges");
	}
    }
    bu_free(edges, "edges");
    edges = NULL;

    /* condense the face list */
    face_sources = (int *)bu_calloc(face_count, sizeof(int),
	"decimated face sources");
    actual_count = 0;
    for (i = 0; i < bot->num_faces; ++i) {
	if (faces[i * 3] != -1)
	    face_sources[actual_count++] = (int)i;
    }
    if (actual_count != face_count) {
	bu_log("rt_bot_decimate: source face count is confused!!\n");
	bu_free(face_sources, "decimated face sources");
	bu_free(faces, "faces");
	return -2;
    }

    actual_count = 0;
    deleted = 0;
    for (i = 0; i < bot->num_faces * 3; i++) {
	if (faces[i] == -1) {
	    deleted++;
	    continue;
	}
	if (deleted) {
	    faces[i-deleted] = faces[i];
	}
	actual_count++;
    }

    if (actual_count % 3) {
	bu_log("rt_bot_decimate: face vertices count is not a multiple of 3!!\n");
	bu_free(face_sources, "decimated face sources");
	bu_free(faces, "faces");
	return -1;
    }

    bu_log("original face count = %zu, edge count = %zu\n", bot->num_faces, edge_count);
    bu_log("\tedges deleted = %zu\n", edges_deleted);
    bu_log("\tnew face_count = %zu\n", face_count);

    actual_count /= 3;

    if (face_count != actual_count) {
	bu_log("rt_bot_decimate: Face count is confused!!\n");
	bu_free(face_sources, "decimated face sources");
	bu_free(faces, "faces");
	return -2;
    }

    struct rt_bot_internal *decimated = rt_bot_gc(bot, faces, face_sources,
	face_count);
    bu_free(face_sources, "decimated face sources");
    bu_free(faces, "faces");
    if (!decimated)
	return -3;

    rt_bot_internal_free(bot);
    *bot = *decimated;
    BU_PUT(decimated, struct rt_bot_internal);

    return edges_deleted;
}


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
