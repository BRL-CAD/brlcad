/*                        S T I T C H . H
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
/** @file Stitch.h
 *
 * Definitions of the functions for the stitching module
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_STITCH_H_
#define SRC_LIBANALYZE_MESHHEALING_STITCH_H_

#include "./MeshConversion.h"
/*#include "./MeshConversion_brlcad.h"*/


/* Function to stitch gaps of chain start_A to end_A with start_B to end_B
 * Advancing rule is that whichever chain has a triangle with the other other chain with the lesser perimeter,
 * Will have it's edge pointer advanced
 */
void stitchGaps(PolygonalMesh *polymesh, DCEL_Edge *start_A, DCEL_Edge *start_B, double tolerance);

/* Function to add a triangle comprising the vertices first_vertex_A and second_vertex_A from the chain A, and
 * first_vertex_B from the chain B
 * This function will add four half edges between the two mesh chains, and
 * Set references for the existing edge records
 */
void addFace(PolygonalMesh *polymesh, DCEL_Vertex *first_vertex_A, DCEL_Vertex *second_vertex_A, DCEL_Vertex *first_vertex_B, \
	DCEL_Edge *prev_A, DCEL_Edge *next_A, DCEL_Edge *prev_B, DCEL_Edge *next_B, double tolerance);

#endif /* SRC_LIBANALYZE_MESHHEALING_STITCH_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
