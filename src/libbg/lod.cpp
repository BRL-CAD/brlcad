/*                       L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright 2001 softSurfer, 2012 Dan Sunday
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 *
 */
/** @file lod.cpp
 *
 * This file implements level-of-detail routines.  Eventually it may have libbg
 * wrappers around more sophisticated algorithms, but for now its purpose is to
 * hold logic intended to help with edge-only wireframe displays of large
 * meshes.
 */

#include "common.h"
#include <stdlib.h>
#include <unordered_set>
#include <unordered_map>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>

#include "bu/malloc.h"
#include "bg/lod.h"
#include "bg/trimesh.h"

struct bg_mesh_lod_internal {
    int level_count;
    void *root;
};

extern "C" struct bg_mesh_lod *
bg_mesh_lod_create(const point_t *v, int vcnt, int *faces, int fcnt)
{
    if (!v || !vcnt || !faces || !fcnt)
	return NULL;

    struct bg_mesh_lod *l = NULL;
    BU_GET(l, struct bg_mesh_lod);
    BU_GET(l->i, struct bg_mesh_lod_internal);

    l->i->root = NULL;

    return l;
}

extern "C" void
bg_mesh_lod_destroy(struct bg_mesh_lod *l)
{
    if (!l)
	return;

    //delete l->i->root;
    BU_PUT(l->i, struct bg_mesh_lod_internal);
    BU_PUT(l, struct bg_mesh_lod);
}

extern "C" int
bg_lod_elist(struct bu_list *elist, struct bview *v, struct bg_mesh_lod *l)
{
    int ecnt = 0;
    if (!elist || !v || !l)
	return -1;


    return ecnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
