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
#include "bu/cmdschema.h"
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
    static const char *usage = "nmg mm name | nmg object {cmface|kill|move|make} ...";

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

    const char *subcmd = argv[1];
    if( BU_STR_EQUAL( "mm", subcmd ) ) {
	return ged_nmg_mm_core(gedp, argc - 1, argv + 1);
    }
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    subcmd = argv[2];
    /* The operation implementations take an argv beginning with the NMG
     * object, followed by their operation selector. */
    --argc;
    ++argv;
    if( BU_STR_EQUAL( "cmface", subcmd ) ) {
	return ged_nmg_cmface_core(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kill", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    return ged_nmg_kill_v_core(gedp, argc, argv);
	} else if ( BU_STR_EQUAL( "F", opt ) ) {
	    return ged_nmg_kill_f_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "move", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    return ged_nmg_move_v_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "make", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    return ged_nmg_make_v_core(gedp, argc, argv);
	}
    }
    else {
	bu_vls_printf(gedp->ged_result_str, "%s is not a subcommand.", subcmd );
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
nmg_alias_call(struct ged *gedp, int argc, const char *argv[], const char *op, const char *kind,
	int (*func)(struct ged *, int, const char **))
{
    if (argc < 2)
	return (*func)(gedp, argc, argv);
    size_t extra = kind ? 2 : 1;
    const char **nargv = (const char **)bu_calloc((size_t)argc + extra, sizeof(char *), "nmg alias argv");
    size_t n = 0;
    nargv[n++] = argv[1];
    nargv[n++] = op;
    if (kind) nargv[n++] = kind;
    for (int i = 2; i < argc; i++) nargv[n++] = argv[i];
    int ret = (*func)(gedp, (int)n, nargv);
    bu_free((void *)nargv, "nmg alias argv");
    return ret;
}

static int nmg_cmface_alias(struct ged *g, int ac, const char *av[]) { return nmg_alias_call(g, ac, av, "cmface", NULL, ged_nmg_cmface_core); }
static int nmg_kill_f_alias(struct ged *g, int ac, const char *av[]) { return nmg_alias_call(g, ac, av, "kill", "F", ged_nmg_kill_f_core); }
static int nmg_kill_v_alias(struct ged *g, int ac, const char *av[]) { return nmg_alias_call(g, ac, av, "kill", "V", ged_nmg_kill_v_core); }
static int nmg_make_v_alias(struct ged *g, int ac, const char *av[]) { return nmg_alias_call(g, ac, av, "make", "V", ged_nmg_make_v_core); }
static int nmg_move_v_alias(struct ged *g, int ac, const char *av[]) { return nmg_alias_call(g, ac, av, "move", "V", ged_nmg_move_v_core); }


#include "../include/plugin.h"

static const struct bu_cmd_operand labelface_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Displayed NMG objects whose faces will be labeled", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_mm_schema_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 1, 1, "New NMG object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_cmface_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND("vertices", BU_CMD_VALUE_NUMBER, 9, BU_CMD_COUNT_UNLIMITED,
	"Three or more XYZ vertices", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_kill_f_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND("face", BU_CMD_VALUE_INTEGER, 1, 1, "Face index", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_kill_v_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND("vertex", BU_CMD_VALUE_NUMBER, 3, 3, "XYZ vertex coordinates", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_make_v_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND("vertices", BU_CMD_VALUE_NUMBER, 3, BU_CMD_COUNT_UNLIMITED,
	"One or more XYZ vertices", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_move_v_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND("coordinates", BU_CMD_VALUE_NUMBER, 6, 6,
	"Old XYZ followed by new XYZ", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand nmg_collapse_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "Input NMG object", "ged.db_object"),
    BU_CMD_OPERAND("new_name", BU_CMD_VALUE_STRING, 1, 1, "New object name", NULL),
    BU_CMD_OPERAND("max_error", BU_CMD_VALUE_NUMBER, 1, 1, "Maximum collapse error", NULL),
    BU_CMD_OPERAND("min_angle", BU_CMD_VALUE_NUMBER, 0, 1, "Optional minimum angle", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const nmg_simplify_modes[] = {"arb", "tgc", "poly", NULL};
static const struct bu_cmd_operand nmg_simplify_schema_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("mode", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Optional output primitive type", NULL, nmg_simplify_modes),
    BU_CMD_OPERAND("new_name", BU_CMD_VALUE_STRING, 1, 1, "New object name", NULL),
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "Input NMG object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(nmg_simplify, 2, 3, GED_SCHEMA_COUNT_NONE)
static const char * const nmg_actions[] = {"cmface", "kill", "move", "make", NULL};
static const struct bu_cmd_operand nmg_schema_operands[] = {
    BU_CMD_OPERAND("nmg", BU_CMD_VALUE_DB_OBJECT, 1, 1, "NMG object", "ged.db_object"),
    BU_CMD_OPERAND_KEYWORDS("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	"NMG edit operation", NULL, nmg_actions),
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Operation-specific arguments", NULL),
    BU_CMD_OPERAND_NULL
};
#define NMG_SCHEMA(_id, _name, _help, _operands, _policy, _validator) \
    static const struct bu_cmd_schema _id##_cmd_schema = {_name, _help, NULL, _operands, _policy, {_validator}}
NMG_SCHEMA(labelface, "labelface", "Label faces of displayed NMG objects", labelface_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_cmface, "nmg_cmface", "Create a manifold NMG face", nmg_cmface_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_collapse, "nmg_collapse", "Collapse short NMG edges", nmg_collapse_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_fix_normals, "nmg_fix_normals", "Repair NMG face normals", labelface_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_kill_f, "nmg_kill_f", "Delete an NMG face", nmg_kill_f_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_kill_v, "nmg_kill_v", "Delete an NMG vertex", nmg_kill_v_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_make_v, "nmg_make_v", "Create NMG vertices", nmg_make_v_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_mm, "nmg_mm", "Create an empty NMG model", nmg_mm_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_move_v, "nmg_move_v", "Move an NMG vertex", nmg_move_v_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
NMG_SCHEMA(nmg_simplify, "nmg_simplify", "Simplify an NMG to another primitive", nmg_simplify_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, nmg_simplify_schema_validate);
#undef NMG_SCHEMA
static const struct bu_cmd_schema nmg_root_schema = {
    "nmg", "Edit NMG topology or create an empty NMG", NULL,
    nmg_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_tree_node nmg_subcommands[] = {
    BU_CMD_TREE_NODE(&nmg_mm_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree nmg_tree = {
    &nmg_root_schema, nmg_subcommands, BU_CMD_TREE_CHILD_FIRST
};
static const struct ged_cmd_native_form nmg_native_forms[] = {
    {"subcommands", NULL, &nmg_tree},
    {"legacy_topology", &nmg_root_schema, NULL},
    {NULL, NULL, NULL}
};

static const struct ged_cmd_native_form *
nmg_select_native_form(const struct ged *UNUSED(gedp), size_t argc,
	const char * const *argv)
{
    const char *word = argc > 1 ? argv[1] : "";
    size_t length = word ? strlen(word) : 0;

    if (!length || !strncmp("mm", word, length))
	return &nmg_native_forms[0];
    return &nmg_native_forms[1];
}

static int
nmg_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, nmg_native_forms,
	nmg_select_native_form, input, cursor_pos, result);
}

static int
nmg_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, nmg_native_forms,
	nmg_select_native_form, input, analysis);
}

static char *
nmg_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json("nmg",
	"Edit NMG topology or create an empty NMG", nmg_native_forms);
}

static int
nmg_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint("nmg", nmg_native_forms, msgs);
}

static const struct ged_cmd_grammar nmg_grammar = {
    "nmg", "Edit NMG topology or create an empty NMG", nmg_grammar_validate,
    nmg_grammar_analyze, nmg_grammar_json, nmg_grammar_lint
};

#define GED_NMG_COMMANDS(X, XID, N, NID, G, GID) \
    G(nmg, ged_nmg_core, GED_CMD_DEFAULT, &nmg_grammar) \
    N(labelface, ged_labelface_core, GED_CMD_DEFAULT, &labelface_cmd_schema) \
    N(nmg_cmface, nmg_cmface_alias, GED_CMD_DEFAULT, &nmg_cmface_cmd_schema) \
    N(nmg_collapse, ged_nmg_collapse_core, GED_CMD_DEFAULT, &nmg_collapse_cmd_schema) \
    N(nmg_fix_normals, ged_nmg_fix_normals_core, GED_CMD_DEFAULT, &nmg_fix_normals_cmd_schema) \
    N(nmg_kill_f, nmg_kill_f_alias, GED_CMD_DEFAULT, &nmg_kill_f_cmd_schema) \
    N(nmg_kill_v, nmg_kill_v_alias, GED_CMD_DEFAULT, &nmg_kill_v_cmd_schema) \
    N(nmg_make_v, nmg_make_v_alias, GED_CMD_DEFAULT, &nmg_make_v_cmd_schema) \
    N(nmg_mm, ged_nmg_mm_core, GED_CMD_DEFAULT, &nmg_mm_cmd_schema) \
    N(nmg_move_v, nmg_move_v_alias, GED_CMD_DEFAULT, &nmg_move_v_cmd_schema) \
    N(nmg_simplify, ged_nmg_simplify_core, GED_CMD_DEFAULT, &nmg_simplify_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_NMG_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_nmg", 1, GED_NMG_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
