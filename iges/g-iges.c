/*
 *			G - I G E S . C
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  Some stupid secret codes:
 *	dp->d_uses - contains the negative of the DE number for the object
 *		     non-negative means it hasn't been written to the IGES file
 *	dp->d_nref - contains a one if the object is written to the IGES file as a BREP,
 *		     zero otherwise.
 */

#ifndef lint
static char RCSid[] = "$Header$";
static char RCSrev[] = "$Revision$";
#endif

#include "conf.h"

extern char	version[];

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "db.h"
#include "externs.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "../iges/iges.h"
#include "../librt/debug.h"
#include <sys/types.h>
#include <sys/stat.h>

#define	CP_BUF_SIZE	1024	/* size of buffer for file copy */
#define SUFFIX_LEN	10	/* max size of suffix for 'part' files (-m option) */

RT_EXTERN( union tree *do_nmg_region_end , (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( void w_start_global , (FILE *fp_dir , FILE *fp_param , char *db_name , char *prog_name , char *output_file , char *id , char *version ));
RT_EXTERN( void w_terminate , (FILE *fp) );
RT_EXTERN( void write_edge_list , (struct nmgregion *r , int vert_de , struct nmg_ptbl *etab , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( void write_vertex_list , ( struct nmgregion *r , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( void nmg_region_edge_list , ( struct nmg_ptbl *tab , struct nmgregion *r ) );
RT_EXTERN( int nmgregion_to_iges , ( char *name , struct nmgregion *r , int dependent , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( int write_shell_face_loop , ( struct nmgregion *r , int edge_de , struct nmg_ptbl *etab , int vert_de , struct nmg_ptbl *vtab , FILE *fp_dir , FILE *fp_param ) );
RT_EXTERN( void csg_comb_func , ( struct db_i *dbip , struct directory *dp ) );
RT_EXTERN( void csg_leaf_func , ( struct db_i *dbip , struct directory *dp ) );
RT_EXTERN( void set_iges_tolerances , ( struct rt_tol *set_tol , struct rt_tess_tol *set_ttol ) );
RT_EXTERN( void count_refs , ( struct db_i *dbip , struct directory *dp ) );

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
static struct rt_tol		tol;
static struct model		*the_model;
static mat_t			identity_mat;
struct db_i		*dbip;

static struct db_tree_state	tree_state;	/* includes tol & model */

/* function table for converting solids to iges */
RT_EXTERN( int null_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int arb_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int ell_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int sph_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int tor_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int tgc_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
RT_EXTERN( int nmg_to_iges , ( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));

struct iges_functab
{
	int (*do_iges_write) RT_ARGS(( struct rt_db_internal *ip , char *name , FILE *fp_dir , FILE *fp_param ));
};

struct iges_functab iges_write[ID_MAXIMUM+1]={
	null_to_iges,	/* ID_NULL */
	tor_to_iges, 	/* ID_TOR */
	tgc_to_iges,	/* ID_TGC */
	ell_to_iges,	/* ID_ELL */
	arb_to_iges,	/* ID_ARB8 */
	nmg_to_iges,	/* ID_ARS */
	nmg_to_iges,	/* ID_HALF */
	nmg_to_iges,	/* ID_REC */
	nmg_to_iges,	/* ID_POLY */
	nmg_to_iges,	/* ID_BSPLINE */
	sph_to_iges,	/* ID_SPH */
	nmg_to_iges,	/* ID_NMG */
	null_to_iges,	/* ID_EBM */
	null_to_iges,	/* ID_VOL */
	nmg_to_iges,	/* ID_ARBN */
	nmg_to_iges,	/* ID_PIPE */
	null_to_iges,	/* ID_PARTICLE */
	null_to_iges,	/* ID_RPC */
	null_to_iges,	/* ID_EPA */
	null_to_iges,	/* ID_EHY */
	null_to_iges,	/* ID_ETO */
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

	port_setlinebuf( stderr );

	rt_log( "%s", version+5);
	rt_log( "Please direct bug reports to <jra@brl.mil>\n\n" );

	mat_idn( identity_mat );

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
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	the_model = nmg_mm();
	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

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
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		default:
			rt_log( usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		rt_log( usage, argv[0]);
		exit(1);
	}

	/* Open brl-cad database */
	argc -= optind;
	argv += optind;
	db_name = argv[0];
	if ((dbip = db_open(db_name, "r")) == DBI_NULL) {
		perror("g-iges");
		exit(1);
	}

	/* Scan the database */
	db_scan(dbip, (int (*)())db_diradd, 1);

	if( !multi_file )
	{
		/* let the IGES routines know the selected tolerances and the database pointer */
		iges_init( &tol , &ttol , verbose , dbip );

		/* Open the output file */
		if( output_file == NULL )
			fp_dir = stdout;
		else {
			if( (fp_dir=fopen( output_file , "w" )) == NULL ) {
				rt_log( "Cannot open output file: %s\n" , output_file );
				perror( output_file );
				exit( 1 );
			}
		}

		/* Open the temporary file for the parameter section */
		if( (fp_param=tmpfile()) == NULL ) {
			rt_log( "Cannot open temporary file\n" );
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
			rt_log( "Cannot determine status of %s\n", output_file );
			perror( prog_name );
			exit( 1 );
		}

		if( !(stat_ptr.st_mode & S_IFDIR) )
		{
			rt_log( "-o option must provide a directory, %s is not a directory\n", output_file );
			exit( 1 );
		}
	}

	/* Count object references */
/*	for( i=1 ; i<argc ; i++ )
	{
		dp = db_lookup( dbip , argv[i] , 1 );
		db_functree( dbip , dp , count_refs , 0 );
	}	*/

	/* tree tops must have independent status, so we need to remember them */
	independent = (argv+1);
	no_of_indeps = argc-1;

	if( mode == FACET_MODE )
	{
		/* Walk indicated tree(s).  Each region will be output
		 * as a single manifold solid BREP object */

		ret = db_walk_tree(dbip, argc-1, (CONST char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_nmg_region_end,
			nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */

		if( ret )
			rt_bomb( "g-iges: Could not facetize anything!!!" );

		if( !multi_file )
		{
			/* Now walk the same trees again, but only output groups */
			for( i=1 ; i<argc ; i++ )
			{
				dp = db_lookup( dbip , argv[i] , 1 );
				db_functree( dbip , dp , csg_comb_func , 0 );
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
			db_functree( dbip , dp , csg_comb_func , csg_leaf_func );
		}
	}
	else if( mode == TRIMMED_SURF_MODE )
	{
		/* Walk the indicated tree(s). Each region is output as a collection
		 * of trimmed NURBS */

		ret = db_walk_tree(dbip, argc-1, (CONST char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_nmg_region_end,
			nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */

		if( ret )
			rt_bomb( "g-iges: Could not facetize anything!!!" );

	}

	if( !multi_file )
	{
		/* Copy the parameter section from the temporary file to the output file */
		if( (fseek( fp_param , (long) 0 , 0 )) ) {
			rt_log( "Cannot seek to start of temporary file\n" );
			perror( "g-iges" );
			exit( 1 );
		}

		while( (i=fread( copy_buffer , 1 , CP_BUF_SIZE , fp_param )) )
			if( fwrite( copy_buffer , 1 , i , fp_dir ) != i ) {
				rt_log( "Error in copying parameter data to %s\n" , output_file );
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
		rt_log("Tried %d regions, %d converted to nmg's successfully.  %g%%\n",
			regions_tried, regions_done, percent);
	}

	/* re-iterate warnings */
	if( scale_error || solid_error || comb_error )
		rt_log( "WARNING: the IGES file produced has errors:\n" );
	if( scale_error )
		rt_log( "\t%d scaled objects found, written to IGES file without being scaled\n" , scale_error );
	if( solid_error )
		rt_log( "\t%d solids were not converted to IGES format\n" , solid_error );
	if( comb_error )
		rt_log( "\t%d combinations were not converted to IGES format\n" , comb_error );
}

/*
*			D O _ N M G _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *
do_nmg_region_end(tsp, pathp, curtree)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	union tree		*result;
	struct nmgregion	*r;
	struct rt_list		vhead;
	struct directory	*dp;
	int 			dependent;
	int			i;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	RT_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		rt_log("\ndo_nmg_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_done * 100) / regions_tried : 0,
			sofar);
		rt_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( RT_SETJUMP )  {
		/* Error, bail out */
		RT_UNSETJUMP;		/* Relinquish the protection */

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release the tree memory & input regions */
		db_free_tree(curtree);		/* Does an nmg_kr() */

		/* Get rid of (m)any other intermediate structures */
		if( (*tsp->ts_m)->magic != -1L )
			nmg_km(*tsp->ts_m);
	
		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	}
	if( verbose )
		rt_log( "\ndoing boolean tree evaluate...\n" );
	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	result = nmg_booltree_evaluate(curtree, tsp->ts_tol);	/* librt/nmg_bool.c */

	if( result )
		r = result->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	if( verbose )
		rt_log( "\nfinished boolean tree evaluate...\n" );
	RT_UNSETJUMP;		/* Relinquish the protection */
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
				multi_name = rt_malloc( sizeof( char )*len, "multi_name" );
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
						rt_log( "Cannot create a unique filename,\n" );
						rt_log( "too many files with the same name (%s)\n", dp->d_namep );
						exit( 1 );
					}
					strcpy( multi_name, output_file );
					strcat( multi_name, "/" );
					strcat( multi_name, dp->d_namep );
					strcat( multi_name, suffix );
					strcat( multi_name, ".igs" );
				}
				if( (fp_dir=fopen( multi_name , "w" )) == NULL ) {
					rt_log( "Cannot open output file: %s\n" , multi_name );
					perror( "g-iges" );
					exit( 1 );
				}
			}

			/* Open the temporary file for the parameter section */
			if( (fp_param=tmpfile()) == NULL ) {
				rt_log( "Cannot open temporary file\n" );
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
				rt_log( "Cannot seek to start of temporary file\n" );
				perror( "g-iges" );
				exit( 1 );
			}

			while( (i=fread( copy_buffer , 1 , CP_BUF_SIZE , fp_param )) )
				if( fwrite( copy_buffer , 1 , i , fp_dir ) != i ) {
					rt_log( "Error in copying parameter data to %s\n" , output_file );
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
	db_free_tree(curtree);		/* Does an nmg_kr() */

out:
	GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}

void
csg_comb_func( dbip , dp )
struct db_i *dbip;
struct directory *dp;
{
	union record *rp;
	struct directory *dp_M;
	struct iges_properties props;
	int comb_len;
	int i;
	int dependent=1;
	int *de_pointers;

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

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
                return;

	if( verbose )
		rt_log( "Combination - %s\n" , dp->d_namep );

	comb_len = dp->d_len - 1;
	if( comb_len == 0 )
	{
		rt_log( "Warning: empty combination (%s)\n" , dp->d_namep );
		dp->d_uses = 0;
		return;
	}
	de_pointers = (int *)rt_calloc( comb_len , sizeof( int ) , "csg_comb_func" );

	comb_form = 0;

	for( i=1 ; i<dp->d_len ; i++ )
	{
		dp_M = db_lookup( dbip , rp[i].M.m_instname , 1 );
		if( dp_M == DIR_NULL )
			continue;

		if( dp_M->d_uses >= 0 )
		{
			rt_log( "g-iges: member (%s) in combination (%s) has not been written to iges file\n" , dp_M->d_namep , dp->d_namep );
			de_pointers[i-1] = 0;
			continue;
		}

		if( !matrix_is_identity( rp[i].M.m_mat ) )
		{
			/* write a solid instance entity for this member
				with a pointer to the new matrix */

			if( !NEAR_ZERO( rp[i].M.m_mat[15] - 1.0 , tol.dist ) )
			{
				/* scale factor is not 1.0, IGES can't handle it.
				   go ahead and write the solid instance anyway,
				   but warn the user twice */
				rt_log( "g-iges: WARNING!! member (%s) of combination (%s) is scaled, IGES cannot handle this\n" , rp[i].M.m_instname , rp[0].c.c_name );
				scale_error++;
			}
			de_pointers[i-1] = write_solid_instance( -dp_M->d_uses , rp[i].M.m_mat , fp_dir , fp_param );
		}
		else
			de_pointers[i-1] = (-dp_M->d_uses);
		if( dp_M->d_nref )
			comb_form = 1;
	}

	get_props( &props , rp );

	dp->d_uses = (-comb_to_iges( rp , comb_len , dependent , &props , de_pointers , fp_dir , fp_param ) );

	if( !dp->d_uses )
	{
		comb_error++;
		rt_log( "g-iges: combination (%s) not written to iges file\n" , dp->d_namep );
	}

        rt_free( (char *)rp , "csg_comb_func record[]" );
        rt_free( (char *)de_pointers , "csg_comb_func de_pointers" );

}

void
csg_leaf_func( dbip , dp )
struct db_i *dbip;
struct directory *dp;
{
	struct rt_external	ep;
	struct rt_db_internal	ip;
	int			id;

	/* if this solid has already been output, don't do it again */
	if( dp->d_uses < 0 )
		return;

	if( verbose )
		rt_log( "solid - %s\n" , dp->d_namep );
	if( db_get_external( &ep , dp , dbip ) )
		rt_log( "Error return from db_get_external for %s\n" , dp->d_namep );

	id = rt_id_solid( &ep );

	if( rt_functab[id].ft_import( &ip , &ep , identity_mat ) )
		rt_log( "Error in import" );

	solid_is_brep = 0;
	dp->d_uses = (-iges_write[id].do_iges_write( &ip , dp->d_namep , fp_dir , fp_param ));

	if( !dp->d_uses )
	{
		rt_log( "g-iges: failed to translate %s to IGES format\n" , dp->d_namep );
		solid_error++;
	}

	if( solid_is_brep )
		dp->d_nref = 1;
	else
		dp->d_nref = 0;
}

void
count_refs( dbip , dp )
struct db_i *dbip;
struct directory *dp;
{
	union record *rp;
	struct directory *dp_M;
	int comb_len;
	int i;

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
                return;

	comb_len = dp->d_len - 1;
	if( comb_len == 0 )
	{
		rt_log( "Warning: empty combination (%s)\n" , dp->d_namep );
		dp->d_uses = 0;
		return;
	}

	comb_form = 0;

	for( i=1 ; i<dp->d_len ; i++ )
	{
		dp_M = db_lookup( dbip , rp[i].M.m_instname , 1 );
		if( dp_M == DIR_NULL )
			continue;

		dp_M->d_nref++;
	}
}
