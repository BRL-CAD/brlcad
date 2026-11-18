/*                         G - S T L . C
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
/** @file g-stl.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an STL file by
 * calling on the NMG booleans.  Based on g-acad.c.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include "bnetwork.h"
#include "bio.h"

/* interface headers */
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/vls.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "gcv.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b) {\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


struct g_stl_args {
    struct g_stl_args *self;
    int help;
    int binary;
    int verbose;
    int inches;
    int marching_cubes;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *output_file;
    const char *output_directory;
};

enum g_stl_tolerance_kind {
    G_STL_ABSOLUTE_TOLERANCE,
    G_STL_NORMAL_TOLERANCE,
    G_STL_DISTANCE_TOLERANCE
};

static struct g_stl_args *
g_stl_args_from_storage(void *storage)
{
    return storage ? *(struct g_stl_args **)storage : NULL;
}

static int
g_stl_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_stl_tolerance_kind kind)
{
    struct g_stl_args *args = g_stl_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    switch (kind) {
	case G_STL_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_STL_NORMAL_TOLERANCE:
	    args->normal_tolerance = value * DEG2RAD;
	    args->relative_tolerance = 0.0;
	    break;
	case G_STL_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    args->distance_tolerance_set = 1;
	    break;
    }
    return 0;
}

#define G_STL_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_stl_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_STL_TOLERANCE_CONSUMER(g_stl_absolute_tolerance_consume,
    G_STL_ABSOLUTE_TOLERANCE)
G_STL_TOLERANCE_CONSUMER(g_stl_normal_tolerance_consume,
    G_STL_NORMAL_TOLERANCE)
G_STL_TOLERANCE_CONSUMER(g_stl_distance_tolerance_consume,
    G_STL_DISTANCE_TOLERANCE)

static const struct bu_arg_option g_stl_options[] = {
    BU_ARG_FLAG("h", "help", struct g_stl_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("b", "binary", struct g_stl_args, binary,
	"Write binary instead of ASCII STL; requires --output or "
	"--output-directory"),
    BU_ARG_FLAG("v", "verbose", struct g_stl_args, verbose,
	"Report objects, tolerances, and conversion statistics"),
    BU_ARG_FLAG("i", "inches", struct g_stl_args, inches,
	"Write coordinates in inches instead of millimeters (the default)"),
    BU_ARG_FLAG("8", "marching-cubes", struct g_stl_args, marching_cubes,
	"Tessellate with marching cubes instead of NMG boolean evaluation"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_stl_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_stl_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_stl_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_stl_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_stl_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_stl_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)",
	BU_ARG_REQUIRED, NULL, g_stl_distance_tolerance_consume),
    BU_ARG_VALUE("x", "rt-debug", struct g_stl_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_stl_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("o", "output", struct g_stl_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.stl",
	"Write all geometry to this STL file (mutually exclusive with "
	"--output-directory)"),
    BU_ARG_VALUE("m", "output-directory", struct g_stl_args,
	output_directory, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "directory",
	"Write each region to a separate STL file here (mutually exclusive "
	"with --output)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_stl_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const g_stl_output_options[] = {
    "output", "output-directory", NULL
};

static const struct bu_arg_constraint g_stl_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT,
	BU_ARG_CONDITION_ALWAYS, g_stl_output_options, 0, 1,
	"--output and --output-directory are mutually exclusive"),
    BU_ARG_CONSTRAINT_NULL
};

static int
g_stl_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t UNUSED(cursor_arg),
	struct bu_arg_validate_result *result)
{
    int has_output;

    if (!result || result->state == BU_ARG_VALIDATE_INVALID ||
	!bu_arg_schema_option_present(schema, argc, argv, "binary"))
	return 0;

    has_output = bu_arg_schema_option_present(schema, argc, argv, "output") ||
	bu_arg_schema_option_present(schema, argc, argv, "output-directory");
    if (!has_output) {
	result->state = BU_ARG_VALIDATE_INVALID;
	result->token_start = argc;
	result->token_end = argc;
	result->hint = "binary output requires --output or --output-directory";
    }
    return 0;
}

