/*
 *			G - V R M L . C
 *
 *  Program to convert a BRL-CAD model (in a .g file) to a VRML  facetted model
 *  by calling on the NMG booleans.
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

#define TXT_BUF_LEN	512
#define TXT_NAME_LEN	128

struct vrml_mat {
	/* typical shader parameters */
	char shader[TXT_NAME_LEN];
	int shininess;
	double transparency;

	/* light paramaters */
	fastf_t lt_fraction;
	vect_t  lt_dir;
	fastf_t lt_angle;

	/* texture parameters */
	char	tx_file[TXT_NAME_LEN];
	int	tx_w;
	int	tx_n;
};

#define PL_O(_m)	offsetof(struct vrml_mat, _m)
#define PL_OA(_m)	offsetofarray(struct vrml_mat, _m)

struct bu_structparse vrml_mat_parse[]={
	{"%s", TXT_NAME_LEN, "ma_shader", PL_OA(shader), 	FUNC_NULL },
	{"%d", 1, "shine",		PL_O(shininess),	FUNC_NULL },
	{"%d", 1, "sh",			PL_O(shininess),	FUNC_NULL },
	{"%f", 1, "transmit",		PL_O(transparency),	FUNC_NULL },
	{"%f", 1, "tr",			PL_O(transparency),	FUNC_NULL },
	{"%f",	1, "angle",		PL_O(lt_angle),		FUNC_NULL },
	{"%f",	1, "fract",		PL_O(lt_fraction),	FUNC_NULL },
	{"%f",	3, "aim",		PL_OA(lt_dir),		FUNC_NULL },
	{"%d",  1, "w",         	PL_O(tx_w),             FUNC_NULL },
	{"%d",  1, "n",         	PL_O(tx_n),             FUNC_NULL },
	{"%s",  TXT_NAME_LEN, "file",	PL_OA(tx_file), 	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

RT_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( struct face *nmg_find_top_face , (struct shell *s , long *flags ));

static char	usage[] = "Usage: %s [-v] [-i] [-xX lvl] [-d tolerance_distance (mm) ] [-a abs_tol (mm)] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

static char	*tok_sep = " \t";
static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;		/* Number of processors */
static int	nmg_count=0;		/* Count of nmgregions written to output */
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct rt_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;

/* macro to determine if one bounding box is within another */
#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )

static int
select_lights( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path		*pathp;
union tree			*curtree;
{
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	RT_CK_FULL_PATH( pathp );
	dp = DB_FULL_PATH_CUR_DIR( pathp );

	if( !(dp->d_flags & DIR_COMB) )
		return( -1 );

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL );
	if( id < 0 )
	{
		rt_log( "Cannot internal form of %s\n", dp->d_namep );
		return( -1 );
	}

	if( id != ID_COMBINATION )
	{
		bu_log( "Directory/database mismatch!!\n\t is '%s' a combination or not???\n",
			dp->d_namep );
		return( -1 );
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( !strcmp( bu_vls_addr( &comb->shader ), "light" ) )
	{
		rt_db_free_internal( &intern );
		return( 0 );
	}
	else
	{
		rt_db_free_internal( &intern );
		return( -1 );
	}
}

static int
select_non_lights( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path		*pathp;
union tree			*curtree;
{
	return( !select_lights( tsp, pathp, curtree ) );
}

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		i;
	register int	c;
	double		percent;

	port_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	the_model = nmg_mm();
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

	/* XXX For visualization purposes, in the debug plot files */
	{
		extern fastf_t	nmg_eue_dist;	/* librt/nmg_plot.c */
		/* XXX This value is specific to the Bradley */
		nmg_eue_dist = 2.0;
	}

	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "d:a:n:o:r:vx:P:X:")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'd':		/* calculational tolerance */
			tol.dist = atof( optarg );
			tol.dist_sq = tol.dist * tol.dist;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'o':		/* Output file name */
			out_file = optarg;
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
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
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

	/* Open brl-cad database */
	if ((dbip = db_open( argv[optind] , "r")) == DBI_NULL)
	{
		rt_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		exit(1);
	}
	db_scan(dbip, (int (*)())db_diradd, 1);

	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			rt_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			return 2;
		}
	}

	fprintf( fp_out, "#VRML V1.0 ascii\n" );
	fprintf( fp_out, "ShapeHints {\n" );
	fprintf( fp_out, "\tvertexOrdering	COUNTERCLOCKWISE\n" );
	fprintf( fp_out, "\tshapeType		SOLID\n" );
	fprintf( fp_out, "\tfaceType		UNKNOWN_FACE_TYPE\n\t}\n" );

