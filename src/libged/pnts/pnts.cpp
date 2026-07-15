/*                       P N T S . C P P
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
/** @file libged/pnts.cpp
 *
 * pnts command for simple Point Set (pnts) primitive operations.
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <vector>

extern "C" {
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/sort.h"
#include "bu/units.h"
#include "bg/ballpivot.h"
#include "bg/spsr.h"
#include "rt/geom.h"
#include "wdb.h"
#include "analyze.h"
}
#include "../ged_private.h"
#include "../pnts_util.h"

/* TODO - need some generic version of this logic in libbn -
 * used in libanalyze's NIRT as well */
void _pnts_fastf_t_to_vls(struct bu_vls *o, fastf_t d, int p)
{
    size_t prec = (p > 0) ? (size_t)p : std::numeric_limits<fastf_t>::max_digits10;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << d;
    std::string sd = ss.str();
    bu_vls_sprintf(o, "%s", sd.c_str());
}


static int
_ged_pnts_cmd_msgs(struct ged *gedp, int argc, const char **argv, const char *us, const char *ps)
{
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

// Collect arrays of points and (optionally) normals from a rt_pnts_internal.
// If normals_required is non-zero, return error if pnts does not have normals.
static int
_pnts_collect_arrays(struct ged *gedp, struct rt_pnts_internal *pnts, point_t **pts, vect_t **nrms, int *cnt, int normals_required)
{
    if (!pnts || !pts || !cnt)
	return BRLCAD_ERROR;
    int has_normals = !(pnts->type == RT_PNT_TYPE_PNT || pnts->type == RT_PNT_TYPE_COL || pnts->type == RT_PNT_TYPE_SCA || pnts->type == RT_PNT_TYPE_COL_SCA);
    if (normals_required && !has_normals) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point cloud data does not define normals\n");
	return BRLCAD_ERROR;
    }

    *cnt = (int)pnts->count;
    if (*cnt <= 0) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: zero points in input pnts object\n");
	return BRLCAD_ERROR;
    }

    *pts = (point_t *)bu_calloc((size_t)*cnt, sizeof(point_t), "pnts pts array");
    if (nrms) *nrms = has_normals ? (vect_t *)bu_calloc((size_t)*cnt, sizeof(vect_t), "pnts normals array") : NULL;

    int idx = 0;
    if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    VMOVE((*pts)[idx], pn->v);
	    if (nrms && *nrms) VMOVE((*nrms)[idx], pn->n);
	    idx++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	struct pnt_color_normal *pcn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pcn, pnt_color_normal, &(pl->l))) {
	    VMOVE((*pts)[idx], pcn->v);
	    if (nrms && *nrms) VMOVE((*nrms)[idx], pcn->n);
	    idx++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *psn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(psn, pnt_scale_normal, &(pl->l))) {
	    VMOVE((*pts)[idx], psn->v);
	    if (nrms && *nrms) VMOVE((*nrms)[idx], psn->n);
	    idx++;
	}
    }
    if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	struct pnt_color_scale_normal *pcsn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pcsn, pnt_color_scale_normal, &(pl->l))) {
	    VMOVE((*pts)[idx], pcsn->v);
	    if (nrms && *nrms) VMOVE((*nrms)[idx], pcsn->n);
	    idx++;
	}
    }
    if (!has_normals && (pnts->type == RT_PNT_TYPE_PNT || pnts->type == RT_PNT_TYPE_COL || pnts->type == RT_PNT_TYPE_SCA || pnts->type == RT_PNT_TYPE_COL_SCA)) {
	struct pnt *pn = NULL;
	struct pnt *pl = (struct pnt *)pnts->point;
	for (BU_LIST_FOR(pn, pnt, &(pl->l))) {
	    VMOVE((*pts)[idx], pn->v);
	    idx++;
	}
    }
    return BRLCAD_OK;
}

/* Point-cloud operation schemas are bound directly to the execution records
 * below.  The same definitions also form the nested pnts/tri command tree,
 * so help, validation, completion, and dispatch cannot drift apart. */
struct pnts_root_args { int help; };
struct pnts_tri_unit_args { int help; fastf_t scale; };
struct pnts_tri_ballpivot_args { int help; std::vector<double> radii; };
struct pnts_tri_spsr_args { int help; struct bg_3d_spsr_opts opts; };
struct pnts_gen_args {
    int help;
    fastf_t tolerance;
    int surface;
    int grid;
    int rand;
    int sobol;
    int max_pnts;
    int max_time;
};
struct pnts_read_args {
    int help;
    struct bu_vls format;
    struct bu_vls units;
    fastf_t size;
};
struct pnts_write_args { int help; int precision; int ply; };

static int _pnts_opt_radius(struct bu_vls *, const char *, void *);
static int _pnts_opt_radii(struct bu_vls *, const char *, void *);
static int _pnts_opt_size_t(struct bu_vls *, const char *, void *);
static int _ged_pnts_tri_cmd_unit(void *, int, const char *[]);
static int _ged_pnts_tri_cmd_ballpivot(void *, int, const char *[]);
static int _ged_pnts_tri_cmd_spsr(void *, int, const char *[]);
static int _ged_pnts_cmd_tri(void *, int, const char *[]);
static int _ged_pnts_cmd_gen(void *, int, const char *[]);
static int _ged_pnts_cmd_read(void *, int, const char *[]);
static int _ged_pnts_cmd_write(void *, int, const char *[]);
static void _pnts_show_help(struct ged *);

