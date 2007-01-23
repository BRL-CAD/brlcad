/*                        G - J A C K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 */
/** @file g-jack.c
 *
 *  Program to convert a BRL-CAD model (in a .g file) to a JACK Psurf file,
 *  by calling on the NMG booleans.
 *
 *  Author -
 *	Michael J. Markowski
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "plot3.h"
#include "../../librt/debug.h"


BU_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));
void	nmg_to_psurf(struct nmgregion *r, FILE *fp_psurf);
void	jack_faces(struct nmgregion *r, FILE *fp_psurf, int *map);

extern double nmg_eue_dist;		/* from nmg_plot.c */

static char	usage[] = "\
Usage: %s [-v] [-d] [-f] [-xX lvl] [-u eu_dist]\n\
	[-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol]\n\
	[-D dist_calc_tol]\n\
	[-p prefix] brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	no_file_output=0;	/* -f:  Don't bother writing output files */
static int	debug_plots;	/* Make debugging plots */
static int	ncpu = 1;	/* Number of processors */
static char	*prefix = NULL;	/* output filename prefix. */
static FILE	*fp_fig;	/* Jack Figure file. */
static struct db_i		*dbip;
static struct bu_vls		base_seg;
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	jack_tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_done = 0;

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	char		*dot;
	register int	c;
	double		percent;
	struct bu_vls	fig_file;

	bu_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	jack_tree_state = rt_initial_tree_state;	/* struct copy */
	jack_tree_state.ts_tol = &tol;
	jack_tree_state.ts_ttol = &ttol;
	jack_tree_state.ts_m = &the_model;

	ttol.magic = RT_TESS_TOL_MAGIC;
	/* Defaults, updated by command line options. */
	ttol.abs = 0.0;
	ttol.rel = 0.01;
	ttol.norm = 0.0;

	/* XXX These need to be improved */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;

	/* XXX For visualization purposes, in the debug plot files */
	{
		extern fastf_t	nmg_eue_dist;	/* librt/nmg_plot.c */
		/* XXX This value is specific to the Bradley */
		/* Set it here, before the getopt() */
		nmg_eue_dist = 2.0;
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	the_model = nmg_mm();
	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "a:dfn:p:r:u:vx:D:P:X:")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			break;
		case 'd':
			debug_plots = 1;
			break;
		case 'f':
			no_file_output = 1;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			break;
		case 'p':		/* Prefix for Jack file names. */
			prefix = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
			break;
		case 'u':
			nmg_eue_dist = atof(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			ncpu = atoi( optarg );
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.debug );
			break;
		case 'D':
			tol.dist = atof(optarg);
			tol.dist_sq = tol.dist * tol.dist;
			rt_pr_tol( &tol );
			break;
		case 'X':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

	/* Open BRL-CAD database */
	argc -= optind;
	argv += optind;
	if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
		perror(argv[0]);
		exit(1);
	}
	db_dirbuild(dbip);

	/* Create .fig file name and open it. */
	bu_vls_init( &fig_file );
	/* Ignore leading path name. */
	if ((dot = strrchr(argv[0], '/')) != (char *)NULL) {
		if (prefix)
			bu_vls_strcpy( &fig_file, prefix );
		bu_vls_strcat( &fig_file, dot+1 );
	} else {
		if (prefix)
			bu_vls_strcpy( &fig_file, prefix );
		bu_vls_strcat( &fig_file, argv[0] );
	}

	/* Get rid of any file name extension (probably .g). */
	if ((dot = strrchr(bu_vls_addr(&fig_file), '.')) != (char *)NULL)  {
		*dot = (char)NULL;
		/* Recalculate shorter VLS length.  Ugh. */
		bu_vls_trunc( &fig_file, dot - bu_vls_addr(&fig_file) );
	}
	bu_vls_strcat( &fig_file, ".fig");	/* Add required Jack suffix. */

	if ((fp_fig = fopen(bu_vls_addr(&fig_file), "w")) == NULL)  {
		perror(bu_vls_addr(&fig_file));
		return 2;
	}
	fprintf(fp_fig, "figure {\n");
	bu_vls_init(&base_seg);		/* .fig figure file's main segment. */

