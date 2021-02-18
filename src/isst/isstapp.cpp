/*                      I S S T A P P . C X X
 * BRL-ISST
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadapp.cxx
 *
 * Application level data and functionality implementations.
 *
 */

#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include "isstapp.h"
#include "bu/malloc.h"
#include "bu/file.h"

#include "gcv.h"
#include "nmg.h"
#include "rt/geom.h"

/* Replace the load_g globals from adrt with a struct that is
 * passed through the callers. */
struct isst_nmg_data {
    TIE_3 **tribuf;
    struct tie_s *cur_tie;
    struct db_i *dbip;
    struct bn_tol *tol;
};

struct gcv_data {
    struct gcv_region_end_data region_end_data;
    struct adrt_mesh_s **meshes;
};

static void nmg_to_adrt_gcvwrite(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int material_id, float color[3], void *client_data);

static struct gcv_data gcvwriter = {{nmg_to_adrt_gcvwrite, NULL}, NULL};

/* load the region into the tie image */
static void
nmg_to_adrt_internal(TIE_3 **tribuf, struct tie_s *cur_tie, struct adrt_mesh_s *mesh, struct nmgregion *r)
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

		TIE_VAL(tie_push)(cur_tie, tribuf, 1, mesh, 0);
		region_polys++;
	    }
	}
    }

    /* region_name must not be freed until we're done with the tie engine. */
}


static int
nmg_to_adrt_regstart(struct db_tree_state *ts, const struct db_full_path *path, const struct rt_comb_internal *rci, void *client_data)
{
    /*
     * if it's a simple single bot region, just eat the bots and return -1.
     * Omnomnom. Return 0 to do nmg eval.
     */
    struct gcv_region_end_data *rd = (struct gcv_region_end_data *)client_data;
    struct isst_nmg_data *d = (struct isst_nmg_data *)rd->client_data;
    struct directory *dir;
    struct rt_db_internal intern;
    struct adrt_mesh_s *mesh;
    unsigned char rgb[3] = { 0xc0, 0xc0, 0xc0 };

    RT_CHECK_COMB(rci);

    /* abort cases, no fast loading. */
    if (rci->tree == NULL)
	return 0;
    RT_CK_TREE(rci->tree);
    if ( rci->tree->tr_op != OP_DB_LEAF )
	return 0;
    if ((dir = db_lookup(d->dbip, rci->tree->tr_l.tl_name, 1)) == NULL) {
	printf("Lookup failed: %s\n", rci->tree->tr_l.tl_name);
	return 0;
    }
    if (dir->d_minor_type != ID_BOT && dir->d_minor_type != ID_NMG)
	return 0;
    if (rt_db_get_internal(&intern, dir, d->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	printf("Failed to load\n");
	return 0;
    }

    if (dir->d_minor_type == ID_NMG)
	return 0;

    /* FIXME: where is this released? */
    BU_ALLOC(mesh, struct adrt_mesh_s);

    BU_LIST_PUSH(&((*gcvwriter.meshes)->l), &(mesh->l));

    mesh->texture = NULL;
    mesh->flags = 0;

    BU_ALLOC(mesh->attributes, struct adrt_mesh_attributes_s);
    mesh->matid = ts->ts_gmater;

    rt_comb_get_color(rgb, rci);
    VSCALE(mesh->attributes->color.v, rgb, 1.0/256.0);

    bu_strlcpy(mesh->name, db_path_to_string(path), sizeof(mesh->name));

    if (intern.idb_minor_type == ID_NMG) {
	nmg_to_adrt_internal(d->tribuf, d->cur_tie, mesh, (struct nmgregion *)intern.idb_ptr);
	return -1;
    } else if (intern.idb_minor_type == ID_BOT) {
	size_t i;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;

	RT_BOT_CK_MAGIC(bot);

	for (i=0;i<bot->num_faces;i++)
	{
	    VSCALE((*d->tribuf[0]).v, (bot->vertices+3*bot->faces[3*i+0]), 1.0/1000.0);
	    VSCALE((*d->tribuf[1]).v, (bot->vertices+3*bot->faces[3*i+1]), 1.0/1000.0);
	    VSCALE((*d->tribuf[2]).v, (bot->vertices+3*bot->faces[3*i+2]), 1.0/1000.0);

	    TIE_VAL(tie_push)(d->cur_tie, d->tribuf, 1, mesh, 0);
	}
	return -1;
    }

    bu_log("Strange, %d is not %d or %d\n", intern.idb_minor_type, ID_BOT, ID_NMG);
    return 0;
}


static void
nmg_to_adrt_gcvwrite(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int material_id, float color[3], void *client_data)
{

    struct isst_nmg_data *d = (struct isst_nmg_data *)client_data;
    struct model *m;
    struct adrt_mesh_s *mesh;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, d->tol);

    /* FIXME: where is this released? */
    BU_ALLOC(mesh, struct adrt_mesh_s);

    BU_LIST_PUSH(&((*gcvwriter.meshes)->l), &(mesh->l));

    mesh->texture = NULL;
    mesh->flags = 0;

    BU_ALLOC(mesh->attributes, struct adrt_mesh_attributes_s);
    mesh->matid = material_id;

    VMOVE(mesh->attributes->color.v, color);
    bu_strlcpy(mesh->name, db_path_to_string(pathp), sizeof(mesh->name));

    nmg_to_adrt_internal(d->tribuf, d->cur_tie, mesh, r);
}

