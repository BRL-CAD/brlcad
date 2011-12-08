/*                        L O A D _ G . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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

#include "gcv.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "tie.h"
#include "adrt.h"
#include "adrt_struct.h"



static struct bn_tol tol;		/* calculation tolerance */
static struct tie_s *cur_tie;
static struct db_i *dbip;
TIE_3 **tribuf;

static void nmg_to_adrt_gcvwrite(struct nmgregion *r, const struct db_full_path *pathp, int region_id, int material_id, float color[3]);

struct gcv_data {
    void (*func)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);
    struct adrt_mesh_s **meshes;
};
static struct gcv_data gcvwriter = {nmg_to_adrt_gcvwrite, NULL};


/* load the region into the tie image */
static void
nmg_to_adrt_internal(struct adrt_mesh_s *mesh, struct nmgregion *r)
{
    struct model *m;
    struct shell *s;
    int region_polys=0;

    NMG_CK_REGION(r);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Check triangles */
    for (BU_LIST_FOR (s, shell, &r->s_hd))
    {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd))
	{
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

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
		    struct vertex *v;

		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    /* convert mm to m */
		    VSCALE((*tribuf[vert_count]).v, v->vg_p->coord, 1.0/1000.0);
		    vert_count++;
		}
		if (vert_count > 3)
		{
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle");
		}
		else if (vert_count < 3)
		    continue;

		tie_push(cur_tie, tribuf, 1, mesh, 0);
		region_polys++;
	    }
	}
    }

    /* region_name must not be freed until we're done with the tie engine. */
}

int
nmg_to_adrt_regstart(struct db_tree_state *ts, const struct db_full_path *path, const struct rt_comb_internal *rci, genptr_t UNUSED(client_data))
{
    /*
     * if it's a simple single bot region, just eat the bots and return -1.
     * Omnomnom. Return 0 to do nmg eval.
     */
    struct directory *dir;
    struct rt_db_internal intern;
    struct adrt_mesh_s *mesh;
    unsigned char rgb[3] = { 0xc0, 0xc0, 0xc0 };

    RT_CHECK_COMB(rci);

    /* abort cases, no fast loading. */
    if(rci->tree == NULL)
	return 0;
    RT_CK_TREE(rci->tree);
    if( rci->tree->tr_op != OP_DB_LEAF )
	return 0;
    dir = db_lookup(dbip, rci->tree->tr_l.tl_name, 1);
    if(dir->d_minor_type != ID_BOT && dir->d_minor_type != ID_NMG)
	return 0;
    if(rt_db_get_internal(&intern, dir, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	printf("Failed to load\n");
	return 0;
    }

    if(dir->d_minor_type == ID_NMG)
	return 0;

    BU_GET(mesh, struct adrt_mesh_s);

    BU_LIST_PUSH(&((*gcvwriter.meshes)->l), &(mesh->l));

    mesh->texture = NULL;
    mesh->flags = 0;
    mesh->attributes = (struct adrt_mesh_attributes_s *)bu_malloc(sizeof(struct adrt_mesh_attributes_s), "adrt mesh attributes");
    mesh->matid = ts->ts_gmater;

    rt_comb_get_color(rgb, rci);
    VSCALE(mesh->attributes->color.v, rgb, 1.0/256.0);

    strncpy(mesh->name, db_path_to_string(path), 255);

    if(intern.idb_minor_type == ID_NMG) {
	nmg_to_adrt_internal(mesh, (struct nmgregion *)intern.idb_ptr);
	return -1;
    } else if (intern.idb_minor_type == ID_BOT) {
	size_t i;
	struct rt_bot_internal *bot = intern.idb_ptr;

	RT_BOT_CK_MAGIC(bot);

	for(i=0;i<bot->num_faces;i++)
	{
	    VSCALE((*tribuf[0]).v, (bot->vertices+3*bot->faces[3*i+0]), 1.0/1000.0);
	    VSCALE((*tribuf[1]).v, (bot->vertices+3*bot->faces[3*i+1]), 1.0/1000.0);
	    VSCALE((*tribuf[2]).v, (bot->vertices+3*bot->faces[3*i+2]), 1.0/1000.0);

	    tie_push(cur_tie, tribuf, 1, mesh, 0);
	}
	return -1;
    }

    bu_log("Strange, %d is not %d or %d\n", intern.idb_minor_type, ID_BOT, ID_NMG);
    return 0;
}


static void
nmg_to_adrt_gcvwrite(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int material_id, float color[3])
{
    struct model *m;
    struct adrt_mesh_s *mesh;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    BU_GET(mesh, struct adrt_mesh_s);

    BU_LIST_PUSH(&((*gcvwriter.meshes)->l), &(mesh->l));

    mesh->texture = NULL;
    mesh->flags = 0;
    mesh->attributes = (struct adrt_mesh_attributes_s *)bu_malloc(sizeof(struct adrt_mesh_attributes_s), "adrt mesh attributes");
    mesh->matid = material_id;

    VMOVE(mesh->attributes->color.v, color);
    strncpy(mesh->name, db_path_to_string(pathp), 255);

    nmg_to_adrt_internal(mesh, r);
}


int
load_g (struct tie_s *tie, const char *db, int argc, const char **argv, struct adrt_mesh_s **meshes)
{
    struct model *the_model;
    struct rt_tess_tol ttol;		/* tesselation tolerance in mm */
    struct db_tree_state tree_state;	/* includes tol & model */

    cur_tie = tie;	/* blehhh, global... need locking. */

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
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    tie_check_degenerate = 0;
    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_model = nmg_mm();
    BU_LIST_INIT(&rt_g.rtg_vlfree);	/* for vlist macros */

    /*
     * these should probably encode so the result can be passed back to client
     */
    if ((dbip = db_open(db, "r")) == DBI_NULL) {
	perror(db);
	bu_log("Unable to open geometry file (%s)\n", db);
	return -1;
    }
    if (db_dirbuild(dbip)) {
	bu_log("ERROR: db_dirbuild failed\n");
	return -1;
    }

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    tie_init(cur_tie, 4096, TIE_KDTREE_FAST);

    BU_GET(*meshes, struct adrt_mesh_s);
    BU_LIST_INIT(&((*meshes)->l));

    gcvwriter.meshes = meshes;

    tribuf = (TIE_3 **)bu_malloc(sizeof(TIE_3 *) * 3, "triangle tribuffer tribuffer");
    tribuf[0] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");
    tribuf[1] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");
    tribuf[2] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");

    (void) db_walk_tree(dbip,
			argc,			/* number of toplevel regions */			
			argv,			/* region names */
			1,			/* ncpu */
			&tree_state,		/* initial tree state */
			nmg_to_adrt_regstart,	/* region start function */
			gcv_region_end,		/* region end function */
			nmg_booltree_leaf_tess,	/* leaf func */
			(genptr_t)&gcvwriter);	/* client data */

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);
    bu_free(tribuf[0], "vert");
    bu_free(tribuf[1], "vert");
    bu_free(tribuf[2], "vert");
    bu_free(tribuf, "tri");

    tie_prep(cur_tie);

    return 0;
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
