/*
 *			G - A C A D . C
 *
 *  Program to convert a BRL-CAD model (in a .g file) to an ACAD file
 *  by calling on the NMG booleans.
 *
 *  Authors -
 *	John R. Anderson
 *	Bill Mermagen Jr.
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"
#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4

RT_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));

extern double nmg_eue_dist;		/* from nmg_plot.c */

static char	usage[] = "\
Usage: %s [-v][-i][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol]\n\
[-e error_file ][-D dist_calc_tol] -o output_file_name brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;	/* Number of processors */
static char	*output_file = NULL;	/* output filename */
static char	*error_file = NULL;	/* error filename */
static FILE	*fp;		/* Output file pointer */
static FILE	*fpe;		/* Error file pointer */
static struct db_i		*dbip;
static struct rt_vls		base_seg;
static struct rt_tess_tol	ttol;
static struct rt_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;
static int	regions_written = 0;
static int	inches = 0;

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	register int	c;
	double		percent;
	char 		buf[80];
#ifdef BSD
	setlinebuf( stderr );
#else
#	if defined( SYSV ) && !defined( sgi ) && !defined(CRAY2) && \
	 !defined(n16)
		(void) setvbuf( stderr, (char *) NULL, _IOLBF, BUFSIZ );
#	endif
#	if defined(sgi) && defined(mips)
		if( setlinebuf( stderr ) != 0 )
			perror("setlinebuf(stderr)");
#	endif
#endif

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	tree_state = rt_initial_tree_state;	/* struct copy */
	tree_state.ts_tol = &tol;
	tree_state.ts_ttol = &ttol;
	tree_state.ts_m = &the_model;

	ttol.magic = RT_TESS_TOL_MAGIC;
	/* Defaults, updated by command line options. */
	ttol.abs = 0.0;
	ttol.rel = 0.01;
	ttol.norm = 0.0;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;

	the_model = nmg_mm();
	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "a:n:o:r:vx:D:P:X:e:i:")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'o':		/* Output file name. */
			output_file = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			ncpu = atoi( optarg );
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'D':
			tol.dist = atof(optarg);
			tol.dist_sq = tol.dist * tol.dist;
			rt_pr_tol( &tol );
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		case 'e':		/* Error file name. */
			error_file = optarg;
			break;
		case 'i':
			inches = 1;
			break;
		default:
			rt_log(  usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		rt_log( usage, argv[0]);
		exit(1);
	}

	if( !output_file )
		fp = stdout;
	else
	{
		/* Open output file */
		if( (fp=fopen( output_file, "w+" )) == NULL )
		{
			rt_log( "Cannot open output file (%s) for writing\n", output_file );
			perror( argv[0] );
			exit( 1 );
		}
	}

	/* Open g-acad error log file */
	if( !error_file)
		fpe = stderr;
	else
	if( (fpe=fopen( error_file, "w" )) == NULL )
	{
	rt_log( "Cannot open output file (%s) for writing\n", error_file );
		perror( argv[0] );
		exit( 1 );
	}

	/* Open brl-cad database */
	argc -= optind;
	argv += optind;
	if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
		perror(argv[0]);
		exit(1);
	}
	db_scan(dbip, (int (*)())db_diradd, 1);


	RT_CK_TOL(tree_state.ts_tol);
	RT_CK_TESS_TOL(tree_state.ts_ttol);

/* Write out ACAD facet header */
	if (inches)
		fprintf(fp,"BRL-CAD generated ACAD FACET FILE (Units in)\n");
	else
		fprintf(fp,"BRL-CAD generated ACAD FACET FILE (Units mm)\n");

/* Generate space for number of facet entities, will write over later */

	fprintf(fp,"               ");

	/* Walk indicated tree(s).  Each region will be output separately */
	(void) db_walk_tree(dbip, argc-1, (CONST char **)(argv+1),
		1,			/* ncpu */
		&tree_state,
		0,			/* take all regions */
		do_region_end,
		nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */

	percent = 0;
	if(regions_tried>0){
		percent = ((double)regions_converted * 100) / regions_tried;
		printf("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
			regions_tried, regions_converted, percent);
	}
	percent = 0;

	if( regions_tried > 0 ){
		percent = ((double)regions_written * 100) / regions_tried;
	printf( "                  %d triangulated successfully. %g%%\n",
		regions_written, percent );
	}
/* XXX Write out number of facet entities to .facet file */

	rewind(fp);
	fseek(fp,46,0); /* Re-position pointer to 2nd line */
	fprintf(fp,"%d\n",regions_written); /* Write out number of regions */
	fclose(fp);

	/* Release dynamic storage */
	nmg_km(the_model);
	rt_vlist_cleanup();
	db_close(dbip);

#if MEMORY_LEAK_CHECKING
	rt_prmem("After complete G-ACAD conversion");
#endif

	return 0;
}

