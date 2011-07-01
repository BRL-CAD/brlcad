/*                     N M G _ T R I _ M C . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_tri_mc.c
 *
 * Triangulate the faces of a polygonal NMG using the marching cubes
 * algorithm.
 *
 *
 * Vertex and edge indices (note that it seems rotated 90 degrees from the stuff
 * you see on the sources. They used OpenGL coordinates, we use BRL-CAD.)
 *              4
 *        4-----------5
 *      8/|         9/|
 *      / |   0     / |
 *     0-----------1  |5
 *     |  |7       |  |
 *     |  |   6   1|  |
 *    3|  7--------|--6
 *     | /         | /
 *     |/11        |/10
 *     3-----------2
 *          2
 */
/** @} */

/* rough game plan
 *   [ ] develop/proof metaball primitive tesselation using MC (end of Jan10)
 *       [ ] asc-g/g-asc of metaballs for regression/comparison testing
 *           [X] asc-g
 *           [X] g-asc
 *	     [ ] regression shtuff (on hold; regress/ has issues)
 *       [X] edge solve cubes
 *       [X] write compiled table shtuff (use existing table, the hex/index one?)
 *       [X] produce NMG mesh
 * ============================================================
 *       [ ] experiment with decimating
 *   [X] hoist code where final cube edge intersections is known
 *   [ ] implement ray-firing to find edge intersects
 *   [X] hoise table stuff
 *   [X] hoist NMG mesh writer. (End of Feb10)
 * ============================================================
 *   [ ] try decimation?
 *   [ ] ???
 *   [ ] profit! (Mid Apr10)
 *   [ ] explore optimizations if time left?
 *   [ ] compare old nmg tesselator for accuracy/performance/robustness?
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"

#define MAX_INTERSECTS 1024

#define VOODOO 10010.001

/* set this to 1 for full midpoint use. Set it to 2 for x/y mid and real z. */
int marching_cubes_use_midpoint = 0;

/*
 * Table data acquired from Paul Borke's page at
 * http://local.wasp.uwa.edu.au/~pbourke/geometry/polygonise/
 *
 * Asserts it's from Cory Gene Bloyde, who has provided "public domain" code
 * including the tables. http://www.siafoo.net/snippet/100
 *
 * Grid definition matches SIGGRAPH 1987 p 164 (original presentation of technique)
 */

