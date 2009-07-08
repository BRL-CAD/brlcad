/*                        L O A D _ G . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2009-2009 United States Government as represented by
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
/** @file load_g.c
 *
 * Attempt to load a single top-level comb from a named .g file. The file must
 * exist on the machine the 'slave' program is running, with the correct path
 * passed to it. Only one combination is used, intended to be the top of the
 * tree of concern. It's assumed that only BOT's are to be loaded, non-bots will
 * be silently ignored for now. No KD-TREE caching is assumed. I like tacos.
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"

#include "load.h"

#include "gcv.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

static struct db_i *dbip;
static struct model *the_model;
static struct rt_tess_tol ttol;		/* tesselation tolerance in mm */
static struct bn_tol tol;		/* calculation tolerance */
static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static unsigned int tot_polygons = 0;

static int verbose = 0;

/* load the region into the tie image */
static void
nmg_to_adrt_internal(struct nmgregion *r, struct db_full_path *pathp, int region_id, int material_id, float color[3])
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    /* Check triangles */
    for (BU_LIST_FOR (s, shell, &r->s_hd))
    {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd))
	{
	    struct loopuse *lu;
	    vect_t facet_normal;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal and save it for all the vertex loops */
	    NMG_GET_FU_NORMAL(facet_normal, fu);

	    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd))
	    {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    /* add facet_normal and v->vg_p->coord */
		}
		if (vert_count > 3)
		{
		    bu_free(region_name, "region name");
		    bu_log("lu x%x has %d vertices!\n", lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle");
		}
		else if (vert_count < 3)
		    continue;

		tot_polygons++;
		region_polys++;
	    }
	}
    }

    printf("Region %s polys: %d\n", region_name, region_polys);

    bu_free(region_name, "region name");
}

int
some_intermediate_function(char *filename, char *region)
{
    register int c;
    double percent;
    int i;

    printf("Give me my cheesypoofs! %s %s\n", filename, region);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tesselation tolerance defaults */
    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* XXX These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-5;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_model = nmg_mm();
    BU_LIST_INIT(&rt_g.rtg_vlfree);	/* for vlist macros */

    if ((dbip = db_open(filename, "r")) == DBI_NULL) {
	perror(filename);
	bu_exit(1, "Unable to open geometry file (%s)\n", filename);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "ERROR: db_dirbuild failed\n");

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    (void) db_walk_tree(dbip, 1, &region,
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			gcv_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)nmg_to_adrt_internal);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
}

int
slave_load_g (tie_t *tie, char *data)
{
    int i = 0;
    char *p = data;

    while(i<16 && data[i])
	i++;

    while(*p && *p != ':')
	++p;

    if(*p == ':')
	*p=0;
    else
	bu_bomb("No colon in file:region identifier\n");
    
    p++;

    return some_intermediate_function(data, p);
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