static const struct bu_arg_schema g_stl_schema = BU_ARG_SCHEMA(
    "g-stl", "Convert BRL-CAD geometry to Stereolithography",
    g_stl_options, g_stl_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, g_stl_constraints, g_stl_validate, NULL,
	NULL));

static const struct bu_arg_grammar g_stl_grammar = {
    &g_stl_schema, NULL, NULL
};

static int
g_stl_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_stl_schema, "g-stl");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-stl help");
    return status;
}


static int verbose;
static int binary = 0;			/* Default output is ASCII */
static const char *output_file = NULL;	/* output filename */
static const char *output_directory = NULL;	/* directory name to hold output files */
static FILE *fp;			/* Output file pointer */
static int bfd;				/* Output binary file descriptor */
static struct db_i *dbip;
static struct model *the_model;
static struct bu_vls file_name = BU_VLS_INIT_ZERO;		/* file name built from region name */
static struct bg_tess_tol ttol;		/* tessellation tolerance in mm */
static struct bn_tol tol;		/* calculation tolerance */
static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;
static size_t tot_polygons = 0;


/* Byte swaps a four byte value */
void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


static void
nmg_to_stl(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *UNUSED(tsp), void *UNUSED(client_data))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int ret;
    struct bu_list *vlfree = &rt_vlfree;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (output_directory) {
	char *c;

	bu_vls_trunc(&file_name, 0);
	bu_vls_strcpy(&file_name, output_directory);
	bu_vls_putc(&file_name, '/');
	c = region_name;
	c++;
	while (*c != '\0') {
	    if (*c == '/') {
		bu_vls_putc(&file_name, '@');
	    } else if (*c == '.' || isspace((int)*c)) {
		bu_vls_putc(&file_name, '_');
	    } else {
		bu_vls_putc(&file_name, *c);
	    }
	    c++;
	}
	bu_vls_strcat(&file_name, ".stl");
	if (!binary) {
	    /* Open ASCII output file */
	    if ((fp=fopen(bu_vls_addr(&file_name), "wb+")) == NULL)
	    {
		perror("g-stl");
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }
	} else {
	    char buf[81] = {0};	/* need exactly 80 char for header */

	    /* Open binary output file */
	    if ((bfd=open(bu_vls_addr(&file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
	    {
		perror("g-stl");
		bu_exit(1, "ERROR: Cannot open binary output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }

	    if (!region_name) {
		snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s)", inches?"inches":"mm");
	    } else {
		if (region_name && strlen(region_name) > 0) {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s", inches?"inches":"mm", region_name);
		} else {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s", inches?"inches":"mm", region_name);
		}
	    }
	    ret = write(bfd, &buf, 80);
	    if (ret < 0)
		perror("write");

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(bfd, &buf, 4);
	    if (ret < 0)
		perror("write");
	}
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Write pertinent info for this region */
    if (!binary)
	fprintf(fp, "solid %s\n", (region_name+1));

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &tol);

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
		float flts[12];
		float *flt_ptr = NULL;
		unsigned char vert_buffer[50];

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		memset(vert_buffer, 0, sizeof(vert_buffer));

		if (!binary) {
		    fprintf(fp, "  facet normal %f %f %f\n", V3ARGS(facet_normal));
		    fprintf(fp, "    outer loop\n");
		} else {
		    flt_ptr = flts;
		    VMOVE(flt_ptr, facet_normal);
		    flt_ptr += 3;
		}

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (!binary)
			fprintf(fp, "      vertex ");
		    if (inches)
			if (!binary) {
			    fprintf(fp, "%f %f %f\n", V3ARGSIN(v->vg_p->coord));
			} else {
			    VSETIN(flt_ptr, v->vg_p->coord);
			    flt_ptr += 3;
			}
		    else
			if (!binary) {
			    fprintf(fp, "%f %f %f\n", V3ARGS(v->vg_p->coord));
			} else {
			    VMOVE(flt_ptr, v->vg_p->coord);
			    flt_ptr += 3;
			}
		}
		if (vert_count > 3)
		{
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle");
		}
		else if (vert_count < 3)
		    continue;
		if (!binary) {
		    fprintf(fp, "    endloop\n");
		    fprintf(fp, "  endfacet\n");
		} else {
		    int i;

		    bu_cv_htonf(vert_buffer, (const unsigned char *)flts, 12);
		    for (i=0; i<12; i++) {
			lswap((unsigned int *)&vert_buffer[i*4]);
		    }
		    ret = write(bfd, vert_buffer, 50);
		    if (ret < 0)
			perror("write");
		}
		tot_polygons++;
		region_polys++;
	    }
	}
    }
    if (!binary)
	fprintf(fp, "endsolid %s\n", (region_name+1));

    if (output_directory) {
	if (binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    bu_lseek(bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)region_polys);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(bfd);
	} else {
	    fclose(fp);
	}
    }
    bu_free(region_name, "region name");
}