static const struct bu_cmd_operand pnts_pair_operands[] = {
    BU_CMD_OPERAND("input_pnts", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input point-cloud object", "ged.db_object"),
    BU_CMD_OPERAND("output_bot", BU_CMD_VALUE_STRING, 1, 1,
	"New BOT object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option pnts_tri_unit_options[] = {
    BU_CMD_FLAG("h", "help", pnts_tri_unit_args, help, "Print help"),
    BU_CMD_NUMBER("s", "scale", pnts_tri_unit_args, scale, "scale",
	"Triangle scale factor"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema pnts_tri_unit_schema = {
    "unit", "Generate a triangle for each oriented point", pnts_tri_unit_options,
    pnts_pair_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
/* The optional historical spelling `pnts tri <pnts> <bot>` is the unit
 * operation.  It has its own node name but deliberately reuses the same
 * rows, instead of retaining a second legacy option description. */
static const struct bu_cmd_schema pnts_tri_legacy_schema = {
    "tri", "Generate a triangle for each oriented point", pnts_tri_unit_options,
    pnts_pair_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_tri_ballpivot_options[] = {
    BU_CMD_FLAG("h", "help", pnts_tri_ballpivot_args, help, "Print help"),
    BU_CMD_CUSTOM("r", "radius", pnts_tri_ballpivot_args, radii, _pnts_opt_radius,
	"radius", "Ball radius (repeatable)"),
    BU_CMD_CUSTOM(NULL, "radii", pnts_tri_ballpivot_args, radii, _pnts_opt_radii,
	"r1,r2,...", "Comma-separated ball radii"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema pnts_tri_ballpivot_schema = {
    "ballpivot", "Reconstruct a surface using ball pivoting", pnts_tri_ballpivot_options,
    pnts_pair_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_tri_spsr_options[] = {
    BU_CMD_FLAG("h", "help", pnts_tri_spsr_args, help, "Print help"),
    BU_CMD_INTEGER(NULL, "degree", pnts_tri_spsr_args, opts.degree, "number", "Finite element degree"),
    BU_CMD_INTEGER(NULL, "btype", pnts_tri_spsr_args, opts.btype, "number", "Boundary type"),
    BU_CMD_INTEGER(NULL, "depth", pnts_tri_spsr_args, opts.depth, "number", "Maximum reconstruction depth"),
    BU_CMD_INTEGER(NULL, "kerneldepth", pnts_tri_spsr_args, opts.kerneldepth, "number", "Kernel depth"),
    BU_CMD_INTEGER(NULL, "iterations", pnts_tri_spsr_args, opts.iterations, "number", "Solver iterations"),
    BU_CMD_INTEGER(NULL, "full-depth", pnts_tri_spsr_args, opts.full_depth, "number", "Full depth"),
    BU_CMD_INTEGER(NULL, "base-depth", pnts_tri_spsr_args, opts.base_depth, "number", "Coarse multigrid depth"),
    BU_CMD_INTEGER(NULL, "base-vcycles", pnts_tri_spsr_args, opts.baseVcycles, "number", "Coarse multigrid cycles"),
    BU_CMD_INTEGER(NULL, "max-mem", pnts_tri_spsr_args, opts.max_memory_GB, "number", "Maximum memory in GB"),
    BU_CMD_CUSTOM(NULL, "threads", pnts_tri_spsr_args, opts.threads, _pnts_opt_size_t, "number", "Thread count"),
    BU_CMD_NUMBER(NULL, "samples-per-node", pnts_tri_spsr_args, opts.samples_per_node, "number", "Minimum samples per node"),
    BU_CMD_NUMBER(NULL, "scale", pnts_tri_spsr_args, opts.scale, "number", "Scale factor"),
    BU_CMD_NUMBER(NULL, "width", pnts_tri_spsr_args, opts.width, "number", "Voxel width"),
    BU_CMD_NUMBER(NULL, "confidence", pnts_tri_spsr_args, opts.confidence, "number", "Normal confidence exponent"),
    BU_CMD_NUMBER(NULL, "confidence-bias", pnts_tri_spsr_args, opts.confidence_bias, "number", "Normal confidence bias exponent"),
    BU_CMD_NUMBER(NULL, "cg-accuracy", pnts_tri_spsr_args, opts.cgsolver_accuracy, "number", "CG solver accuracy"),
    BU_CMD_NUMBER(NULL, "point-weight", pnts_tri_spsr_args, opts.point_weight, "number", "Interpolation weight"),
    BU_CMD_INTEGER(NULL, "nonmanifold", pnts_tri_spsr_args, opts.nonManifold, "number", "Permit non-manifold output"),
    BU_CMD_INTEGER(NULL, "linearfit", pnts_tri_spsr_args, opts.linearFit, "number", "Use linear fitting"),
    BU_CMD_INTEGER(NULL, "exact", pnts_tri_spsr_args, opts.exact, "number", "Use exact interpolation"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema pnts_tri_spsr_schema = {
    "spsr", "Reconstruct a surface using screened Poisson reconstruction", pnts_tri_spsr_options,
    pnts_pair_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_tri_options[] = {
    BU_CMD_FLAG("h", "help", pnts_root_args, help, "Print help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema pnts_tri_schema = {
    "tri", "Generate BOT geometry from a point cloud", pnts_tri_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node pnts_tri_subcommands[] = {
    BU_CMD_TREE_NODE(&pnts_tri_unit_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_tri_cmd_unit),
    BU_CMD_TREE_NODE(&pnts_tri_ballpivot_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_tri_cmd_ballpivot),
    BU_CMD_TREE_NODE(&pnts_tri_spsr_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_tri_cmd_spsr),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree pnts_tri_tree = {
    &pnts_tri_schema, pnts_tri_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct bu_cmd_option pnts_gen_options[] = {
    BU_CMD_FLAG("h", "help", pnts_gen_args, help, "Print help"),
    BU_CMD_NUMBER("t", "tolerance", pnts_gen_args, tolerance, "distance", "Sampling grid spacing"),
    BU_CMD_FLAG(NULL, "surface", pnts_gen_args, surface, "Save only first and last ray points"),
    BU_CMD_FLAG(NULL, "grid", pnts_gen_args, grid, "Use gridded sampling"),
    BU_CMD_FLAG(NULL, "rand", pnts_gen_args, rand, "Use random sampling"),
    BU_CMD_FLAG(NULL, "sobol", pnts_gen_args, sobol, "Use Sobol sampling"),
    BU_CMD_INTEGER(NULL, "max-pnts", pnts_gen_args, max_pnts, "number", "Maximum points"),
    BU_CMD_INTEGER(NULL, "max-time", pnts_gen_args, max_time, "seconds", "Maximum sampling time"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pnts_gen_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1, "Object to sample", "ged.db_path"),
    BU_CMD_OPERAND("output_pnts", BU_CMD_VALUE_STRING, 1, 1, "New point-cloud object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema pnts_gen_schema = {
    "gen", "Generate a point cloud from geometry", pnts_gen_options, pnts_gen_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_read_options[] = {
    BU_CMD_FLAG("h", "help", pnts_read_args, help, "Print help"),
    BU_CMD_VLS_APPEND("f", "format", pnts_read_args, format, "format", "Input column format"),
    BU_CMD_VLS_APPEND("u", "units", pnts_read_args, units, "unit", "Input units"),
    BU_CMD_NUMBER(NULL, "size", pnts_read_args, size, "size", "Default point size"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pnts_read_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 1, 1, "Point data file", "ged.file_path"),
    BU_CMD_OPERAND("output_pnts", BU_CMD_VALUE_STRING, 1, 1, "New point-cloud object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema pnts_read_schema = {
    "read", "Read point data from a file", pnts_read_options, pnts_read_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_write_options[] = {
    BU_CMD_FLAG("h", "help", pnts_write_args, help, "Print help"),
    BU_CMD_INTEGER("p", "precision", pnts_write_args, precision, "digits", "Digits after the decimal point"),
    BU_CMD_FLAG(NULL, "ply", pnts_write_args, ply, "Write ASCII PLY format"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pnts_write_operands[] = {
    BU_CMD_OPERAND("input_pnts", BU_CMD_VALUE_DB_OBJECT, 1, 1, "Point-cloud object", "ged.db_object"),
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 1, 1, "New output file", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema pnts_write_schema = {
    "write", "Write point data to a file", pnts_write_options, pnts_write_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_option pnts_root_options[] = {
    BU_CMD_FLAG("h", "help", pnts_root_args, help, "Print help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema pnts_root_schema = {
    "pnts", "Read, write, generate, and triangulate point clouds", pnts_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node pnts_subcommands[] = {
    BU_CMD_TREE_NODE(&pnts_gen_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_cmd_gen),
    BU_CMD_TREE_NODE(&pnts_read_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_cmd_read),
    /* tri keeps an executor to preserve the documented implicit-unit form;
     * its nested children are still exposed to grammar consumers. */
    BU_CMD_TREE_NODE(&pnts_tri_schema, NULL, pnts_tri_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_cmd_tri),
    BU_CMD_TREE_NODE(&pnts_write_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _ged_pnts_cmd_write),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_pnts_tree = {
    &pnts_root_schema, pnts_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static void
pnts_show_schema_help(struct ged *gedp, const char *usage,
	const struct bu_cmd_schema *schema)
{
    char *help = bu_cmd_schema_describe(schema);

    if (usage)
	bu_vls_strcat(gedp->ged_result_str, usage);
    if (help) {
	bu_vls_putc(gedp->ged_result_str, '\n');
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "pnts native schema help");
    }
}

static int
_pnts_write_bot_mesh(struct ged *gedp, const char *bot_name, int *faces, int nfaces, point_t *verts, int nverts)
{
    struct rt_db_internal internal;
    struct directory *dp = NULL;

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];

    struct rt_bot_internal *bot_ip;
    BU_ALLOC(bot_ip, struct rt_bot_internal);
    internal.idb_ptr = (void *)bot_ip;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->mode = RT_BOT_SURFACE;
    bot_ip->orientation = RT_BOT_UNORIENTED;
    bot_ip->num_vertices = (size_t)nverts;
    bot_ip->num_faces = (size_t)nfaces;
    bot_ip->faces = faces;
    bot_ip->vertices = (fastf_t *)verts;
    bot_ip->thickness = NULL;
    bot_ip->face_mode = NULL;
    bot_ip->num_normals = 0;
    bot_ip->num_face_normals = 0;
    bot_ip->normals = NULL;
    bot_ip->face_normals = NULL;
    bot_ip->num_uvs = 0;
    bot_ip->num_face_uvs = 0;
    bot_ip->uvs = NULL;
    bot_ip->face_uvs = NULL;

    GED_DB_DIRADD(gedp, dp, bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);
    return BRLCAD_OK;
}

static int
_ged_pnts_tri_cmd_unit(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;
    const char *usage = "Usage: pnts tri unit [options] <pnts> <output_bot>\n";
    const char *purpose = "Generate unit or scaled triangles for each point in a set, oriented by point normals";
    if (_ged_pnts_cmd_msgs(gedp, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct rt_db_internal intern, internal;
    struct rt_bot_internal *bot_ip;
    struct directory *pnt_dp;
    struct pnts_tri_unit_args args = {0, 0.0};
    struct rt_pnts_internal *pnts = NULL;
    const char *pnt_prim= NULL;
    const char *bot_name = NULL;
    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_unit_schema);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_tri_unit_schema, &args,
	gedp->ged_result_str, argc, argv);

	if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_unit_schema);
	return BRLCAD_OK;
    }
	if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_unit_schema);
	return BRLCAD_ERROR;
    }

    argc -= opt_ret;
    argv += opt_ret;

    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_unit_schema);
	return BRLCAD_ERROR;
    }

    pnt_prim = argv[0];
    bot_name = argv[1];

    /* get pnt */
    GED_DB_LOOKUP(gedp, pnt_dp, pnt_prim, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, pnt_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "pnts tri: %s is not a pnts object!", pnt_prim);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* Sanity */
    if (!bot_name || !gedp) return BRLCAD_ERROR;
    GED_CHECK_EXISTS(gedp, bot_name, LOOKUP_QUIET, BRLCAD_ERROR);

    /* For the moment, only generate BoTs when we have a normal to guide us.  Eventually,
     * we might add logic to find the avg center point and calculate normals radiating out
     * from that center, but for now skip anything that doesn't provide normals up front. */
    int ncnt = 0;
    point_t *ipts = NULL;
    vect_t *inrms = NULL;
    if (_pnts_collect_arrays(gedp, pnts, &ipts, &inrms, &ncnt, 1) != BRLCAD_OK) {
	rt_db_free_internal(&intern);
	if (ipts) bu_free(ipts, "ipts");
	if (inrms) bu_free(inrms, "inrms");
	return BRLCAD_ERROR;
    }

    // Set default scale if not specified
    if (NEAR_ZERO(args.scale, SMALL_FASTF)) {
	switch (pnts->type) {
	    case RT_PNT_TYPE_SCA_NRM:
	    case RT_PNT_TYPE_COL_SCA_NRM:
		args.scale = pnts->scale; break;
	    default: args.scale = 1.0; break;
	}
    }

    /* Set up BoT container */
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(bot_ip, struct rt_bot_internal);
    internal.idb_ptr = (void *)bot_ip;
    bot_ip = (struct rt_bot_internal *)internal.idb_ptr;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->mode = RT_BOT_SURFACE;
    bot_ip->orientation = 2;

    /* Allocate BoT memory */
    bot_ip->num_vertices = pnts->count * 3;
    bot_ip->num_faces = pnts->count;
    bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3 + 3, sizeof(int), "bot faces");
    bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3 + 3, sizeof(fastf_t), "bot vertices");
    bot_ip->thickness = (fastf_t *)NULL;
    bot_ip->face_mode = (struct bu_bitv *)NULL;

    auto pnt_to_tri = [](point_t *p, vect_t *n, struct rt_bot_internal *botp, fastf_t tscale, unsigned long pntcnt) {
	fastf_t ty1 =  0.57735026918962573 * tscale; // tan(PI/6)
	fastf_t ty2 = -0.28867513459481287 * tscale; // 0.5 * tan(PI/6)
	fastf_t tx1 = 0.5 * tscale;
	point_t v1, v2, v3;
	vect_t n1;
	vect_t v1pp, v2pp, v3pp = {0.0, 0.0, 0.0};
	vect_t v1fp, v2fp, v3fp = {0.0, 0.0, 0.0};
	mat_t rot;
	struct bn_tol btol = BN_TOL_INIT_TOL;

	VSET(n1, 0, 0, 1);
	VSET(v1, 0, ty1, 0);
	VSET(v2, -1*tx1, ty2, 0);
	VSET(v3, tx1, ty2, 0);

	VMOVE(v1pp, v1);
	VMOVE(v2pp, v2);
	VMOVE(v3pp, v3);
	bn_mat_fromto(rot, n1, *n, &btol);
	MAT4X3VEC(v1fp, rot, v1pp);
	MAT4X3VEC(v2fp, rot, v2pp);
	MAT4X3VEC(v3fp, rot, v3pp);
	VADD2(v1pp, v1fp, *p);
	VADD2(v2pp, v2fp, *p);
	VADD2(v3pp, v3fp, *p);
	VMOVE(&botp->vertices[pntcnt*3*3], v1pp);
	VMOVE(&botp->vertices[pntcnt*3*3+3], v2pp);
	VMOVE(&botp->vertices[pntcnt*3*3+6], v3pp);
	botp->faces[pntcnt*3] = (int)(pntcnt*3);
	botp->faces[pntcnt*3+1] = (int)(pntcnt*3+1);
	botp->faces[pntcnt*3+2] = (int)(pntcnt*3+2);
    };

    for (int i = 0; i < ncnt; i++) {
	pnt_to_tri(&ipts[i], &inrms[i], bot_ip, args.scale, (unsigned long)i);
    }

    struct directory *dp = NULL;
    GED_DB_DIRADD(gedp, dp, bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated BoT object %s with %d triangles", bot_name, ncnt);

    // cleanup
    rt_db_free_internal(&intern);
    if (ipts)
	bu_free(ipts, "ipts");
    if (inrms)
	bu_free(inrms, "inrms");

    return BRLCAD_OK;
}

/* Native option consumers for ballpivot radii. */
static int
_pnts_opt_radius(struct bu_vls *msg, const char *arg, void *set_c)
{
    std::vector<double> *rset = (std::vector<double> *)set_c;
    fastf_t rv = 0.0;
    if (!arg || !bu_cmd_number_from_str(&rv, arg)) {
	if (msg)
	    bu_vls_printf(msg, "invalid ball radius: %s\n", arg ? arg : "");
	return -1;
    }
    rset->push_back(rv);
    return 0;
}
static int
_pnts_opt_radii(struct bu_vls *msg, const char *arg, void *set_c)
{
    std::vector<double> *rset = (std::vector<double> *)set_c;
    if (!arg)
	return -1;
    std::string s(arg);
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
	if (item.empty()) continue;
	fastf_t rv = 0.0;
	if (!bu_cmd_number_from_str(&rv, item.c_str())) {
	    if (msg)
		bu_vls_printf(msg, "invalid ball radius: %s\n", item.c_str());
	    return -1;
	}
	rset->push_back(rv);
    }
    return 0;
}

/* tri ballpivot */
static int
_ged_pnts_tri_cmd_ballpivot(void *bs, int argc, const char **argv)
{
    const char *usage = "Usage: pnts tri ballpivot [options] <pnts> <output_bot>\n";
    const char *purpose = "Surface reconstruction from oriented points using Ball Pivoting";
    struct ged *gedp = (struct ged *)bs;
    if (_ged_pnts_cmd_msgs(gedp, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct pnts_tri_ballpivot_args args = {0, {}};

    argc -= (argc>0); argv += (argc>0); // skip "ballpivot"
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_ballpivot_schema);
	return BRLCAD_OK;
    }
    int opt_ret = bu_cmd_schema_parse(&pnts_tri_ballpivot_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_ballpivot_schema);
	return BRLCAD_OK;
    }
    if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_ballpivot_schema);
	return BRLCAD_ERROR;
    }
    argc -= opt_ret;
    argv += opt_ret;
    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_ballpivot_schema);
	return BRLCAD_ERROR;
    }

    const char *pnt_prim = argv[0];
    const char *bot_name = argv[1];
    GED_CHECK_EXISTS(gedp, bot_name, LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_db_internal intern;
    struct directory *dp = NULL;
    GED_DB_LOOKUP(gedp, dp, pnt_prim, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a PNTS primitive", __func__, pnt_prim);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    // collect arrays (normals required)
    int pcnt = 0;
    point_t *ipts = NULL;
    vect_t *inrms = NULL;
    if (_pnts_collect_arrays(gedp, pnts, &ipts, &inrms, &pcnt, 1) != BRLCAD_OK) {
	rt_db_free_internal(&intern);
	if (ipts) bu_free(ipts, "ipts");
	if (inrms) bu_free(inrms, "inrms");
	return BRLCAD_ERROR;
    }

    // radii array (optional)
    double *rptr = NULL;
    int rcnt = (int)args.radii.size();
    if (rcnt > 0) {
	rptr = (double *)bu_calloc((size_t)rcnt, sizeof(double), "bp radii");
	for (int i = 0; i < rcnt; i++) rptr[i] = args.radii[(size_t)i];
    }

    // run ballpivot
    int *faces = NULL;
    int nfaces = 0;
    point_t *overts = NULL;
    int nverts = 0;

    int bret = bg_3d_ballpivot(&faces, &nfaces, &overts, &nverts, (const point_t *)ipts, (const vect_t *)inrms, pcnt, rptr, rcnt);
    if (rptr) bu_free(rptr, "bp radii");
    // free input arrays
    if (ipts)
	bu_free(ipts, "ipts");
    ipts = NULL;
    if (inrms)
	bu_free(inrms, "inrms");
    inrms = NULL;

    if (bret != 0 || nfaces <= 0 || nverts <= 0) {
	bu_vls_printf(gedp->ged_result_str, "Ball Pivoting reconstruction failed");
	rt_db_free_internal(&intern);
	// faces/overts may be NULL or partially allocated; nothing else to do
	return BRLCAD_ERROR;
    }

    // write bot
    int wret = _pnts_write_bot_mesh(gedp, bot_name, faces, nfaces, overts, nverts);
    if (wret == BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Generated BoT object %s (Ball Pivoting) with %d faces", bot_name, nfaces);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Failed to write BoT object %s", bot_name);
    }

    rt_db_free_internal(&intern);
    return wret;
}

/* Native parser for the unsigned thread-count option. */
static int
_pnts_opt_size_t(struct bu_vls *msg, const char *arg, void *set_c)
{
    size_t *t = (size_t *)set_c;
    int tmp = 0;
    if (!arg || !bu_cmd_integer_from_str(&tmp, arg) || tmp < 0) {
	if (msg)
	    bu_vls_printf(msg, "non-negative thread count required: %s\n", arg ? arg : "");
	return -1;
    }
    *t = (size_t)tmp;
    return 0;
}

/* tri spsr */
static int
_ged_pnts_tri_cmd_spsr(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;
    const char *usage = "Usage: pnts tri spsr [options] <pnts> <output_bot>\n";
    const char *purpose = "Screened Poisson Surface Reconstruction from oriented points";
    if (_ged_pnts_cmd_msgs(gedp, argc, argv, usage, purpose)) return BRLCAD_OK;

    struct pnts_tri_spsr_args args = {0, BG_3D_SPSR_OPTS_DEFAULT};

    argc -= (argc>0); argv += (argc>0); // skip "spsr"
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_spsr_schema);
	return BRLCAD_OK;
    }
    int opt_ret = bu_cmd_schema_parse(&pnts_tri_spsr_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_spsr_schema);
	return BRLCAD_OK;
    }
    if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_spsr_schema);
	return BRLCAD_ERROR;
    }
    argc -= opt_ret;
    argv += opt_ret;
    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_tri_spsr_schema);
	return BRLCAD_ERROR;
    }

    const char *pnt_prim = argv[0];
    const char *bot_name = argv[1];
    GED_CHECK_EXISTS(gedp, bot_name, LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_db_internal intern;
    struct directory *dp = NULL;
    GED_DB_LOOKUP(gedp, dp, pnt_prim, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a PNTS primitive", __func__, pnt_prim);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    // collect arrays (normals required)
    int pcnt = 0;
    point_t *ipts = NULL;
    vect_t *inrms = NULL;
    if (_pnts_collect_arrays(gedp, pnts, &ipts, &inrms, &pcnt, 1) != BRLCAD_OK) {
	rt_db_free_internal(&intern);
	if (ipts) bu_free(ipts, "ipts");
	if (inrms) bu_free(inrms, "inrms");
	return BRLCAD_ERROR;
    }

    // run spsr
    int *faces = NULL;
    int nfaces = 0;
    point_t *overts = NULL;
    int nverts = 0;

    int sret = bg_3d_spsr(&faces, &nfaces, &overts, &nverts, (const point_t *)ipts, (const vect_t *)inrms, pcnt, &args.opts);

    // free inputs
    if (ipts)
	bu_free(ipts, "ipts");
    ipts = NULL;
    if (inrms)
	bu_free(inrms, "inrms");
    inrms = NULL;

    if (sret != 0 || nfaces <= 0 || nverts <= 0) {
	bu_vls_printf(gedp->ged_result_str, "SPSR reconstruction failed");
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    // write bot
    int wret = _pnts_write_bot_mesh(gedp, bot_name, faces, nfaces, overts, nverts);
    if (wret == BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Generated BoT object %s (SPSR) with %d faces", bot_name, nfaces);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Failed to write BoT object %s", bot_name);
    }

    rt_db_free_internal(&intern);
    return wret;
}


// NOTE - for the moment, we're deliberately not documenting the ballpivot
// option - in its current form it is primarily useful for algorithm
// experimentation and not real-world use.
static void
_pnts_tri_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&pnts_tri_tree);

    bu_vls_strcat(gedp->ged_result_str,
	"Usage: pnts tri <subcommand> [options] <pnts> <output_bot>\n");
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "pnts tri native tree help");
    }
}

static int
_ged_pnts_cmd_tri(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;

    struct pnts_root_args args = {0};

    argc--; argv++; // skip "tri"

    if (!argc) {
	_pnts_tri_show_help(gedp);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_tri_schema, &args,
	gedp->ged_result_str, argc, argv);

    if (args.help) {
	_pnts_tri_show_help(gedp);
	return BRLCAD_OK;
    }
    if (opt_ret < 0) {
	_pnts_tri_show_help(gedp);
	return BRLCAD_ERROR;
    }
    argc -= opt_ret;
    argv += opt_ret;

	const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(&pnts_tri_tree,
	argc ? argv[0] : NULL);
    if (!node) {
	/* The historical spelling defaults to unit when the first word is not
	 * one of the explicit tri operations. */
	std::vector<const char *> nargv;
	nargv.push_back("unit");
	for (int i = 0; i < argc; i++) nargv.push_back(argv[i]);
	return _ged_pnts_tri_cmd_unit(bs, (int)nargv.size(), nargv.data());
    }

    int ret = BRLCAD_ERROR;
    if (bu_cmd_tree_dispatch(&pnts_tri_tree, bs, argc, argv, &ret) == 0)
	return ret;
    _pnts_tri_show_help(gedp);
    return BRLCAD_ERROR;
}


static int
_ged_pnts_cmd_gen(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;
    struct directory *dp;
    struct pnts_gen_args args = {0, 0.0, 0, 0, 0, 0, 0, 0};
    int flags = 0;
    double avg_thickness = 0.0;
    struct rt_db_internal internal;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    struct rt_pnts_internal *pnts = NULL;
    const char *pnt_prim= NULL;
    const char *obj_name = NULL;
    const char *usage = "Usage: pnts gen [options] <obj> <output_pnts>\n\n";

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_gen_schema);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_gen_schema, &args,
	gedp->ged_result_str, argc, argv);

	if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_gen_schema);
	return BRLCAD_OK;
    }
	if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_gen_schema);
	return BRLCAD_ERROR;
	}

    argc -= opt_ret;
    argv += opt_ret;

    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_gen_schema);
	return BRLCAD_ERROR;
    }

    obj_name = argv[0];
    pnt_prim = argv[1];

    /* Sanity */
    if (db_lookup(gedp->dbip, obj_name, LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s doesn't exist!\n", obj_name);
	return BRLCAD_ERROR;
    }
    GED_CHECK_EXISTS(gedp, pnt_prim, LOOKUP_QUIET, BRLCAD_ERROR);


    if (args.surface) {
	flags |= RT_GEN_OBJ_PNTS_SURF;
    }

    /* Pick our mode(s) */
    if (!args.grid && !args.rand && !args.sobol) {
	flags |= RT_GEN_OBJ_PNTS_GRID;
    } else {
	if (args.grid)  flags |= RT_GEN_OBJ_PNTS_GRID;
	if (args.rand)  flags |= RT_GEN_OBJ_PNTS_RAND;
	if (args.sobol) flags |= RT_GEN_OBJ_PNTS_SOBOL;
    }

    /* If we don't have a tolerance, try to guess something sane from the bbox */
    if (NEAR_ZERO(args.tolerance, RT_LEN_TOL)) {
	point_t rpp_min, rpp_max;
	point_t obj_min, obj_max;
	VSETALL(rpp_min, INFINITY);
	VSETALL(rpp_max, -INFINITY);
	rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&obj_name, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	args.tolerance = DIST_PNT_PNT(rpp_max, rpp_min) * 0.01;
	bu_log("Note - no tolerance specified, using %f\n", args.tolerance);
    }
    btol.dist = args.tolerance;

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->type = RT_PNT_TYPE_NRM;

    if (rt_gen_obj_pnts(pnts, &avg_thickness, gedp->dbip, obj_name, &btol, flags,
	args.max_pnts, args.max_time, 2)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: point generation failed\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %ld points, avg. partition thickness %g", pnt_prim, pnts->count, avg_thickness);

    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    return BRLCAD_OK;
}

int
_pnt_read(struct rt_pnts_internal *pnts, int numcnt, const char **nums, const char *fmt, fastf_t conv_factor)
{
    int i = 0;
    char fc = '\0';
    void *point = _ged_pnts_new_pnt(pnts->type);
    for (i = 0; i < numcnt; i++) {
	fastf_t val;
	fc = fmt[i];

	// trim trailing whitespace
	char* num = (char*)nums[i];
	int j = strlen(nums[i]) - 1;
	while (j > -1) {
	    if (num[j] == ' ') {
		j--;
	    } else {
		num[j+1] = '\0';
		break;
	    }
	}

	if (!bu_cmd_number_from_str(&val, num)) {
	    bu_log("Error - failed to read number %s\n", nums[i]);
	    return BRLCAD_ERROR;
	}
	if ((fc == 'x') || (fc == 'y') || (fc == 'z')) {
	    _ged_pnt_v_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'i') || (fc == 'j') || (fc == 'k')) {
	    _ged_pnt_n_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
	if ((fc == 'r') || (fc == 'g') || (fc == 'b')) {
	    _ged_pnt_c_set(point, pnts->type, fc, val / 255.0);
	    continue;
	}
	if (fc == 's') {
	    _ged_pnt_s_set(point, pnts->type, fc, val * conv_factor);
	    continue;
	}
    }

    _ged_pnts_add(pnts, point);

    return BRLCAD_OK;
}


static int
_ged_pnts_cmd_read(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;
    FILE *fp;
    struct rt_db_internal internal;
    struct rt_pnts_internal *pnts = NULL;
    struct directory *dp;
    const char *pnt_prim = NULL;
    const char *filename = NULL;
    const char *usage = "Usage: pnts read [options] <input_file> <pnts_obj> \n\nReads in point data.\n\n";
    struct bu_vls fl  = BU_VLS_INIT_ZERO;
    struct pnts_read_args args = {0, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, 0.0};
    char **nums = NULL;
    int numcnt = 0;
    int pnts_cnt = 0;
    int array_size = 0;
    fastf_t conv_factor = 1.0;

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_read_schema);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_read_schema, &args,
	gedp->ged_result_str, argc, argv);

	if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_read_schema);
	bu_vls_free(&args.format);
	bu_vls_free(&args.units);
	return BRLCAD_OK;
    }
	if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_read_schema);
	bu_vls_free(&args.format);
	bu_vls_free(&args.units);
	return BRLCAD_ERROR;
	}

    argc -= opt_ret;
    argv += opt_ret;

    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_read_schema);
	bu_vls_free(&args.format);
	bu_vls_free(&args.units);
	return BRLCAD_ERROR;
    }

    /* got a unit, see if we can do something with it */
	if (bu_vls_strlen(&args.units)) {
	const char *cunit = bu_vls_addr(&args.units);
	if (!bu_cmd_number_from_str(&conv_factor, cunit)) {
	    conv_factor = bu_mm_value(bu_vls_addr(&args.units));
	}
	if (conv_factor < 0) {
	    bu_vls_sprintf(gedp->ged_result_str, "invalid unit specification: %s\n", bu_vls_addr(&args.units));
	    bu_vls_free(&args.units);
	    bu_vls_free(&args.format);
	    return BRLCAD_ERROR;
	}
    }

    filename = argv[0];
    pnt_prim = argv[1];

    if (!bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s does not exist\n", filename);
	bu_vls_free(&args.units);
	bu_vls_free(&args.format);
	return BRLCAD_ERROR;
    }

    if (db_lookup(gedp->dbip, pnt_prim, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: object %s already exists\n", pnt_prim);
	bu_vls_free(&args.units);
	bu_vls_free(&args.format);
	return BRLCAD_ERROR;
    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Could not open file '%s'.\n", filename);
	bu_vls_free(&args.units);
	bu_vls_free(&args.format);
	return BRLCAD_ERROR;
    }

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &OBJ[ID_PNTS];
    BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
    pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = args.size;
    pnts->type = _ged_pnts_fmt_type(bu_vls_addr(&args.format));
    if (pnts->type != RT_PNT_UNKNOWN) {
	pnts->point = _ged_pnts_new_pnt(pnts->type);
	_ged_pnts_init_head_pnt(pnts);
    }

    while (!(bu_vls_gets(&fl, fp) < 0)) {

	char *input = bu_strdup(bu_vls_addr(&fl));
	/* Because a valid point array will always have many fewer numbers than characters,
	 * we should only need to allocate this array once.  However, guard against a
	 * pathologic case by making sure our array size is at least strlen(input) large */
	if (!nums || ((int)strlen(input) > array_size)) {
	    if (nums) bu_free(nums, "free old nums array");
	    nums = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	    array_size = strlen(input);
	}
	numcnt = bu_argv_from_string(nums, strlen(input), input);

	if (!numcnt) {
	    bu_vls_trunc(&fl, 0);
	    bu_free(input, "input cpy");
	    continue;
	}

	if (pnts->type == RT_PNT_UNKNOWN) {
	    /* If we don't have a specified format, try to guess */
	    bu_log("Warning - no point format specified, trying to match template xyz[ijk][s][rgb]\n");
	    pnts->type = _ged_pnts_fmt_guess(numcnt);
	    if (pnts->type != RT_PNT_UNKNOWN) {
		pnts->point = _ged_pnts_new_pnt(pnts->type);
		_ged_pnts_init_head_pnt(pnts);
		bu_vls_sprintf(&args.format, "%s", _ged_pnt_default_fmt_str(pnts->type));
		bu_log("Assuming format %s\n", bu_vls_addr(&args.format));
	    }
	}

	/* Validate numcnt against type */
	if (!_ged_pnts_fmt_match(pnts->type, numcnt)) {
	    if (pnts->type == RT_PNT_UNKNOWN) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: could not determine point type, aborting\n");
	    } else {
		bu_vls_sprintf(gedp->ged_result_str, "found invalid number count %d for point type, aborting:\n%s\n", numcnt, bu_vls_addr(&fl));
	    }
	    rt_db_free_internal(&internal);
	    bu_vls_free(&args.format);
	    bu_vls_free(&args.units);
	    bu_free(input, "input cpy");
	    bu_free(nums, "nums array");
	    fclose(fp);
	    return BRLCAD_ERROR;
	}

	_pnt_read(pnts, numcnt, (const char **)nums, bu_vls_addr(&args.format), conv_factor);
	pnts_cnt++;
	bu_vls_trunc(&fl, 0);
	bu_free(input, "input cpy");
    }

    pnts->count = pnts_cnt;
    fclose(fp);

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %ld points", pnt_prim, pnts->count);

    GED_DB_DIRADD(gedp, dp, pnt_prim, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    bu_vls_free(&args.format);
    bu_vls_free(&args.units);
    if (nums) bu_free(nums, "free old nums array");
    return BRLCAD_OK;
}