static void
nmg_to_acad( r, pathp, region_id, material_id )
struct nmgregion *r;
struct db_full_path *pathp;
int region_id;
int material_id;
{
	struct model *m;
	struct shell *s;
	struct vertex *v;
	struct nmg_ptbl verts;
	char *region_name;
	int numverts = 0;		/* Number of vertices to output */
	int numtri   = 0;		/* Number of triangles to output */
	int tricount = 0;		/* Triangle number */
	int i;
	float npercent;
	float tpercent;

	NMG_CK_REGION( r );
	RT_CK_FULL_PATH(pathp);

	region_name = db_path_to_string( pathp );

	m = r->m_p;
	NMG_CK_MODEL( m );

	/* triangulate model */
	nmg_triangulate_model( m, &tol );


	/* list all vertices in result */
	nmg_vertex_tabulate( &verts, &r->l.magic );

	/* Get number of vertices */

	numverts = NMG_TBL_END (&verts);

/* XXX Check vertices, shells faces first? Do not want to punt mid-stream */
/* BEGIN CHECK SECTION */
/* Check vertices */

	for( i=0 ; i<numverts ; i++ )
	{
		v = (struct vertex *)NMG_TBL_GET( &verts, i );
		NMG_CK_VERTEX( v );
	}

/* Check triangles */
 	for( RT_LIST_FOR( s, shell, &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );

		for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
			struct loopuse *lu;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
			{
				struct edgeuse *eu;
				int vert_count=0;

				NMG_CK_LOOPUSE( lu );

				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				/* check vertex numbers for each triangle */
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );

					v = eu->vu_p->v_p;
					NMG_CK_VERTEX( v );

					vert_count++;
					i = nmg_tbl( &verts, TBL_LOC, (long *)v );
					if( i < 0 )
					{
		/*XXX*/				nmg_tbl( &verts, TBL_FREE, (long *)NULL );
		/*XXX*/				rt_free( region_name, "region name" );
						rt_log( "Vertex from eu x%x is not in nmgregion x%x\n", eu, r );
						rt_bomb( "Can't find vertex in list!!!" );
					}
				}
				if( vert_count != 3 )
				{
		/*XXX*/			nmg_tbl( &verts, TBL_FREE, (long *)NULL );
		/*XXX*/			rt_free( region_name, "region name" );
					rt_log( "lu x%x has %d vertices!!!!\n", lu, vert_count );
					rt_bomb( "LU is not a triangle" );
				}
				numtri++;
			}
		}
	}

/* END CHECK SECTION */
/* Write pertinent info for this region */

	fprintf( fp, "%s\n", (region_name+1)); 
/* No mirror plane */
	fprintf( fp, "%d\n", 0);
/* Number of vertices */
	fprintf( fp, "%d\n",numverts);


	/* Write numverts, then vertices */

	for( i=0 ; i<numverts ; i++ )
	{
		v = (struct vertex *)NMG_TBL_GET( &verts, i );
		NMG_CK_VERTEX( v );
		if (inches)
			fprintf( fp, "%f %f %f\n", V3ARGSIN( v->vg_p->coord ));
		else
			fprintf( fp, "%f %f %f\n", V3ARGS( v->vg_p->coord ));
	}

/* Number of sub-parts (always 1 with BRL-CAD) */
	fprintf( fp, "%d\n",1);
/* Write out name again */
	fprintf( fp, "%s\n", (region_name+1)); 
/* Number of triangles, number of vert/tri (3) */
	fprintf( fp,"%d       %d\n",numtri,3);

	/* output triangles */
 	for( RT_LIST_FOR( s, shell, &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );

		for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
			struct loopuse *lu;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
			{
				struct edgeuse *eu;
				int vert_count=0;

				NMG_CK_LOOPUSE( lu );

				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				/* list vertex numbers for each triangle */
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );

					v = eu->vu_p->v_p;
					NMG_CK_VERTEX( v );

					vert_count++;
					i = nmg_tbl( &verts, TBL_LOC, (long *)v );
					if( i < 0 )
					{
						nmg_tbl( &verts, TBL_FREE, (long *)NULL );
						rt_log( "Vertex from eu x%x is not in nmgregion x%x\n", eu, r );
		/*XXX*/				rt_free( region_name, "region name" );
		/*XXX*/				rt_bomb( "Can't find vertex in list!!!" );
					}

					fprintf( fp, " %d", i+1 );
				}
/* Output other info. for triangle ICOAT, component#, facet# */
/* XXX Map Icoat from material table later */
/* fprintf( fp, "%s icomp=%d material=%d:\n", (region_name+1), region_id, material_id );*/

				fprintf( fp, " %d    %d    %d\n", 0, region_id, ++tricount);

				if( vert_count != 3 )
				{
					nmg_tbl( &verts, TBL_FREE, (long *)NULL );
					rt_free( region_name, "region name" );
					rt_log( "lu x%x has %d vertices!!!!\n", lu, vert_count );
					rt_bomb( "LU is not a triangle" );
				}
			}
		}
	}
/*	regions_converted++;  
	printf("Processed region %s\n",region_name);
	printf("Regions attempted = %d Regions done = %d\n",regions_tried,regions_converted);
	fflush(stdout);
*/
	nmg_tbl( &verts, TBL_FREE, (long *)NULL );
	rt_free( region_name, "region name" );
}

