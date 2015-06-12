/*                  G - X X X _ F A C E T S . C
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
/** @file conv/g-xxx_facets.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a faceted
 * format by calling on the NMG booleans.  Based on g-stl.c.
 *
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "bu/getopt.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b)	{\
	(a)[X] = (b)[X]/25.4; \
	(a)[Y] = (b)[Y]/25.4; \
	(a)[Z] = (b)[Z]/25.4; \
    }

extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);

extern double nmg_eue_dist;		/* from nmg_plot.c */

#define CPP_QUOTE(string) #string
#define CPP_XQUOTE(symbol) CPP_QUOTE(symbol)

#define DEFAULT_ABS_TOL 0.0
#define DEFAULT_REL_TOL 0.01
#define DEFAULT_NORM_TOL 0.0

static const char *usage =
    "[-v][-xX lvl][-a abs_tess_tol (default: " CPP_XQUOTE(DEFAULT_ABS_TOL) ")]"
    "[-r rel_tess_tol (default: " CPP_XQUOTE(DEFAULT_REL_TOL) ")]\n"
    "\t[-n norm_tess_tol (default: " CPP_XQUOTE(DEFAULT_NORM_TOL) ")]"
    "[-D dist_calc_tol (default: " CPP_XQUOTE(BN_TOL_DIST) ")]\n"
    "\t-o output_file_name brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;	/* tessellation tolerance in mm */
static struct bn_tol		tol;	/* calculation tolerance */
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int		regions_tried = 0;
static int		regions_converted = 0;
static int		regions_written = 0;
static size_t tot_polygons = 0;

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}

int
main(int argc, char **argv)
{
    int	c;
    double percent;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tessellation tolerance defaults */
    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = DEFAULT_ABS_TOL;
    ttol.rel = DEFAULT_REL_TOL;
    ttol.norm = DEFAULT_NORM_TOL;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    /* make empty NMG model */
    the_model = nmg_mm();

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "r:a:n:o:vx:D:X:h?")) != -1) {
	switch (c) {
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		/* grab output file name */
		break;
	    case 'v':
		verbose++;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&RTG.debug);
		break;
	    case 'D':
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol(&tol);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&RTG.NMG_debug);
		NMG_debug = RTG.NMG_debug;
		break;
	    default:
		print_usage(argv[0]);
	}
    }

    if (bu_optind+1 >= argc)
	print_usage(argv[0]);

    /* Open output file */


    /* Open BRL-CAD database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "ERROR: Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild failed\n");

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

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

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(void *)NULL);	/* in librt/nmg_bool.c */

    percent = 0;
    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
    }
    percent = 0;

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	bu_log("                  %d triangulated successfully. %g%%\n",
		regions_written, percent);
    }

    bu_log("%zd triangles written\n", tot_polygons);

    bu_log("Tessellation parameters used:\n");
    bu_log("  abs  [-a]    %g\n", ttol.abs);
    bu_log("  rel  [-r]    %g\n", ttol.rel);
    bu_log("  norm [-n]    %g\n", ttol.norm);
    bu_log("  dist [-D]    %g\n", tol.dist);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
}

/* routine to output the faceted NMG representation of a BRL-CAD region */
static void
output_nmg(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    /* Output triangles */
    if (verbose) {
	printf("Convert these triangles to your format for region %s\n", region_name);
    } else {
	printf("Converted %s\n", region_name);
    }
    for (BU_LIST_FOR(s, shell, &r->s_hd))
    {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd))
	{
	    struct loopuse *lu;
	    /* vect_t facet_normal; */

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal if needed */
	    /* NMG_GET_FU_NORMAL(facet_normal, fu); */

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
	    {
		struct edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* loop through the edges in this loop (facet) */
		if (verbose)
		    printf("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (verbose)
			printf("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));
		}
		tot_polygons++;
	    }
	}
    }

    bu_free(region_name, "region name");
}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the facetized region to the output file */
	output_nmg(r, pathp, tsp->ts_regionid, tsp->ts_gmater);

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	RTG.NMG_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    }  BU_UNSETJUMP;
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);

    } else  {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	RTG.NMG_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree, &rt_uniresource);/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	bu_free(name, "db_path_to_string");
	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    } BU_UNSETJUMP;/* Relinquish the protection */

    return ret_tree;
}


/*
 *  Called from db_walk_tree().
 *
 *  This routine must be prepared to run in parallel.
 */
union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    union tree		*ret_tree;
    struct bu_list		vhead;
    struct nmgregion	*r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    {
	char	*sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    regions_tried++;

    if (verbose)
	bu_log("Attempting to process region %s\n", db_path_to_string(pathp));

    ret_tree= process_boolean(curtree, tsp, pathp);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
    {
	if (verbose)
	    bu_log("\tNothing left of this region after Boolean evaluation\n");
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

    regions_converted++;

    if (r != (struct nmgregion *)NULL)
    {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd))
	{
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s))
	    {
		if (nmg_ks(s))
		{
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if (!empty_region) {
	    empty_model = nmg_kill_zero_length_edgeuses(*tsp->ts_m);
	}

	if (!empty_region && !empty_model) {
	    process_triangulation(r, pathp, tsp);

	    regions_written++;
	}

	if (!empty_model)
	    nmg_kr(r);
    }

    /*
     *  Dispose of original tree, so that all associated dynamic
     *  memory is released now, not at the end of all regions.
     *  A return of TREE_NULL from this routine signals an error,
     *  and there is no point to adding _another_ message to our output,
     *  so we need to cons up an OP_NOP node to return.
     */


    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
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
