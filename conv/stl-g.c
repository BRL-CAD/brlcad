/*
 *		S T L - G
 *
 * Code to convert Stereolithography format files to BRL-CAD
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
 *	This software is Copyright (C) 2002 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

extern char *optarg;
extern int optind,opterr,optopt;
extern int errno;

static struct vert_root *tree_root;
static struct wmember all_head;
static char *input_file;	/* name of the input file */
static char *brlcad_file;	/* name of output file */
static struct bu_vls ret_name;	/* unique name built by Build_unique_name() */
static char *forced_name=NULL;	/* name specified on command line */
static int solid_count=0;	/* count of solids converted */
static struct bn_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static int const_id=-1;		/* Constant ident number (assigned to all regions if non-negative) */
static int mat_code=1;		/* default material code */
static int debug=0;		/* Debug flag */
static int binary=0;		/* flag indicating input is binary format */

static char *usage="%s [-dab] [-t tolerance] [-N forced_name] [-i initial_ident] [-I constant_ident] [-m material_code] [-c units_str] [-x rt_debug_flag] input.stl output.g\n\
	where input.stl is a STereoLithography file\n\
	and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
        The -b option specifies that the input file is in the binary STL format (default is ASCII). \n\
	The -c option specifies the units used in the STL file (units_str may be \"in\", \"ft\",... default is \"mm\"\n\
	The -N option specifies a name to use for the object.\n\
	The -d option prints additional debugging information.\n\
	The -i option sets the initial region ident number (default is 1000).\n\
	The -I option sets the ident number that will be assigned to all regions (conflicts with -i).\n\
	The -m option sets the integer material code for all the parts (default is 1).\n\
	The -a option creates BRL-CAD 'air' regions from everything in the model.\n\
	The -t option specifies the minumim distance between two distinct vertices (mm).\n\
	The -x option specifies an RT debug flags (see cad/librt/debug.h).\n";
static FILE *fd_in;		/* input file */
static struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
static float conv_factor=1.0;	/* conversion factor from model units to mm */
static unsigned int obj_count=0; /* Count of parts converted for "stl-g" conversions */
static int *bot_faces=NULL;	 /* array of ints (indices into tree_root->the_array array) three per face */
static int bot_fsize=0;		/* current size of the bot_faces array */
static int bot_fcurr=0;		/* current bot face */

/* Size of blocks of faces to malloc */
#define BOT_FBLOCK	128

#define	MAX_LINE_LEN	512

void
Add_face( face )
int face[3];
{
	if( !bot_faces )
	{
		bot_faces = (int *)bu_malloc( 3 * BOT_FBLOCK * sizeof( int ), "bot_faces" );
		bot_fsize = BOT_FBLOCK;
		bot_fcurr = 0;
	}
	else if( bot_fcurr >= bot_fsize )
	{
		bot_fsize += BOT_FBLOCK;
		bot_faces = (int *)bu_realloc( (void *)bot_faces, 3 * bot_fsize * sizeof( int ), "bot_faces increase" );
	}

	VMOVE( &bot_faces[3*bot_fcurr], face );
	bot_fcurr++;
}

char *
mk_unique_brlcad_name( char *name )
{
	char *c;
	struct bu_vls vls;
	int count=0;
	int len;

	bu_vls_init( &vls );

	bu_vls_strcpy( &vls, name );

	c = bu_vls_addr( &vls );

	while( *c != '\0' ) {
		if( *c == '/' || !isprint( *c ) ) {
			*c = '_';
		}
		c++;
	}

	len = bu_vls_strlen( &vls );
	while( db_lookup( fd_out->dbip, bu_vls_addr( &vls ), LOOKUP_QUIET ) != DIR_NULL ) {
		char suff[10];

		bu_vls_trunc( &vls, len );
		count++;
		sprintf( suff, "_%d", count );
		bu_vls_strcat( &vls, suff );
	}

	return( bu_vls_strgrab( &vls ) );
	
}

static void
Convert_part_ascii( line )
char line[MAX_LINE_LEN];
{
	char line1[MAX_LINE_LEN];
	char name[MAX_LINE_LEN + 1];
	unsigned int obj=0;
	char *solid_name;
	int start;
	int i;
	int face_count=0;
	int degenerate_count=0;
	int small_count=0;
	float colr[3]={0.5, 0.5, 0.5};
	unsigned char color[3]={ 128, 128, 128 };
	char *brlcad_name;
	struct wmember head;
	vect_t normal={0,0,0};
	int solid_in_region=0;

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_prmem( "At start of Conv_prt():\n" );

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
	{
		bu_log( "Barrier check at start of Convet_part:\n" );
		if( bu_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}


	bot_fcurr = 0;
	BU_LIST_INIT( &head.l );

	start = (-1);
	/* skip leading blanks */
	while( isspace( line[++start] ) && line[start] != '\0' );
	if( strncmp( &line[start] , "solid" , 5 ) && strncmp( &line[start] , "SOLID" , 5 ) )
	{
		bu_log( "Convert_part_ascii: Called for non-part\n%s\n" , line );
		return;
	}

	/* skip blanks before name */
	start += 4;
	while( isspace( line[++start] ) && line[start] != '\0' );

	if( line[start] != '\0' )
	{
		/* get name */
		i = (-1);
		start--;
		while( !isspace( line[++start] ) && line[start] != '\0' && line[start] != '\n' )
			name[++i] = line[start];
		name[++i] = '\0';

		/* get object id */
		sscanf( &line[start] , "%x" , &obj );
	}
	else if( forced_name )
		strcpy( name, forced_name );
	else /* build a name from the file name */
	{
		char tmp_str[512];
		char *ptr;
		int len, suff_len;

		obj_count++;
		obj = obj_count;

		/* copy the file name into our work space */
		strncpy( tmp_str, input_file, 512 );
		tmp_str[511] = '\0';

		/* eliminate a trailing ".stl" */
		len = strlen( tmp_str );
		if( len > 4 )
		{
			if( !strncmp( &tmp_str[len-4], ".stl", 4 ) )
				tmp_str[len-4] = '\0';
		}

		/* skip over all characters prior to the last '/' */
		ptr = strrchr( tmp_str, '/' );
		if( !ptr )
			ptr = tmp_str;
		else
			ptr++;

		/* now copy what is left to the name */
		strncpy( name, ptr, MAX_LINE_LEN );
		name[MAX_LINE_LEN] = '\0';
		sprintf( tmp_str, "_%d", obj_count );
		len = strlen( name );
		suff_len = strlen( tmp_str );
		if( len + suff_len < MAX_LINE_LEN )
			strcat( name, tmp_str );
		else
			sprintf( &name[MAX_LINE_LEN-suff_len-1], tmp_str );
	}

	bu_log( "Converting Part: %s\n" , name );

	if( debug )
		bu_log( "Conv_part %s x%x\n" , name , obj );

	solid_count++;
	solid_name = mk_unique_brlcad_name( name );

	bu_log( "\tUsing solid name: %s\n" , solid_name );

	if( RT_G_DEBUG & DEBUG_MEM || RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_prmem( "At start of Convert_part_ascii()" );

	while( fgets( line1, MAX_LINE_LEN, fd_in ) != NULL )
	{
		start = (-1);
		while( isspace( line1[++start] ) );
		if( !strncmp( &line1[start] , "endsolid" , 8 ) || !strncmp( &line1[start] , "ENDSOLID" , 8 ) )
			break;
		else if( !strncmp( &line1[start] , "color" , 5 ) || !strncmp( &line1[start] , "COLOR" , 5 ) )
		{
			sscanf( &line1[start+5] , "%f%f%f" , &colr[0] , &colr[1] , &colr[2] );
			for( i=0 ; i<3 ; i++ )
				color[i] = (int)(colr[i] * 255.0);
		}
		else if( !strncmp( &line1[start] , "normal" , 6 ) || !strncmp( &line1[start] , "NORMAL" , 6 ) )
		{
			float x,y,z;

			start += 6;
			sscanf( &line1[start] , "%f%f%f" , &x , &y , &z );
			VSET( normal , x , y , z );
		}
		else if( !strncmp( &line1[start] , "facet" , 5 ) || !strncmp( &line1[start] , "FACET" , 5 ) )
		{
			VSET( normal , 0.0 , 0.0 , 0.0 );

			start += 4;
			while( line1[++start] && isspace( line1[start] ) );

			if( line1[start] )
			{
				if( !strncmp( &line1[start] , "normal" , 6 ) || !strncmp( &line1[start] , "NORMAL" , 6 ) )
				{
					float x,y,z;

					start += 6;
					sscanf( &line1[start] , "%f%f%f" , &x , &y , &z );
					VSET( normal , x , y , z );
				}
			}
		}
		else if( !strncmp( &line1[start] , "outer loop" , 10 ) || !strncmp( &line1[start] , "OUTER LOOP" , 10 ) )
		{
			int endloop=0;
			int vert_no=0;
			int tmp_face[3];

			while( !endloop )
			{
				if( fgets( line1, MAX_LINE_LEN, fd_in ) == NULL )
					bu_bomb( "Unexpected EOF while reading a loop in a part!!!\n" );
				
				start = (-1);
				while( isspace( line1[++start] ) );

				if( !strncmp( &line1[start] , "endloop" , 7 ) || !strncmp( &line1[start] , "ENDLOOP" , 7 ) )
					endloop = 1;
				else if ( !strncmp( &line1[start] , "vertex" , 6 ) || !strncmp( &line1[start] , "VERTEX" , 6 ) )
				{
					double x,y,z;

					sscanf( &line1[start+6] , "%lf%lf%lf" , &x , &y , &z );

					if( vert_no > 2 )
					{
						int n;

						bu_log( "Non-triangular loop:\n" );
						for( n=0 ; n<3 ; n++ )
							bu_log( "\t( %g %g %g )\n", V3ARGS( &tree_root->the_array[tmp_face[n]] ) );

						bu_log( "\t( %g %g %g )\n", x, y, z );
					}
					tmp_face[vert_no++] = Add_vert( x, y, z, tree_root, tol.dist_sq );
				}
				else
					bu_log( "Unrecognized line: %s\n", line1 );
			}

			/* check for degenerate faces */
			if( tmp_face[0] == tmp_face[1] )
			{
				degenerate_count++;
				continue;
			}

			if( tmp_face[0] == tmp_face[2] )
			{
				degenerate_count++;
				continue;
			}

			if( tmp_face[1] == tmp_face[2] )
			{
				degenerate_count++;
				continue;
			}

			if( debug )
			{
				int n;

				bu_log( "Making Face:\n" );
				for( n=0 ; n<3; n++ )
					bu_log( "\tvertex #%d: ( %g %g %g )\n", tmp_face[n], V3ARGS( &tree_root->the_array[3*tmp_face[n]] ) );
				VPRINT(" normal", normal);
			}

			Add_face( tmp_face );
			face_count++;
		}
	}

	/* Check if this part has any solid parts */
	if( face_count == 0 )
	{
		bu_log( "\t%s has no solid parts, ignoring\n" , name );
		if( degenerate_count )
			bu_log( "\t%d faces were degenerate\n", degenerate_count );
		if( small_count )
			bu_log( "\t%d faces were too small\n", small_count );
		return;
	}
	else
	{
		if( degenerate_count )
			bu_log( "\t%d faces were degenerate\n", degenerate_count );
		if( small_count )
			bu_log( "\t%d faces were too small\n", small_count );
	}

	mk_bot( fd_out, solid_name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, tree_root->curr_vert, bot_fcurr,
		tree_root->the_array, bot_faces, NULL, NULL );
	clean_vert_tree( tree_root );

	if( face_count && !solid_in_region )
	{
		(void)mk_addmember( solid_name , &head.l , NULL, WMOP_UNION );
	}

	brlcad_name = bu_malloc( strlen( solid_name ) + 3, "region name" );
	sprintf( brlcad_name, "%s.r", solid_name );
	bu_log( "\tMaking region (%s)\n" , brlcad_name );

	if( const_id >= 0 ) {
		mk_lrcomb( fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
			   color, const_id, 0, mat_code, 100, 0 );
		if( face_count ) {
				(void)mk_addmember( brlcad_name, &all_head.l, NULL, WMOP_UNION );
		}
	} else {
		mk_lrcomb( fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
			   color, id_no, 0, mat_code, 100, 0 );
		if( face_count )
			(void)mk_addmember( brlcad_name, &all_head.l, NULL, WMOP_UNION );
		id_no++;
	}

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
	{
		bu_log( "Barrier check at end of Convert_part_ascii:\n" );
		if( bu_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}

	return;
}

/* Byte swaps a 4-byte val */
void lswap(unsigned int *v) 
{
	unsigned int r;

	r = *v;
	*v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
		| ((r & 0xff000000) >> 24);
}

static void
Convert_part_binary()
{
	unsigned char buf[51];
	unsigned long num_facets=0;
	float flts[12];
	vect_t normal;
	int tmp_face[3];
	struct wmember head;
	char *solid_name="s.stl";
	int face_count=0;
	int degenerate_count=0;
	int small_count=0;

	solid_count++;
	bu_log( "\tUsing solid name: %s\n" , solid_name );

	fread( buf, 4, 1, fd_in );
	lswap( (unsigned int *)buf );
	num_facets = bu_glong( buf );
	bu_log( "\t%d facets\n", num_facets );
	while( fread( buf, 48, 1, fd_in ) ) {
		int i;
		double pt[3];

		for( i=0 ; i<12 ; i++ ) {
			lswap( (unsigned int *)&buf[i*4] );
		}
		ntohf( (unsigned char *)flts, buf, 12 );

		fread( buf, 2, 1, fd_in );

		VMOVE( normal, flts );
		VMOVE( pt, &flts[3] );
		tmp_face[0] = Add_vert( V3ARGS( pt ), tree_root, tol.dist_sq );
		VMOVE( pt, &flts[6] );
		tmp_face[1] = Add_vert( V3ARGS( pt ), tree_root, tol.dist_sq );
		VMOVE( pt, &flts[9] );
		tmp_face[2] = Add_vert( V3ARGS( pt ), tree_root, tol.dist_sq );

		/* check for degenerate faces */
		if( tmp_face[0] == tmp_face[1] ) {
			degenerate_count++;
			continue;
		}

		if( tmp_face[0] == tmp_face[2] ) {
			degenerate_count++;
			continue;
		}

		if( tmp_face[1] == tmp_face[2] ) {
			degenerate_count++;
			continue;
		}

		if( debug ) {
			int n;

			bu_log( "Making Face:\n" );
			for( n=0 ; n<3; n++ )
				bu_log( "\tvertex #%d: ( %g %g %g )\n", tmp_face[n], V3ARGS( &tree_root->the_array[3*tmp_face[n]] ) );
			VPRINT(" normal", normal);
		}

		Add_face( tmp_face );
		face_count++;
	}

	/* Check if this part has any solid parts */
	if( face_count == 0 )
	{
		bu_log( "\tpart has no solid parts, ignoring\n" );
		if( degenerate_count )
			bu_log( "\t%d faces were degenerate\n", degenerate_count );
		if( small_count )
			bu_log( "\t%d faces were too small\n", small_count );
		return;
	}
	else
	{
		if( degenerate_count )
			bu_log( "\t%d faces were degenerate\n", degenerate_count );
		if( small_count )
			bu_log( "\t%d faces were too small\n", small_count );
	}

	mk_bot( fd_out, solid_name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, tree_root->curr_vert, bot_fcurr,
		tree_root->the_array, bot_faces, NULL, NULL );
	clean_vert_tree( tree_root );

	BU_LIST_INIT( &head.l );
	if( face_count )
	{
		(void)mk_addmember( solid_name , &head.l , NULL, WMOP_UNION );
	}
	bu_log( "\tMaking region (r.stl)\n" );

	if( const_id >= 0 ) {
		mk_lrcomb( fd_out, "r.stl", &head, 1, (char *)NULL, (char *)NULL,
			   NULL, const_id, 0, mat_code, 100, 0 );
		if( face_count ) {
				(void)mk_addmember( "r.stl", &all_head.l, NULL, WMOP_UNION );
		}
	} else {
		mk_lrcomb( fd_out, "r.stl", &head, 1, (char *)NULL, (char *)NULL,
			   NULL, id_no, 0, mat_code, 100, 0 );
		if( face_count )
			(void)mk_addmember( "r.stl", &all_head.l, NULL, WMOP_UNION );
		id_no++;
	}

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
	{
		bu_log( "Barrier check at end of Convert_part_ascii:\n" );
		if( bu_mem_barriercheck() )
			rt_bomb( "Barrier check failed!!!\n" );
	}

	return;
}


static void
Convert_input()
{
	char line[ MAX_LINE_LEN ];

	if( binary ) {
		if( fread( line, 80, 1, fd_in ) < 1 ) {
			if( feof( fd_in ) ) {
				bu_bomb( "Unexpected EOF in input file!!!\n" );
			} else {
				bu_log( "Error reading input file\n" );
				perror( "stl-g" );
				bu_bomb( "Error reading input file\n" );
			}
		}
		line[80] = '\0';
		bu_log( "header data:\n%s\n\n", line );
		Convert_part_binary();
	} else {
		while( fgets( line, MAX_LINE_LEN, fd_in ) != NULL ) {
			if( !strncmp( line , "solid" , 5 ) || !strncmp( line , "SOLID" , 5 ) )
				Convert_part_ascii( line );
			else
				bu_log( "Unrecognized line:\n%s\n" , line );
		}
	}
}


/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	register int c;

        tol.magic = BN_TOL_MAGIC;

	/* this value selected as a resaonable compromise between eliminating
	 * needed faces and keeping degenerate faces
	 */
        tol.dist = 0.005;	/* default, same as MGED, RT, ... */
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	bu_vls_init( &ret_name );

	forced_name = NULL;

	conv_factor = 1.0;	/* default */

	if( argc < 2 )
	{
		bu_log( usage, argv[0]);
		exit(1);
	}

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "bt:i:I:m:dx:N:c:")) != EOF) {
		double tmp;

		switch (c) {
		case 'b':	/* binary format */
			binary = 1;
			break;
		case 't':	/* tolerance */
			tmp = atof( optarg );
			if( tmp <= 0.0 ) {
				bu_log( "Tolerance must be greater then zero, using default (%g)\n",
					tol.dist );
				break;
			}
			tol.dist = tmp;
			tol.dist_sq = tmp * tmp;
			break;
		case 'c':	/* convert from units */
			conv_factor = bu_units_conversion( optarg );
			if( conv_factor == 0.0 )
			{
				bu_log( "Illegal units: (%s)\n", optarg );
				bu_bomb( "Illegal units!!\n" );
			}
			else
				bu_log( "Converting units from %s to mm (conversion factor is %g)\n", optarg, conv_factor );
			break;
		case 'N':	/* force a name on this object */
			forced_name = optarg;
			break;
		case 'i':
			id_no = atoi( optarg );
			break;
		case  'I':
			const_id = atoi( optarg );
			if( const_id < 0 )
			{
				bu_log( "Illegal value for '-I' option, must be zero or greater!!!\n" );
				bu_log( usage, argv[0] );
				bu_bomb( "Illegal value for option '-I'\n" );
			}
			break;
		case 'm':
			mat_code = atoi( optarg );
			break;
		case 'd':
			debug = 1;
			break;
		case 'x':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.debug );
			bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
			bu_log("\n");
			break;
		default:
			bu_log( usage, argv[0]);
			exit(1);
			break;
		}
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	input_file = argv[optind];
	if( (fd_in=fopen( input_file, "r")) == NULL )
	{
		bu_log( "Cannot open input file (%s)\n" , input_file );
		perror( argv[0] );
		exit( 1 );
	}
	optind++;
	brlcad_file = argv[optind];
	if( (fd_out=wdb_fopen( brlcad_file)) == NULL )
	{
		bu_log( "Cannot open BRL-CAD file (%s)\n" , brlcad_file );
		perror( argv[0] );
		exit( 1 );
	}

	mk_id_units( fd_out , "Conversion from Stereolithography format" , "mm" );

	BU_LIST_INIT( &all_head.l );

	/* create a tree sructure to hold the input vertices */
	tree_root = create_vert_tree();

	Convert_input();

	/* make a top level group */
	mk_lcomb( fd_out, "all", &all_head, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0 );

	fclose( fd_in );

	wdb_close( fd_out );

	return 0;
}
