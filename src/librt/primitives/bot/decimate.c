/*                      D E C I M A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2021 United States Government as represented by
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

#include "bu.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"

#if !defined(BRLCAD_DISABLE_GCT)
#  include "./gct_decimation/meshdecimation.h"
#  include "./gct_decimation/meshoptimizer.h"
#endif

#include "./bot_edge.h"


/* for simplicity, only consider vertices that are shared with less
 * than MAX_AFFECTED_FACES when decimating using the non gct method.
 */
#define MAX_AFFECTED_FACES 128


HIDDEN void
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
HIDDEN int
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
HIDDEN int
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
 * decimate a BOT using the new GCT decimator.
 * `feature_size` is the smallest feature size to keep undecimated.
 * returns the number of edges removed.
 */
size_t
#if defined(BRLCAD_DISABLE_GCT)
rt_bot_decimate_gct(struct rt_bot_internal *UNUSED(bot), fastf_t UNUSED(feature_size)) {
    bu_log("GCT decimation currently disabled - can not decimate.");
    return 0;
#else
rt_bot_decimate_gct(struct rt_bot_internal *bot, fastf_t feature_size) {

    RT_BOT_CK_MAGIC(bot);

    if (feature_size < 0.0)
	bu_bomb("invalid feature_size");

    mdOperation mdop;
    mdOperationInit(&mdop);
    mdOperationData(&mdop, bot->num_vertices, bot->vertices,
		    sizeof(bot->vertices[0]), 3 * sizeof(bot->vertices[0]), bot->num_faces,
		    bot->faces, sizeof(bot->faces[0]), 3 * sizeof(bot->faces[0]));
    mdOperationStrength(&mdop, feature_size);
    mdOperationAddAttrib(&mdop, bot->face_normals, sizeof(bot->face_normals[0]), 3,
			 3 * sizeof(bot->face_normals[0]), MD_ATTRIB_FLAGS_COMPUTE_NORMALS);
    mdMeshDecimation(&mdop, MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW);

    bot->num_vertices = mdop.vertexcount;
    bot->num_faces = mdop.tricount;

    moOptimizeMesh(bot->num_vertices, bot->num_faces,
	    bot->faces, sizeof(bot->faces[0]),
	    3 * sizeof(bot->faces[0]), 0, 0, 32, 0);

    return mdop.decimationcount;
#endif
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
	bu_free(faces, "faces");
	return -1;
    }

    bu_log("original face count = %zu, edge count = %zu\n", bot->num_faces, edge_count);
    bu_log("\tedges deleted = %zu\n", edges_deleted);
    bu_log("\tnew face_count = %zu\n", face_count);

    actual_count /= 3;

    if (face_count != actual_count) {
	bu_log("rt_bot_decimate: Face count is confused!!\n");
	bu_free(faces, "faces");
	return -2;
    }

    if (bot->faces)
	bu_free(bot->faces, "bot->faces");
    bot->faces = (int *)bu_realloc(faces, sizeof(int) * face_count * 3, "bot->faces");
    bot->num_faces = face_count;

    /* removed unused vertices */
    (void)rt_bot_condense(bot);

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