static int mc_tris[256][16] = {
/* 00 */  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 01 */  {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 02 */  {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 03 */  {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 04 */  {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 05 */  {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 06 */  {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 07 */  {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
/* 08 */  {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 09 */  {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 0a */  {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 0b */  {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
/* 0c */  {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 0d */  {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
/* 0e */  {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
/* 0f */  {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 10 */  {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 11 */  {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 12 */  {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 13 */  {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
/* 14 */  {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 15 */  {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
/* 16 */  {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
/* 17 */  {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
/* 18 */  {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 19 */  {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
/* 1a */  {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
/* 1b */  {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
/* 1c */  {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
/* 1d */  {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
/* 1e */  {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
/* 1f */  {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
/* 20 */  {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 21 */  {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 22 */  {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 23 */  {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
/* 24 */  {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 25 */  {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
/* 26 */  {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
/* 27 */  {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
/* 28 */  {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 29 */  {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
/* 2a */  {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
/* 2b */  {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
/* 2c */  {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
/* 2d */  {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
/* 2e */  {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
/* 2f */  {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
/* 30 */  {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 31 */  {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
/* 32 */  {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
/* 33 */  {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 34 */  {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
/* 35 */  {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
/* 36 */  {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
/* 37 */  {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
/* 38 */  {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
/* 39 */  {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
/* 3a */  {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
/* 3b */  {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
/* 3c */  {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
/* 3d */  {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
/* 3e */  {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
/* 3f */  {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 40 */  {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 41 */  {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 42 */  {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 43 */  {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
/* 44 */  {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 45 */  {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
/* 46 */  {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
/* 47 */  {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
/* 48 */  {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 49 */  {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
/* 4a */  {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
/* 4b */  {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
/* 4c */  {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
/* 4d */  {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
/* 4e */  {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
/* 4f */  {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
/* 50 */  {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 51 */  {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
/* 52 */  {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
/* 53 */  {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
/* 54 */  {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
/* 55 */  {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
/* 56 */  {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
/* 57 */  {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
/* 58 */  {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
/* 59 */  {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
/* 5a */  {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
/* 5b */  {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
/* 5c */  {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
/* 5d */  {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
/* 5e */  {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
/* 5f */  {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
/* 60 */  {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 61 */  {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
/* 62 */  {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
/* 63 */  {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
/* 64 */  {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
/* 65 */  {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
/* 66 */  {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 67 */  {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
/* 68 */  {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
/* 69 */  {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
/* 6a */  {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
/* 6b */  {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
/* 6c */  {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
/* 6d */  {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
/* 6e */  {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
/* 6f */  {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 70 */  {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
/* 71 */  {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
/* 72 */  {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
/* 73 */  {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
/* 74 */  {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
/* 75 */  {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
/* 76 */  {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
/* 77 */  {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 78 */  {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
/* 79 */  {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
/* 7a */  {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
/* 7b */  {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
/* 7c */  {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
/* 7d */  {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 7e */  {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
/* 7f */  {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 80 */  {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 81 */  {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 82 */  {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 83 */  {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
/* 84 */  {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 85 */  {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
/* 86 */  {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
/* 87 */  {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
/* 88 */  {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 89 */  {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
/* 8a */  {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
/* 8b */  {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
/* 8c */  {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
/* 8d */  {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
/* 8e */  {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
/* 8f */  {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
/* 90 */  {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 91 */  {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
/* 92 */  {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
/* 93 */  {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
/* 94 */  {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
/* 95 */  {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
/* 96 */  {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
/* 97 */  {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
/* 98 */  {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
/* 99 */  {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* 9a */  {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
/* 9b */  {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
/* 9c */  {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
/* 9d */  {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
/* 9e */  {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
/* 9f */  {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* a0 */  {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* a1 */  {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
/* a2 */  {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
/* a3 */  {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
/* a4 */  {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
/* a5 */  {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
/* a6 */  {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
/* a7 */  {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
/* a8 */  {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
/* a9 */  {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
/* aa */  {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
/* ab */  {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
/* ac */  {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
/* ad */  {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
/* ae */  {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
/* af */  {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
/* b0 */  {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
/* b1 */  {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
/* b2 */  {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
/* b3 */  {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
/* b4 */  {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
/* b5 */  {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
/* b6 */  {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
/* b7 */  {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
/* b8 */  {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
/* b9 */  {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
/* ba */  {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
/* bb */  {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* bc */  {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
/* bd */  {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
/* be */  {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* bf */  {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* c0 */  {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* c1 */  {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
/* c2 */  {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
/* c3 */  {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
/* c4 */  {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
/* c5 */  {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
/* c6 */  {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
/* c7 */  {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
/* c8 */  {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
/* c9 */  {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
/* ca */  {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
/* cb */  {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
/* cc */  {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* cd */  {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
/* ce */  {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
/* cf */  {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* d0 */  {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
/* d1 */  {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
/* d2 */  {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
/* d3 */  {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
/* d4 */  {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
/* d5 */  {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
/* d6 */  {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
/* d7 */  {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* d8 */  {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
/* d9 */  {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
/* da */  {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
/* db */  {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
/* dc */  {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
/* dd */  {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* de */  {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
/* df */  {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* e0 */  {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
/* e1 */  {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
/* e2 */  {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
/* e3 */  {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
/* e4 */  {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
/* e5 */  {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
/* e6 */  {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
/* e7 */  {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
/* e8 */  {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
/* e9 */  {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
/* ea */  {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
/* eb */  {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* ec */  {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
/* ed */  {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
/* ee */  {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* ef */  {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* f0 */  {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* f1 */  {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
/* f2 */  {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
/* f3 */  {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* f4 */  {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
/* f5 */  {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
/* f6 */  {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* f7 */  {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* f8 */  {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
/* f9 */  {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* fa */  {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
/* fb */  {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* fc */  {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* fd */  {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* fe */  {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
/* ff */  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

/* pairs of vertices associated with the edge. */
static int edge_vertex[12][2] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 0},
	{4, 5}, {5, 6}, {6, 7}, {7, 4},
	{0, 4}, {1, 5}, {2, 6}, {3, 7}};

static point_t point_offset[8] = {
	{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0},
	{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}};


/* returns the number of triangles added or -1 on failure */
int
rt_tri_mc_realize_cube(fastf_t *tris, int pv, point_t *edges)
{
    int *vi, fo;
    fastf_t *mytri = tris;

    vi = (int *)(mc_tris[pv]);

    fo = 0;
    while( *vi >= 0 ) {
	if(++fo > 5) {
	    bu_log("Whoa, too many triangles?\n");
	    return -1;
	}

	VMOVE(mytri, edges[vi[0]]);
	VMOVE(mytri+1, edges[vi[1]]);
	VMOVE(mytri+2, edges[vi[2]]);

	mytri+=3;
	vi+=3;
    }

    return fo;
}

int
nmg_mc_realize_cube(struct shell *s, int pv, point_t *edges, const struct bn_tol *tol)
{
    int *vi, fo, valids=0;
    struct faceuse *fu;
    struct vertex *vertl[3], **f_vertl[3];

    f_vertl[0] = &vertl[0];
    f_vertl[1] = &vertl[2];
    f_vertl[2] = &vertl[1];

    vi = (int *)(mc_tris[pv]);
    /*
     * vi now has the vertex list for our (possibly up to) 5 triangles as
     * offsets into "edges". If the tuple is all '-1', there is no triangle. The
     * vi array is actually 16 triangles, with an extra terminal -1.
     */

    fo = 0;
    while( *vi >= 0 ) {
	if(++fo > 5) {
	    bu_log("Whoa, too many triangles?\n");
	    return -1;
	}

	if (!bn_3pts_distinct(edges[vi[0]], edges[vi[1]], edges[vi[2]], tol) ||
		bn_3pts_collinear(edges[vi[0]], edges[vi[1]], edges[vi[2]], tol)) {
	    bu_log("Heh, throwing away a triangle %d/%d/%d (%g %g %g | %g %g %g | %g %g %g)\n",
		V3ARGS(vi), V3ARGS(edges[vi[0]]), V3ARGS(edges[vi[1]]), V3ARGS(edges[vi[2]]));
	    if(NEAR_EQUAL(edges[vi[0]][X], VOODOO, tol->dist) ||
		NEAR_EQUAL(edges[vi[1]][X], VOODOO, tol->dist) ||
		NEAR_EQUAL(edges[vi[2]][X], VOODOO, tol->dist)) {
		bu_log("Heh, VOODOO!\n");
	    }
	    vi+=3;
	    continue;
	}

	valids++;

	memset((char *)vertl, 0, sizeof(vertl));
	/* LOCK */
	bu_semaphore_acquire(RT_SEM_WORKER);

	fu = nmg_cmface(s, f_vertl, 3);

	nmg_vertex_gv(vertl[0], edges[vi[0]]);
	nmg_vertex_gv(vertl[1], edges[vi[1]]);
	nmg_vertex_gv(vertl[2], edges[vi[2]]);
	if (nmg_calc_face_g(fu))	/* this flips out and spins. */
	    nmg_kfu(fu);

	if(nmg_fu_planeeqn(fu, tol))
	    bu_log("Tiny triangle! <%g %g %g> <%g %g %g> <%g %g %g> (%g %g %g)\n",
		    V3ARGS(edges[vi[0]]), V3ARGS(edges[vi[1]]), V3ARGS(edges[vi[2]]),
		    DIST_PT_PT(edges[vi[0]],edges[vi[1]]),
		    DIST_PT_PT(edges[vi[0]],edges[vi[2]]),
		    DIST_PT_PT(edges[vi[1]],edges[vi[2]]));
	/* UNLOCK */
	bu_semaphore_release(RT_SEM_WORKER);

	vi+=3;
    }

    return valids;
}

static fastf_t bin(fastf_t val, fastf_t step) {return step*floor(val/step);}

#define INHIT 1
#define OUTHIT 2
#define NOHIT -1
struct whack {
    point_t hit;
    int in;	/* 1 for inhit, 2 for outhit, -1 to terminate */
};

static int
bangbang(struct application * a, struct partition *PartHeadp, struct seg * UNUSED(s))
{
    struct partition *pp;
    struct whack *t = (struct whack *)a->a_uptr;
    int intersects = 0;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	if(pp->pt_outhit->hit_dist>0.0) {
	    if(pp->pt_inhit->hit_dist>0.0) {
		VJOIN1(t->hit, a->a_ray.r_pt, pp->pt_inhit->hit_dist, a->a_ray.r_dir);
		t->in=INHIT;
		t++;
		intersects++;
	    }
	    VJOIN1(t->hit, a->a_ray.r_pt, pp->pt_outhit->hit_dist, a->a_ray.r_dir);
	    t->in=OUTHIT;
	    t++;
	    intersects++;
	    if(intersects >= MAX_INTERSECTS)
		bu_bomb("Too many intersects in marching cubes");
	}
    }
    t->in = NOHIT;
    return 0;
}

static int
missed(struct application *a)
{
    struct whack *t = (struct whack *)a->a_uptr;
    t->in = NOHIT;
    return 0;
}

static int
bitdiff(unsigned char t, unsigned char a, unsigned char b)
{
    unsigned char ma, mb, hb;
    ma = 1<<a;
    mb = 1<<b;
    hb = t&(ma|mb);
    return hb == ma || hb == mb;
}

static int
rt_nmg_mc_crosspew(struct application *a, int edge, point_t *p, point_t *edges, struct whack *muh, const fastf_t step, const struct bn_tol *tol)
{
    struct whack *puh;
    int i;
    fastf_t dist;

    for(i=0;i<MAX_INTERSECTS;i++) {
	muh[i].in=0;
	VSETALL(muh[i].hit,VOODOO);
    }

    VJOIN1(a->a_ray.r_pt, *p, -2*tol->dist, a->a_ray.r_dir);
    rt_shootray(a);
    puh=muh;
    while(puh->in > 0 && puh->hit[Z] <= a->a_ray.r_pt[Z]-tol->dist) {
	bu_log("%d %g isn't close enough, moving on\n", puh->in, puh->hit[Z]);
	puh++;
	if(puh->in < 1)
	    bu_log("puhh?\n");
    }
    dist = DIST_PT_PT(a->a_ray.r_pt, puh->hit);
    if(dist > (step + 2.5*tol->dist)) {
	bu_log("spooky action on edge:%d. (in:%d) (%g %g %g -> %g %g %g) step:%g dist:%g\n", edge, puh->in, V3ARGS(a->a_ray.r_pt), V3ARGS(a->a_ray.r_dir), step, dist);
	VJOIN1(edges[edge], a->a_ray.r_pt, 0.5*step+tol->dist, a->a_ray.r_dir);
    } else if(puh->in > 0)
	VMOVE(edges[edge], muh->hit);
    return 0;
}

static int
rt_nmg_mc_pew(struct shell *s, struct whack  *primp[4], struct application *a, fastf_t x, fastf_t y, fastf_t b, fastf_t step, const struct bn_tol *tol)
{
    int i, in[4] = { 0, 0, 0, 0}, count=0;
    fastf_t last_b = -VOODOO;

    while(primp[0]->in>0 || primp[1]->in>0 || primp[2]->in>0 || primp[3]->in>0) {
	unsigned char pv;
	point_t edges[12];
	struct whack muh[MAX_INTERSECTS];
	point_t p[8];

	a->a_uptr = muh;
	if((in[0]|in[1]|in[2]|in[3]) == 0) {
	    b = +INFINITY;
	    /* figure out the first hit distance and bin it */
	    for(i=0;i<4;i++)
		if(primp[i]->in>0 && primp[i]->hit[Z] < b) b = primp[i]->hit[Z];
	    b = bin(b, step);
	} else { /* iff we know we're intersecting the surface, walk slow. */
	    if(NEAR_ZERO(last_b+VOODOO, tol->dist))
		bu_log("teh fux? lastb = %g\n", last_b);
	    b = last_b + step;
	}

	for(i=0;i<8;i++)
	    VSET(p[i], x+step*point_offset[i][X], y+step*point_offset[i][Y], b+step*point_offset[i][Z]);

	/* build the point vector */
	pv = 0;
	if(in[0] && primp[0]->hit[Z] > b+step) pv |= 0x09;
	if(in[1] && primp[1]->hit[Z] > b+step) pv |= 0x06;
	if(in[2] && primp[2]->hit[Z] > b+step) pv |= 0x90;
	if(in[3] && primp[3]->hit[Z] > b+step) pv |= 0x60;

#define MEH(A,I,O) \
	if(primp[A][1].in > 0 && primp[A][1].hit[Z] < b+step+tol->dist) primp[A]+=2; \
	if(primp[A]->hit[Z] < b+step+tol->dist) {  \
	    if(primp[A]->in==1) { in[A]=1; pv |= 1<<I;} \
	    if(primp[A]->in==2) { in[A]=0; pv |= 1<<O;} \
	} else pv |= in[A]<<I | in[A]<<O;

	/*  p   t  b */
	MEH(0, 0, 3);
	MEH(1, 1, 2);
	MEH(2, 4, 7);
	MEH(3, 5, 6);
#undef MEH

#define MUH(a,l) if(bitdiff(pv,edge_vertex[a][0],edge_vertex[a][1])) { VMOVE(edges[a], l->hit); l++; } /* we already have ray intersect data for these. */
	for(i=0;i<12;i++)
	    VSETALL(edges[i], VOODOO);

	MUH(1 ,primp[1]);
	MUH(3 ,primp[0]);
	MUH(5 ,primp[3]);
	MUH(7 ,primp[2]);
#undef MUH

	if(marching_cubes_use_midpoint) {
	    if(marching_cubes_use_midpoint==1)
		for(i=1;i<8;i+=2)
		    if(bitdiff(pv,edge_vertex[i][0],edge_vertex[i][1]))
			VADD2SCALE(edges[i], p[edge_vertex[i][0]], p[edge_vertex[i][1]], 0.5);
	    for(i=0;i<7;i+=2)
		if(bitdiff(pv,edge_vertex[i][0],edge_vertex[i][1]))
		    VADD2SCALE(edges[i], p[edge_vertex[i][0]], p[edge_vertex[i][1]], 0.5);
	    for(i=8;i<12;i++)
		if(bitdiff(pv,edge_vertex[i][0],edge_vertex[i][1]))
		    VADD2SCALE(edges[i], p[edge_vertex[i][0]], p[edge_vertex[i][1]], 0.5);
	} else {
	    /* the 'muh' list may have to be walked. */
#define MEH(A,B,C) if(bitdiff(pv,B,C)) rt_nmg_mc_crosspew(a, A, p+B, edges, muh, step, tol)
	    VSET(a->a_ray.r_dir, 1, 0, 0);
	    MEH(0 ,0,1);
	    MEH(2 ,3,2);
	    MEH(4 ,4,5);
	    MEH(6 ,7,6);

	    VSET(a->a_ray.r_dir, 0, 1, 0);
	    MEH(8 ,0,4);
	    MEH(9 ,1,5);
	    MEH(10,2,6);
	    MEH(11,3,7);
#undef MEH
#define MEH(A,B,C,D) if(NEAR_EQUAL(edges[B][Z], p[A][Z], tol->dist)) { VMOVE(edges[C], p[A]); VMOVE(edges[D], p[A]); }
	    MEH(0,3,0,8);
	    MEH(1,1,0,9);
	    MEH(2,1,2,10);
	    MEH(3,3,2,11);
	    MEH(4,7,4,8);
	    MEH(5,5,4,9);
	    MEH(6,5,6,10);
	    MEH(7,7,6,11);
#undef MEH
	}

	/* stuff it into an nmg shell */
	if(pv != 0 && pv != 0xff && s)	/* && s should go away. */
	    count += nmg_mc_realize_cube(s, pv, edges, tol);

	last_b = b;
    }
    return count;
}


struct mci_s {
    struct shell *s;	/* where to put it. */
    double step;
    struct rt_i *rtip;
    const struct bn_tol *tol;
    struct resource *resources;
    fastf_t endx, endy;
    unsigned long count;
    int ncpu;
};

static void
fire_row(int cpu, void * ptr)
{
    struct mci_s *m = (struct mci_s *)ptr;
    struct application a;
    struct whack prim[4][MAX_INTERSECTS];
    struct whack *primp[4];
    fastf_t x, y, z;
    unsigned long count = 0;

    RT_APPLICATION_INIT(&a);
    a.a_rt_i = m->rtip;
    a.a_rt_i->useair = 1;
    a.a_hit = bangbang;
    a.a_miss = missed;
    a.a_onehit = MAX_INTERSECTS;
    a.a_resource = m->resources + cpu;

    x=bin(a.a_rt_i->mdl_min[X], m->step) - m->step + (m->step * cpu);

    for(; x<m->endx; x += m->step * (fastf_t)m->ncpu) {
	y=bin(a.a_rt_i->mdl_min[Y], m->step) - m->step;
	for(; y<m->endy; y+=m->step) {
	    int i, j;

	    for(i=0;i<4;i++)
		primp[i] = prim[i];

	    for(i=0;i<4;i++)
		for(j=0;j<MAX_INTERSECTS-1;j++) {
		    prim[i][j].in = 0;
		    VSETALL(prim[i][j].hit, VOODOO);
		}

	    z = bin(a.a_rt_i->mdl_min[Z] - m->tol->dist - m->step, m->step);

	    VSET(a.a_ray.r_dir, 0, 0, 1);
	    a.a_uptr = primp[0];
	    VSET(a.a_ray.r_pt, x, y, z);
	    rt_shootray(&a);

	    a.a_uptr = primp[1];
	    VSET(a.a_ray.r_pt, x+m->step, y, z);
	    rt_shootray(&a);

	    a.a_uptr = primp[2];
	    VSET(a.a_ray.r_pt, x, y+m->step, z);
	    rt_shootray(&a);

	    a.a_uptr = primp[3];
	    VSET(a.a_ray.r_pt, x+m->step, y+m->step, z);
	    rt_shootray(&a);

	    z = +INFINITY;

	    count += rt_nmg_mc_pew(m->s, primp, &a, x, y, z, m->step, m->tol);
	}
    }
    bu_log("%d done, %d\n", cpu, count);
    m->count += count;
    /* free the rt stuff we don't need anymore */
}

/* rtip needs to be valid, s is where the results are stashed */
int
nmg_mc_evaluate (struct shell *s, struct rt_i *rtip, const struct db_full_path *pathp, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct mci_s m;
    int i;

    m.s = s;
    m.rtip = rtip;
    m.tol = tol;
    m.count = 0;

    m.ncpu = bu_avail_cpus();
    m.ncpu = 1; /* seems to be an issue with confused loop calculation in the NMG code. */
    m.resources = bu_malloc(m.ncpu * sizeof(struct resource), "Resource array");
    for(i=0;i<m.ncpu;i++)
	rt_init_resource(&m.resources[i], i, rtip);

    rt_gettree( rtip, db_path_to_string(pathp) );
    rt_prep_parallel(rtip, m.ncpu);

    /* use rel value * bounding spheres diameter or the abs tolerance */
    m.step = NEAR_ZERO(ttol->abs, tol->dist) ? 0.5 * rtip->rti_radius * ttol->rel : ttol->abs;

    m.endx=bin(rtip->mdl_max[X], m.step) + m.step;
    m.endy=bin(rtip->mdl_max[Y], m.step) + m.step;

    bu_parallel(fire_row, m.ncpu, &m);

    return m.count;
}

void
nmg_triangulate_model_mc(struct model *m, const struct bn_tol *tol)
{
    BN_CK_TOL(tol);
    NMG_CK_MODEL(m);
    nmg_vmodel(m);

    if (rt_g.NMG_debug & DEBUG_TRI)
	bu_log("Triangulating NMG\n");

    if (rt_g.NMG_debug & DEBUG_TRI)
	bu_log("Triangulation completed\n");
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
