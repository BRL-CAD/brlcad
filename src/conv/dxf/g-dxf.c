/*                         G - D X F . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file g-dxf.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a DXF file by
 * calling on the NMG booleans.  Based on g-acad.c.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/arg.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "gcv.h"

/* private headers */
#include "brlcad_ident.h"
#include "./dxf.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b)	{\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


static int	verbose;
static int	polyface_mesh = 0;	/* flag for output type (default is 3DFACE) */
static const char *output_file = NULL;	/* output filename */
static FILE	*fp;		/* Output file pointer */
static struct db_i		*dbip;
static struct bg_tess_tol	ttol;	/* tessellation tolerance in mm */
static struct bn_tol		tol;	/* calculation tolerance */
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int		regions_tried = 0;
static int		regions_converted = 0;
static int		regions_written = 0;
static int		inches = 0;
static unsigned int	tot_polygons = 0;

struct g_dxf_args {
    struct g_dxf_args *self;
    int help;
    int verbose;
    int inches;
    int polyface_mesh;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    int processors;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *output_file;
};

enum g_dxf_tolerance_kind {
    G_DXF_ABSOLUTE_TOLERANCE,
    G_DXF_NORMAL_TOLERANCE
};

static int
g_dxf_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_dxf_tolerance_kind kind)
{
    struct g_dxf_args *args = storage ? *(struct g_dxf_args **)storage : NULL;
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    if (kind == G_DXF_ABSOLUTE_TOLERANCE)
	args->absolute_tolerance = value;
    else
	args->normal_tolerance = value * DEG2RAD;
    args->relative_tolerance = 0.0;
    return 0;
}

static int
g_dxf_absolute_tolerance_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage)
{
    return g_dxf_tolerance_consume(msg, argc, argv, storage,
	G_DXF_ABSOLUTE_TOLERANCE);
}

static int
g_dxf_normal_tolerance_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage)
{
    return g_dxf_tolerance_consume(msg, argc, argv, storage,
	G_DXF_NORMAL_TOLERANCE);
}

static int
g_dxf_distance_tolerance_consume(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage)
{
    struct g_dxf_args *args = storage ? *(struct g_dxf_args **)storage : NULL;
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (args) {
	args->distance_tolerance = value;
	args->distance_tolerance_set = 1;
    }
    return 0;
}

