/** \addtogroup librt */
/*@{*/
/** \file importFg4Section.
 *      Some of this code was taken from conv/fast4-g.c and libwdb/bot.c
 *      and modified to behave as a method of the BRL-CAD database object
 *      that imports a Fastgen4 section from a string. This section can
 *      only contain GRIDs, CTRIs and CQUADs.
 *
 *  Author of fast4-g.c -
 *      John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

/* system headers */
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <errno.h>
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif

/* interface headers */
#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"

/* local headers */
#include "../librt/debug.h"


static int	grid_size;		/* Number of points that will fit in current grid_pts array */
static int	max_grid_no=0;		/* Maximum grid number used */
static int	mode=0;			/* Plate mode (1) or volume mode (2), of current component */
static int	group_id=(-1);		/* Group identification number from SECTION card */
static int	comp_id=(-1);		/* Component identification number from SECTION card */
static int	region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static char	field[9];		/* Space for storing one field from an input line */
static int	name_count;		/* Count of number of times this name_name has been used */
static int	bot=0;			/* Flag: >0 -> There are BOT's in current component */
static int	warnings=0;		/* Flag: >0 -> Print warning messages */
static int	debug=0;		/* Debug flag */
static int	rt_debug=0;		/* RT_G_DEBUG */
static int	quiet=0;		/* flag to not blather */
static int	comp_count=0;		/* Count of components in FASTGEN4 file */

static int		*faces=NULL;	/* one triplet per face indexing three grid points */
static fastf_t		*thickness;	/* thickness of each face */
static char		*facemode;	/* mode for each face */
static int		face_size=0;	/* actual length of above arrays */
static int		face_count=0;	/* number of faces in above arrays */

static int	*int_list;		/* Array of integers */
static int	int_list_count=0;	/* Number of ints in above array */
static int	int_list_length=0;	/* Length of int_list array */

#define		PLATE_MODE	1
#define		VOLUME_MODE	2

#define		POS_CENTER	1	/* face positions for facets */
#define		POS_FRONT	2

#define		END_OPEN	1	/* End closure codes for cones */
#define		END_CLOSED	2

#define		GRID_BLOCK	256	/* allocate space for grid points in blocks of 256 points */

#define		CLINE		'l'
#define		CHEX1		'p'
#define		CHEX2		'b'
#define		CTRI		't'
#define		CQUAD		'q'
#define		CCONE1		'c'
#define		CCONE2		'd'
#define		CCONE3		'e'
#define		CSPHERE		's'
#define		NMG		'n'
#define		BOT		't'
#define		COMPSPLT	'h'

point_t *grid_pts;

void do_grid(char *line);
void do_tri(char *line);
void do_quad(char *line);
void make_bot_object(char		*name,
		     struct rt_wdb	*wdbp);

/*************************** code from libwdb/bot.c ***************************/

static int
rt_mk_bot_w_normals(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	flags,
	int		num_vertices,
	int		num_faces,
	fastf_t		*vertices,	/* array of floats for vertices [num_vertices*3] */
	int		*faces,		/* array of ints for faces [num_faces*3] */
	fastf_t		*thickness,	/* array of plate mode thicknesses (corresponds to array of faces)
					 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
					 */
	struct bu_bitv	*face_mode,	/* a flag for each face indicating thickness is appended to hit point,
					 * otherwise thickness is centered about hit point
					 */
	int		num_normals,	/* number of unit normals in normals array */
	fastf_t		*normals,	/* array of floats for normals [num_normals*3] */
	int		*face_normals )	/* array of ints (indices into normals array), must have 3*num_faces entries */
{
	struct rt_bot_internal *bot;
	int i;

	if( (num_normals > 0) && (fp->dbip->dbi_version < 5 ) ) {
		bu_log( "You are using an old database format which does not support surface normals for BOT primitives\n" );
		bu_log( "You are attempting to create a BOT primitive named \"%s\" with surface normals\n" );
		bu_log( "The surface normals will not be saved\n" );
		bu_log( "Please upgrade to the current database format by using \"dbupgrade\"\n" );
	}

	BU_GETSTRUCT( bot, rt_bot_internal );
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = mode;
	bot->orientation = orientation;
	bot->bot_flags = flags;
	bot->num_vertices = num_vertices;
	bot->num_faces = num_faces;
	bot->vertices = (fastf_t *)bu_calloc( num_vertices * 3, sizeof( fastf_t ), "bot->vertices" );
	for( i=0 ; i<num_vertices*3 ; i++ )
		bot->vertices[i] = vertices[i];
	bot->faces = (int *)bu_calloc( num_faces * 3, sizeof( int ), "bot->faces" );
	for( i=0 ; i<num_faces*3 ; i++ )
		bot->faces[i] = faces[i];
	if( mode == RT_BOT_PLATE )
	{
		bot->thickness = (fastf_t *)bu_calloc( num_faces, sizeof( fastf_t ), "bot->thickness" );
		for( i=0 ; i<num_faces ; i++ )
			bot->thickness[i] = thickness[i];
		bot->face_mode = bu_bitv_dup( face_mode );
	}
	else
	{
		bot->thickness = (fastf_t *)NULL;
		bot->face_mode = (struct bu_bitv *)NULL;
	}

	if( (num_normals > 0) && (fp->dbip->dbi_version >= 5 ) ) {
		bot->num_normals = num_normals;
		bot->num_face_normals = bot->num_faces;
		bot->normals = (fastf_t *)bu_calloc( bot->num_normals * 3, sizeof( fastf_t ), "BOT normals" );
		bot->face_normals = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "BOT face normals" );
		memcpy( bot->normals, normals, bot->num_normals * 3 * sizeof( fastf_t ) );
		memcpy( bot->face_normals, face_normals, bot->num_faces * 3 * sizeof( int ) );
	} else {
		bot->bot_flags = 0;
		bot->num_normals = 0;
		bot->num_face_normals = 0;
		bot->normals = (fastf_t *)NULL;
		bot->face_normals = (int *)NULL;
	}

	return wdb_export(fp, name, (genptr_t)bot, ID_BOT, 1.0);
}