static int
_ged_pnts_cmd_write(void *bs, int argc, const char **argv)
{
    struct ged *gedp = (struct ged *)bs;
    struct pnts_write_args args = {0, 0, 0};
    FILE *fp;
    struct rt_db_internal intern;
    struct rt_pnts_internal *pnts = NULL;
    struct directory *pnt_dp;
    struct bu_vls pnt_str = BU_VLS_INIT_ZERO;
    const char *pnt_prim = NULL;
    const char *filename = NULL;
    const char *usage = "Usage: pnts write [options] <pnts_obj> <output_file>\n\nWrites out data based on the point type, one row per point, using a format of x y z [i j k] [scale] [R G B] (bracketed groups may or may not be present depending on point type.)\n\n";
    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	pnts_show_schema_help(gedp, usage, &pnts_write_schema);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_write_schema, &args,
	gedp->ged_result_str, argc, argv);

	if (args.help) {
	pnts_show_schema_help(gedp, usage, &pnts_write_schema);
	return BRLCAD_OK;
    }
	if (opt_ret < 0) {
	pnts_show_schema_help(gedp, usage, &pnts_write_schema);
	return BRLCAD_ERROR;
	}

    argc -= opt_ret;
    argv += opt_ret;

    if (argc != 2) {
	pnts_show_schema_help(gedp, usage, &pnts_write_schema);
	return BRLCAD_ERROR;
    }

    pnt_prim = argv[0];
    filename = argv[1];

    if (bu_file_exists(filename, NULL)) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: file %s already exists\n", filename);
	return BRLCAD_ERROR;
    }

    /* get pnt */
    GED_DB_LOOKUP(gedp, pnt_dp, pnt_prim, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, pnt_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "pnts write: %s is not a pnts object!", pnt_prim);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    pnts = (struct rt_pnts_internal *)intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->type == RT_PNT_UNKNOWN) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: unknown pnts type\n");
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    /* Write points */
    fp = fopen(filename, "wb+");
    if (fp == NULL) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: cannot open file %s for writing\n", filename);
	return BRLCAD_ERROR;
    }

    if (args.ply) {
	fprintf(fp, "ply\nformat ascii 1.0\ncomment %s\n", pnt_dp->d_namep);
	fprintf(fp, "element vertex %ld\n", pnts->count);
	fprintf(fp, "property double x\n");
	fprintf(fp, "property double y\n");
	fprintf(fp, "property double z\n");
	if (pnts->type == RT_PNT_TYPE_NRM || pnts->type == RT_PNT_TYPE_SCA_NRM
	   || pnts->type == RT_PNT_TYPE_COL_NRM || pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	    fprintf(fp, "property double nx\n");
	    fprintf(fp, "property double ny\n");
	    fprintf(fp, "property double nz\n");
	}
	if (pnts->type == RT_PNT_TYPE_COL || pnts->type == RT_PNT_TYPE_COL_SCA
	   || pnts->type == RT_PNT_TYPE_COL_NRM || pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	    fprintf(fp, "property uchar red\n");
	    fprintf(fp, "property uchar green\n");
	    fprintf(fp, "property uchar blue\n");
	}
	fprintf(fp, "element face 0\n");
	fprintf(fp, "end_header\n");
    }

    if (pnts->type == RT_PNT_TYPE_PNT) {
	struct pnt *pn = NULL;
	struct pnt *pl = (struct pnt *)pnts->point;
	for (BU_LIST_FOR(pn, pnt, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		if (i != 2) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }


    if (pnts->type == RT_PNT_TYPE_COL) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color *pn = NULL;
	struct pnt_color *pl = (struct pnt_color *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return BRLCAD_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_SCA) {
	struct pnt_scale *pn = NULL;
	struct pnt_scale *pl = (struct pnt_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		if (i != 2 || (i == 2 && !args.ply)) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }

	    /* TODO - not sure how to handle scale with PLY */
	    if (!args.ply) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->s, args.precision);
		fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_NRM) {
	struct pnt_normal *pn = NULL;
	struct pnt_normal *pl = (struct pnt_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], args.precision);
		if (i != 2) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_SCA) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale *pn = NULL;
	struct pnt_color_scale *pl = (struct pnt_color_scale *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    /* TODO - not sure how to handle scale with PLY */
	    if (!args.ply) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->s, args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return BRLCAD_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_normal *pn = NULL;
	struct pnt_color_normal *pl = (struct pnt_color_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return BRLCAD_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_SCA_NRM) {
	struct pnt_scale_normal *pn = NULL;
	struct pnt_scale_normal *pl = (struct pnt_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_scale_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], args.precision);
		if (i != 2 || (i == 2 && !args.ply)) {
		    fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
		} else {
		    fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
		}
	    }
	    /* TODO - not sure how to handle scale with PLY */
	    if (!args.ply) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->s, args.precision);
		fprintf(fp, "%s\n", bu_vls_addr(&pnt_str));
	    }
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    if (pnts->type == RT_PNT_TYPE_COL_SCA_NRM) {
	unsigned char rgb[3] = {0, 0, 0};
	struct pnt_color_scale_normal *pn = NULL;
	struct pnt_color_scale_normal *pl = (struct pnt_color_scale_normal *)pnts->point;
	for (BU_LIST_FOR(pn, pnt_color_scale_normal, &(pl->l))) {
	    int i = 0;
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->v[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    for (i = 0; i < 3; i++) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->n[i], args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    /* TODO - not sure how to handle scale with PLY */
	    if (!args.ply) {
		_pnts_fastf_t_to_vls(&pnt_str, pn->s, args.precision);
		fprintf(fp, "%s ", bu_vls_addr(&pnt_str));
	    }
	    if (!bu_color_to_rgb_chars(&(pn->c), rgb)) {
		bu_vls_sprintf(gedp->ged_result_str, "Error: cannot process point color\n");
		rt_db_free_internal(&intern);
		fclose(fp);
		return BRLCAD_ERROR;
	    }
	    fprintf(fp, "%d %d %d\n", rgb[RED], rgb[GRN], rgb[BLU]);
	}
	rt_db_free_internal(&intern);
	fclose(fp);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Error - pnts write: unsupported point type");
    rt_db_free_internal(&intern);
    fclose(fp);
    return BRLCAD_OK;
}

static void
_pnts_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_pnts_tree);

    bu_vls_strcat(gedp->ged_result_str,
	"Usage: pnts [options] <subcommand> [subcommand arguments]\n");
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "pnts native tree help");
    }
}