static const struct bu_arg_option g_dxf_options[] = {
    BU_ARG_FLAG("h", "help", struct g_dxf_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct g_dxf_args, verbose,
	BU_ARG_VALUE_INTEGER, "Increase conversion reporting"),
    BU_ARG_FLAG("i", "inches", struct g_dxf_args, inches,
	"Write coordinates in inches instead of millimeters"),
    BU_ARG_FLAG("p", "polyface-mesh", struct g_dxf_args, polyface_mesh,
	"Write POLYFACE MESH entities instead of 3DFACE entities"),
    BU_ARG_VALUE("x", "rt-debug", struct g_dxf_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_dxf_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_dxf_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative chord deviation in millimeters and clear relative "
	"tolerance", BU_ARG_REQUIRED, NULL,
	g_dxf_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_dxf_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of bounding-box size"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_dxf_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance", BU_ARG_REQUIRED, NULL,
	g_dxf_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_dxf_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set the vertex-coincidence distance in millimeters", BU_ARG_REQUIRED,
	NULL, g_dxf_distance_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("P", "processors", struct g_dxf_args, processors,
	BU_ARG_VALUE_INTEGER, bu_arg_positive_integer_validate, "count",
	"Accept a legacy processor count; conversion remains serial"),
    BU_ARG_VALUE("o", "output", struct g_dxf_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.dxf",
	"Write DXF output to this file instead of standard output"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_dxf_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_dxf_schema = BU_ARG_SCHEMA(
    "g-dxf", "Convert BRL-CAD geometry to AutoCAD DXF",
    g_dxf_options, g_dxf_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar g_dxf_grammar = {
    &g_dxf_schema, NULL, NULL
};

static int
g_dxf_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_dxf_schema, "g-dxf");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-dxf help");
    return status;
}


static int
find_closest_color(float color[3])
{
    int icolor[3];
    int i;
    int dist_sq;
    int color_num;

    VSCALE(icolor, color, 255);

    color_num = 0;
    dist_sq = MAGSQ(icolor);

    for (i = 1; i < 256; i++) {
	int tmp_dist;
	int diff[3];

	VSUB2(diff, icolor, &rgb[i*3]);
	tmp_dist = MAGSQ(diff);
	if (tmp_dist < dist_sq) {
	    dist_sq = tmp_dist;
	    color_num = i;
	}
    }

    return color_num;
}


static void
nmg_to_dxf(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp, void *UNUSED(client_data))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    char *region_name;
    int tri_count=0;
    int color_num;
    int do_triangulate=0;
    struct bu_list *vlfree = &rt_vlfree;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Count triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    int vert_count=0;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    vert_count++;
		}

		if (vert_count > 3) {
		    do_triangulate = 1;
		    goto triangulate;
		}

		tri_count++;
	    }
	}
    }

 triangulate:
    if (do_triangulate) {
	/* triangulate model */
	nmg_triangulate_model(m, vlfree, &tol);

	/* Count triangles */
	tri_count = 0;
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    struct faceuse *fu;

	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		struct loopuse *lu;

		if (fu->orientation != OT_SAME)
		    continue;

		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		    tri_count++;
		}
	    }
	}
    }

    nmg_vertex_tabulate(&verts, &r->l.magic, vlfree);

    color_num = find_closest_color(tsp->ts_mater.ma_color);

    if (polyface_mesh) {
	size_t i;


	fprintf(fp, "0\nPOLYLINE\n8\n%s\n62\n%d\n70\n64\n71\n%lu\n72\n%d\n",
		 region_name, color_num, (unsigned long)BU_PTBL_LEN(&verts), tri_count);
	for (i = 0; i < BU_PTBL_LEN(&verts); i++) {
	    fprintf(fp, "0\nVERTEX\n8\n%s\n", region_name);
	    v = (struct vertex *)BU_PTBL_GET(&verts, i);
	    NMG_CK_VERTEX(v);
	    if (inches) {
		fprintf(fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGSIN(v->vg_p->coord));
	    } else {
		fprintf(fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGS(v->vg_p->coord));
	    }
	}
    }

    /* Check triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		if (polyface_mesh) {
		    fprintf(fp, "0\nVERTEX\n8\n%s\n70\n128\n10\n0.0\n20\n0.0\n30\n0.0\n",
			     region_name);
		} else {
		    fprintf(fp, "0\n3DFACE\n8\n%s\n62\n%d\n", region_name, color_num);
		}

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    if (polyface_mesh) {
			fprintf(fp, "%d\n%jd\n",
				 vert_count+70, bu_ptbl_locate(&verts, (long *)v) + 1);
		    } else {
			if (inches) {
			    fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X] / 25.4,
				     20 + vert_count - 1,
				     v->vg_p->coord[Y] / 25.4,
				     30 + vert_count -1,
				     v->vg_p->coord[Z] / 25.4);
			} else {
			    fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X],
				     20 + vert_count - 1,
				     v->vg_p->coord[Y],
				     30 + vert_count -1,
				     v->vg_p->coord[Z]);
			}
		    }
		}
		if (vert_count > 3) {
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3) {
		    continue;
		} else {
		    /* repeat the last vertex for the benefit of codes
		     * that interpret the dxf specification for
		     * 3DFACES as requiring a fourth vertex even when
		     * only three are input.
		     */
		    if (!polyface_mesh) {
			vert_count++;
			if (inches) {
			    fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X] / 25.4,
				     20 + vert_count - 1,
				     v->vg_p->coord[Y] / 25.4,
				     30 + vert_count -1,
				     v->vg_p->coord[Z] / 25.4);
			} else {
			    fprintf(fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X],
				     20 + vert_count - 1,
				     v->vg_p->coord[Y],
				     30 + vert_count -1,
				     v->vg_p->coord[Z]);
			}
		    }
		}

		tot_polygons++;
	    }
	}
    }

    bu_ptbl_free(&verts);
    bu_free(region_name, "region name");

    if (polyface_mesh) {
	fprintf(fp, "0\nSEQEND\n");
    }

}