static int
rt_mk_bot(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	flags,
	int		num_vertices,
	int		num_faces,
	fastf_t		*vertices,	/* array of floats for vertices [num_vertices*3] */
	int		*faces,		/* array of ints for faces [num_faces*3] */
	fastf_t		*thickness,	/* array of plate mode thicknesses (corresponds to array of faces)
					 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
					 */
	struct bu_bitv	*face_mode )	/* a flag for each face indicating thickness is appended to hit point,
					 * otherwise thickness is centered about hit point
					 */
{
	return( rt_mk_bot_w_normals( fp, name, mode, orientation, flags, num_vertices, num_faces, vertices,
				  faces, thickness, face_mode, 0, NULL, NULL ) );
}

/*************************** code from conv/fast4-g.c ***************************/

void
do_grid(char *line)
{
	int grid_no;
	fastf_t x,y,z;

	if( RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed at start of do_grid\n" );

	strncpy( field , &line[8] , 8 );
	grid_no = atoi( field );

	if( grid_no < 1 )
	{
		bu_log( "ERROR: grid id number = %d\n" , grid_no );
		rt_bomb( "BAD GRID ID NUMBER\n" );
	}

	strncpy( field , &line[24] , 8 );
	x = atof( field );

	strncpy( field , &line[32] , 8 );
	y = atof( field );

	strncpy( field , &line[40] , 8 );
	z = atof( field );

	while( grid_no > grid_size - 1 )
	{
		grid_size += GRID_BLOCK;
		grid_pts = (point_t *)bu_realloc( (char *)grid_pts , grid_size * sizeof( point_t ) , "fast4-g: grid_pts" );
	}

	VSET( grid_pts[grid_no] , x*25.4 , y*25.4 , z*25.4 );

	if( grid_no > max_grid_no )
		max_grid_no = grid_no;
	if( RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed at end of do_grid\n" );
}

void
Add_bot_face(int pt1, int pt2, int pt3, fastf_t thick, int pos)
{

	if( pt1 == pt2 || pt2 == pt3 || pt1 == pt3 )
	{
		bu_log( "Add_bot_face: ignoring degenerate triangle in group %d component %d\n", group_id, comp_id );
		return;
	}

	if( pos == 0 )	/* use default */
		pos = POS_FRONT;

	if( mode == PLATE_MODE )
	{
		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			bu_log( "Add_bot_face: illegal postion parameter (%d), must be one or two (ignoring face for group %d component %d)\n" , pos, group_id, comp_id );
			return;
		}
	}

	if( face_count >= face_size )
	{
		face_size += GRID_BLOCK;
		if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
			bu_log( "memory corrupted before realloc of faces, thickness, and facemode\n" );
		faces = (int *)bu_realloc( (void *)faces,  face_size*3*sizeof( int ), "faces" );
		thickness = (fastf_t *)bu_realloc( (void *)thickness, face_size*sizeof( fastf_t ), "thickness" );
		facemode = (char *)bu_realloc( (void *)facemode, face_size*sizeof( char ), "facemode" );
		if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
			bu_log( "memory corrupted after realloc of faces, thickness, and facemode\n" );
	}

	faces[face_count*3] = pt1;
	faces[face_count*3+1] = pt2;
	faces[face_count*3+2] = pt3;

	if( mode == PLATE_MODE )
	{
		thickness[face_count] = thick;
		facemode[face_count] = pos;
	}
	else
	{
		thickness[face_count] = 0,0;
		facemode[face_count] = 0;
	}

	face_count++;

	if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
		bu_log( "memory corrupted at end of Add_bot_face()\n" );
}

void
do_tri(char *line)
{
	int element_id;
	int pt1,pt2,pt3;
	fastf_t thick;
	int pos;

	if( debug )
		bu_log( "do_tri: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !bot )
		bot = element_id;

	if( faces == NULL )
	{
		if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
			bu_log( "memory corrupted before malloc of faces\n" );
		faces = (int *)bu_malloc( GRID_BLOCK*3*sizeof( int ), "faces" );
		thickness = (fastf_t *)bu_malloc( GRID_BLOCK*sizeof( fastf_t ), "thickness" );
		facemode = (char *)bu_malloc( GRID_BLOCK*sizeof( char ), "facemode" );
		face_size = GRID_BLOCK;
		face_count = 0;
		if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
			bu_log( "memory corrupted after malloc of faces , thickness, and facemode\n" );
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[40] , 8 );
	pt3 = atoi( field );

	thick = 0.0;
	pos = 0;

	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );
		if( pos == 0 )
			pos = POS_FRONT;

		if( debug )
			bu_log( "\tplate mode: thickness = %f\n" , thick );

	}

	if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
		bu_log( "memory corrupted before call to Add_bot_face()\n" );

	Add_bot_face( pt1, pt2, pt3, thick, pos );

	if(bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck() )
		bu_log( "memory corrupted after call to Add_bot_face()\n" );
}