static struct gcv_region_end_data gcvwriter = {nmg_to_stl, NULL, NULL};


int
main(int argc, char *argv[])
{
    struct g_stl_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    double percent;
    int i;
    int introspection;
    int operand_index;
    int ret;
    int use_mc = 0;

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

    /* make empty NMG model */
    the_model = nmg_mm();

    args.self = &args;
    args.absolute_tolerance = ttol.abs;
    args.relative_tolerance = ttol.rel;
    args.normal_tolerance = ttol.norm;
    args.distance_tolerance = tol.dist;

    introspection = bu_arg_introspect(&msg, &g_stl_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_stl_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_stl_help(stdout, 0);

    binary = args.binary;
    verbose = args.verbose;
    inches = args.inches;
    use_mc = args.marching_cubes;
    output_file = args.output_file;
    output_directory = args.output_directory;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance;
    tol.dist = args.distance_tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    if (args.distance_tolerance_set)
	rt_pr_tol(&tol);

    if (!output_file && !output_directory) {
	if (binary) {
	    bu_exit(1, "%s: Can't output binary to stdout\n",argv[0]);
	}
	fp = stdout;
    } else if (output_file) {
	if (!binary) {
	    /* Open ASCII output file */
	    if ((fp=fopen(output_file, "wb+")) == NULL)
	    {
		perror(argv[0]);
		bu_exit(1, "%s: Cannot open ASCII output file (%s) for writing\n",argv[0],output_file);
	    }
	} else {
	    /* Open binary output file */
	    if ((bfd=open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
	    {
		perror(argv[0]);
		bu_exit(1, "%s: Cannot open binary output file (%s) for writing\n",argv[0],output_file);
	    }
	}
    }

    /* Open brl-cad database */
    argc -= operand_index + 1;
    argv += operand_index + 1;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "ERROR: db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    if (verbose) {
	bu_log("Model: %s\n", argv[0]);
	bu_log("Objects:");
	for (i=1; i<argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* Write out STL header if output file is binary */
    if (binary && output_file) {
	char buf[81];	/* need exactly 80 char for header */

	memset(buf, 0, sizeof(buf));
	if (inches) {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=inches)", sizeof(buf));
	} else {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=mm)", sizeof(buf));
	}
	ret = write(bfd, &buf, 80);
	if (ret < 0)
	    perror("write");

	/* write a place keeper for the number of triangles */
	memset(buf, 0, 4);
	ret = write(bfd, &buf, 4);
	if (ret < 0)
	    perror("write");
    }

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,
			&tree_state,
			0,			/* take all regions */
			use_mc?gcv_region_end_mc:gcv_region_end,
			use_mc?NULL:rt_booltree_leaf_tess,
			(void *)&gcvwriter);

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

    bu_log("%zu triangles written\n", tot_polygons);

    if (output_file) {
	if (binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    bu_lseek(bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)tot_polygons);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(bfd);
	} else {
	    fclose(fp);
	}
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