int
ISSTApp::load_g(const char *filename, int argc, const char *argv[])
{
    struct model *the_model;
    struct bn_tol tol;
    struct bg_tess_tol ttol;		/* tessellation tolerance in mm */
    struct db_tree_state tree_state;	/* includes tol & model */
    struct isst_nmg_data d;

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tessellation tolerance defaults */
    ttol.magic = BG_TESS_TOL_MAGIC;
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
    d.tol = &tol;

    tie_check_degenerate = 0;

    /* make empty NMG model */
    the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /*
     * these should probably encode so the result can be passed back to client
     */
    if ((dbip = db_open(filename, DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Unable to open geometry database file (%s)\n", filename);
	return -1;
    }
    if (db_dirbuild(dbip)) {
	bu_log("ERROR: db_dirbuild failed\n");
	return -1;
    }
    d.dbip = dbip;

    BU_ALLOC(tie, struct tie_s);
    d.cur_tie = this->tie;

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    TIE_VAL(tie_init)(d.cur_tie, BU_PAGE_SIZE, TIE_KDTREE_FAST);

    /* FIXME: where is this released? */
    BU_ALLOC(this->meshes, struct adrt_mesh_s);
    BU_LIST_INIT(&((this->meshes)->l));

    gcvwriter.meshes = &this->meshes;
    gcvwriter.region_end_data.client_data = &d;

    tribuf = (TIE_3 **)bu_malloc(sizeof(TIE_3 *) * 3, "triangle tribuffer tribuffer");
    tribuf[0] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");
    tribuf[1] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");
    tribuf[2] = (TIE_3 *)bu_malloc(sizeof(TIE_3) * 3, "triangle tribuffer");
    d.tribuf = this->tribuf;

    (void) db_walk_tree(dbip,
			argc,			/* number of toplevel regions */
			argv,			/* region names */
			1,			/* ncpu */
			&tree_state,		/* initial tree state */
			nmg_to_adrt_regstart,	/* region start function */
			gcv_region_end,		/* region end function */
			nmg_booltree_leaf_tess,	/* leaf func */
			(void *)&gcvwriter);	/* client data */

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);
    bu_free(tribuf[0], "vert");
    bu_free(tribuf[1], "vert");
    bu_free(tribuf[2], "vert");
    bu_free(tribuf, "tri");

    TIE_VAL(tie_prep)(d.cur_tie);

    return 0;
}

void
ISSTApp::closedb()
{
    bu_free(tie, "free tie");
    current_file.clear();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