/*
*			D O _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *do_region_end(tsp, pathp, curtree)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	extern FILE		*fp_fig;
	union tree		*ret_tree;
	struct rt_list		vhead;
	struct nmgregion	*r;
	int			failed;

	RT_CK_FULL_PATH(pathp);
	RT_CK_TREE(curtree);
	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	RT_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		rt_log("\ndo_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
			sofar);
		rt_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( ncpu == 1 ) {
		if( RT_SETJUMP )  {
			/* Error, bail out */
			char *sofar;
			RT_UNSETJUMP;		/* Relinquish the protection */
			
			sofar = db_path_to_string(pathp);
	                rt_log( "FAILED in Boolean evaluation: %s\n", sofar );
			fprintf(fpe,"Failed Bool. Eval.: %s\n",sofar);
			fflush(fpe);
                        rt_free( (char *)sofar, "sofar" );
                                
			/* Sometimes the NMG library adds debugging bits when
			 * it detects an internal error, before rt_bomb().
			 */
			rt_g.NMG_debug = NMG_debug;	/* restore mode */

			/* Release any intersector 2d tables */
			nmg_isect2d_final_cleanup();

			/* Release the tree memory & input regions */
/*XXX*/			/* db_free_tree(curtree);*/		/* Does an nmg_kr() */

			/* Get rid of (m)any other intermediate structures */
			if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )  {
				nmg_km(*tsp->ts_m);
			} else {
				rt_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
			}

			/* Now, make a new, clean model structure for next pass. */
			*tsp->ts_m = nmg_mm();
			goto out;
		}
	}
	printf("Attempting to process region %s\n",db_path_to_string( pathp ));
	fflush(stdout);
	ret_tree = nmg_booltree_evaluate( curtree, tsp->ts_tol );	/* librt/nmg_bool.c */

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
	{
		printf( "\tNothing left of this region after Boolean evaluation!!!\n" );
		fprintf( fpe, "WARNING: Nothing left after Boolean evaluation: %s\n",
			db_path_to_string( pathp ));
		fflush(fpe);
		regions_written++; /* don't count as a failure */
		r = (struct nmgregion *)NULL;
	}
/*	regions_done++;  XXX */
	
	RT_UNSETJUMP;		/* Relinquish the protection */
	regions_converted++;

	if (r != 0)
	{
		struct shell *s;
		int empty_region=0;
		int empty_model=0;

		/* Kill cracks */
		s = RT_LIST_FIRST( shell, &r->s_hd );
		while( RT_LIST_NOT_HEAD( &s->l, &r->s_hd ) )
		{
			struct shell *next_s;

			next_s = RT_LIST_PNEXT( shell, &s->l );
			if( nmg_kill_cracks( s ) )
			{
				if( nmg_ks( s ) )
				{
					empty_region = 1;
					break;
				}
			}
			s = next_s;
		}

		/* kill zero length edgeuses */
		if( !empty_region )
		{
			 empty_model = nmg_kill_zero_length_edgeuses( *tsp->ts_m );
		}

		if( !empty_region && !empty_model )
		{
			if( RT_SETJUMP )
			{
				char *sofar;

				RT_UNSETJUMP;

				sofar = db_path_to_string(pathp);
				rt_log( "FAILED in triangulator: %s\n", sofar );
				fprintf(fpe,"Failed in triangulator: %s\n",sofar);
				fflush(fpe);
				rt_free( (char *)sofar, "sofar" );

				/* Sometimes the NMG library adds debugging bits when
				 * it detects an internal error, before rt_bomb().
				 */
				rt_g.NMG_debug = NMG_debug;	/* restore mode */

				/* Release any intersector 2d tables */
				nmg_isect2d_final_cleanup();

				/* Get rid of (m)any other intermediate structures */
				if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )
				{
					nmg_km(*tsp->ts_m);
				}
				else
				{
					rt_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
				}
			
				/* Now, make a new, clean model structure for next pass. */
				*tsp->ts_m = nmg_mm();
				goto out;
			}
		/* Write the region to the TANKILL file */
			nmg_to_acad( r, pathp, tsp->ts_regionid, tsp->ts_gmater );

			regions_written++;

			RT_UNSETJUMP;
		}

		if( !empty_model )
			nmg_kr( r );
	}

out:
	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  and there is no point to adding _another_ message to our output,
	 *  so we need to cons up an OP_NOP node to return.
	 */


	if(regions_tried>0){
		float npercent, tpercent;

		npercent = (float)(regions_converted * 100) / regions_tried;
		tpercent = (float)(regions_written * 100) / regions_tried;
		printf("Tried %d regions, %d conv. to NMG's %d conv. to tri. nmgper = %.2f%% triper = %.2f%% \n",
		regions_tried, regions_converted, regions_written, npercent,tpercent);
	}

	db_free_tree(curtree);		/* Does an nmg_kr() */

	GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}
