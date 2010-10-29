/*                         G - E G G . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2010 United States Government as represented by
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
/** @file g-egg.c
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
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

static char usage[] = "Usage: %s [-bviM] [-xX lvl] [-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol] [-D dist_calc_tol] [-o output_file_name.egg] brlcad_db.g object(s)\n";

static int verbose;
static int NMG_debug;			/* saved arg of -X, for longjmp handling */
static int ncpu = 1;			/* Number of processors */
static char *output_file = NULL;	/* output filename */
static FILE *fp;			/* Output file pointer */
static struct db_i *dbip;
static struct model *the_model;
static struct rt_tess_tol ttol;		/* tesselation tolerance in mm */
static struct bn_tol tol;		/* calculation tolerance */
static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;
static unsigned int tot_polygons = 0;


static void
nmg_to_egg(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), float UNUSED(color[3]))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int vert_count=0;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    /* Write pertinent info for this region */
    fprintf(fp, "  <VertexPool> %s {\n", (region_name+1));

    /* Build the VertexPool */
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

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    fprintf(fp, "    <Vertex> %d {\n      %lf %lf %lf\n      <Normal> { %lf %lf %lf }\n    }\n",
			    vert_count,
			    V3ARGS(v->vg_p->coord),
			    V3ARGS(facet_normal));
		}
	    }
	}
    }
    fprintf(fp, "  }\n");
    vert_count = 0;

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

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

	        fprintf(fp, "  <Polygon> { \n    <RGBA> { 1 1 1 1 } \n    <VertexRef> { ");
		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    fprintf(fp, " %d", vert_count);
		}
	        fprintf(fp, " <Ref> { \"%s\" } }\n  }\n", region_name+1);

		region_polys++;
	    }
	}
    }

    tot_polygons += region_polys;
    bu_free(region_name, "region name");
}


/* FIXME: this be a dumb hack to avoid void* conversion */
struct gcv_data {
    void (*func)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);
};
static struct gcv_data gcvwriter = {nmg_to_egg};


/*
 *			M A I N
 */
int
main(int argc, char *argv[])
{
    double percent;
    int i, use_mc=0;

    bu_setlinebuf(stderr);

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
    tol.perp = 1e-5;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_model = nmg_mm();
    BU_LIST_INIT(&rt_g.rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    while ((i = bu_getopt(argc, argv, "a:b8n:o:r:vx:D:P:X:i")) != EOF) {
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
		rt_g.debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug);
		break;
	    case 'D':
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol(&tol);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug);
		NMG_debug = rt_g.NMG_debug;
		break;
	    case 'i':
		inches = 1;
		break;
	    case '8':
		use_mc = 1;
		break;
	    case '?':
		bu_log("Unknown argument: \"%c\"\n", i);
	    default:
		bu_log("Booga. %c\n", i);
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    fp = stdout;
    if (output_file) {
	if ((fp=fopen(output_file, "wb+")) == NULL)
	{
	    perror(argv[0]);
	    bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", output_file);
	}
    }

    /* Open brl-cad database */
    argc -= bu_optind;
    argv += bu_optind;

    if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry file (%s)\n", argv[0]);
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
	bu_log("\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* print the egg header shtuff, including the command line to execute it */
    fprintf(fp, "<CoordinateSystem> { Z-Up }\n\n");
    fprintf(fp, "<Comment> {\n  \"%s", *argv);
    for (i=1; i<argc; i++)
	fprintf(fp, " %s", argv[i]);
    fprintf(fp, "\"\n}\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    while (--argc) {
	fprintf(fp, "<Group> %s {\n", *(argv+1));
	(void) db_walk_tree(dbip,		/* db_i */
			    1,		/* argc */
			    (const char **)(++argv), /* argv */
			    1,		/* ncpu */
			    &tree_state,	/* state */
			    NULL,		/* start func */
			    use_mc?gcv_region_end_mc:gcv_region_end,	/* end func */
			    use_mc?NULL:nmg_booltree_leaf_tess, /* leaf func */
			    (genptr_t)&gcvwriter);  /* client_data */
	fprintf(fp, "}\n");
    }

    percent = 0;
    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	if (verbose)
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   regions_tried, regions_converted, percent);
    }
    percent = 0;

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	if (verbose)
	    bu_log("                  %d triangulated successfully. %g%%\n",
		   regions_written, percent);
    }

    bu_log("%ld triangles written\n", tot_polygons);

    if (output_file)
	fclose(fp);

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
