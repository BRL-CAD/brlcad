/*                         G - O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2014 United States Government as represented by
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
/** @file conv/g-obj.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a Wavefront
 * '.obj' file by calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4

extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);

static char usage[] =
"[-m][-v][-i][-u][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol][-P #_of_CPUs]\n"
"[-e error_file_name ][-D dist_calc_tol][-o output_file_name ] brlcad_db.g object(s)\n";

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}

static off_t vert_offset=0;
static off_t norm_offset=0;
static int do_normals=0;
static int NMG_debug;	/* saved arg of -X, for longjmp handling */
static int verbose=0;
static int usemtl=0;	/* flag to include 'usemtl' statements with a
			 * code for GIFT materials:
			 *
			 * usemtl 0_100_32
			 * means aircode is 0
			 * los is 100
			 * GIFT material is 32
			 */
static int ncpu = 1;	/* Number of processors */
static char *output_file = NULL;	/* output filename */
static char *error_file = NULL;		/* error filename */
static FILE *fp;			/* Output file pointer */
static FILE *fpe;			/* Error file pointer */
static struct db_i *dbip;
static struct rt_tess_tol ttol;
static struct bn_tol tol;
static struct model *the_model;

static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;
static int print_help = 0;

static int
parse_tol_abs(struct bu_vls *UNUSED(error_msg), int UNUSED(argc), const char **argv, void *UNUSED(set_var))
{
    ttol.abs = atof(argv[0]);
    ttol.rel = 0.0;
    return 1;
}

static int
parse_tol_norm(struct bu_vls *UNUSED(error_msg), int UNUSED(argc), const char **argv, void *UNUSED(set_var))
{
    ttol.norm = atof(argv[0]);
    ttol.rel = 0.0;
    return 1;
}

static int
parse_tol_dist(struct bu_vls *UNUSED(error_msg), int UNUSED(argc), const char **argv, void *UNUSED(set_var))
{
    tol.dist = atof(argv[0]);
    tol.dist_sq = tol.dist * tol.dist;
    rt_pr_tol(&tol);
    return 1;
}

static int
parse_debug_rt(struct bu_vls *UNUSED(error_msg), int UNUSED(argc), const char **argv, void *UNUSED(set_var))
{
    sscanf(argv[0], "%x", (unsigned int *)&RTG.debug);
    return 1;
}

static int
parse_debug_nmg(struct bu_vls *UNUSED(error_msg), int UNUSED(argc), const char **argv, void *UNUSED(set_var))
{
    sscanf(argv[0], "%x", (unsigned int *)&RTG.NMG_debug);
    NMG_debug = RTG.NMG_debug;
    return 1;
}

static struct bu_opt_desc options[] = {
    {"?", "", NULL,         NULL,            &print_help,  "print help and exit"},
    {"h", "", NULL,         NULL,            &print_help,  "print help and exit"},
    {"i", "", NULL,         NULL,            &inches,      "change output units from mm to inches"},
    {"m", "", NULL,         NULL,            &usemtl,      "output usemtl statements"},
    {"u", "", NULL,         NULL,            &do_normals,  "output vertex normals"},
    {"v", "", NULL,         NULL,            &verbose,     "verbose output"},
    {"a", "", "#",          parse_tol_abs,   &ttol,        "absolute tolerance"},
    {"n", "", "#",          parse_tol_norm,  &ttol,        "surface normal tolerance"},
    {"D", "", "#",          parse_tol_dist,  &tol,         "distance tolerance"},
    {"x", "", "level",      parse_debug_rt,  NULL,         "set RT debug flag"},
    {"X", "", "level",      parse_debug_nmg, NULL,         "set NMG debug flag"},
    {"e", "", "error_file", bu_opt_str,      &error_file,  "error file name"},
    {"o", "", "output.obj", bu_opt_str,      &output_file, "output file name"},
    {"P", "", "#",          bu_opt_int,      &ncpu,        "number of CPUs"},
    {"r", "", "#",          bu_opt_fastf_t,  &ttol.rel,    "relative tolerance"},
    BU_OPT_DESC_NULL
};

int
main(int argc, const char **argv)
{
    int c;
    double percent;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    const char *prog_name = argv[0];

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    ++argv; --argc;

    argc = bu_opt_parse(&parse_msgs, argc, argv, options);

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_cstr(&parse_msgs));
    }
    if (argc < 2 || print_help) {
	print_usage(prog_name);
    }

    if (!output_file)
	fp = stdout;
    else {
	/* Open output file */
	if ((fp=fopen(output_file, "wb+")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, "Cannot open output file (%s) for writing\n", output_file);
	}
    }

    /* Open g-obj error log file */
    if (!error_file) {
	fpe = stderr;
	setmode(fileno(fpe), O_BINARY);
    } else if ((fpe=fopen(error_file, "wb")) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open output file (%s) for writing\n", error_file);
    }

    /* Open BRL-CAD database */
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild failed\n");

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    /* Write out header */
    if (inches)
	fprintf(fp, "# BRL-CAD generated Wavefront OBJ file (Units in)\n");
    else
	fprintf(fp, "# BRL-CAD generated Wavefront OBJ file (Units mm)\n");

    fprintf(fp, "# BRL-CAD model: %s\n# BRL_CAD objects:", argv[0]);

    for (c=1; c<argc; c++)
	fprintf(fp, " %s", argv[c]);
    fprintf(fp, "\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(void *)NULL);	/* in librt/nmg_bool.c */

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100.0) / regions_tried;
	bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
	percent = ((double)regions_written * 100.0) / regions_tried;
	bu_log("                 %d triangulated successfully. %g%%\n",
	       regions_written, percent);
    }

    fclose(fp);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return 0;
}