extern "C" int
ged_pnts_core(struct ged *gedp, int argc, const char *argv[])
{
    struct pnts_root_args args = {0};

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_pnts_show_help(gedp);
	return BRLCAD_OK;
    }

    int opt_ret = bu_cmd_schema_parse(&pnts_root_schema, &args,
	gedp->ged_result_str, argc, argv);

	if (args.help) {
	_pnts_show_help(gedp);
	return BRLCAD_OK;
    }
    if (opt_ret < 0) {
	_pnts_show_help(gedp);
	return BRLCAD_ERROR;
    }

    argc -= opt_ret;
    argv += opt_ret;

    int ret = BRLCAD_ERROR;
    if (bu_cmd_tree_dispatch(&ged_pnts_tree, gedp, argc, argv, &ret) == 0) {
	return ret;
    }

    bu_vls_printf(gedp->ged_result_str, "pnts: subcommand %s not defined\n",
	argc && argv[0] ? argv[0] : "");
	_pnts_show_help(gedp);
    return BRLCAD_ERROR;
}

/* Compatibility wrapper for the old make_pnts command ordering/prompting:
 *
 * Input values:
 * argv[1] object name
 * argv[2] filename with path
 * argv[3] point data file format string
 * argv[4] point data file units string or conversion factor to millimeters
 * argv[5] default size of each point
 */
