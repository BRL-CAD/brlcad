/*                         G - X 3 D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file g-x3d.c
 *
 *  Program to convert a BRL-CAD model (in a .g file) to a X3D facetted model
 *  by calling on the NMG booleans. This program is a modified version of
 *  g-vrml (authored by John R. Anderson).
 *
 *  Author -
 *      Bob Parker
 *
 *  Author of g-vrml -
 *	John R. Anderson
 *
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
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
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"

/* local headers */
#include "../librt/debug.h"


/* #define MEMORY_LEAK_CHECKING 1 */

#ifdef MEMORY_LEAK_CHECKING
#define BARRIER_CHECK { \
	if( bu_mem_barriercheck() ) { \
		bu_log( "memory is corrupted at line %d in file %d\n", __LINE__, __FILE__ ); \
	} \
}
#else
#define BARRIER_CHECK /* */
#endif

#define TXT_BUF_LEN	512
#define TXT_NAME_LEN	128

struct plate_mode {
	int num_bots;
	int num_nonbots;
	int array_size;
	struct rt_bot_internal **bots;
};

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

#define PL_O(_m)	bu_offsetof(struct vrml_mat, _m)
#define PL_OA(_m)	bu_offsetofarray(struct vrml_mat, _m)

struct bu_structparse vrml_mat_parse[]={
	{"%s", TXT_NAME_LEN, "ma_shader", PL_OA(shader), 	FUNC_NULL },
	{"%d", 1, "shine",		PL_O(shininess),	FUNC_NULL },
	{"%d", 1, "sh",			PL_O(shininess),	FUNC_NULL },
	{"%f", 1, "transmit",		PL_O(transparency),	FUNC_NULL },
	{"%f", 1, "tr",			PL_O(transparency),	FUNC_NULL },
	{"%f",	1, "angle",		PL_O(lt_angle),		FUNC_NULL },
	{"%f",	1, "fract",		PL_O(lt_fraction),	FUNC_NULL },
	{"%f",	3, "aim",		PL_OA(lt_dir),		FUNC_NULL },
	{"%d",  1, "w",         	PL_O(tx_w),             BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "n",         	PL_O(tx_n),             BU_STRUCTPARSE_FUNC_NULL },
	{"%s",  TXT_NAME_LEN, "file",	PL_OA(tx_file), 	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

BU_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));
BU_EXTERN(union tree *nmg_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));

static char	usage[] = "Usage: %s [-v] [-xX lvl] [-d tolerance_distance (mm) ] [-a abs_tol (mm)] [-r rel_tol] [-n norm_tol] [-o out_file] [-u units] brlcad_db.g object(s)\n";

static char	*tok_sep = " \t";
static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose=0;
/* static int	ncpu = 1; */		/* Number of processors */
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static	char*	units=NULL;
static fastf_t	scale_factor=1.0;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;


/*
 * Replace all occurences of "old" with "new" in str.
 */
static void
char_replace(char *str,
	     char old,
	     char new)
{
  if (str == (char *)0)
    return;

  while (*str != '\0') {
    if (*str == old)
      *str = new;

    ++str;
  }
}

static void
clean_pmp( struct plate_mode *pmp )
{
	int i;

	BARRIER_CHECK;

	pmp->num_bots = 0;
	pmp->num_nonbots = 0;
	for( i=0 ; i<pmp->array_size ; i++ ) {
		if( pmp->bots[i] ) {
			struct rt_db_internal intern;

			intern.idb_ptr = (genptr_t) pmp->bots[i];
			intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
			intern.idb_type = ID_BOT;
			intern.idb_meth = &rt_functab[ID_BOT];
			intern.idb_magic = RT_DB_INTERNAL_MAGIC;
			intern.idb_meth->ft_ifree( &intern, &rt_uniresource );
			pmp->bots[i] = NULL;
		}
	}
	BARRIER_CHECK;
}

struct rt_bot_internal *
dup_bot( struct rt_bot_internal *bot_in )
{
	struct rt_bot_internal *bot;
	int i;

	RT_BOT_CK_MAGIC( bot_in );

	bot = (struct rt_bot_internal *)bu_malloc( sizeof( struct rt_bot_internal ), "dup bot" );

	*bot = *bot_in;	/* struct copy */

	bot->faces = (int *)bu_calloc( bot_in->num_faces*3, sizeof( int ), "bot faces" );
	for( i=0 ; i<bot_in->num_faces*3 ; i++ )
		bot->faces[i] = bot_in->faces[i];

	bot->vertices = (fastf_t *)bu_calloc( bot_in->num_vertices*3, sizeof( fastf_t ), "bot verts" );
	for( i=0 ; i<bot_in->num_vertices*3 ; i++ )
		bot->vertices[i] = bot_in->vertices[i];

	if( bot_in->thickness ) {
		bot->thickness = (fastf_t *)bu_calloc( bot_in->num_faces, sizeof( fastf_t ), "bot thickness" );
		for( i=0 ; i<bot_in->num_faces ; i++ )
			bot->thickness[i] = bot_in->thickness[i];
	}

	if( bot_in->face_mode ) {
		bot->face_mode = bu_bitv_dup( bot_in->face_mode );
	}

	return( bot );
}