void
do_quad(char *line)
{
	int element_id;
	int pt1,pt2,pt3,pt4;
	fastf_t thick = 0.0;
	int pos = 0;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( debug )
		bu_log( "do_quad: %s\n" , line );

	if( !bot )
		bot = element_id;

	if( faces == NULL )
	{
		faces = (int *)bu_malloc( GRID_BLOCK*3*sizeof( int ), "faces" );
		thickness = (fastf_t *)bu_malloc( GRID_BLOCK*sizeof( fastf_t ), "thickness" );
		facemode = (char *)bu_malloc( GRID_BLOCK*sizeof( char ), "facemode" );
		face_size = GRID_BLOCK;
		face_count = 0;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[40] , 8 );
	pt3 = atoi( field );

	strncpy( field , &line[48] , 8 );
	pt4 = atoi( field );

	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( pos == 0 )	/* use default */
			pos = POS_FRONT;

		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			bu_log( "do_quad: illegal postion parameter (%d), must be one or two\n" , pos );
			bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}
	}

	Add_bot_face( pt1, pt2, pt3, thick, pos );
	Add_bot_face( pt1, pt3, pt4, thick, pos );
}

void
make_bot_object(char		*name,
		struct rt_wdb	*wdbp)
{
	int i;
	int max_pt=0, min_pt=999999;
	int num_vertices;
	struct bu_bitv *bv=NULL;
	int bot_mode;
	int element_id=bot;
	int count;
	struct rt_bot_internal bot_ip;

	bot_ip.magic = RT_BOT_INTERNAL_MAGIC;
	for( i=0 ; i<face_count ; i++ )
	{
		V_MIN( min_pt, faces[i*3] );
		V_MAX( max_pt, faces[i*3] );
		V_MIN( min_pt, faces[i*3+1] );
		V_MAX( max_pt, faces[i*3+1] );
		V_MIN( min_pt, faces[i*3+2] );
		V_MAX( max_pt, faces[i*3+2] );
	}

	num_vertices = max_pt - min_pt + 1;
	bot_ip.num_vertices = num_vertices;
	bot_ip.vertices = (fastf_t *)bu_calloc( num_vertices*3, sizeof( fastf_t ), "BOT vertices" );
	for( i=0 ; i<num_vertices ; i++ )
		VMOVE( &bot_ip.vertices[i*3], grid_pts[min_pt+i] )