union tree *get_layer(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *UNUSED(curtree), void *UNUSED(client_data))
{
    char *layer_name;
    int color_num;

    layer_name = db_path_to_string(pathp);
    color_num = find_closest_color(tsp->ts_mater.ma_color);

    fprintf(fp, "0\nLAYER\n2\n%s\n62\n%d\n", layer_name, color_num);

    bu_free(layer_name, "layer name");

    return (union tree *)NULL;
}


static struct gcv_region_end_data gcvwriter = {nmg_to_dxf, NULL, NULL};


/**
 * This is the gist for what is going on (not verified):
 *
 * 1. initialize tree_state (db_tree_state)
 * 2. Deal with command line arguments. Strip off everything but regions for processing.
 * 3. Open geometry (.g) file and build directory db_dirbuild
 * 4. db_walk_tree (get_layer) for layer names only
 * 5. Initialize tree_state
 * 6. Initialize model (nmg)\
 * 7. db_walk_tree (gcv_region_end)
 * 8. Cleanup
 */
int
main(int argc, char *argv[])
{
    struct g_dxf_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;
    double percent;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    gcvwriter.vlfree = &rt_vlfree;

    RT_DBTS_INIT(&tree_state);
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

    args.self = &args;
    args.absolute_tolerance = ttol.abs;
    args.relative_tolerance = ttol.rel;
    args.normal_tolerance = ttol.norm;
    args.distance_tolerance = tol.dist;
    introspection = bu_arg_introspect(&msg, &g_dxf_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    operand_index = bu_arg_schema_parse_complete(&g_dxf_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	bu_flog(stderr, "%s\n", brlcad_ident("BRL-CAD to DXF Exporter"));
	return g_dxf_help(stderr, EXIT_FAILURE);
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_dxf_help(stdout, EXIT_SUCCESS);

    verbose = args.verbose;
    inches = args.inches;
    polyface_mesh = args.polyface_mesh;
    output_file = args.output_file;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance;
    tol.dist = args.distance_tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    if (args.distance_tolerance_set)
	rt_pr_tol(&tol);

    if (!output_file) {
	fp = stdout;
	setmode(fileno(fp), O_BINARY);
    } else {
	/* Open output file */
	if ((fp=fopen(output_file, "w+b")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, " Cannot open output file (%s) for writing\n", output_file);
	}
    }

    /* Open BRL-CAD database */
    argc -= operand_index + 1;
    argv += operand_index + 1;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }

    if (db_dirbuild(dbip)) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    if (verbose) {
	int i;

	bu_log("Model: %s\n", argv[0]);
	bu_log("Objects:");
	for (i = 1; i < argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
		tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
		tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* output DXF header and start of TABLES section */
    fprintf(fp,
	    "0\nSECTION\n2\nHEADER\n999\n%s\n0\nENDSEC\n0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n",
	    argv[argc-1]);

    /* Walk indicated tree(s) just for layer names to put in TABLES section */
    (void)db_walk_tree(dbip, argc-1, (const char **)(argv+1),
		       1,			/* ncpu */
		       &tree_state,
		       0,			/* take all regions */
		       get_layer,
		       NULL,
		       (void *)NULL);	/* in librt/nmg_bool.c */

    /* end of layers section, start of ENTITIES SECTION */
    fprintf(fp, "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    /* make empty NMG model */
    the_model = nmg_mm();
    tree_state.ts_m = &the_model;
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			gcv_region_end,
			rt_booltree_leaf_tess,
			(void *)&gcvwriter);	/* callback for gcv_region_end */

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	if (verbose)
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   regions_tried, regions_converted, percent);
    }

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	if (verbose)
	    bu_log("                  %d triangulated successfully. %g%%\n",
		    regions_written, percent);
    }

    bu_log("%ld triangles written\n", (long int)tot_polygons);

    fprintf(fp, "0\nENDSEC\n0\nEOF\n");

    if (output_file) {
	fclose(fp);
    }

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

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