static void
nmg_to_obj(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int aircode, int los, int material_id)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    struct bu_ptbl norms;
    char *region_name;
    size_t numverts = 0;		/* Number of vertices to output */
    size_t numtri   = 0;		/* Number of triangles to output */
    size_t i;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    /* list all vertices in result */
    nmg_vertex_tabulate(&verts, &r->l.magic);

    /* Get number of vertices */
    numverts = BU_PTBL_END(&verts);

    /* get list of vertexuse normals */
    if (do_normals)
	nmg_vertexuse_normal_tabulate(&norms, &r->l.magic);

    /* BEGIN CHECK SECTION */
    /* Check vertices */

    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
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

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    int loc;
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    loc = bu_ptbl_locate(&verts, (long *)v);
		    if (loc < 0) {
			bu_ptbl_free(&verts);
			bu_free(region_name, "region name");
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_exit(1, "ERROR: Can't find vertex in list!");
		    }
		}
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3)
		    continue;
		numtri++;
	    }
	}
    }

    /* END CHECK SECTION */
    /* Write pertinent info for this region */

    if (usemtl)
	fprintf(fp, "usemtl %d_%d_%d\n", aircode, los, material_id);

    fprintf(fp, "g %s", pathp->fp_names[0]->d_namep);
    for (i=1; i<pathp->fp_len; i++)
	fprintf(fp, "/%s", pathp->fp_names[i]->d_namep);
    fprintf(fp, "\n");

    /* Write vertices */
    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
	if (inches)
	    fprintf(fp, "v %f %f %f\n", V3ARGSIN(v->vg_p->coord));
	else
	    fprintf(fp, "v %f %f %f\n", V3ARGS(v->vg_p->coord));
    }

    /* Write vertexuse normals */
    if (do_normals) {
	for (i=0; i<BU_PTBL_LEN(&norms); i++) {
	    struct vertexuse_a_plane *va;

	    va = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
	    NMG_CK_VERTEXUSE_A_PLANE(va);
	    if (inches)
		fprintf(fp, "vn %f %f %f\n", V3ARGSIN(va->N));
	    else
		fprintf(fp, "vn %f %f %f\n", V3ARGS(va->N));
	}
    }

    /* output triangles */
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
		int use_normals=1;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* Each vertexuse of the face must have a normal in order
		 * to use the normals in Wavefront
		 */
		if (do_normals) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);

			if (!eu->vu_p->a.magic_p) {
			    use_normals = 0;
			    break;
			}

			if (*eu->vu_p->a.magic_p != NMG_VERTEXUSE_A_PLANE_MAGIC) {
			    use_normals = 0;
			    break;
			}
		    }
		} else
		    use_normals = 0;

		fprintf(fp, "f");

		/* list vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    int loc;
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    loc = bu_ptbl_locate(&verts, (long *)v);
		    if (loc < 0) {
			bu_ptbl_free(&verts);
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_free(region_name, "region name");
			bu_exit(1, "Can't find vertex in list!\n");
		    }

		    if (use_normals) {
			int j;

			j = bu_ptbl_locate(&norms, (long *)eu->vu_p->a.magic_p);
			fprintf(fp, " %ld//%ld", loc+1+(long)vert_offset, j+1+(long)norm_offset);
		    } else
			fprintf(fp, " %ld", loc+1+(long)vert_offset);
		}

		fprintf(fp, "\n");

		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		}
	    }
	}
    }
    vert_offset += numverts;
    bu_ptbl_free(&verts);
    if (do_normals) {
	norm_offset += BU_PTBL_END(&norms);
	bu_ptbl_free(&norms);
    }
    bu_free(region_name, "region name");
}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the TANKILL file */
	nmg_to_obj(r, pathp, tsp->ts_regionid, tsp->ts_aircode, tsp->ts_los, tsp->ts_gmater);

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

    } else {
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
 * Called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    regions_tried++;

    ret_tree = process_boolean(curtree, tsp, pathp);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    regions_converted++;

    if (r != 0) {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s)) {
		if (nmg_ks(s)) {
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

	    BU_UNSETJUMP;
	}

	if (!empty_model)
	    nmg_kr(r);
    }

    /* Dispose of original tree, so that all associated dynamic memory
     * is released now, not at the end of all regions.  A return of
     * TREE_NULL from this routine signals an error, and there is no
     * point to adding _another_ message to our output, so we need to
     * cons up an OP_NOP node to return.
     */


    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    if (regions_tried>0) {
	float npercent;
	float tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	bu_log("Tried %d regions; %d conv. to NMG's, %d conv. to tri.; nmgper = %.2f%%, triper = %.2f%%\n",
	       regions_tried, regions_converted, regions_written, npercent, tpercent);
    }

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
