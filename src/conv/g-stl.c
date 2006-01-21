/*                         G - S T L . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file g-stl.c
 *
 *  Program to convert a BRL-CAD model (in a .g file) to an STL file
 *  by calling on the NMG booleans.  Based on g-acad.c.
 *
 *  Authors -
 *	Charles M. Kennedy
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

/* system headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif

/* interface headers */
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local headers */
#include "../librt/debug.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN( a, b )	{\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}

BU_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));

static char	usage[] = "\
Usage: %s [-b][-v][-i][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol]\n\
[-D dist_calc_tol] [-o output_file_name.stl | -m directory_name] brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;	/* Number of processors */
static int	binary = 0;	/* Default output is ASCII */
static char	*output_file = NULL;	/* output filename */
static char	*output_directory = NULL; /* directory name to hold output files */
static struct bu_vls file_name;	/* file name built from region name */
static FILE	*fp;		/* Output file pointer */
static int	bfd;		/* Output binary file descriptor */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;	/* tesselation tolerance in mm */
static struct bn_tol		tol;	/* calculation tolerance */
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int		regions_tried = 0;
static int		regions_converted = 0;
static int		regions_written = 0;
static int		inches = 0;
static unsigned int	tot_polygons = 0;


/* Byte swaps a four byte value */
void
lswap( unsigned int *v)
{
	unsigned int r;

	r =*v;
	*v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
		| ((r & 0xff000000) >> 24);
}

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
	int		i;

	bu_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
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
	/* XXX These need to be improved */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;

	/* init resources we might need */
	rt_init_resource( &rt_uniresource, 0, NULL );

	/* make empty NMG model */
	the_model = nmg_mm();
	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = bu_getopt(argc, argv, "a:bm:n:o:r:vx:D:P:X:i")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(bu_optarg);
			ttol.rel = 0.0;
			break;
		case 'b':		/* Binary output file */
			binary=1;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(bu_optarg);
			ttol.rel = 0.0;
			break;
		case 'o':		/* Output file name. */
			output_file = bu_optarg;
			break;
		case 'm':
			output_directory = bu_optarg;
			bu_vls_init( &file_name );
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(bu_optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			ncpu = atoi( bu_optarg );
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
			break;
		case 'D':
			tol.dist = atof(bu_optarg);
			tol.dist_sq = tol.dist * tol.dist;
			rt_pr_tol( &tol );
			break;
		case 'X':
			sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		case 'i':
			inches = 1;
			break;
		default:
			bu_log(  usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (bu_optind+1 >= argc) {
		bu_log( usage, argv[0]);
		exit(1);
	}

	if( output_file && output_directory ) {
		bu_log( "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n" );
		bu_log( usage, argv[0] );
		exit( 1 );
	}

	if( !output_file && !output_directory ) {
		if( binary ) {
			bu_log( "Can't output binary to stdout\n");
			exit( 1 );
		}
		fp = stdout;
	} else if( output_file ) {
		if( !binary ) {
			/* Open ASCII output file */
			if( (fp=fopen( output_file, "w+" )) == NULL )
			{
				bu_log( "Cannot open ASCII output file (%s) for writing\n", output_file );
				perror( argv[0] );
				exit( 1 );
			}
		} else {
			/* Open binary output file */
#ifdef _WIN32
			if ((bfd=open(output_file, _O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY, _S_IREAD|_S_IWRITE)) < 0)
#else
			if( (bfd=open( output_file, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 )
#endif
			{
				bu_log( "Cannot open binary output file (%s) for writing\n", output_file );
				perror( argv[0] );
				exit( 1 );
			}
		}
	}

	/* Open brl-cad database */
	argc -= bu_optind;
	argv += bu_optind;
	if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
		perror(argv[0]);
		exit(1);
	}
	db_dirbuild( dbip );

	BN_CK_TOL(tree_state.ts_tol);
	RT_CK_TESS_TOL(tree_state.ts_ttol);

	if( verbose ) {
		bu_log( "Model: %s\n", argv[0] );
		bu_log( "Objects:" );
		for( i=1 ; i<argc ; i++ )
			bu_log( " %s", argv[i] );
		bu_log( "\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
			tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm );
		bu_log( "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
			tree_state.ts_tol->dist, tree_state.ts_tol->perp );
	}

	/* Write out STL header if output file is binary */
	if( binary && output_file ) {
		char buf[81];	/* need exactly 80 char for header */

		bzero( buf, sizeof( buf ) );
		if (inches) {
			strcpy( buf, "BRL-CAD generated STL FILE (Units=inches)");
		} else {
			strcpy( buf, "BRL-CAD generated STL FILE (Units=mm)");
		}
		write(bfd, &buf, 80);

		/* write a place keeper for the number of triangles */
		bzero( buf, 4 );
		write(bfd, &buf, 4);
	}

	/* Walk indicated tree(s).  Each region will be output separately */
	(void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
		1,			/* ncpu */
		&tree_state,
		0,			/* take all regions */
		do_region_end,
		nmg_booltree_leaf_tess,
		(genptr_t)NULL);	/* in librt/nmg_bool.c */

	percent = 0;
	if(regions_tried>0){
		percent = ((double)regions_converted * 100) / regions_tried;
		if( verbose )
			bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
				regions_tried, regions_converted, percent);
	}
	percent = 0;

	if( regions_tried > 0 ){
		percent = ((double)regions_written * 100) / regions_tried;
		if( verbose )
			bu_log( "                  %d triangulated successfully. %g%%\n",
				regions_written, percent );
	}

	bu_log( "%ld triangles written\n", tot_polygons );

	if( output_file ) {
		if( binary ) {
			unsigned char tot_buffer[4];

			/* Re-position pointer to 80th byte */
			lseek( bfd, 80, SEEK_SET );

			/* Write out number of triangles */
			bu_plong( tot_buffer, (unsigned long)tot_polygons );
			lswap( (unsigned int *)tot_buffer );
			write(bfd, tot_buffer, 4);
			close( bfd );
		} else {
			fclose(fp);
		}
	}

	/* Release dynamic storage */
	nmg_km(the_model);
	rt_vlist_cleanup();
	db_close(dbip);

#if MEMORY_LEAK_CHECKING
	bu_prmem("After complete G-STL conversion");
#endif

	return 0;
}

static void
nmg_to_stl( r, pathp, region_id, material_id )
struct nmgregion *r;
struct db_full_path *pathp;
int region_id;
int material_id;
{
	struct model *m;
	struct shell *s;
	struct vertex *v;
	char *region_name;
	int region_polys=0;

	NMG_CK_REGION( r );
	RT_CK_FULL_PATH(pathp);

	region_name = db_path_to_string( pathp );

	if( output_directory ) {
		char *c;

		bu_vls_trunc( &file_name, 0 );
		bu_vls_strcpy( &file_name, output_directory );
		bu_vls_putc( &file_name, '/' );
		c = region_name;
		c++;
		while( *c != '\0' ) {
			if( *c == '/' ) {
				bu_vls_putc( &file_name, '@' );
			} else if( *c == '.' || isspace( *c ) ) {
				bu_vls_putc( &file_name, '_' );
			} else {
				bu_vls_putc( &file_name, *c );
			}
			c++;
		}
		bu_vls_strcat( &file_name, ".stl" );
		if( !binary ) {
			/* Open ASCII output file */
			if( (fp=fopen( bu_vls_addr( &file_name ), "w+" )) == NULL )
			{
				bu_log( "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr( &file_name ) );
				perror( "g-stl" );
				exit( 1 );
			}
		} else {
			char buf[81];	/* need exactly 80 char for header */

			/* Open binary output file */
#ifdef _WIN32
			if ((bfd=open(bu_vls_addr(&file_name), _O_WRONLY|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE)) < 0)
#else
			if( (bfd=open( bu_vls_addr( &file_name ), O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 )
#endif
			{
				bu_log( "Cannot open binary output file (%s) for writing\n", bu_vls_addr( &file_name ) );
				perror( "g-stl" );
				exit( 1 );
			}

			bzero( buf, sizeof( buf ) );
			if (inches) {
				strcpy( buf, "BRL-CAD generated STL FILE (Units=inches)");
			} else {
				strcpy( buf, "BRL-CAD generated STL FILE (Units=mm)");
			}
			if( strlen( buf ) + strlen( region_name ) + 1 < 80 ) {
				strcat( buf, " " );
				strcat( buf, region_name );
			}
			write(bfd, &buf, 80);

			/* write a place keeper for the number of triangles */
			bzero( buf, 4 );
			write(bfd, &buf, 4);
		}
	}

	m = r->m_p;
	NMG_CK_MODEL( m );

	/* Write pertinent info for this region */
	if( !binary )
		fprintf( fp, "solid %s\n", (region_name+1));

	/* triangulate model */
	nmg_triangulate_model( m, &tol );

	/* Check triangles */
 	for( BU_LIST_FOR( s, shell, &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );

		for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
			struct loopuse *lu;
			vect_t facet_normal;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			/* Grab the face normal and save it for all the vertex loops */
			NMG_GET_FU_NORMAL( facet_normal, fu);

			for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
			{
				struct edgeuse *eu;
				int vert_count=0;
				float flts[12];
				float *flt_ptr;
				unsigned char vert_buffer[50];

				NMG_CK_LOOPUSE( lu );

				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				bzero( vert_buffer, sizeof( vert_buffer ) );

				if( !binary ) {
					fprintf( fp, "  facet normal %f %f %f\n", V3ARGS( facet_normal ) );
					fprintf( fp, "    outer loop\n");
				} else {
					flt_ptr = flts;
					VMOVE( flt_ptr, facet_normal );
					flt_ptr += 3;
				}

				/* check vertex numbers for each triangle */
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );

					vert_count++;

					v = eu->vu_p->v_p;
					NMG_CK_VERTEX( v );
					if( !binary )
						fprintf( fp, "      vertex ");
					if (inches)
						if( !binary ) {
							fprintf( fp, "%f %f %f\n", V3ARGSIN( v->vg_p->coord ));
						} else {
							VSETIN( flt_ptr, v->vg_p->coord );
							flt_ptr += 3;
						}
					else
						if( !binary ) {
							fprintf( fp, "%f %f %f\n", V3ARGS( v->vg_p->coord ));
						} else {
							VMOVE( flt_ptr, v->vg_p->coord );
							flt_ptr += 3;
						}
				}
				if( vert_count > 3 )
				{
					bu_free( region_name, "region name" );
					bu_log( "lu x%x has %d vertices!!!!\n", lu, vert_count );
					rt_bomb( "LU is not a triangle" );
				}
				else if( vert_count < 3 )
					continue;
				if( !binary ) {
					fprintf( fp, "    endloop\n");
					fprintf( fp, "  endfacet\n");
				} else {
					int i;

					htonf(vert_buffer, (const unsigned char *)flts, 12 );
					for( i=0 ; i<12 ; i++ ) {
						lswap( (unsigned int *)&vert_buffer[i*4] );
					}
					write(bfd, vert_buffer, 50);
				}
				tot_polygons++;
				region_polys++;
			}
		}
	}
	if( !binary )
		fprintf( fp, "endsolid %s\n", (region_name+1));

	if( output_directory ) {
		if( binary ) {
			unsigned char tot_buffer[4];

			/* Re-position pointer to 80th byte */
			lseek( bfd, 80, SEEK_SET );

			/* Write out number of triangles */
			bu_plong( tot_buffer, (unsigned long)region_polys );
			lswap( (unsigned int *)tot_buffer );
			write(bfd, tot_buffer, 4);
			close( bfd );
		} else {
			fclose(fp);
		}
	}
	bu_free( region_name, "region name" );
}

/*
*			D O _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *do_region_end(tsp, pathp, curtree, client_data)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
genptr_t		client_data;
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
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
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
			char *sofar;
			BU_UNSETJUMP;		/* Relinquish the protection */

			sofar = db_path_to_string(pathp);
	                bu_log( "FAILED in Boolean evaluation: %s\n", sofar );
                        bu_free( (char *)sofar, "sofar" );

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
				bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
			}

			/* Now, make a new, clean model structure for next pass. */
			*tsp->ts_m = nmg_mm();
			goto out;
		}
	}
	if( verbose )
		bu_log("Attempting to process region %s\n",db_path_to_string( pathp ));

	ret_tree = nmg_booltree_evaluate( curtree, tsp->ts_tol, &rt_uniresource );	/* librt/nmg_bool.c */
	BU_UNSETJUMP;		/* Relinquish the protection */

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
	{
	    if( verbose ) {
		bu_log( "\tNothing left of this region after Boolean evaluation\n" );
	    }
	    regions_written++; /* don't count as a failure */
	    r = (struct nmgregion *)NULL;
	}