int
ged_make_pnts_core(struct ged *gedp, int argc, const char *argv[])
{
    double conv_factor = -1.0;
    double psize = -1.0;
    char *endp = (char *)NULL;
    static const char *usage = "point_cloud_name filename_with_path file_format file_data_units default_point_size";
    static const char *prompt[] = {
	"Enter point-cloud name: ",
	"Enter point file path and name: ",
	"Enter file data format (xyzrgbsijk?): ",
	"Enter file data units (um|mm|cm|m|km|in|ft|yd|mi)\nor conversion factor from file data units to millimeters: ",
	"Enter default point size: "
    };
    static const char *r_cmd = "read";
    static const char *s_opt = "--size";
    static const char *f_opt = "--format";
    static const char *u_opt = "--units";
    const char *nargv[10];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* prompt for point-cloud name */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[0]);
	return GED_MORE;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    /* prompt for data file name with path */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[1]);
	return GED_MORE;
    }

    /* prompt for data file format */
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[2]);
	return GED_MORE;
    }

    /* Validate 'point file data format string' and return point-cloud type. */
    if ((_ged_pnts_fmt_type(argv[3]) == RT_PNT_UNKNOWN)) {
	bu_vls_printf(gedp->ged_result_str, "could not validate format string: %s\n", argv[3]);
	return BRLCAD_ERROR;
    }

    /* prompt for data file units */
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[3]);
	return GED_MORE;
    }

    /* Validate unit */
	if (!bu_cmd_number_from_str(&conv_factor, argv[4])) {
	conv_factor = bu_mm_value(argv[4]);
    }
    if (conv_factor < 0) {
	bu_vls_sprintf(gedp->ged_result_str, "invalid unit specification: %s\n", argv[4]);
	return BRLCAD_ERROR;
    }

    /* prompt for default point size */
    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[4]);
	return GED_MORE;
    }

    psize = strtod(argv[5], &endp);
    if ((endp != argv[5]) && (*endp == '\0')) {
	/* convert to double success */
	if (psize < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "Default point size '%lf' must be non-negative.\n", psize);
	    return BRLCAD_ERROR;
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid default point size '%s'\n", argv[5]);
	return BRLCAD_ERROR;
    }

    nargv[0] = "pnts";
    nargv[1] = r_cmd;
    nargv[2] = s_opt;
    nargv[3] = argv[5];
    nargv[4] = f_opt;
    nargv[5] = argv[3];
    nargv[6] = u_opt;
    nargv[7] = argv[4];
    nargv[8] = argv[2];
    nargv[9] = argv[1];

    return ged_exec_pnts(gedp, 10, (const char **)nargv);
}

