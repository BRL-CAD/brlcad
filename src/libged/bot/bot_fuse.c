/*                         B O T _ F U S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/bot_fuse.c
 *
 * The bot_fuse command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bu/parallel.h"
#include "rt/geom.h"
#include "bv/plot3.h"

#include "../ged_private.h"


struct bot_fuse_args {
    int print_help;
    int show_open_edges;
    int plot_open_edges;
};

static int
bot_fuse_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option bot_fuse_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_fuse_args, print_help,
	"Print command help"),
    BU_CMD_FLAG("s", NULL, struct bot_fuse_args, show_open_edges,
	"Show open edges in the display"),
    BU_CMD_FLAG("p", NULL, struct bot_fuse_args, plot_open_edges,
	"Plot open edges to a temporary file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_fuse_schema_operands[] = {
    BU_CMD_OPERAND("new_bot", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the fused BOT", NULL),
    BU_CMD_OPERAND("old_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Source BOT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const char * const bot_fuse_output_options[] = {"s", "p", NULL};
static const struct bu_cmd_constraint bot_fuse_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(bot_fuse_output_options, 0, 1,
	"-s and -p are mutually exclusive"),
    BU_CMD_CONSTRAINT_NULL
};
const struct bu_cmd_schema ged_bot_fuse_schema = {
    "bot_fuse", "Fuse BOT vertices and faces", bot_fuse_schema_options,
    bot_fuse_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_fuse_schema_validate, bot_fuse_schema_constraints)
};

static void
bot_fuse_usage(struct bu_vls *result, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_fuse_schema);

    bu_vls_sprintf(result, "Usage: %s [--help] [-s | -p] new_bot old_bot\n", cmd);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "help str");
    }
}


static size_t
show_dangling_edges(struct ged *gedp, const uint32_t *magic_p, const char *name, int out_type, struct bu_list *vlfree)
{
    FILE *plotfp = NULL;
    const char *manifolds = NULL;
    const struct edgeuse *eur;
    int done;
    point_t pt1, pt2;
    size_t i, cnt;
    struct bv_vlblock *vbp = NULL;
    struct bu_list *vhead = NULL;
    struct bu_ptbl faces;
    struct bu_vls plot_file_name = BU_VLS_INIT_ZERO;
    struct edgeuse *eu = NULL;
    struct face *fp = NULL;
    struct faceuse *fu, *fu1, *fu2;
    struct faceuse *newfu = NULL;
    struct loopuse *lu = NULL;

    /* out_type: 0 = none, 1 = show, 2 = plot */
    if (out_type < 0 || out_type > 2) {
	bu_log("Internal error, open edge test failed.\n");
	return 0;
    }

    if (out_type == 1) {
	vbp = bv_vlblock_init(vlfree, 32);
	vhead = bv_vlblock_find(vbp, 0xFF, 0xFF, 0x00);
    }

    bu_ptbl_init(&faces, 64, "faces buffer");
    nmg_face_tabulate(&faces, magic_p, vlfree);

    cnt = 0;
    for (i = 0; i < (size_t)BU_PTBL_LEN(&faces) ; i++) {
	fp = (struct face *)BU_PTBL_GET(&faces, i);
	NMG_CK_FACE(fp);
	fu = fu1 = fp->fu_p;
	NMG_CK_FACEUSE(fu1);
	fu2 = fp->fu_p->fumate_p;
	NMG_CK_FACEUSE(fu2);
	done = 0;
	while (!done) {
	    NMG_CK_FACEUSE(fu);
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			eur = nmg_radial_face_edge_in_shell(eu);
			newfu = eur->up.lu_p->up.fu_p;
			while (manifolds &&
			       NMG_MANIFOLDS(manifolds, newfu) &
			       NMG_2MANIFOLD &&
			       eur != eu->eumate_p) {
			    eur = nmg_radial_face_edge_in_shell(eur->eumate_p);
			    newfu = eur->up.lu_p->up.fu_p;
			}
			if (eur == eu->eumate_p) {
			    VMOVE(pt1, eu->vu_p->v_p->vg_p->coord);
			    VMOVE(pt2, eu->eumate_p->vu_p->v_p->vg_p->coord);
			    if (out_type == 1) {
				BV_ADD_VLIST(vbp->free_vlist_hd, vhead, pt1, BV_VLIST_LINE_MOVE);
				BV_ADD_VLIST(vbp->free_vlist_hd, vhead, pt2, BV_VLIST_LINE_DRAW);
			    } else if (out_type == 2) {
				if (!plotfp) {
				    bu_vls_sprintf(&plot_file_name, "%s.%p.pl", name, (void *)magic_p);
				    plotfp = fopen(bu_vls_addr(&plot_file_name), "wb");
				    if (plotfp == (FILE *)NULL) {
					bu_vls_free(&plot_file_name);
					bu_log("Error, unable to create plot file (%s), open edge test failed.\n",
					       bu_vls_addr(&plot_file_name));
					return 0;
				    }
				}
				pdv_3line(plotfp, pt1, pt2);
			    }
			    cnt++;
			}
		    }
		}
	    }
	    if (fu == fu1) fu = fu2;
	    if (fu == fu2) done = 1;
	};

    }

    if (out_type == 1) {
	/* Add overlay */
	if (gedp->new_cmd_forms) {
	    struct bu_vls nroot = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&nroot, "bot_fuse::%s", name);
	    struct bview *view = gedp->ged_gvp;
	    bv_vlblock_obj(vbp, view, bu_vls_cstr(&nroot));
	    bu_vls_free(&nroot);
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, vbp, name, 0);
	}
	bv_vlblock_free(vbp);
	bu_log("Showing open edges...\n");
    } else if (out_type == 2) {
	if (plotfp) {
	    (void)fclose(plotfp);
	    bu_log("Wrote plot file (%s)\n", bu_vls_addr(&plot_file_name));
	    bu_vls_free(&plot_file_name);
	}
    }
    bu_ptbl_free(&faces);

    return cnt;
}