static int
select_lights(register struct db_tree_state *tsp, struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	RT_CK_FULL_PATH( pathp );
	dp = DB_FULL_PATH_CUR_DIR( pathp );

	if( !(dp->d_flags & DIR_COMB) )
		return( -1 );

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL, &rt_uniresource );
	if( id < 0 )
	{
		bu_log( "Cannot internal form of %s\n", dp->d_namep );
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
		rt_db_free_internal( &intern, &rt_uniresource );
		return( 0 );
	}
	else
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		return( -1 );
	}
}

static int
select_non_lights(register struct db_tree_state *tsp, struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
	int ret;

	ret =  select_lights( tsp, pathp, combp, client_data );
	if( ret == 0 )
		return( -1 );
	else
		return( 0 );
}

union tree *
leaf_tess(struct db_tree_state *tsp, struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
	struct rt_bot_internal *bot;
	struct plate_mode *pmp = (struct plate_mode *)client_data;

	BARRIER_CHECK;

	if( ip->idb_type != ID_BOT ) {
		pmp->num_nonbots++;
		return( nmg_booltree_leaf_tess(tsp, pathp, ip, client_data) );
	}

	bot = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC( bot );

	if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_SURFACE )
	{
		if( pmp->array_size <= pmp->num_bots ) {
			pmp->array_size += 5;
			pmp->bots = (struct rt_bot_internal **)bu_realloc(
				    (char *)pmp->bots,
				    pmp->array_size * sizeof( struct rt_bot_internal *),
				    "pmp->bots" );
		}

		/* walk tree will free the BOT, so we need a copy */
		pmp->bots[pmp->num_bots] = dup_bot( bot );
		BARRIER_CHECK;
		pmp->num_bots++;
		return( (union tree *)NULL );
	}

	pmp->num_nonbots++;

	BARRIER_CHECK;

	return( nmg_booltree_leaf_tess(tsp, pathp, ip, client_data) );
}

void
writeX3dHeader(FILE *fp_out,
	       char *x3dFileName) {
    fprintf(fp_out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp_out, "<!DOCTYPE X3D PUBLIC \"http://www.web3D.org/TaskGroups/x3d/translation/x3d-compact.dtd\"\n\"/www.web3D.org/TaskGroups/x3d/translation/x3d-compact.dtd\">\n");
    fprintf(fp_out, "<X3D>\n");
    fprintf(fp_out, "  <head>\n");
    fprintf(fp_out, "    <meta name=\"filename\" content=\"%s\"/>\n", x3dFileName);
    fprintf(fp_out, "    <meta name=\"description\" content=\"*enter description here, short-sentence summaries preferred*\"/>\n");
    fprintf(fp_out, "    <meta name=\"author\" content=\"*enter name of original author here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"translator\" content=\"*if manually translating VRML-to-X3D, enter name of person translating here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"created\" content=\"*enter date of initial version here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"revised\" content=\"*enter date of latest revision here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"version\" content=\"*enter version here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"reference\" content=\"*enter reference citation or relative/online url here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"reference\" content=\"*enter additional url/bibliographic reference information here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"copyright\" content=\"*enter copyright information here* Example:  Copyright (c) Web3D Consortium Inc. 2001\"/>\n");
    fprintf(fp_out, "    <meta name=\"drawing\" content=\"*enter drawing filename/url here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"image\" content=\"*enter image filename/url here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"movie\" content=\"*enter movie filename/url here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"photo\" content=\"*enter photo filename/url here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"keywords\" content=\"*enter keywords here*\"/>\n");
    fprintf(fp_out, "    <meta name=\"url\" content=\"*enter online url address for this file here*\"/>\n");
    fprintf(fp_out, "  </head>\n");
    fprintf(fp_out, "  <Scene>\n");

    /* Note we may want to inquire about bounding boxes for the various groups and add Viewpoints nodes that
     * point the camera to the center and orient for Top, Side, etc Views
     *
     * We will add some default Material Color definitions (for thousands groups) before we start defining the geometry
     */
    fprintf( fp_out, "\t<Material DEF=\"Material_999\" diffuseColor=\"0.78 0.78 0.78\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_1999\" diffuseColor=\"0.88 0.29 0.29\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_2999\" diffuseColor=\"0.82 0.53 0.54\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_3999\" diffuseColor=\"0.39 0.89 0.00\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_4999\" diffuseColor=\"1.00 0.00 0.00\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_5999\" diffuseColor=\"0.82 0.00 0.82\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_6999\" diffuseColor=\"0.62 0.62 0.62\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_7999\" diffuseColor=\"0.49 0.49 0.49\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_8999\" diffuseColor=\"0.18 0.31 0.31\"/>\n");
    fprintf( fp_out, "\t<Material DEF=\"Material_9999\" diffuseColor=\"0.00 0.41 0.82\"/>\n");
}

