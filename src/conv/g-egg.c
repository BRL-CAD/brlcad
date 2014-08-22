/*                         G - E G G . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2014 United States Government as represented by
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
/** @file conv/g-egg.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a panda3d egg file by
 * calling on the NMG booleans.  Based on g-stl.c.
 *
 * Format information is (currently) available at
 * http://panda3d.cvs.sourceforge.net/panda3d/panda/src/doc/eggSyntax.txt?view=markup
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "gcv.h"

/* interface headers */
#include "bu/getopt.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

extern union tree *gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);

struct gcv_data {
    void (*func)(struct shell *, const struct db_full_path *, int, int, float [3]);
    FILE *fp;
    unsigned long tot_polygons;
    struct bn_tol tol;
};

static struct gcv_data gcvwriter;

static void
usage(const char *argv0)
{
    bu_log("\nUsage: %s [-bv89] [-xX lvl] [-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol]\n", argv0);
    bu_log("[-D dist_calc_tol] [-P #_of_CPUs] [-o output_file_name.egg] brlcad_db.g object(s)\n");
    bu_exit(1,NULL);
}

static void
nmg_to_egg(struct shell *s, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), float UNUSED(color[3]))
{
    struct faceuse *fu;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int vert_count=0;

    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    /* triangulate model */
    nmg_triangulate_shell(s, &gcvwriter.tol);

    /* Write pertinent info for this region */
    fprintf(gcvwriter.fp, "  <VertexPool> %s {\n", (region_name+1));

    NMG_CK_SHELL(s);

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;
	vect_t facet_normal;

	NMG_CK_FACEUSE(fu);

	if (fu->orientation != OT_SAME)
	    continue;

	/* Grab the face normal and save it for all the vertex loops */
	NMG_GET_FU_NORMAL(facet_normal, fu);

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    /* check vertex numbers for each triangle */
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);

		vert_count++;

		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);
		fprintf(gcvwriter.fp, "    <Vertex> %d {\n      %f %f %f\n      <Normal> { %f %f %f }\n    }\n",
			vert_count,
			V3ARGS(v->vg_p->coord),
			V3ARGS(facet_normal));
	    }
	}
    }
    fprintf(gcvwriter.fp, "  }\n");
    vert_count = 0;

    NMG_CK_SHELL(s);

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);

	if (fu->orientation != OT_SAME)
	    continue;

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    fprintf(gcvwriter.fp, "  <Polygon> { \n    <RGBA> { 1 1 1 1 } \n    <VertexRef> { ");
	    /* check vertex numbers for each triangle */
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);

		vert_count++;

		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);
		fprintf(gcvwriter.fp, " %d", vert_count);
	    }

	    fprintf(gcvwriter.fp, " <Ref> { \"%s\" } }\n  }\n", region_name+1);
	    region_polys++;
	}
    }

    gcvwriter.tot_polygons += region_polys;
    bu_free(region_name, "region name");
}


int
main(int argc, char *argv[])
{
    int verbose = 0;
    int ncpu = 1;			/* Number of processors */
    char *output_file = NULL;	/* output filename */
    struct db_i *dbip;
    struct shell *the_shell;
    struct rt_tess_tol ttol;		/* tessellation tolerance in mm */
    struct db_tree_state tree_state;	/* includes tol & model */

    int i, use_mc = 0, use_bottess = 0;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &gcvwriter.tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_s = &the_shell;

    /* Set up tessellation tolerance defaults */
    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    gcvwriter.tol.magic = BN_TOL_MAGIC;
    gcvwriter.tol.dist = 0.0005;
    gcvwriter.tol.dist_sq = gcvwriter.tol.dist * gcvwriter.tol.dist;
    gcvwriter.tol.perp = 1e-6;
    gcvwriter.tol.para = 1 - gcvwriter.tol.perp;

    gcvwriter.tot_polygons = 0;

    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_shell = nmg_ms();
    BU_LIST_INIT(&rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    while ((i = bu_getopt(argc, argv, "a:b89n:o:r:vx:D:P:X:h?")) != -1) {
	switch (i) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		output_file = bu_optarg;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi(bu_optarg);
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&RTG.debug);
		break;
	    case 'D':
		gcvwriter.tol.dist = atof(bu_optarg);
		gcvwriter.tol.dist_sq = gcvwriter.tol.dist * gcvwriter.tol.dist;
		rt_pr_tol(&gcvwriter.tol);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&nmg_debug);
		break;
	    case '8':
		use_mc = 1;
		break;
	    case '9':
		use_bottess = 1;
		break;
	    default:
		usage(argv[0]);
	}
    }

    if (bu_optind+1 >= argc)
	usage(argv[0]);

    gcvwriter.fp = stdout;
    if (output_file) {
	if ((gcvwriter.fp=fopen(output_file, "wb+")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", output_file);
	}
    }

    /* Open brl-cad database */
    argc -= bu_optind;
    argv += bu_optind;
    if (argc < 2 || argv[0] == NULL || argv[1] == NULL)
	usage(argv[0]);

    gcvwriter.func = nmg_to_egg;

    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "ERROR: db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

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

    /* print the egg header stuff, including the command line to execute it */
    fprintf(gcvwriter.fp, "<CoordinateSystem> { Z-Up }\n\n");
    fprintf(gcvwriter.fp, "<Comment> {\n  \"%s", *argv);
    for (i=1; i<argc; i++)
	fprintf(gcvwriter.fp, " %s", argv[i]);
    fprintf(gcvwriter.fp, "\"\n}\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    while (--argc) {
	fprintf(gcvwriter.fp, "<Group> %s {\n", *(argv+1));
	(void) db_walk_tree(dbip,		/* db_i */
			    1,		/* argc */
			    (const char **)(++argv), /* argv */
			    ncpu,		/* ncpu */
			    &tree_state,	/* state */
			    NULL,		/* start func */
			    use_mc?gcv_region_end_mc:use_bottess?gcv_bottess_region_end:gcv_region_end,	/* end func */
			    use_mc?NULL:rt_nmg_booltree_leaf_tess, /* leaf func */
			    (void *)&gcvwriter);  /* client_data */
	fprintf(gcvwriter.fp, "}\n");
    }

    bu_log("%ld triangles written\n", gcvwriter.tot_polygons);

    if (output_file)
	fclose(gcvwriter.fp);

    /* Release dynamic storage */
    nmg_ks(the_shell);
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