	optind++;

	for( i=optind ; i<argc ; i++ )
	{
		struct directory *dp;

		dp = db_lookup( dbip, argv[i], LOOKUP_QUIET );
		if( dp == DIR_NULL )
		{
			rt_log( "Cannot find %s\n", argv[i] );
			continue;
		}

		/* light source must be a combibation */
		if( !(dp->d_flags & DIR_COMB) )
			continue;

		/* walk trees selecting only light source regions */
		(void)db_walk_tree(dbip, 1, (CONST char **)(&argv[i]),
			1,				/* ncpu */
			&tree_state,
			select_lights,
			do_region_end,
			nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */


	}
	/* Walk indicated tree(s).  Each non-light-source region will be output separately */
	(void)db_walk_tree(dbip, argc-optind, (CONST char **)(&argv[optind]),
		1,				/* ncpu */
		&tree_state,
		select_non_lights,
		do_region_end,
		nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */

	/* Release dynamic storage */
	nmg_km(the_model);

	rt_vlist_cleanup();
	db_close(dbip);

#if MEMORY_LEAK_CHECKING
	rt_prmem("After complete G-NMG conversion");
#endif

	return 0;
}

void
nmg_2_vrml( fp, pathp, m, mater )
FILE *fp;
struct db_full_path *pathp;
struct model *m;
struct mater_info *mater;
{
	struct nmgregion *reg;
	struct nmg_ptbl verts;
	struct vrml_mat mat;
	struct bu_vls vls;
	char *tok;
	int i;
	int first=1;
	int is_light=0;
	float r,g,b;
	point_t ave_pt;
	fastf_t pt_count=0.0;
	char *full_path;

	NMG_CK_MODEL( m );

	full_path = db_path_to_string( pathp );

	if( mater->ma_override )
	{
		r = mater->ma_color[0];
		g = mater->ma_color[1];
		b = mater->ma_color[2];
	}
	else
	{
		r = g = b = 0.5;
	}

	if( mater->ma_shader )
	{
		tok = strtok( mater->ma_shader, tok_sep );
		strcpy( mat.shader, tok );
	}
	else
		mat.shader[0] = '\0';
	mat.shininess = -1;
	mat.transparency = -1.0;
	mat.lt_fraction = -1.0;
	VSETALL( mat.lt_dir, 0.0 );
	mat.lt_angle = -1.0;
	mat.tx_file[0] = '\0';
	mat.tx_w = -1;
	mat.tx_n = -1;
	bu_vls_init( &vls );
	bu_vls_strcpy( &vls, &mater->ma_shader[strlen(mat.shader)] );
	(void)bu_struct_parse( &vls, vrml_mat_parse, (char *)&mat );

	if( strncmp( "light", mat.shader, 5 ) == 0 )
	{
		/* this is a light source */
		is_light = 1;
	}
	else if( strncmp( "plastic", mat.shader, 7 ) == 0 )
	{
		if( mat.shininess < 0 )
			mat.shininess = 10;
		if( mat.transparency < 0.0 )
			mat.transparency = 0.0;

		fprintf( fp, "Separator { # start of %s\n", full_path );
		fprintf( fp, "\tMaterial {\n" );
		fprintf( fp, "\t\tdiffuseColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tambientColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0 ) );
		if( mat.transparency > 0.0 )
			fprintf( fp, "\t\ttransparency %g\n", mat.transparency );
		fprintf( fp, "\t\tspecularColor %g %g %g }\n", 1.0, 1.0, 1.0 );
	}
	else if( strncmp( "glass", mat.shader, 5 ) == 0 )
	{
		if( mat.shininess < 0 )
			mat.shininess = 4;
		if( mat.transparency < 0.0 )
			mat.transparency = 0.8;

		fprintf( fp, "Separator { # start of %s\n", full_path );
		fprintf( fp, "\tMaterial {\n" );
		fprintf( fp, "\t\tdiffuseColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tambientColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0 ) );
		if( mat.transparency > 0.0 )
			fprintf( fp, "\t\ttransparency %g\n", mat.transparency );
		fprintf( fp, "\t\tspecularColor %g %g %g }\n", 1.0, 1.0, 1.0 );
	}
	else if( strncmp( "texture", mat.shader, 7 ) == 0 )
	{
		if( mat.tx_w < 0 )
			mat.tx_w = 512;
		if( mat.tx_n < 0 )
			mat.tx_n = 512;

		fprintf( fp, "Separator { # start of %s\n", full_path );
		if( strlen( mat.tx_file ) )
		{
			int tex_fd;
			int nbytes;
			long tex_len;
			long bytes_read=0;
			int buf_start=0;
			unsigned char tex_buf[TXT_BUF_LEN*3];

			if( (tex_fd = open( mat.tx_file, O_RDONLY )) == (-1) )
			{
				rt_log( "Cannot open texture file (%s)\n", mat.tx_file );
				perror( "g-vrml: " );
			}
			else
			{
				fprintf( fp, "\tTexture2Transform {\n" );
				fprintf( fp, "\t\tscaleFactor 1.33333 1.33333\n" );
				fprintf( fp, "\t\t}\n" );
				fprintf( fp, "\tTexture2 {\n" );
				fprintf( fp, "\t\twrapS REPEAT\n" );
				fprintf( fp, "\t\twrapT REPEAT\n" );
				fprintf( fp, "\t\timage %d %d %d\n", mat.tx_w, mat.tx_n, 3 );
				tex_len = mat.tx_w*mat.tx_n*3;
				while( bytes_read < tex_len )
				{
					long bytes_to_go=tex_len;

					bytes_to_go = tex_len - bytes_read;
					if( bytes_to_go > TXT_BUF_LEN*3 )
						bytes_to_go = TXT_BUF_LEN*3;
					nbytes = 0;
					while( nbytes < bytes_to_go )
						nbytes += read( tex_fd, &tex_buf[nbytes],
							bytes_to_go-nbytes );

					bytes_read += nbytes;
					for( i=0 ; i<nbytes ; i += 3 )
						fprintf( fp, "\t\t\t0x%02x%02x%02x\n",
							tex_buf[i],
							tex_buf[i+1],
							tex_buf[i+2] );
				}
				fprintf( fp, "\t}\n" );
			}
		}
	}
	else
	{
		fprintf( fp, "Separator { # start of %s\n", full_path );
		fprintf( fp, "\tMaterial {\n" );
		fprintf( fp, "\t\tdiffuseColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tambientColor %g %g %g \n", r, g, b );
		fprintf( fp, "\t\tspecularColor %g %g %g }\n", 1.0, 1.0, 1.0 );
	}

	if( !is_light )
	{
		/* triangulate any faceuses with holes */
		for( RT_LIST_FOR( reg, nmgregion, &m->r_hd ) )
		{
			struct shell *s;

			NMG_CK_REGION( reg );
			s = RT_LIST_FIRST( shell, &reg->s_hd );
			while( RT_LIST_NOT_HEAD( s, &reg->s_hd ) )
			{
				struct shell *next_s;
				struct faceuse *fu;

				NMG_CK_SHELL( s );
				next_s = RT_LIST_PNEXT( shell, &s->l );
				fu = RT_LIST_FIRST( faceuse, &s->fu_hd );
				while( RT_LIST_NOT_HEAD( &fu->l, &s->fu_hd ) )
				{
					struct faceuse *next_fu;
					struct loopuse *lu;
					int shell_is_dead=0;
					int face_is_dead=0;

					NMG_CK_FACEUSE( fu );

					next_fu = RT_LIST_PNEXT( faceuse, &fu->l );

					if( fu->orientation != OT_SAME )
					{
						fu = next_fu;
						continue;
					}

					/* check if this faceuse has any holes */
					for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
					{
						NMG_CK_LOOPUSE( lu );
						if( lu->orientation == OT_OPPOSITE )
						{
							/* this is a hole, so
							 * triangulate the faceuse
							 */
							if( RT_SETJUMP )
							{
								RT_UNSETJUMP;
								rt_log( "A face has failed triangulation!!!!\n" );
								if( next_fu == fu->fumate_p )
									next_fu = RT_LIST_PNEXT( faceuse, &next_fu->l );
								if( nmg_kfu( fu ) )
								{
									(void) nmg_ks( s );
									shell_is_dead = 1;
								}
								face_is_dead = 1;
							}
							if( !face_is_dead )
								nmg_triangulate_fu( fu, &tol );
							RT_UNSETJUMP;
							break;
						}

					}
					if( shell_is_dead )
						break;
					fu = next_fu;
				}
				s = next_s;
			}
		}
	}

	/* get list of vertices */
	nmg_vertex_tabulate( &verts, &m->magic );
	if( !is_light )
		fprintf( fp, "\tCoordinate3 {\n\t\tpoint [" );
	else
	{
		VSETALL( ave_pt, 0.0 );
	}

	for( i=0 ; i<NMG_TBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;
		point_t pt_meters;

		v = (struct vertex *)NMG_TBL_GET( &verts, i );
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );

		/* convert to meters */
		VSCALE( pt_meters, vg->coord, 0.001 );

		if( is_light )
			VADD2( ave_pt, ave_pt, pt_meters )
		if( first )
		{
			if( !is_light )
				fprintf( fp, " %10.10e %10.10e %10.10e, # point %d\n", V3ARGS( pt_meters ), i );
			first = 0;
		}
		else
			if( !is_light )
				fprintf( fp, "\t\t\t%10.10e %10.10e %10.10e, # point %d\n", V3ARGS( pt_meters ), i );
	}
	if( !is_light )
		fprintf( fp, "\t\t\t]\n\t\t}\n" );
	else
	{
		fastf_t one_over_count;

		one_over_count = 1.0/(fastf_t)NMG_TBL_END( &verts );
		VSCALE( ave_pt, ave_pt, one_over_count );
	}

	first = 1;
	if( !is_light )
	{
		fprintf( fp, "\tIndexedFaceSet {\n\t\tcoordIndex [\n" );
		for( RT_LIST_FOR( reg, nmgregion, &m->r_hd ) )
		{
			struct shell *s;

			NMG_CK_REGION( reg );
			for( RT_LIST_FOR( s, shell, &reg->s_hd ) )
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

						NMG_CK_LOOPUSE( lu );

						if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;

						if( !first )
							fprintf( fp, ",\n" );
						else
							first = 0;

						fprintf( fp, "\t\t\t" );
						for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
						{
							struct vertex *v;

							NMG_CK_EDGEUSE( eu );

							v = eu->vu_p->v_p;
							NMG_CK_VERTEX( v );
							fprintf( fp, " %d,", nmg_tbl( &verts, TBL_LOC, (long *)v ) );
						}
						fprintf( fp, "-1" );
					}
				}
			}
		}
		fprintf( fp, " ]\n\t}\n}\n" );
	}
	else
	{
		mat.lt_fraction = 0.0;
		mat.lt_angle = 180.0;
		VSETALL( mat.lt_dir, 0.0 );

		if( mat.lt_dir[X] != 0.0 || mat.lt_dir[Y] != 0.0 ||mat.lt_dir[Z] != 0.0 )
		{
			fprintf( fp, "SpotLight {\n" );
			fprintf( fp, "\ton \tTRUE\n" );
			if( mat.lt_fraction > 0.0 )
				fprintf( fp, "\tintensity \t%g\n", mat.lt_fraction );
			fprintf( fp, "\tcolor \t%g %g %g\n", r,g,b );
			fprintf( fp, "\tlocation \t%g %g %g\n", V3ARGS( ave_pt ) );
			fprintf( fp, "\tdirection \t%g %g %g\n", V3ARGS( mat.lt_dir ) );
			fprintf( fp, "\tcutOffAngle \t%g }\n", mat.lt_angle );
		}
		else
			fprintf( fp, "PointLight {\n\ton TRUE\n\tintensity 1\n\tcolor %g %g %g\n\tlocation %g %g %g\n}\n",r,g,b,V3ARGS( ave_pt ) );
	}
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
	struct nmgregion	*r;
	struct rt_list		vhead;
	union tree		*ret_tree;

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

	bu_log( "Attempting %s\n", db_path_to_string( pathp ) );

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( RT_SETJUMP )
	{
		/* Error, bail out */
		RT_UNSETJUMP;		/* Relinquish the protection */

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release any intersector 2d tables */
		nmg_isect2d_final_cleanup();

		/* Release the tree memory & input regions */
		db_free_tree(curtree);		/* Does an nmg_kr() */

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
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol);	/* librt/nmg_bool.c */

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	RT_UNSETJUMP;		/* Relinquish the protection */
	regions_converted++;
	if (r != 0)
	{
		char nmg_name[16];
		unsigned char rgb[3];
		struct wmember headp;
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
			/* Write the nmgregion to the output file */
			nmg_2_vrml( fp_out, pathp, r->m_p, &tsp->ts_mater );
		}

		/* NMG region is no longer necessary */
		if( !empty_model )
			nmg_kr(r);

	}
	else
		bu_log( "WARNING: Nothing left after Boolean evaluation of %s\n",
			db_path_to_string( pathp ) );

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
