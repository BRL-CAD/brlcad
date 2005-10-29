/*                        G - I G E S . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 */
/** @file g-iges.c
 *
 *  Program to convert a BRL-CAD model (in a .g file) to an IGES BREP file
 *  or an IGES CSG file
 *
 *  Authors -
 *	John R. Anderson
 *	Michael J. Markowski
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *
 *  Some stupid secret codes:
 *	dp->d_uses - contains the negative of the DE number for the object
 *		     non-negative means it hasn't been written to the IGES file
 *	dp->d_nref - contains a one if the object is written to the IGES file as a BREP,
 *		     zero otherwise.
 */

static const char RCSid[] = "$Header$";
static const char RCSrev[] = "$Revision$";

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "../../iges/iges.h"
#include "../../librt/debug.h"


extern char	version[];

#define	CP_BUF_SIZE	1024	/* size of buffer for file copy */
#define SUFFIX_LEN	10	/* max size of suffix for 'part' files (-m option) */

BU_EXTERN( union tree *do_nmg_region_end , (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));
void w_start_global(
	FILE *fp_dir,
	FILE *fp_param,
	const char *db_name,
	const char *prog_name,
	const char *output_file,
	const char *id,
	const char *version);
BU_EXTERN( void w_terminate , (FILE *fp) );
BU_EXTERN( void write_edge_list , (struct nmgregion *r , int vert_de , struct bu_ptbl *etab , struct bu_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
BU_EXTERN( void write_vertex_list , ( struct nmgregion *r , struct bu_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
BU_EXTERN( void nmg_region_edge_list , ( struct bu_ptbl *tab , struct nmgregion *r ) );
BU_EXTERN( int nmgregion_to_iges , ( char *name , struct nmgregion *r , int dependent , FILE *fp_dir , FILE *fp_param ) );
BU_EXTERN( int write_shell_face_loop , ( struct nmgregion *r , int edge_de , struct bu_ptbl *etab , int vert_de , struct bu_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
BU_EXTERN( void csg_comb_func , ( struct db_i *dbip , struct directory *dp , genptr_t ptr ) );
BU_EXTERN( void csg_leaf_func , ( struct db_i *dbip , struct directory *dp , genptr_t ptr ) );
BU_EXTERN( void set_iges_tolerances , ( struct bn_tol *set_tol , struct rt_tess_tol *set_ttol ) );
BU_EXTERN( void count_refs , ( struct db_i *dbip , struct directory *dp , genptr_t ptr ) );
BU_EXTERN( int nmgregion_to_tsurf , ( char *name, struct nmgregion *r, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( int write_solid_instance , ( int orig_de, mat_t mat, FILE *fp_dir, FILE *fp_param ) );
BU_EXTERN( void get_props , ( struct iges_properties *props, struct rt_comb_internal *comb ) );
BU_EXTERN( int comb_to_iges , ( struct rt_comb_internal *comb, int length, int dependent, struct iges_properties *props, int de_pointers[], FILE *fp_dir, FILE *fp_param ) );

static char usage[] = "Usage: %s [-f|t|m] [-v] [-s] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-d dist_tol] [-o output_file] brlcad_db.g object(s)\n\
	options:\n\
		f - convert each region to facetted BREP before output\n\
		t - produce a file of trimmed surfaces (experimental)\n\
		m - produces a seperate IGES file for each region,\n\
			implies -t, -o gives directory for output IGES file\n\
		s - produce NURBS for faces of any BREP objects\n\
		v - verbose\n\
		a - absolute tolerance for tessellation (mm)\n\
		r - relative tolerance for tessellation\n\
		n - normal tolerance for tessellation\n\
		d - distance tolerance (mm) (minimum distance between distinct points)\n\
		x - librt debug flag\n\
		X - nmg debug flag\n\
		o - file to receive IGES output (or directory when '-m' option is used)\n\
	The f and t options are mutually exclusive. If neither is specified,\n\
	the default output is a CSG file to the maximum extent possible\n";

int		verbose=0;
static char	*db_name;	/* name of the BRL-CAD database */
static char	*prog_name;	/* name of this program as it was invoked */
static int	multi_file=0;	/* Flag to indicate output of seperate IGES file for each region */
static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	scale_error=0;	/* Count indicating how many scaled objects were encountered */
static int	solid_error=0;	/* Count indicating how many solids were not converted */
static int	comb_error=0;	/* Count indicating  how many combinations were not converted */
static int	ncpu = 1;	/* Number of processors */
static char	*output_file = NULL;	/* output filename */
static FILE	*fp_dir;	/* IGES start, global, and directory sections */
static FILE	*fp_param;	/* IGES parameter section */
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;
struct db_i		*dbip;

static struct db_tree_state	tree_state;	/* includes tol & model */

/* function table for converting solids to iges */
BU_EXTERN( int null_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int arb_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int ell_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int sph_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int tor_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int tgc_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( int nmg_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
BU_EXTERN( void iges_init , ( struct bn_tol *set_tol, struct rt_tess_tol *set_ttol, int set_verbose, struct db_i *dbip_set ) );
BU_EXTERN( void Print_stats , ( FILE *fp ) );

struct iges_functab
{
	int (*do_iges_write) BU_ARGS(( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
};

struct iges_functab iges_write[ID_MAXIMUM+1]={
	{null_to_iges},	/* ID_NULL */
	{tor_to_iges}, 	/* ID_TOR */
	{tgc_to_iges},	/* ID_TGC */
	{ell_to_iges},	/* ID_ELL */
	{arb_to_iges},	/* ID_ARB8 */
	{nmg_to_iges},	/* ID_ARS */
	{nmg_to_iges},	/* ID_HALF */
	{nmg_to_iges},	/* ID_REC */
	{nmg_to_iges},	/* ID_POLY */
	{nmg_to_iges},	/* ID_BSPLINE */
	{sph_to_iges},	/* ID_SPH */
	{nmg_to_iges},	/* ID_NMG */
	{null_to_iges},	/* ID_EBM */
	{null_to_iges},	/* ID_VOL */
	{nmg_to_iges},	/* ID_ARBN */
	{nmg_to_iges},	/* ID_PIPE */
	{null_to_iges},	/* ID_PARTICLE */
	{null_to_iges},	/* ID_RPC */
	{null_to_iges},	/* ID_RHC */
	{null_to_iges},	/* ID_EPA */
	{null_to_iges},	/* ID_EHY */
	{null_to_iges},	/* ID_ETO */
	{null_to_iges},	/* ID_GRIP */
	{null_to_iges},	/* ID_JOINT */
	{nmg_to_iges},	/* ID_HF */
	{nmg_to_iges},	/* ID_DSP */
	{null_to_iges},	/* ID_SKETCH */
	{nmg_to_iges},	/* ID_EXTRUDE */
	{null_to_iges},	/* ID_SUBMODEL */
	{nmg_to_iges},	/* ID_CLINE */
	{nmg_to_iges}	/* ID_BOT */
};

static int	regions_tried = 0;
static int	regions_done = 0;
int		mode=CSG_MODE;	/* indicates which type of IGES file is desired */
int		solid_is_brep;
int		comb_form;
char		**independent;
int		no_of_indeps=0;
int		do_nurbs=0;

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int			i, ret;
	register int		c;
	double			percent;
	char			copy_buffer[CP_BUF_SIZE];
	struct directory	*dp;

	bu_setlinebuf( stderr );

	bu_log( "%s", version+5);
	bu_log( "Please direct bug reports to <bugs@brlcad.org>\n\n" );

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
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	the_model = nmg_mm();
	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	rt_init_resource( &rt_uniresource, 0, NULL );

	prog_name = argv[0];

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "ftsmd:a:n:o:p:r:vx:P:X:")) != EOF) {
		switch (c) {
		case 'f':		/* Select facetized output */
			mode = FACET_MODE;
			multi_file = 0;
			break;
		case 't':
			mode = TRIMMED_SURF_MODE;
			multi_file = 0;
			break;
		case 's':		/* Select NURB output */
			do_nurbs = 1;
			break;
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			break;
		case 'o':		/* Output file name. */
			output_file = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
			break;
		case 'd':		/* distance tolerance */
			tol.dist = atof( optarg );
			tol.dist_sq = tol.dist * tol.dist;
			break;
		case 'm':		/* multi-file mode */
			multi_file = 1;
			mode = TRIMMED_SURF_MODE;
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
		case 'X':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		default:
			bu_log( usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		bu_log( usage, argv[0]);
		exit(1);
	}

	/* Open BRL-CAD database */
	argc -= optind;
	argv += optind;
	db_name = argv[0];
	if ((dbip = db_open(db_name, "r")) == DBI_NULL) {
		perror("g-iges");
		exit(1);
	}

	/* Scan the database */
	db_dirbuild( dbip );

	if( !multi_file )
	{
		/* let the IGES routines know the selected tolerances and the database pointer */
		iges_init( &tol , &ttol , verbose , dbip );

		/* Open the output file */
		if( output_file == NULL )
			fp_dir = stdout;
		else {
			if( (fp_dir=fopen( output_file , "w" )) == NULL ) {
				bu_log( "Cannot open output file: %s\n" , output_file );
				perror( output_file );
				exit( 1 );
			}
		}

		/* Open the temporary file for the parameter section */
		if( (fp_param=tmpfile()) == NULL ) {
			bu_log( "Cannot open temporary file\n" );
			perror( "g-iges" );
			exit( 1 );
		}

		/* Write start and global sections of the IGES file */
		w_start_global( fp_dir , fp_param , argv[0] , prog_name , output_file , RCSid , RCSrev );
	}
	else
	{
		struct stat stat_ptr;

		if( stat( output_file, &stat_ptr ) )
		{
			bu_log( "Cannot determine status of %s\n", output_file );
			perror( prog_name );
			exit( 1 );
		}

		if( !(stat_ptr.st_mode & S_IFDIR) )
		{
			bu_log( "-o option must provide a directory, %s is not a directory\n", output_file );
			exit( 1 );
		}
	}

	/* Count object references */
/*	for( i=1 ; i<argc ; i++ )
	{
		dp = db_lookup( dbip , argv[i] , 1 );
		db_functree( dbip , dp , count_refs , 0 , NULL );
	}	*/

	/* tree tops must have independent status, so we need to remember them */
	independent = (argv+1);
	no_of_indeps = argc-1;

	if( mode == FACET_MODE )
	{
		/* Walk indicated tree(s).  Each region will be output
		 * as a single manifold solid BREP object */

		ret = db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			ncpu,
			&tree_state,
			0,			/* take all regions */
			do_nmg_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */

		if( ret )
			rt_bomb( "g-iges: Could not facetize anything!!!" );

		if( !multi_file )
		{
			/* Now walk the same trees again, but only output groups */
			for( i=1 ; i<argc ; i++ )
			{
				char *ptr;

				ptr = strrchr( argv[i], '/' );
				if( ptr != NULL ) {
					ptr++;
				} else {
					ptr = argv[i];
				}
				dp = db_lookup( dbip , ptr , 1 );
				db_functree( dbip , dp , csg_comb_func , 0 , &rt_uniresource , NULL );
			}
		}
	}
	else if( mode == CSG_MODE )
	{
		/* Walk indicated tree(s). Each combination and solid will be output
		 * as a CSG object, unless there is no IGES equivalent (then the
		 * solid will be tessellated and output as a BREP object) */

		for( i=1 ; i<argc ; i++ )
		{
			dp = db_lookup( dbip , argv[i] , 1 );
			db_functree( dbip , dp , csg_comb_func , csg_leaf_func , &rt_uniresource , NULL );
		}
	}
	else if( mode == TRIMMED_SURF_MODE )
	{
		/* Walk the indicated tree(s). Each region is output as a collection
		 * of trimmed NURBS */

		ret = db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			ncpu,
			&tree_state,
			0,			/* take all regions */
			do_nmg_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */

		if( ret )
			rt_bomb( "g-iges: Could not facetize anything!!!" );

	}

	if( !multi_file )
	{
		/* Copy the parameter section from the temporary file to the output file */
		if( (fseek( fp_param , (long) 0 , 0 )) ) {
			bu_log( "Cannot seek to start of temporary file\n" );
			perror( "g-iges" );
			exit( 1 );
		}

		while( (i=fread( copy_buffer , 1 , CP_BUF_SIZE , fp_param )) )
			if( fwrite( copy_buffer , 1 , i , fp_dir ) != i ) {
				bu_log( "Error in copying parameter data to %s\n" , output_file );
				perror( "g-iges" );
				exit( 1 );
			}

		/* Write the terminate section */
		w_terminate( fp_dir );
	}

	/* Print some statistics */
	Print_stats( stdout );

	/* report on the success rate for facetizing regions */
	if( mode == FACET_MODE || mode == TRIMMED_SURF_MODE )
	{
		percent = 0;
		if(regions_tried>0)  percent = ((double)regions_done * 100) / regions_tried;
		bu_log("Tried %d regions, %d converted to nmg's successfully.  %g%%\n",
			regions_tried, regions_done, percent);
	}

	/* re-iterate warnings */
	if( scale_error || solid_error || comb_error )
		bu_log( "WARNING: the IGES file produced has errors:\n" );
	if( scale_error )
		bu_log( "\t%d scaled objects found, written to IGES file without being scaled\n" , scale_error );
	if( solid_error )
		bu_log( "\t%d solids were not converted to IGES format\n" , solid_error );
	if( comb_error )
		bu_log( "\t%d combinations were not converted to IGES format\n" , comb_error );

	return( 0 );
}

/*
*			D O _ N M G _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *
do_nmg_region_end(tsp, pathp, curtree, client_data)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
genptr_t		client_data;
{
	union tree		*result;
	struct nmgregion	*r;
	struct bu_list		vhead;
	struct directory	*dp;
	int 			dependent;
	int			i;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BU_LIST_INIT(&vhead);

	if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("\ndo_nmg_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_done * 100) / regions_tried : 0,
			sofar);
		bu_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( BU_SETJUMP )  {
		char *sofar;

		/* Error, bail out */
		BU_UNSETJUMP;		/* Relinquish the protection */

		sofar = db_path_to_string(pathp);
		bu_log( "FAILED: Cannot convert %s!!!\n", sofar );
		bu_free( sofar, "path string" );

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release the tree memory & input regions */
		db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

		/* Get rid of (m)any other intermediate structures */
		if( (*tsp->ts_m)->magic != -1L )
			nmg_km(*tsp->ts_m);

		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	}
	if( verbose )
		bu_log( "\ndoing boolean tree evaluate...\n" );
	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	result = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);	/* librt/nmg_bool.c */

	if( result )
		r = result->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	if( verbose )
		bu_log( "\nfinished boolean tree evaluate...\n" );
	BU_UNSETJUMP;		/* Relinquish the protection */
	regions_done++;
	if (r != 0) {

		dp = DB_FULL_PATH_CUR_DIR(pathp);

		if( multi_file )
		{
			/* Open the output file */
			if( output_file == NULL )
				fp_dir = stdout;
			else {
				char *multi_name;
				int len;
				int unique=0;
				struct stat stat_ptr;
				char suffix[SUFFIX_LEN+1];

				/* construct a unique file name */
				len = strlen( output_file ) + strlen( dp->d_namep ) + 6 + SUFFIX_LEN;
				multi_name = bu_malloc( sizeof( char )*len, "multi_name" );
				strcpy( multi_name, output_file );
				strcat( multi_name, "/" );
				strcat( multi_name, dp->d_namep );
				strcat( multi_name, ".igs" );
				strcpy( suffix, "a" );
				suffix[0]--;
				while( !unique )
				{
					int i;

					if( stat( multi_name, &stat_ptr ) )
					{
						unique = 1;
						break;
					}

					/* not unique, try adding a suffix */
					len = strlen( suffix );
					i = len - 1;;
					suffix[i]++;
					while( suffix[i] > 'z' && i > 0 )
					{
						suffix[i] = 'a';
						i--;
						suffix[i]++;
					}

					if( suffix[0] > 'z' && len < SUFFIX_LEN )
					{
						for( i=0 ; i<=len ; i++ )
							suffix[i] = 'a';
					}
					else if( suffix[0] > 'z' && len >= SUFFIX_LEN )
					{
						bu_log( "Cannot create a unique filename,\n" );
						bu_log( "too many files with the same name (%s)\n", dp->d_namep );
						exit( 1 );
					}
					strcpy( multi_name, output_file );
					strcat( multi_name, "/" );
					strcat( multi_name, dp->d_namep );
					strcat( multi_name, suffix );
					strcat( multi_name, ".igs" );
				}
				if( (fp_dir=fopen( multi_name , "w" )) == NULL ) {
					bu_log( "Cannot open output file: %s\n" , multi_name );
					perror( "g-iges" );
					exit( 1 );
				}
			}

			/* Open the temporary file for the parameter section */
			if( (fp_param=tmpfile()) == NULL ) {
				bu_log( "Cannot open temporary file\n" );
				perror( "g-iges" );
				exit( 1 );
			}

			/* let the IGES routines know the selected tolerances and the database pointer */
			iges_init( &tol , &ttol , verbose , dbip );

			/* Write start and global sections of the IGES file */
			w_start_global( fp_dir , fp_param , db_name , prog_name , output_file , RCSid , RCSrev );
		}

		if( mode == FACET_MODE )
		{
			dependent = 1;
			for( i=0 ; i<no_of_indeps ; i++ )
			{
				if( !strncmp( dp->d_namep , independent[i] , NAMESIZE ) )
				{
					dependent = 0;
					break;
				}
			}

			dp->d_uses = (-nmgregion_to_iges( dp->d_namep , r , dependent , fp_dir , fp_param ));
		}
		else if( mode == TRIMMED_SURF_MODE )
			dp->d_uses = (-nmgregion_to_tsurf( dp->d_namep , r , fp_dir , fp_param ));

		/* NMG region is no longer necessary */
		nmg_kr(r);

		if( multi_file )
		{
			char copy_buffer[CP_BUF_SIZE];

			/* Copy the parameter section from the temporary file to the output file */
			if( (fseek( fp_param , (long) 0 , 0 )) ) {
				bu_log( "Cannot seek to start of temporary file\n" );
				perror( "g-iges" );
				exit( 1 );
			}

			while( (i=fread( copy_buffer , 1 , CP_BUF_SIZE , fp_param )) )
				if( fwrite( copy_buffer , 1 , i , fp_dir ) != i ) {
					bu_log( "Error in copying parameter data to %s\n" , output_file );
					perror( "g-iges" );
					exit( 1 );
				}

			/* Write the terminate section */
			w_terminate( fp_dir );
			fclose( fp_dir );
			fclose( fp_param );
		}
	}

	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  so we need to cons up an OP_NOP node to return.
	 */
	db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

out:
	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}

static int de_pointer_number;

int
get_de_pointers( tp, dp, de_len, de_pointers )
union tree *tp;
struct directory *dp;
int de_len;
int *de_pointers;
{
	RT_CK_TREE( tp );
	RT_CK_DIR( dp );

	switch( tp->tr_op )
	{
		case OP_UNION:
		case OP_SUBTRACT:
		case OP_INTERSECT:
			get_de_pointers( tp->tr_b.tb_left, dp, de_len, de_pointers );
			get_de_pointers( tp->tr_b.tb_right, dp, de_len, de_pointers );
			break;
		case OP_DB_LEAF:
		{
			struct directory *dp_M;

			dp_M = db_lookup( dbip , tp->tr_l.tl_name , LOOKUP_NOISY );
			if( dp_M == DIR_NULL )
				return( 1 );

			if( dp_M->d_uses >= 0 )
			{
				bu_log( "g-iges: member (%s) in combination (%s) has not been written to iges file\n" , dp_M->d_namep , dp->d_namep );
				de_pointers[de_pointer_number++] = 0;
				return( 1 );
			}

			if( tp->tr_l.tl_mat && !bn_mat_is_identity( tp->tr_l.tl_mat ) )
			{
				/* write a solid instance entity for this member
					with a pointer to the new matrix */

				if( !NEAR_ZERO( tp->tr_l.tl_mat[15] - 1.0 , tol.dist ) )
				{
					/* scale factor is not 1.0, IGES can't handle it.
					   go ahead and write the solid instance anyway,
					   but warn the user twice */
					bu_log( "g-iges: WARNING!! member (%s) of combination (%s) is scaled, IGES cannot handle this\n" , dp_M->d_namep , dp->d_namep );
					scale_error++;
				}
				de_pointers[de_pointer_number++] = write_solid_instance( -dp_M->d_uses , tp->tr_l.tl_mat , fp_dir , fp_param );
			}
			else
				de_pointers[de_pointer_number++] = (-dp_M->d_uses);
			if( dp_M->d_nref )
				comb_form = 1;
			}
			break;
		default:
			bu_log( "Unrecognized operator in combination!!\n" );
			return( 1 );
	}
	return( 0 );
}

void
csg_comb_func( dbip , dp , ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t	ptr;
{
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	struct iges_properties props;
	int comb_len;
	int i;
	int dependent=1;
	int *de_pointers;
	int id;

	/* when this is called in facet mode, we only want groups */
	if( mode == FACET_MODE && (dp->d_flags & DIR_REGION ) )
		return;

	/* check if already written */
	if( dp->d_uses < 0 )
		return;

	for( i=0 ; i<no_of_indeps ; i++ )
	{
		if( !strncmp( dp->d_namep , independent[i] , NAMESIZE ) )
		{
			dependent = 0;
			break;
		}
	}

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL , &rt_uniresource);
	if( id < 0 )
		return;
	if( id != ID_COMBINATION )
	{
		bu_log( "Directory/Database mismatch!!!! is %s a combination or not????\n", dp->d_namep );
		return;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( verbose )
		bu_log( "Combination - %s\n" , dp->d_namep );

	if( !comb->tree )
	{
		bu_log( "Warning: empty combination (%s)\n" , dp->d_namep );
		dp->d_uses = 0;
		return;
	}
	comb_len = db_tree_nleaves( comb->tree );
	de_pointers = (int *)bu_calloc( comb_len , sizeof( int ) , "csg_comb_func" );

	comb_form = 0;

	de_pointer_number = 0;
	if( get_de_pointers( comb->tree, dp, comb_len, de_pointers ) )
	{
		bu_log( "Error in combination %s\n", dp->d_namep );
	        bu_free( (char *)de_pointers , "csg_comb_func de_pointers" );
		rt_db_free_internal( &intern , &rt_uniresource);
		return;
	}

	strcpy( props.name, dp->d_namep );
	props.material_name[0] = '\0';
	props.material_params[0] = '\0';
	props.region_flag = ' ';
	props.ident = 0;
	props.air_code = 0;
	props.material_code = 0;
	props.los_density = 0;
	props.color[0] = 0;
	props.color[1] = 0;
	props.color[2] = 0;
	get_props( &props , comb );

	dp->d_uses = (-comb_to_iges( comb , comb_len , dependent , &props , de_pointers , fp_dir , fp_param ) );

	if( !dp->d_uses )
	{
		comb_error++;
		bu_log( "g-iges: combination (%s) not written to iges file\n" , dp->d_namep );
	}

	rt_db_free_internal( &intern , &rt_uniresource);
        bu_free( (char *)de_pointers , "csg_comb_func de_pointers" );

}

void
csg_leaf_func( dbip , dp , ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t ptr;
{
	struct rt_db_internal	ip;

	/* if this solid has already been output, don't do it again */
	if( dp->d_uses < 0 )
		return;

	if( verbose )
		bu_log( "solid - %s\n" , dp->d_namep );

	if( rt_db_get_internal( &ip, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
		bu_log( "Error in import" );

	solid_is_brep = 0;
	dp->d_uses = (-iges_write[ip.idb_type].do_iges_write( &ip , dp->d_namep , fp_dir , fp_param ));

	if( !dp->d_uses )
	{
		bu_log( "g-iges: failed to translate %s to IGES format\n" , dp->d_namep );
		solid_error++;
	}

	if( solid_is_brep )
		dp->d_nref = 1;
	else
		dp->d_nref = 0;

	rt_db_free_internal( &ip, &rt_uniresource );
}

void
incr_refs( dbip, comb, tp, user_ptr1, user_ptr2, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal	*comb;
union tree		*tp;
genptr_t		user_ptr1,user_ptr2,user_ptr3;
{
	struct directory *dp;

	RT_CK_COMB( comb );
	RT_CK_DBI( dbip );
	RT_CK_TREE( tp );

	if( (dp=db_lookup( dbip, tp->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
		return;

	dp->d_nref++;
}

void
count_refs( dbip , dp , ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t ptr;
{
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	if( !(dp->d_flags & DIR_COMB) )
		return;

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL , &rt_uniresource);
	if( id < 0 )
	{
		bu_log( "Cannot get internal form of %s\n", dp->d_namep );
		return;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( !comb->tree )
	{
		bu_log( "Warning: empty combination (%s)\n" , dp->d_namep );
		dp->d_uses = 0;
		return;
	}

	comb_form = 0;

	db_tree_funcleaf( dbip, comb, comb->tree, incr_refs,
		(genptr_t)NULL, (genptr_t)NULL, (genptr_t)NULL );

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