BN_CK_TOL(jack_tree_state.ts_tol);
RT_CK_TESS_TOL(jack_tree_state.ts_ttol);

	/* Walk indicated tree(s).  Each region will be output separately */
	(void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
		1,			/* ncpu */
		&jack_tree_state,
		0,			/* take all regions */
		do_region_end,
		nmg_booltree_leaf_tess,
		(genptr_t)NULL);	/* in librt/nmg_bool.c */

	fprintf(fp_fig, "\troot=%s_seg.base;\n", bu_vls_addr(&base_seg));
	fprintf(fp_fig, "}\n");
	fclose(fp_fig);
	bu_vls_free(&fig_file);
	bu_vls_free(&base_seg);

	percent = 0;
	if(regions_tried>0)  percent = ((double)regions_done * 100) / regions_tried;
	printf("Tried %d regions, %d converted successfully.  %g%%\n",
		regions_tried, regions_done, percent);

	/* Release dynamic storage */
	nmg_km(the_model);
	rt_vlist_cleanup();
	db_close(dbip);

#if MEMORY_LEAK_CHECKING
	bu_prmem("After complete G-JACK conversion");
#endif

	return 0;
}

/*
*			D O _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *do_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
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

	if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("\ndo_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_done * 100) / regions_tried : 0,
			sofar);
		bu_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( ncpu == 1 ) {
		if( BU_SETJUMP )  {
			/* Error, bail out */
			BU_UNSETJUMP;		/* Relinquish the protection */

			/* Sometimes the NMG library adds debugging bits when
			 * it detects an internal error, before rt_bomb().
			 */
			rt_g.NMG_debug = NMG_debug;	/* restore mode */

			/* Release any intersector 2d tables */
			nmg_isect2d_final_cleanup();

			/* Release the tree memory & input regions */
			db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

			/* Get rid of (m)any other intermediate structures */
			if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )  {
				nmg_km(*tsp->ts_m);
			} else {
				bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
			}

			/* Now, make a new, clean model structure for next pass. */
			*tsp->ts_m = nmg_mm();
			goto out;
		}
	}
	ret_tree = nmg_booltree_evaluate( curtree, tsp->ts_tol, &rt_uniresource );	/* librt/nmg_bool.c */
	BU_UNSETJUMP;		/* Relinquish the protection */
	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;
	regions_done++;
	if( r && !no_file_output )  {
		FILE	*fp_psurf;
		int	i;
		struct bu_vls	file_base;
		struct bu_vls	file;

		bu_vls_init(&file_base);
		bu_vls_init(&file);
		bu_vls_strcpy(&file_base, prefix);
		bu_vls_strcat(&file_base, DB_FULL_PATH_CUR_DIR(pathp)->d_namep);
		/* Dots confuse Jack's Peabody language.  Change to '_'. */
		for (i = 0; i < file_base.vls_len; i++)
			if (file_base.vls_str[i] == '.')
				file_base.vls_str[i] = '_';

		/* Write color attribute to .fig figure file. */
		if (tsp->ts_mater.ma_color_valid != 0) {
			fprintf(fp_fig, "\tattribute %s {\n",
				bu_vls_addr(&file_base));
			fprintf(fp_fig, "\t\trgb = (%f, %f, %f);\n",
				V3ARGS(tsp->ts_mater.ma_color));
			fprintf(fp_fig, "\t\tambient = 0.18;\n");
			fprintf(fp_fig, "\t\tdiffuse = 0.72;\n");
			fprintf(fp_fig, "\t}\n");
		}

		/* Write segment attributes to .fig figure file. */
		fprintf(fp_fig, "\tsegment %s_seg {\n", bu_vls_addr(&file_base));
		fprintf(fp_fig, "\t\tpsurf=\"%s.pss\";\n", bu_vls_addr(&file_base));
		if (tsp->ts_mater.ma_color_valid != 0)
			fprintf(fp_fig,
				"\t\tattribute=%s;\n", bu_vls_addr(&file_base));
		fprintf(fp_fig, "\t\tsite base->location=trans(0,0,0);\n");
		fprintf(fp_fig, "\t}\n");

		if( bu_vls_strlen(&base_seg) <= 0 )  {
			bu_vls_vlscat( &base_seg, &file_base );
		} else {
			fprintf(fp_fig, "\tjoint %s_jt {\n",
				bu_vls_addr(&file_base));
			fprintf(fp_fig,
				"\t\tconnect %s_seg.base to %s_seg.base;\n",
				bu_vls_addr(&file_base),
				bu_vls_addr(&base_seg) );
			fprintf(fp_fig, "\t}\n");
		}

		bu_vls_vlscat(&file, &file_base);
		bu_vls_strcat(&file, ".pss");	/* Required Jack suffix. */

		/* Write psurf to .pss file. */
		if ((fp_psurf = fopen(bu_vls_addr(&file), "w")) == NULL)
			perror(bu_vls_addr(&file));
		else {
			nmg_to_psurf(r, fp_psurf);
			fclose(fp_psurf);
			if(verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
		}
		bu_vls_free(&file);

		/* Also write as UNIX-plot file, if desired */
		if( debug_plots )  {
			FILE	*fp;
			bu_vls_vlscat(&file, &file_base);
			bu_vls_strcat(&file, ".pl");

			if ((fp = fopen(bu_vls_addr(&file), "w")) == NULL)
				perror(bu_vls_addr(&file));
			else {
				struct bu_list	vhead;
				pl_color( fp,
					(int)(tsp->ts_mater.ma_color[0] * 255),
					(int)(tsp->ts_mater.ma_color[1] * 255),
					(int)(tsp->ts_mater.ma_color[2] * 255) );
				/* nmg_pl_r( fp, r ); */
				BU_LIST_INIT( &vhead );
				nmg_r_to_vlist( &vhead, r, 0 );
				rt_vlist_to_uplot( fp, &vhead );
				fclose(fp);
				RT_FREE_VLIST( &vhead );
				if(verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
			}
			bu_vls_free(&file);
		}
		bu_vls_free(&file_base);
	}
	if( no_file_output )  {
		if(verbose) bu_log("*** Completed %s\n",
			DB_FULL_PATH_CUR_DIR(pathp)->d_namep );
	}

	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  and there is no point to adding _another_ message to our output,
	 *  so we need to cons up an OP_NOP node to return.
	 */
	db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

out:
	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}

/*
*	N M G _ T O _ P S U R F
*
*	Convert an nmg region into Jack format.  This routine makes a
*	list of unique vertices and writes them to the ascii Jack
*	data base file.  Then a routine to generate the face vertex
*	data is called.
*/

void
nmg_to_psurf(struct nmgregion *r, FILE *fp_psurf)
                    		/* NMG region to be converted. */
    		          	/* Jack format file to write vertex list to. */
{
	int			i;
	int			*map;	/* map from v->index to Jack vert # */
	struct bu_ptbl		vtab;	/* vertex table */

	map = (int *)bu_calloc(r->m_p->maxindex, sizeof(int *), "Jack vert map");

	/* Built list of vertex structs */
	nmg_vertex_tabulate( &vtab, &r->l.magic );

	/* XXX What to do if 0 vertices?  */

	/* Print list of unique vertices and convert from mm to cm. */
	for (i = 0; i < BU_PTBL_END(&vtab); i++)  {
		struct vertex			*v;
		register struct vertex_g	*vg;
		v = (struct vertex *)BU_PTBL_GET(&vtab,i);
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		NMG_CK_VERTEX_G(vg);
		NMG_INDEX_ASSIGN( map, v, i+1 );  /* map[v->index] = i+1 */
		fprintf(fp_psurf, "%f\t%f\t%f\n",
			vg->coord[X] / 10.,
			vg->coord[Y] / 10.,
			vg->coord[Z] / 10.);
	}
	fprintf(fp_psurf, ";;\n");

	jack_faces(r, fp_psurf, map);

	bu_ptbl( &vtab, BU_PTBL_FREE, 0 );
	bu_free( (char *)map, "Jack vert map" );
}


/*
*	J A C K _ F A C E S
*
*	Continues the conversion of an nmg into Jack format.  Before
*	this routine is called, a list of unique vertices has been
*	stored in a heap.  Using this heap and the nmg structure, a
*	list of face vertices is written to the Jack data base file.
*/
void
jack_faces(struct nmgregion *r, FILE *fp_psurf, int *map)
                    		/* NMG region to be converted. */
    		          	/* Jack format file to write face vertices to. */

{
	struct edgeuse		*eu;
	struct faceuse		*fu;
	struct loopuse		*lu;
	struct shell		*s;
	struct vertex		*v;

	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
		/* Shell is made of faces. */
		for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
			NMG_CK_FACEUSE(fu);
			if (fu->orientation != OT_SAME)
				continue;
			for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
				NMG_CK_LOOPUSE(lu);
				if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
					for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
						NMG_CK_EDGEUSE(eu);
						NMG_CK_EDGE(eu->e_p);
						NMG_CK_VERTEXUSE(eu->vu_p);
						NMG_CK_VERTEX(eu->vu_p->v_p);
						NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		    				fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map,eu->vu_p->v_p));
					}
				} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd)
		  			== NMG_VERTEXUSE_MAGIC) {
		  			v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
					NMG_CK_VERTEX(v);
					NMG_CK_VERTEX_G(v->vg_p);
		  			fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map,v));
				} else
					bu_log("jack_faces: loopuse mess up! (1)\n");
				fprintf(fp_psurf, ";\n");
			}
		}

		/* Shell contains loops. */
		for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
			NMG_CK_LOOPUSE(lu);
			if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
				for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
					NMG_CK_EDGEUSE(eu);
					NMG_CK_EDGE(eu->e_p);
					NMG_CK_VERTEXUSE(eu->vu_p);
					NMG_CK_VERTEX(eu->vu_p->v_p);
					NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		  			fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map,eu->vu_p->v_p));
				}
			} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd)
				== NMG_VERTEXUSE_MAGIC) {
				v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
				NMG_CK_VERTEX(v);
				NMG_CK_VERTEX_G(v->vg_p);
				fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map,v));
			} else
				bu_log("jack_faces: loopuse mess up! (1)\n");
			fprintf(fp_psurf, ";\n");
		}

		/* Shell contains edges. */
		for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
			fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map,eu->vu_p->v_p));
		}
		if (BU_LIST_FIRST_MAGIC(&s->eu_hd) == NMG_EDGEUSE_MAGIC)
			fprintf(fp_psurf, ";\n");

		/* Shell contains a single vertex. */
		if (s->vu_p) {
			NMG_CK_VERTEXUSE(s->vu_p);
			NMG_CK_VERTEX(s->vu_p->v_p);
			NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
			fprintf(fp_psurf, "%d;\n", NMG_INDEX_GET(map,s->vu_p->v_p));
		}

		if (BU_LIST_IS_EMPTY(&s->fu_hd) &&
			BU_LIST_IS_EMPTY(&s->lu_hd) &&
			BU_LIST_IS_EMPTY(&s->eu_hd) && !s->vu_p) {
			bu_log("WARNING jack_faces: empty shell\n");
		}

	}
	fprintf(fp_psurf, ";;\n");
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