/*	regions_done++;  XXX */

	regions_converted++;

	if (r != (struct nmgregion *)NULL)
	{
		struct shell *s;
		int empty_region=0;
		int empty_model=0;

		/* Kill cracks */
		s = BU_LIST_FIRST( shell, &r->s_hd );
		while( BU_LIST_NOT_HEAD( &s->l, &r->s_hd ) )
		{
			struct shell *next_s;

			next_s = BU_LIST_PNEXT( shell, &s->l );
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
			if( BU_SETJUMP )
			{
				char *sofar;

				BU_UNSETJUMP;

				sofar = db_path_to_string(pathp);
				bu_log( "FAILED in triangulator: %s\n", sofar );
				bu_free( (char *)sofar, "sofar" );

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
					bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
				}

				/* Now, make a new, clean model structure for next pass. */
				*tsp->ts_m = nmg_mm();
				goto out;
			}
			/* Write the region to the STL file */
			nmg_to_stl( r, pathp, tsp->ts_regionid, tsp->ts_gmater );

			regions_written++;

			BU_UNSETJUMP;
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
		if( verbose )
			bu_log("Tried %d regions, %d conv. to NMG's %d conv. to tri. nmgper = %.2f%% triper = %.2f%% \n",
				regions_tried, regions_converted, regions_written, npercent,tpercent);
	}

	db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
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