#include "../include/plugin.h"

static const struct bu_cmd_operand make_pnts_operands[] = {
    BU_CMD_OPERAND("point_cloud_name", BU_CMD_VALUE_STRING, 1, 1,
	"New point-cloud object name", NULL),
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 1, 1,
	"Point data file", "ged.file_path"),
    BU_CMD_OPERAND("format", BU_CMD_VALUE_STRING, 1, 1,
	"Point data column format", NULL),
    BU_CMD_OPERAND("units", BU_CMD_VALUE_STRING, 1, 1,
	"Point data units", NULL),
    BU_CMD_OPERAND("default_size", BU_CMD_VALUE_NUMBER, 1, 1,
	"Default point size", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema make_pnts_schema = {
    "make_pnts", "Read point data using the legacy argument order", NULL,
    make_pnts_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node pnts_legacy_subcommands[] = {
    BU_CMD_TREE_NODE(&pnts_gen_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&pnts_read_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&pnts_tri_legacy_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&pnts_write_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree pnts_legacy_tree = {
    &pnts_root_schema, pnts_legacy_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static int
ged_pnts_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    int ret = ged_cmd_tree_validate(gedp, &ged_pnts_tree, input, cursor_pos, result);

    /* The long-standing implicit-unit form takes effect only when the word
     * after `tri` is not a recognized tri subcommand or its prefix. */
    if (ret == 0 && result->state == BU_CMD_VALIDATE_INVALID && result->hint &&
	BU_STR_EQUAL(result->hint, "unknown subcommand")) {
	struct ged_cmd_validate_result legacy = GED_CMD_VALIDATE_RESULT_NULL;
	if (ged_cmd_tree_validate(gedp, &pnts_legacy_tree, input, cursor_pos, &legacy) == 0 &&
	    legacy.state != BU_CMD_VALIDATE_INVALID) {
	    ged_cmd_validate_result_clear(result);
	    *result = legacy;
	    return 0;
	}
	ged_cmd_validate_result_clear(&legacy);
    }
    return ret;
}

static int
ged_pnts_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    int ret = ged_cmd_tree_analyze(gedp, &ged_pnts_tree, input, analysis);

    if (ret == 0 && analysis->token_count > 2 &&
	analysis->tokens[1].role == GED_CMD_TOKEN_SUBCOMMAND &&
	analysis->tokens[1].char_end - analysis->tokens[1].char_start == 3 &&
	strncmp(input + analysis->tokens[1].char_start, "tri", 3) == 0 &&
	analysis->tokens[2].role == GED_CMD_TOKEN_SUBCOMMAND &&
	analysis->tokens[2].semantic_state == GED_CMD_SEMANTIC_INVALID)
	return ged_cmd_tree_analyze(gedp, &pnts_legacy_tree, input, analysis);
    return ret;
}

static char *
ged_pnts_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_pnts_tree);
}

static int
ged_pnts_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_pnts_tree, msgs) +
	bu_cmd_tree_lint(&pnts_legacy_tree, msgs);
}

static const struct ged_cmd_grammar ged_pnts_grammar = {
    "pnts", "Read, write, generate, and triangulate point clouds",
    ged_pnts_grammar_validate, ged_pnts_grammar_analyze,
    ged_pnts_grammar_json, ged_pnts_grammar_lint
};

#define GED_PNTS_COMMANDS(X, XID, N, NID, G, GID) \
    N(make_pnts, ged_make_pnts_core, GED_CMD_DEFAULT, &make_pnts_schema) \
    G(pnts, ged_pnts_core, GED_CMD_DEFAULT, &ged_pnts_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_PNTS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_pnts", 1, GED_PNTS_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