void
writeX3dEnd(FILE *fp_out) {
    fprintf(fp_out, "  </Scene>\n\n");
    fprintf(fp_out, "</X3D>\n");
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	int		i;
	register int	c;
	struct plate_mode pm;

	bu_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	bu_debug |= BU_DEBUG_MEM_CHECK;
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
	tol.magic = BN_TOL_MAGIC;
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

	rt_init_resource( &rt_uniresource, 0, NULL );

	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	BARRIER_CHECK;
	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "d:a:n:o:r:vx:P:X:u:")) != EOF) {
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
			ttol.norm = atof(optarg)*bn_pi/180.0;
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
/*			ncpu = atoi( optarg ); */
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		case 'u':
			units = bu_strdup( optarg );
			scale_factor = bu_units_conversion( units );
			if( scale_factor == 0.0 )
			{
				bu_log( "Unrecognized units (%s)\n", units );
				bu_bomb( "Unrecognized units\n" );
			}
			scale_factor = 1.0 / scale_factor;
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

	if( !units )
		units = "mm";

	/* Open BRL-CAD database */
	if ((dbip = db_open( argv[optind] , "r")) == DBI_NULL)
	{
		bu_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		exit(1);
	}
	if( db_dirbuild( dbip ) ) {
		bu_bomb( "db_dirbuild() failed!\n" );
	}

	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			bu_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			return 2;
		}
	}

	writeX3dHeader(fp_out, out_file);
	optind++;

	BARRIER_CHECK;

	pm.num_bots = 0;
	pm.num_nonbots = 0;
	pm.array_size = 5;
	pm.bots = (struct rt_bot_internal **)bu_calloc( pm.array_size,
		sizeof( struct rt_bot_internal *), "pm.bots" );
	for( i=optind ; i<argc ; i++ )
	{
		struct directory *dp;

		dp = db_lookup( dbip, argv[i], LOOKUP_QUIET );
		if( dp == DIR_NULL )
		{
			bu_log( "Cannot find %s\n", argv[i] );
			continue;
		}

#if 0
		fprintf ( fp_out, "#Includes group %s\n", argv[i]);
#endif

		/* light source must be a combibation */
		if( !(dp->d_flags & DIR_COMB) )
			continue;

		/* walk trees selecting only light source regions */
		(void)db_walk_tree(dbip, 1, (const char **)(&argv[i]),
			1,				/* ncpu */
			&tree_state,
			select_lights,
			do_region_end,
			leaf_tess,
			(genptr_t)&pm);	/* in librt/nmg_bool.c */


	}
	BARRIER_CHECK;

	/* Walk indicated tree(s).  Each non-light-source region will be output separately */
	(void)db_walk_tree(dbip, argc-optind, (const char **)(&argv[optind]),
		1,				/* ncpu */
		&tree_state,
		select_non_lights,
		do_region_end,
		leaf_tess,
		(genptr_t)&pm);	/* in librt/nmg_bool.c */

	BARRIER_CHECK;
	/* Release dynamic storage */
	nmg_km(the_model);

	db_close(dbip);

	/* Now we need to close each group set */
	writeX3dEnd(fp_out);

	if( verbose )
		bu_log( "Total of %d regions converted of %d regions attempted\n",
			regions_converted, regions_tried );

	return 0;
}

