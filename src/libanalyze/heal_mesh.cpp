/*                   H E A L _ M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file heal_mesh.cpp
 *
 * Interface for the heal command in the mesh healing module
 *
 */

#include "common.h"

#include <stddef.h>
#include <queue>
#include <vector>

#include "analyze.h"
#include "rt/geom.h"

#include "MeshHealing/MeshConversion_brlcad.h"
#include "MeshHealing/Stitch.h"
#include "MeshHealing/Zipper.h"


void
analyze_heal_bot(struct rt_bot_internal *bot, double zipper_tol)
{
    PolygonalMesh *polymesh = new BrlcadMesh(bot);

    /* Zippering the gaps */

    polymesh->is_edge_checked.assign(polymesh->getNumEdges(), false);

    DCEL_Edge *start;
    do {
	std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ;
	start = polymesh->findFreeEdgeChain();

	PQ = initPriorityQueue(polymesh, start, zipper_tol);
	zipperGaps(polymesh, zipper_tol, PQ);
	/*break;*/
    }
    while (start != NULL);

    /* For testing purposes - creating the bot from scratch */
    polymesh->setVertices();
    polymesh->setFaces();


    /*Stitching bigger defects

    polymesh->is_edge_checked.assign(polymesh->getNumEdges(), false);

    DCEL_Edge *closest_edge = NULL;

    while (start != NULL) {
	start = polymesh->findFreeEdgeChain();
	if (start == NULL)
	    break;

	closest_edge = polymesh->findClosestEdge(start->origin);

	if (!polymesh->isEdgeOnChain(closest_edge, start)) {
	    stitchGaps(polymesh, start, closest_edge, stitch_tol);
	}
    }*/
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