int
ged_bot_fuse_core(struct ged *gedp, int argc, const char **argv)
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal intern, intern2;
    struct rt_bot_internal *bot;
    int count=0;
    struct bu_list *vlfree = &rt_vlfree;

    struct model *m;
    struct nmgregion *r;
    int ret;
    struct rt_wdb *wdbp;
    struct bn_tol *tol;
    int total = 0;
    volatile int out_type = 0; /* open edge output type: 0 = none, 1 = show, 2 = plot */
    size_t open_cnt;
    struct bu_vls name_prefix = BU_VLS_INIT_ZERO;
    struct bot_fuse_args args = {0};
    const char *cmdname;
    int operand_index;
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmdname = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_fuse_usage(gedp->ged_result_str, cmdname);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_fuse_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_fuse_usage(gedp->ged_result_str, cmdname);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_fuse_usage(gedp->ged_result_str, cmdname);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;
    out_type = args.show_open_edges ? 1 : (args.plot_open_edges ? 2 : 0);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    tol = &wdbp->wdb_tol;

    bu_log("%s: start\n", cmdname);

    GED_DB_LOOKUP(gedp, old_dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, old_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", cmdname, argv[1]);
	return BRLCAD_ERROR;
    }

    /* create nmg model structure */
    m = nmg_mm();

    /* place bot in nmg structure */
    bu_log("%s: running rt_bot_tess\n", cmdname);
    ret = rt_bot_tess(&r, m, &intern, &wdbp->wdb_ttol, tol);

    /* free internal representation of original bot */
    rt_db_free_internal(&intern);

    if (ret != 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (1).\n", cmdname, argv[1]);
	nmg_km(m);
	return BRLCAD_ERROR;
    }

    total = 0;

    /* Step 1 -- the vertices. */
    bu_log("%s: running nmg_vertex_fuse\n", cmdname);
    count = nmg_vertex_fuse(&m->magic, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d vertex fused\n", cmdname, argv[1], count);

    /* Step 1.5 -- break edges on vertices, before fusing edges */
    bu_log("%s: running nmg_break_e_on_v\n", cmdname);
    count = nmg_break_e_on_v(&m->magic, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d broke 'e' on 'v'\n", cmdname, argv[1], count);

    if (total) {
	struct nmgregion *r2;
	struct shell *s;

	bu_log("%s: running nmg_make_faces_within_tol\n", cmdname);

	/* vertices and/or edges have been moved,
	 * may have created out-of-tolerance faces
	 */

	for (BU_LIST_FOR(r2, nmgregion, &m->r_hd)) {
	    for (BU_LIST_FOR(s, shell, &r2->s_hd))
		nmg_make_faces_within_tol(s, vlfree, tol);
	}
    }

    /* Step 2 -- the face geometry */
    bu_log("%s: running nmg_model_face_fuse\n", cmdname);
    count = nmg_model_face_fuse(m, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d faces fused\n", cmdname, argv[1], count);

    /* Step 3 -- edges */
    bu_log("%s: running nmg_edge_fuse\n", cmdname);
    count = nmg_edge_fuse(&m->magic, vlfree, tol);
    total += count;

    bu_log("%s: %s, %d edges fused\n", cmdname, argv[1], count);

    bu_log("%s: %s, %d total fused\n", cmdname, argv[1], total);

    if (!BU_SETJUMP) {
	/* try */

	/* convert the nmg model back into a bot */
	bot = nmg_bot(BU_LIST_FIRST(shell, &r->s_hd), vlfree, tol);

	bu_vls_sprintf(&name_prefix, "open_edges.%s", argv[0]);
	bu_log("%s: running show_dangling_edges\n", cmdname);
	open_cnt = show_dangling_edges(gedp, &m->magic, bu_vls_addr(&name_prefix), out_type, vlfree);
	bu_log("%s: WARNING %zu open edges, new BOT may be invalid!!!\n", argv[0], open_cnt);
	bu_vls_free(&name_prefix);

	/* free the nmg model structure */
	nmg_km(m);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (2).\n", cmdname, argv[1]);
	return BRLCAD_ERROR;
    } BU_UNSETJUMP;

    RT_DB_INTERNAL_INIT(&intern2);
    intern2.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern2.idb_type = ID_BOT;
    intern2.idb_meth = &OBJ[ID_BOT];
    intern2.idb_ptr = (void *)bot;

    GED_DB_DIRADD(gedp, new_dp, argv[0], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern2.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, new_dp, &intern2, BRLCAD_ERROR);

    bu_log("%s: Created new BOT (%s)\n", cmdname, argv[0]);
    bu_log("%s: Done.\n", cmdname);

    return BRLCAD_OK;
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
