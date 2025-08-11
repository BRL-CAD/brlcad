/*                             N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2025 United States Government as represented by
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
/** @file libged/nmg.c
 *
 * The nmg command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "dm.h"  // For labelface - see if the dm_set_dirty is really needed

#include "ged.h"
#include "../ged_private.h"


static void
get_face_list( const struct model* m, struct bu_list* f_list )
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct face* curr_f;
    int found;

    NMG_CK_MODEL(m);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);

	if (r->ra_p) {
	    NMG_CK_REGION_A(r->ra_p);
	}

	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);

	    if (s->sa_p) {
		NMG_CK_SHELL_A(s->sa_p);
	    }

	    /* Faces in shell */
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		f = fu->f_p;
		NMG_CK_FACE(f);

		found = 0;
		/* check for duplicate face struct */
		for (BU_LIST_FOR(curr_f, face, f_list)) {
		    if (curr_f->index == f->index) {
			found = 1;
			break;
		    }
		}

		if ( !found )
		    BU_LIST_INSERT( f_list, &(f->l) );

		if (f->g.magic_p) switch (*f->g.magic_p) {
		    case NMG_FACE_G_PLANE_MAGIC:
			break;
		    case NMG_FACE_G_SNURB_MAGIC:
			break;
		}

	    }
	}
    }
}

/* Usage:  labelface solid(s) */
int
ged_labelface_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bv_vlblock *vbp;
    mat_t mat;
    fastf_t scale;
    struct model* m;
    const char* name;
    struct bu_list f_list;
    static const char *usage = "object(s) - label faces of wireframes of objects (currently NMG only)";

    BU_LIST_INIT( &f_list );

    if (!gedp || !gedp->dbip)
	return BRLCAD_ERROR;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[1];

    if ( (dp=db_lookup(gedp->dbip, name, LOOKUP_QUIET))
	    == RT_DIR_NULL ) {
	bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, gedp->dbip,
		bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return BRLCAD_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
	rt_db_free_internal(&internal);
	return BRLCAD_ERROR;
    }

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, gedp->ged_gvp->gv_rotation);
    scale = gedp->ged_gvp->gv_size / 100;      /* divide by # chars/screen */
    for (i=1; i<argc; i++) {
	struct bv_scene_obj *s;
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	/* Find uses of this solid in the solid table */
	gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);
	    for (BU_LIST_FOR(s, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!s->s_u_data)
		    continue;
		struct ged_bv_data *bdata = (struct ged_bv_data *)s->s_u_data;
		if (db_full_path_search(&bdata->s_fullpath, dp)) {
		    get_face_list(m, &f_list);
		    rt_label_vlist_faces(vbp, &f_list, mat, scale, gedp->dbip->dbi_base2local);
		}
	    }

	    gdlp = next_gdlp;
	}
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, "_LABELFACE_", 0);

    bv_vlblock_free(vbp);

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (dmp)
	dm_set_dirty(dmp, 1);
    return BRLCAD_OK;
}



extern int ged_nmg_cmface_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_collapse_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_fix_normals_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kill_f_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_kill_v_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_make_v_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mm_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_move_v_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_simplify_core(struct ged *gedp, int argc, const char *argv[]);

int
ged_nmg_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "nmg object subcommand [V|F|R|S] [suffix]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* must be wanting help */
    if (argc < 3) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
    bu_vls_printf(gedp->ged_result_str, "commands:\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmm             -  creates a new "
		  "NMG model structure and fills the appropriate fields. The result "
		  "is an empty model.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tcmface         -  creates a "
		  "manifold face in the first encountered shell of the NMG "
		  "object. Vertices are listed as the suffix and define the "
		  "winding-order of the face.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tkill V         -  removes the "
		  "vertexuse and vertex geometry of the selected vertex (via its "
		  "coordinates) and higher-order topology containing the vertex. "
		  "When specifying vertex to be removed, user generally will display "
		  "vertex coordinates in object via the GED command labelvert.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tkill F         -  removes the "
		  "faceuse and face geometry of the selected face (via its "
		  "index). When specifying the face to be removed, user generally "
		  "will display face indices in object via the MGED command "
		  "labelface.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmove V         -  moves an existing "
		  "vertex specified by the coordinates x_initial y_initial "
		  "z_initial to the position with coordinates x_final y_final "
		  "z_final.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmake V         -  creates a new "
		  "vertex in the nmg object.\n");
    return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* advance CLI arguments for subcommands */
    --argc;
    ++argv;

    const char *subcmd = argv[0];
    if( BU_STR_EQUAL( "mm", subcmd ) ) {
	ged_nmg_mm_core(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "cmface", subcmd ) ) {
	ged_nmg_cmface_core(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kill", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_kill_v_core(gedp, argc, argv);
	} else if ( BU_STR_EQUAL( "F", opt ) ) {
	    ged_nmg_kill_f_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "move", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_move_v_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "make", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_make_v_core(gedp, argc, argv);
	}
    }
    else {
	bu_vls_printf(gedp->ged_result_str, "%s is not a subcommand.", subcmd );
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl nmg_cmd_impl = {"nmg", ged_nmg_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_cmd = { &nmg_cmd_impl };

struct ged_cmd_impl labelface_cmd_impl = {"labelface", ged_labelface_core, GED_CMD_DEFAULT};
const struct ged_cmd labelface_cmd = { &labelface_cmd_impl };

struct ged_cmd_impl nmg_cmface_cmd_impl = {"nmg_cmface", ged_nmg_cmface_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_cmface_cmd = { &nmg_cmface_cmd_impl };

struct ged_cmd_impl nmg_collapse_cmd_impl = {"nmg_collapse", ged_nmg_collapse_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_collapse_cmd = { &nmg_collapse_cmd_impl };

struct ged_cmd_impl nmg_fix_normals_cmd_impl = {"nmg_fix_normals", ged_nmg_fix_normals_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_fix_normals_cmd = { &nmg_fix_normals_cmd_impl };

struct ged_cmd_impl nmg_kill_f_cmd_impl = {"nmg_kill_f", ged_nmg_kill_f_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_kill_f_cmd = { &nmg_kill_f_cmd_impl };

struct ged_cmd_impl nmg_kill_v_cmd_impl = {"nmg_kill_v", ged_nmg_kill_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_kill_v_cmd = { &nmg_kill_v_cmd_impl };

struct ged_cmd_impl nmg_make_v_cmd_impl = {"nmg_make_v", ged_nmg_make_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_make_v_cmd = { &nmg_make_v_cmd_impl };

struct ged_cmd_impl nmg_mm_cmd_impl = {"nmg_mm", ged_nmg_mm_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_mm_cmd = { &nmg_mm_cmd_impl };

struct ged_cmd_impl nmg_move_v_cmd_impl = {"nmg_move_v", ged_nmg_move_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_move_v_cmd = { &nmg_move_v_cmd_impl };

struct ged_cmd_impl nmg_simplify_cmd_impl = {"nmg_simplify", ged_nmg_simplify_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_simplify_cmd = { &nmg_simplify_cmd_impl };

const struct ged_cmd *nmg_cmds[] = {
    &nmg_cmd,
    &labelface_cmd,
    &nmg_cmface_cmd,
    &nmg_collapse_cmd,
    &nmg_fix_normals_cmd,
    &nmg_kill_f_cmd,
    &nmg_kill_v_cmd,
    &nmg_make_v_cmd,
    &nmg_mm_cmd,
    &nmg_move_v_cmd,
    &nmg_simplify_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  nmg_cmds, 11 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
