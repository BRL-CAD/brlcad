/*                      B O T _ E D G E . C
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

#include "common.h"

/* interface header */
#include "./bot_edge.h"

/* implementation headers */
#include "bu.h"


int
bot_edge_table(struct rt_bot_internal *bot, struct bot_edge ***edges)
{
    size_t tmp, flen, currFace;
    int currVert, nextVert, from, to;
    size_t numVertices, numEdges = 0;
    int *faces;
    struct bot_edge *edge;

    RT_BOT_CK_MAGIC(bot);

    numVertices = bot->num_vertices;
    faces = bot->faces;

    /* allocate array with one index per vertex */
    *edges = (struct bot_edge**)bu_calloc(numVertices, sizeof(struct bot_edge*), "edges");

    /* for each face */
    flen = bot->num_faces * 3;
    for (currFace = 0; currFace < flen; currFace += 3) {

	for (currVert = 0; currVert < 3; currVert++) {

	    /* curr and next vertex form an edge of curr face */

	    /* get relative indices */
	    nextVert = currVert + 1;

	    if (nextVert > 2) {
		nextVert = 0;
	    }

	    /* get actual indices */
	    from = faces[currFace + currVert];
	    to = faces[currFace + nextVert];

	    /* make sure 'from' is the lower index */
	    if (to < from) {
		tmp = from;
		from = to;
		to = tmp;
	    }

	    /* get the list for this index */
	    edge = (*edges)[from];

	    /* make new list */
	    if (edge == (struct bot_edge*)NULL) {

		BU_ALLOC(edge, struct bot_edge);
		(*edges)[from] = edge;
	    }

	    /* list already exists */
	    else {

		/* look for existing entry */
		while (edge->next && edge->v != to) {
		    edge = edge->next;
		}

		/* this edge found previously - update use count */
		if (edge->v == to) {

		    edge->use_count++;
		    continue;
		}

		/* this edge is new - append a new entry for it */
		BU_ALLOC(edge->next, struct bot_edge);
		edge = edge->next;
	    }

	    /* initialize new entry */
	    edge->v         = to;
	    edge->use_count = 1;
	    edge->next      = (struct bot_edge*)NULL;

	    numEdges++;

	} /* indices loop */
    } /* faces loop */

    return numEdges;
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