	for( i=0 ; i<face_count*3 ; i++ )
		faces[i] -= min_pt;
	bot_ip.num_faces = face_count;
	bot_ip.faces = bu_calloc( face_count*3, sizeof( int ), "BOT faces" );
	for( i=0 ; i<face_count*3 ; i++ )
		bot_ip.faces[i] = faces[i];

	bot_ip.face_mode = (struct bu_bitv *)NULL;
	bot_ip.thickness = (fastf_t *)NULL;
	if( mode == PLATE_MODE )
	{
		bot_mode = RT_BOT_PLATE;
		bv = bu_bitv_new( face_count );
		bu_bitv_clear( bv );
		for( i=0 ; i<face_count ; i++ )
		{
			if( facemode[i] == POS_FRONT )
				BU_BITSET( bv, i );
		}
		bot_ip.face_mode = bv;
		bot_ip.thickness = (fastf_t *)bu_calloc( face_count, sizeof( fastf_t ), "BOT thickness" );
		for( i=0 ; i<face_count ; i++ )
			bot_ip.thickness[i] = thickness[i];
	}
	else
		bot_mode = RT_BOT_SOLID;

	bot_ip.mode = bot_mode;
	bot_ip.orientation = RT_BOT_UNORIENTED;
	bot_ip.bot_flags = 0;

	count = rt_bot_vertex_fuse( &bot_ip );
	if( count )
		(void)rt_bot_condense( &bot_ip );

	count = rt_bot_face_fuse( &bot_ip );
	if( count )
		bu_log( "WARNING: %d duplicate faces eliminated from group %d component %d\n", count, group_id, comp_id );

	rt_mk_bot(wdbp, name, bot_mode, RT_BOT_UNORIENTED, 0,
	       bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
	       bot_ip.faces, bot_ip.thickness, bot_ip.face_mode);

	if( mode == PLATE_MODE )
	{
		bu_free( (char *)bot_ip.thickness, "BOT thickness" );
		bu_free( (char *)bot_ip.face_mode, "BOT face_mode" );
	}
	bu_free( (char *)bot_ip.vertices, "BOT vertices" );
	bu_free( (char *)bot_ip.faces, "BOT faces" );
}


/*************************** Start of new code. ***************************/

#define FIND_NEWLINE(_cp,_eosFlag) \
    while (*(_cp) != '\0' && \
	   *(_cp) != '\n') \
	++(_cp); \
\
    if (*(_cp) == '\0') \
	_eosFlag = 1; \
    else \
	*(_cp) = '\0';

#ifdef IMPORT_FASTGEN4_SECTION
int
wdb_importFg4Section_cmd(struct rt_wdb	*wdbp,
			 Tcl_Interp	*interp,
			 int		argc,
			 char 		**argv)
{
    char *cp;
    char *line;
    char *lines;
    int eosFlag = 0;

    if (argc != 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias wdb_importFg4Section %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    grid_size = GRID_BLOCK;
    grid_pts = (point_t *)bu_malloc(grid_size * sizeof(point_t) ,
				    "importFg4Section: grid_pts");

    lines = strdup(argv[2]);
    cp = line = lines;

    FIND_NEWLINE(cp,eosFlag);

    strncpy(field, line+8, 8);
    group_id = atoi(field);

    strncpy(field, line+16, 8);
    comp_id = atoi(field);

    region_id = group_id * 1000 + comp_id;

    if (comp_id > 999) {
	bu_log( "Illegal component id number %d, changed to 999\n" , comp_id );
	comp_id = 999;
    }

    strncpy(field, line+24, 8);
    mode = atoi(field);
    if (mode != 1 && mode != 2) {
	bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
	       mode, group_id, comp_id);
	mode = 2;
    }

    while (!eosFlag) {
	++cp;
	line = cp;
	FIND_NEWLINE(cp,eosFlag);

	if (!strncmp(line , "GRID" , 4))
	    do_grid(line);
	else if (!strncmp(line , "CTRI" , 4))
	    do_tri(line);
	else if (!strncmp(line , "CQUAD" , 4))
	    do_quad(line);
    }

    make_bot_object(argv[1], wdbp);
    free((void *)lines);
    bu_free((void *)grid_pts, "importFg4Section: grid_pts");

    /* free memory associated with globals */
    bu_free((void *)faces, "importFg4Section: faces");
    bu_free((void *)thickness, "importFg4Section: thickness");
    bu_free((void *)facemode, "importFg4Section: facemode");

    faces = NULL;
    thickness = NULL;
    facemode = NULL;

    return TCL_OK;
}
#endif