void
nmg_2_vrml(FILE *fp, struct db_full_path *pathp, struct model *m, struct mater_info *mater)
{
	struct nmgregion *reg;
	struct bu_ptbl verts;
	struct vrml_mat mat;
	struct bu_vls vls;
	char *tok;
	int i;
	int first=1;
	int is_light=0;
	float r,g,b;
	point_t ave_pt;
	char *full_path;
	/*There may be a better way to capture the region_id, than getting the rt_comb_internal structure,
	 * (and may be a better way to capture the rt_comb_internal struct), but for now I just copied the
	 * method used in select_lights/select_non_lights above, could have used a global variable but I noticed
	 * none other were used, so I didn't want to be the first
	 */
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	int id;

	NMG_CK_MODEL( m );

	BARRIER_CHECK;

	full_path = db_path_to_string( pathp );

	/* replace all occurences of '.' with '_' */
	char_replace(full_path, '.', '_');

	RT_CK_FULL_PATH( pathp );
	dp = DB_FULL_PATH_CUR_DIR( pathp );

	if( !(dp->d_flags & DIR_COMB) )
		return;

	id = rt_db_get_internal( &intern, dp, dbip, (matp_t)NULL, &rt_uniresource );
	if( id < 0 )
	{
		bu_log( "Cannot internal form of %s\n", dp->d_namep );
		return;
	}

	if( id != ID_COMBINATION )
	{
		bu_log( "Directory/database mismatch!!\n\t is '%s' a combination or not???\n",
			dp->d_namep );
		return;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );

	if( mater->ma_color_valid )
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
	else
	{
#if 1
		fprintf( fp, "\t<Shape DEF=\"%s\">\n", full_path);
		fprintf( fp, "\t\t<Appearance>\n");
#else
		fprintf( fp, "\t\tShape { \n");
		fprintf( fp, "\t\t\t# Component_ID: %d   %s\n",comb->region_id,full_path);
		fprintf( fp, "\t\t\tappearance Appearance { \n");
#endif


		if( strncmp( "plastic", mat.shader, 7 ) == 0 )
		{
			if( mat.shininess < 0 )
				mat.shininess = 10;
			if( mat.transparency < 0.0 )
				mat.transparency = 0.0;

#if 1
			fprintf( fp, "\t\t\t<Material diffuseColor=\"%g %g %g\" shininess=\"%g\" transparency=\"%g\" specularColor=\"%g %g %g\"/>\n", r, g, b, 1.0-exp(-(double)mat.shininess/20.0), mat.transparency, 1.0, 1.0, 1.0);
#else
			fprintf( fp, "\t\t\t\tmaterial Material {\n" );
			fprintf( fp, "\t\t\t\t\tdiffuseColor %g %g %g \n", r, g, b );
			fprintf( fp, "\t\t\t\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0 ) );
			if( mat.transparency > 0.0 )
				fprintf( fp, "\t\t\t\t\ttransparency %g\n", mat.transparency );
			fprintf( fp, "\t\t\t\t\tspecularColor %g %g %g \n\t\t\t\t}\n", 1.0, 1.0, 1.0 );
#endif
		}
		else if( strncmp( "glass", mat.shader, 5 ) == 0 )
		{
			if( mat.shininess < 0 )
				mat.shininess = 4;
			if( mat.transparency < 0.0 )
				mat.transparency = 0.8;

#if 1
			fprintf( fp, "\t\t\t<Material diffuseColor=\"%g %g %g\" shininess=\"%g\" transparency=\"%g\" specularColor=\"%g %g %g\"/>\n", r, g, b, 1.0-exp(-(double)mat.shininess/20.0), mat.transparency, 1.0, 1.0, 1.0);
#else
			fprintf( fp, "\t\t\t\tmaterial Material {\n" );
			fprintf( fp, "\t\t\t\t\tdiffuseColor %g %g %g \n", r, g, b );
			fprintf( fp, "\t\t\t\t\tshininess %g\n", 1.0-exp(-(double)mat.shininess/20.0 ) );
			if( mat.transparency > 0.0 )
				fprintf( fp, "\t\t\t\t\ttransparency %g\n", mat.transparency );
			fprintf( fp, "\t\t\t\t\tspecularColor %g %g %g \n\t\t\t\t}\n", 1.0, 1.0, 1.0 );
#endif
		}
#if 0
/*XXX please fix */
		else if( strncmp( "texture", mat.shader, 7 ) == 0 )
		{
			if( mat.tx_w < 0 )
				mat.tx_w = 512;
			if( mat.tx_n < 0 )
				mat.tx_n = 512;

			if( strlen( mat.tx_file ) )
			{
				int tex_fd;
				int nbytes;
				long tex_len;
				long bytes_read=0;
				unsigned char tex_buf[TXT_BUF_LEN*3];

				if( (tex_fd = open( mat.tx_file, O_RDONLY )) == (-1) )
				{
					bu_log( "Cannot open texture file (%s)\n", mat.tx_file );
					perror( "g-x3d: " );
				}
				else
				{
					/* Johns note - need to check (test) the texture stuff */
					fprintf( fp, "\t\t\t\ttextureTransform TextureTransform {\n");
					fprintf( fp, "\t\t\t\t\tscale 1.33333 1.33333\n\t\t\t\t}\n");
					fprintf( fp, "\t\t\t\ttexture PixelTexture {\n");
					fprintf( fp, "\t\t\t\t\trepeatS TRUE\n");
					fprintf( fp, "\t\t\t\t\trepeatT TRUE\n");
					fprintf( fp, "\t\t\t\t\timage %d %d %d\n", mat.tx_w, mat.tx_n, 3 );
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
					fprintf( fp, "\t\t\t\t}\n" );
				}
			}
		}
#endif
		else if( mater->ma_color_valid )
		{
#if 1
			fprintf( fp, "\t\t\t<Material diffuseColor=\"%g %g %g\"/>\n", r, g, b);
#else
			/* no shader specified, but a color is assigned */
			fprintf( fp, "\t\t\t\tmaterial Material {\n" );
			fprintf( fp, "\t\t\t\t\tdiffuseColor %g %g %g }\n", r, g, b );
#endif
		}
		else
		{
			/* If no color was defined set the colors according to the thousands groups */
			int thou = comb->region_id/1000;
#if 1
			thou == 0 ? fprintf( fp, "\t\t\t<Material USE=\"Material_999\"/>\n")
			: thou == 1 ? fprintf( fp, "\t\t\t<Material USE=\"Material_1999\"/>\n")
			: thou == 2 ? fprintf( fp, "\t\t\t<Material USE=\"Material_2999\"/>\n")
			: thou == 3 ? fprintf( fp, "\t\t\t<Material USE=\"Material_3999\"/>\n")
			: thou == 4 ? fprintf( fp, "\t\t\t<Material USE=\"Material_4999\"/>\n")
			: thou == 5 ? fprintf( fp, "\t\t\t<Material USE=\"Material_5999\"/>\n")
			: thou == 6 ? fprintf( fp, "\t\t\t<Material USE=\"Material_6999\"/>\n")
			: thou == 7 ? fprintf( fp, "\t\t\t<Material USE=\"Material_7999\"/>\n")
			: thou == 8 ? fprintf( fp, "\t\t\t<Material USE=\"Material_8999\"/>\n")
			: fprintf( fp, "\t\t\t<Material USE=\"Material_9999\"/>\n");
#else
			thou == 0 ? fprintf( fp, "\t\t\tmaterial USE Material_999\n")
			: thou == 1 ? fprintf( fp, "\t\t\tmaterial USE Material_1999\n")
			: thou == 2 ? fprintf( fp, "\t\t\tmaterial USE Material_2999\n")
			: thou == 3 ? fprintf( fp, "\t\t\tmaterial USE Material_3999\n")
			: thou == 4 ? fprintf( fp, "\t\t\tmaterial USE Material_4999\n")
			: thou == 5 ? fprintf( fp, "\t\t\tmaterial USE Material_5999\n")
			: thou == 6 ? fprintf( fp, "\t\t\tmaterial USE Material_6999\n")
			: thou == 7 ? fprintf( fp, "\t\t\tmaterial USE Material_7999\n")
			: thou == 8 ? fprintf( fp, "\t\t\tmaterial USE Material_8999\n")
			: fprintf( fp, "\t\t\tmaterial USE Material_9999\n");
#endif

/*			fprintf( fp, "\t\t\t\tmaterial Material {\n" );
 *			fprintf( fp, "\t\t\t\t\tdiffuseColor %g %g %g \n", r, g, b );
 *			fprintf( fp, "\t\t\t\t\tspecularColor %g %g %g \n\t\t\t\t}\n", 1.0, 1.0, 1.0 );
*/
		}
	}

	if( !is_light )
	{
		/* triangulate any faceuses with holes */
		for( BU_LIST_FOR( reg, nmgregion, &m->r_hd ) )
		{
			struct shell *s;

			NMG_CK_REGION( reg );
			s = BU_LIST_FIRST( shell, &reg->s_hd );
			while( BU_LIST_NOT_HEAD( s, &reg->s_hd ) )
			{
				struct shell *next_s;
				struct faceuse *fu;

				NMG_CK_SHELL( s );
				next_s = BU_LIST_PNEXT( shell, &s->l );
				fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
				while( BU_LIST_NOT_HEAD( &fu->l, &s->fu_hd ) )
				{
					struct faceuse *next_fu;
					struct loopuse *lu;
					int shell_is_dead=0;
					int face_is_dead=0;

					NMG_CK_FACEUSE( fu );

					next_fu = BU_LIST_PNEXT( faceuse, &fu->l );

					if( fu->orientation != OT_SAME )
					{
						fu = next_fu;
						continue;
					}

					/* check if this faceuse has any holes */
					for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
					{
						NMG_CK_LOOPUSE( lu );
						if( lu->orientation == OT_OPPOSITE )
						{
							/* this is a hole, so
							 * triangulate the faceuse
							 */
							if( BU_SETJUMP )
							{
								BU_UNSETJUMP;
								bu_log( "A face has failed triangulation!!!!\n" );
								if( next_fu == fu->fumate_p )
									next_fu = BU_LIST_PNEXT( faceuse, &next_fu->l );
								if( nmg_kfu( fu ) )
								{
									(void) nmg_ks( s );
									shell_is_dead = 1;
								}
								face_is_dead = 1;
							}
							if( !face_is_dead )
								nmg_triangulate_fu( fu, &tol );
							BU_UNSETJUMP;
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
#if 1
		fprintf( fp, "\t\t</Appearance>\n");
#else
		fprintf( fp, "\t\t\t} \n");
		fprintf( fp, "\t\t\tgeometry IndexedFaceSet { \n");
		fprintf( fp, "\t\t\t\tcoord Coordinate { \n");
#endif

	}

#if 1
        /* XXX need code to handle light */

	/* get list of vertices */
	nmg_vertex_tabulate( &verts, &m->magic );

	fprintf( fp, "\t\t<IndexedFaceSet coordIndex=\"\n");
	first = 1;
	if( !is_light )
	{
		for( BU_LIST_FOR( reg, nmgregion, &m->r_hd ) )
		{
			struct shell *s;

			NMG_CK_REGION( reg );
			for( BU_LIST_FOR( s, shell, &reg->s_hd ) )
			{
				struct faceuse *fu;

				NMG_CK_SHELL( s );
				for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
				{
					struct loopuse *lu;

					NMG_CK_FACEUSE( fu );

					if( fu->orientation != OT_SAME )
						continue;

					for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
					{
						struct edgeuse *eu;

						NMG_CK_LOOPUSE( lu );

						if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;

						if( !first )
							fprintf( fp, ",\n" );
						else
							first = 0;

						fprintf( fp, "\t\t\t\t" );
						for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
						{
							struct vertex *v;

							NMG_CK_EDGEUSE( eu );

							v = eu->vu_p->v_p;
							NMG_CK_VERTEX( v );
							fprintf( fp, " %d,", bu_ptbl_locate( &verts, (long *)v ) );
						}
						fprintf( fp, "-1" );
					}
				}
			}
		}
                /* close coordIndex */
		fprintf( fp, "\" ");
		fprintf( fp, "normalPerVertex=\"false\" ");
		fprintf( fp, "convex=\"false\" ");
		fprintf( fp, "creaseAngle=\"0.5\" ");
                /* close IndexedFaceSet open tag */
		fprintf( fp, ">\n");
	}

	fprintf( fp, "\t\t\t<Coordinate point=\"");

	for( i=0 ; i<BU_PTBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;
		point_t pt_meters;

		v = (struct vertex *)BU_PTBL_GET( &verts, i );
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );

		/* convert to desired units */
		VSCALE( pt_meters, vg->coord, scale_factor );

		if( is_light )
			VADD2( ave_pt, ave_pt, pt_meters )
		if( first )
		{
			if( !is_light )
				fprintf( fp, " %10.10e %10.10e %10.10e, ", V3ARGS(pt_meters));
			first = 0;
		}
		else
			if( !is_light )
				fprintf( fp, "%10.10e %10.10e %10.10e, ", V3ARGS( pt_meters ));
	}

       /* close point */
       fprintf(fp, "\"");
       /* close Coordinate */
       fprintf(fp, "/>\n");
       /* IndexedFaceSet end tag */
       fprintf( fp, "\t\t</IndexedFaceSet>\n");
       /* Shape end tag */
       fprintf( fp, "\t</Shape>\n");
#else
	/* get list of vertices */
	nmg_vertex_tabulate( &verts, &m->magic );
	if( !is_light )
		fprintf( fp, "\t\t\t\t\tpoint [");
	else
	{
		VSETALL( ave_pt, 0.0 );
	}

	for( i=0 ; i<BU_PTBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;
		point_t pt_meters;

		v = (struct vertex *)BU_PTBL_GET( &verts, i );
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );

		/* convert to desired units */
		VSCALE( pt_meters, vg->coord, scale_factor );

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
				fprintf( fp, "\t\t\t\t\t%10.10e %10.10e %10.10e, # point %d\n", V3ARGS( pt_meters ), i );
	}
	if( !is_light )
		fprintf( fp, "\t\t\t\t\t]\n\t\t\t\t}\n" );
	else
	{
		fastf_t one_over_count;

		one_over_count = 1.0/(fastf_t)BU_PTBL_END( &verts );
		VSCALE( ave_pt, ave_pt, one_over_count );
	}

	first = 1;
	if( !is_light )
	{
		fprintf( fp, "\t\t\t\tcoordIndex [\n");
		for( BU_LIST_FOR( reg, nmgregion, &m->r_hd ) )
		{
			struct shell *s;

			NMG_CK_REGION( reg );
			for( BU_LIST_FOR( s, shell, &reg->s_hd ) )
			{
				struct faceuse *fu;

				NMG_CK_SHELL( s );
				for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
				{
					struct loopuse *lu;

					NMG_CK_FACEUSE( fu );

					if( fu->orientation != OT_SAME )
						continue;

					for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
					{
						struct edgeuse *eu;

						NMG_CK_LOOPUSE( lu );

						if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
							continue;

						if( !first )
							fprintf( fp, ",\n" );
						else
							first = 0;

						fprintf( fp, "\t\t\t\t\t" );
						for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
						{
							struct vertex *v;

							NMG_CK_EDGEUSE( eu );

							v = eu->vu_p->v_p;
							NMG_CK_VERTEX( v );
							fprintf( fp, " %d,", bu_ptbl_locate( &verts, (long *)v ) );
						}
						fprintf( fp, "-1" );
					}
				}
			}
		}
		fprintf( fp, "\n\t\t\t\t]\n\t\t\t\tnormalPerVertex FALSE\n");
		fprintf( fp, "\t\t\t\tconvex FALSE\n");
		fprintf( fp, "\t\t\t\tcreaseAngle 0.5\n");
		fprintf( fp, "\t\t\t}\n\t\t}\n");
	}
#endif

#if 0
/*XXX please fix */
	else
	{
		mat.lt_fraction = 0.0;
		mat.lt_angle = 180.0;
		VSETALL( mat.lt_dir, 0.0 );

		if( mat.lt_dir[X] != 0.0 || mat.lt_dir[Y] != 0.0 ||mat.lt_dir[Z] != 0.0 )
		{
			fprintf( fp, "\t\tSpotLight {\n" );
			fprintf( fp, "\t\t\ton \tTRUE\n" );
			if( mat.lt_fraction > 0.0 )
				fprintf( fp, "\t\t\tintensity \t%g\n", mat.lt_fraction );
			fprintf( fp, "\t\t\tcolor \t%g %g %g\n", r,g,b );
			fprintf( fp, "\t\t\tlocation \t%g %g %g\n", V3ARGS( ave_pt ) );
			fprintf( fp, "\t\t\tdirection \t%g %g %g\n", V3ARGS( mat.lt_dir ) );
			fprintf( fp, "\t\t\tcutOffAngle \t%g }\n", mat.lt_angle );
		}
		else
			fprintf( fp, "\t\tPointLight {\n\t\t\ton TRUE\n\t\t\tintensity 1\n\t\t\tcolor %g %g %g\n\t\t\tlocation %g %g %g\n\t\t}\n",r,g,b,V3ARGS( ave_pt ) );
	}
#endif
	BARRIER_CHECK;
}

void
bot2vrml( struct plate_mode *pmp, struct db_full_path *pathp, int region_id )
{
	char *path_str;
	int appearance;
	struct rt_bot_internal *bot;
	int bot_num;
	int i;
	int vert_count=0;

	BARRIER_CHECK;

	path_str = db_path_to_string( pathp );
	/* replace all occurences of '.' with '_' */
	char_replace(path_str, '.', '_');

#if 1
	fprintf( fp_out, "\t<Shape DEF=\"%s\">\n", path_str);
#else
	fprintf( fp_out, "\t\tShape {\n\t\t\t# Component_ID: %d   %s\n",
		 region_id, path_str );
#endif
	bu_free( path_str, "result of db_path_to_string" );

	appearance = region_id / 1000;
	appearance = appearance * 1000 + 999;
#if 1
	fprintf( fp_out, "\t\t<Appearance USE=\"Material_%d\">\n", appearance);
#else
	fprintf( fp_out, "\t\t\tappearance Appearance {\n\t\t\tmaterial USE Material_%d\n\t\t\t}\n", appearance );
#endif

#if 1
	fprintf( fp_out, "\t\t<IndexedFaceSet coordIndex=\"\n");
	vert_count = 0;
	for( bot_num = 0 ; bot_num < pmp->num_bots ; bot_num++ ) {
		bot = pmp->bots[bot_num];
		RT_BOT_CK_MAGIC( bot );
		for( i=0 ; i<bot->num_faces ; i++ )
			fprintf( fp_out, "\t\t\t\t%d, %d, %d, -1,\n",
				 vert_count+bot->faces[i*3],
				 vert_count+bot->faces[i*3+1],
				 vert_count+bot->faces[i*3+2]);
		vert_count += bot->num_vertices;
	}
	/* close coordIndex */
	fprintf( fp_out, "\" ");
	fprintf( fp_out, "normalPerVertex=\"false\" ");
	fprintf( fp_out, "convex=\"true\" ");
	fprintf( fp_out, "creaseAngle=\"0.5\" ");
	fprintf( fp_out, "solid=\"false\" ");
	/* close IndexedFaceSet open tag */
	fprintf( fp_out, ">\n");

	fprintf( fp_out, "\t\t<Coordinate point=\"");
	for( bot_num = 0 ; bot_num < pmp->num_bots ; bot_num++ ) {
		bot = pmp->bots[bot_num];
		RT_BOT_CK_MAGIC( bot );
		for( i=0 ; i<bot->num_vertices ; i++ )
			{
				point_t pt;

				VSCALE( pt, &bot->vertices[i*3], scale_factor );
				fprintf( fp_out, "%10.10e %10.10e %10.10e, ", V3ARGS( pt ));
				vert_count++;
			}
	}
	/* close point */
	fprintf(fp_out, "\"");
	/* close Coordinate */
	fprintf(fp_out, "/>\n");
       /* IndexedFaceSet end tag */
       fprintf( fp_out, "\t\t</IndexedFaceSet>\n");
       /* Shape end tag */
       fprintf( fp_out, "\t</Shape>\n");
#else
	fprintf( fp_out, "\t\t\tgeometry IndexedFaceSet {\n\t\t\t\tcoord Coordinate {\n\t\t\t\tpoint [\n" );
	for( bot_num = 0 ; bot_num < pmp->num_bots ; bot_num++ ) {
		bot = pmp->bots[bot_num];
		RT_BOT_CK_MAGIC( bot );
		for( i=0 ; i<bot->num_vertices ; i++ )
			{
				point_t pt;

				VSCALE( pt, &bot->vertices[i*3], scale_factor );
				fprintf( fp_out, "\t\t\t\t\t%10.10e %10.10e %10.10e, # point %d\n", V3ARGS( pt ), vert_count );
				vert_count++;
			}
	}
	fprintf( fp_out, "\t\t\t\t\t]\n\t\t\t\t}\n\t\t\t\tcoordIndex [\n" );
	vert_count = 0;
	for( bot_num = 0 ; bot_num < pmp->num_bots ; bot_num++ ) {
		bot = pmp->bots[bot_num];
		RT_BOT_CK_MAGIC( bot );
		for( i=0 ; i<bot->num_faces ; i++ )
			fprintf( fp_out, "\t\t\t\t\t%d, %d, %d, -1,\n",
				 vert_count+bot->faces[i*3],
				 vert_count+bot->faces[i*3+1],
				 vert_count+bot->faces[i*3+2]);
		vert_count += bot->num_vertices;
	}
	fprintf( fp_out, "\t\t\t\t]\n\t\t\t\tnormalPerVertex FALSE\n" );
	fprintf( fp_out, "\t\t\t\tconvex TRUE\n" );
	fprintf( fp_out, "\t\t\t\tcreaseAngle 0.5\n" );
	fprintf( fp_out, "\t\t\t\tsolid FALSE\n" );
	fprintf( fp_out, "\t\t\t}\n\t\t}\n" );
#endif
	BARRIER_CHECK;
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
	struct plate_mode *pmp = (struct plate_mode *)client_data;
	char *name;

	BARRIER_CHECK;
	if( tsp->ts_is_fastgen != REGION_FASTGEN_PLATE ) {
		clean_pmp( pmp );
		return( nmg_region_end(tsp, pathp, curtree, client_data) );
	}

	/* FASTGEN plate mode region, just spew the bot triangles */
	if( pmp->num_bots < 1 || pmp->num_nonbots > 0 ) {
		clean_pmp( pmp );
		BARRIER_CHECK;
		return( nmg_region_end(tsp, pathp, curtree, client_data) );
	}

	if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
		bu_log("\nConverted %d%% so far (%d of %d)\n",
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
		       regions_converted, regions_tried );
	}

	regions_tried++;
	name = db_path_to_string( pathp );
	bu_log( "Attempting %s\n", name );
	bu_free( name, "db_path_to_string" );
	bot2vrml( pmp, pathp, tsp->ts_regionid );
	clean_pmp( pmp );
	regions_converted++;
	BARRIER_CHECK;
	return( (union tree *)NULL );
}

union tree *nmg_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	union tree		*ret_tree;
	char			*name;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BARRIER_CHECK;
	BU_LIST_INIT(&vhead);

	if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
		bu_log("\nConverted %d%% so far (%d of %d)\n",
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
		       regions_converted, regions_tried );
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	name = db_path_to_string( pathp );
	bu_log( "Attempting %s\n", name );

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( BU_SETJUMP )
	{
		/* Error, bail out */
		BU_UNSETJUMP;		/* Relinquish the protection */
		bu_log( "conversion of %s FAILED!!!\n", name );

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release any intersector 2d tables */
		nmg_isect2d_final_cleanup();

		/* Release the tree memory & input regions */
		db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

		/* Get rid of (m)any other intermediate structures */
		if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )
		{
			nmg_km(*tsp->ts_m);
		}
		else
		{
			bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
		}

		bu_free( name, "db_path_to_string" );
		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	}
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	BU_UNSETJUMP;		/* Relinquish the protection */
	bu_free( name, "db_path_to_string" );
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
	db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

out:
	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	BARRIER_CHECK;
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
