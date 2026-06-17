/*                             N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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

#include "dm.h"  // For labelface - see if the dm_set_native_repaint_pending is really needed

#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/render.h"
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
/* Callback data for nmg labelface solid lookup */
struct labelface_data {
    struct directory *dp;
    struct model *m;
    struct db_i *dbip;
    struct bu_list *f_list;
    struct bsg_feature_label_data *labels;
    size_t label_count;
    size_t label_capacity;
};

#define LABELFACE_FEATURE_NAME "_LABELFACE_ffffff"

static int
labelface_record_matches(struct db_i *dbip, const struct bsg_export_record *rec, struct directory *dp)
{
    if (!dbip || !rec || !dp || rec->source.scope != BSG_RENDER_SOURCE_SCOPE_DATABASE)
	return 0;

    struct db_full_path fp;
    db_full_path_init(&fp);
    if (db_string_to_path(&fp, dbip, bu_vls_cstr(&rec->path)) < 0)
	return 0;
    int ret = db_full_path_search(&fp, dp);
    db_free_full_path(&fp);
    return ret;
}

static void
labelface_export_record(const struct bsg_export_record *rec, struct labelface_data *lfd)
{
    if (!lfd || !labelface_record_matches(lfd->dbip, rec, lfd->dp))
	return;

    get_face_list(lfd->m, lfd->f_list);
    struct face *curr_f;
    for (BU_LIST_FOR(curr_f, face, lfd->f_list)) {
	char label[256];
	point_t avg_pt;

	if (lfd->label_count + 1 > lfd->label_capacity) {
	    size_t ncap = lfd->label_capacity ? lfd->label_capacity * 2 : 64;
	    lfd->labels = (struct bsg_feature_label_data *)bu_realloc(lfd->labels,
		    ncap * sizeof(struct bsg_feature_label_data), "labelface label data");
	    for (size_t i = lfd->label_capacity; i < ncap; i++) {
		struct bsg_feature_label_data init = BSG_FEATURE_LABEL_DATA_INIT;
		lfd->labels[i] = init;
	    }
	    lfd->label_capacity = ncap;
	}

	avg_pt[0] = (curr_f->min_pt[0] + curr_f->max_pt[0]) / 2;
	avg_pt[1] = (curr_f->min_pt[1] + curr_f->max_pt[1]) / 2;
	avg_pt[2] = (curr_f->min_pt[2] + curr_f->max_pt[2]) / 2;
	snprintf(label, sizeof(label), " %d", (int)curr_f->index);

	struct bsg_feature_label_data init = BSG_FEATURE_LABEL_DATA_INIT;
	lfd->labels[lfd->label_count] = init;
	lfd->labels[lfd->label_count].text = bu_strdup(label);
	VMOVE(lfd->labels[lfd->label_count].point, avg_pt);
	lfd->labels[lfd->label_count].color_valid = 1;
	VSET(lfd->labels[lfd->label_count].color, 255, 255, 255);
	lfd->label_count++;
    }
}

static void
labelface_data_free(struct labelface_data *lfd)
{
    if (!lfd)
	return;

    for (size_t i = 0; i < lfd->label_count; i++) {
	if (lfd->labels[i].text)
	    bu_free((char *)lfd->labels[i].text, "labelface label text");
    }
    if (lfd->labels)
	bu_free(lfd->labels, "labelface label data");
    lfd->labels = NULL;
    lfd->label_count = 0;
    lfd->label_capacity = 0;
}

int
ged_labelface_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    int i;
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
		bn_mat_identity) < 0) {
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

    struct labelface_data lfd;
    memset(&lfd, 0, sizeof(lfd));
    lfd.m = m;
    lfd.dbip = gedp->dbip;
    lfd.f_list = &f_list;

    for (i=1; i<argc; i++) {
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	/* Find displayed uses of this database object. */
	lfd.dp = dp;
	struct bsg_export_request request;
	bsg_export_request_init(&request, gedp->ged_gvp);
	request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY | BSG_EXPORT_QUERY_DB_OBJECTS;
	request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
	struct bsg_export_result *result = bsg_export_query(&request);
	if (!result)
	    continue;
	for (size_t ri = 0; ri < bsg_export_result_count(result); ri++)
	    labelface_export_record(bsg_export_result_get(result, ri), &lfd);
	bsg_export_result_free(result);
    }

    (void)bsg_feature_remove(gedp->ged_gvp, LABELFACE_FEATURE_NAME);
    if (lfd.label_count) {
	bsg_feature_ref ref = bsg_feature_create_label(gedp->ged_gvp,
		LABELFACE_FEATURE_NAME, 0);
	if (bsg_feature_ref_is_null(ref) ||
		!bsg_feature_labels_replace(ref, lfd.labels, lfd.label_count)) {
	    labelface_data_free(&lfd);
	    bu_vls_printf(gedp->ged_result_str, "failed to create labelface feature\n");
	    return BRLCAD_ERROR;
	}
    }

    labelface_data_free(&lfd);
    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (dmp)
	dm_set_native_repaint_pending(dmp, 1);
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


#include "../include/plugin.h"

#define GED_NMG_COMMANDS(X, XID) \
    X(nmg, ged_nmg_core, GED_CMD_DEFAULT) \
    X(labelface, ged_labelface_core, GED_CMD_DEFAULT) \
    X(nmg_cmface, ged_nmg_cmface_core, GED_CMD_DEFAULT) \
    X(nmg_collapse, ged_nmg_collapse_core, GED_CMD_DEFAULT) \
    X(nmg_fix_normals, ged_nmg_fix_normals_core, GED_CMD_DEFAULT) \
    X(nmg_kill_f, ged_nmg_kill_f_core, GED_CMD_DEFAULT) \
    X(nmg_kill_v, ged_nmg_kill_v_core, GED_CMD_DEFAULT) \
    X(nmg_make_v, ged_nmg_make_v_core, GED_CMD_DEFAULT) \
    X(nmg_mm, ged_nmg_mm_core, GED_CMD_DEFAULT) \
    X(nmg_move_v, ged_nmg_move_v_core, GED_CMD_DEFAULT) \
    X(nmg_simplify, ged_nmg_simplify_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_NMG_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_nmg", 1, GED_NMG_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
