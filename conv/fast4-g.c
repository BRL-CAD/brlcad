/*
 *                      F A S T 4 - G
 *
 *  Program to convert the FASTGEN4 format to BRL-CAD.
 *
 *  Author -
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
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "db.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

extern int errno;

#define		LINELEN		128	/* Length of char array for input line */

static char	line[LINELEN];		/* Space for input line */
static FILE	*fdin;			/* Input FASTGEN4 file pointer */
static FILE	*fdout;			/* Output BRL-CAD file pointer */
static point_t	*grid_pts;		/* Points from GRID cards */
static int	grid_size;		/* Number of points that will fit in current grid_pts array */
static int	max_grid_no=0;		/* Maximum grid number used */
static int	mode=0;			/* Plate mode (1) or volume mode (2) */
static int	group_id;		/* Group identification number from SECTION card */
static int	comp_id;		/* Component identification number from SECTION card */
static int	region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static int	joint_no=0;		/* Count of CLINE joints (sph solids created to connect CLINES) */
static char	field[9];		/* Space for storing one field from an input line */
static char	vehicle[17];		/* Title for BRLCAD model from VEHICLE card */
static char	name_name[NAMESIZE+1];	/* Component name built from $NAME card */
static int	name_count;		/* Count of number of times this name_name has been used */
static int	pass;			/* Pass number (0 -> only make names, 1-> do geometry ) */
static struct cline	*cline_last_ptr; /* Pointer to last element in linked list of clines */
static struct wmember group_head[11];	/* Lists of regions for groups */
static struct nmg_ptbl stack;		/* Stack for traversing name_tree */

static char	*mode_str[3]=		/* mode strings */
{
	"unknown mode",
	"plate mode",
	"volume mode"
};

#define		PLATE_MODE	1
#define		VOLUME_MODE	2

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
#define		CSPHERE		's'

/* convenient macro for building regions */
#define		MK_REGION( fp , headp , g_id , c_id , e_id , type ) \
			{\
				int r_id; \
				char name[NAMESIZE]; \
				r_id = g_id * 1000 + c_id; \
				make_region_name( name , g_id , c_id , e_id , type ); \
				mk_lrcomb( fp , name , headp , 1 ,\
					(char *)NULL, (char *)NULL, (char *)NULL, r_id, 0, 0, 0, 0 ); \
			}

#define	PUSH( ptr )	nmg_tbl( &stack , TBL_INS , (long *)ptr )
#define POP( structure , ptr )	{ if( NMG_TBL_END( &stack ) == 0 ) \
				ptr = (struct structure *)NULL; \
			  else \
			  { \
			  	ptr = (struct structure *)NMG_TBL_GET( &stack , NMG_TBL_END( &stack )-1 ); \
			  	nmg_tbl( &stack , TBL_RM , (long *)ptr ); \
			  } \
			}

struct cline
{
	int pt1,pt2;
	int element_id;
	int made;
	fastf_t thick;
	fastf_t radius;
	struct cline *next;
} *cline_root;

struct name_tree
{
	int region_id;
	int element_id;
	char name[NAMESIZE+1];
	struct name_tree *nleft,*nright,*rleft,*rright;
} *sect_name_root,*model_name_root;

struct hole_list
{
	int group;
	int component;
	struct hole_list *next;
};

struct holes
{
	int group;
	int component;
	struct hole_list *holes;
	struct holes *next;
} *hole_root;

void
Subtract_holes( head , comp_id , group_id )
struct wmember *head;
int comp_id;
int group_id;
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	hole_ptr = hole_root;
	while( hole_ptr )
	{
		if( hole_ptr->group == group_id && hole_ptr->component == comp_id )
		{
			list_ptr = hole_ptr->holes;
			while( list_ptr )
			{
				struct name_tree *ptr;
				int reg_id;

				reg_id = list_ptr->group * 1000 + list_ptr->component;
				ptr = model_name_root;
				while( ptr && ptr->region_id != reg_id )
				{
					int diff;

					diff = reg_id - ptr->region_id;
					if( diff > 0 )
						ptr = ptr->rright;
					else if( diff < 0 )
						ptr = ptr->rleft;
				}

				if( !ptr || ptr->region_id != reg_id )
				{
					rt_log( "ERROR: Can't find holes to subtract for group %d, component %d\n" , group_id , comp_id );
					return;
				}

				nmg_tbl( &stack , TBL_RST , (long *)NULL );

				while( ptr && ptr->region_id == reg_id )
				{
					while( ptr && ptr->region_id == reg_id )
					{
						PUSH( ptr );
						ptr = ptr->rleft;
					}
					POP( name_tree , ptr );
					if( !ptr ||  ptr->region_id != reg_id )
						break;

					if( mk_addmember( ptr->name , head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
						rt_bomb( "Subtract_holes: mk_addmember failed\n" );

					ptr = ptr->rright;
				}

				list_ptr = list_ptr->next;
			}
			break;
		}
		hole_ptr = hole_ptr->next;
	}
}

void
List_holes()
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	hole_ptr = hole_root;

	while( hole_ptr )
	{
		rt_log( "Holes for Group %d, Component %d:\n" , hole_ptr->group, hole_ptr->component );
		list_ptr = hole_ptr->holes;
		while( list_ptr )
		{
			rt_log( "\tgroup %d component %d\n" , list_ptr->group, list_ptr->component );
			list_ptr = list_ptr->next;
		}
		hole_ptr = hole_ptr->next;
	}
}

void
Free_tree()
{
	struct name_tree *ptr;
	struct name_tree *tmp_ptr;

	nmg_tbl( &stack , TBL_RST , (long *)NULL );

	ptr = sect_name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		tmp_ptr = ptr;
		ptr = ptr->nright;
		rt_free( (char *)tmp_ptr , "Free_tree: ptr" );
	}

	sect_name_root = (struct name_tree *)NULL;
}

void
List_names()
{
	struct name_tree *ptr;

	nmg_tbl( &stack , TBL_RST , (long *)NULL );

	rt_log( "\nNames for current section in alphabetical order:\n" );
	ptr = sect_name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		rt_log( "%s %d %d\n" , ptr->name , ptr->region_id , ptr->element_id );
		ptr = ptr->nright;
	}

	rt_log( "\nNames for current section in ident order:\n" );
	ptr = sect_name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->rleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		rt_log( "%s %d %d\n" , ptr->name , ptr->region_id , ptr->element_id );
		ptr = ptr->rright;
	}

	rt_log( "\tAlphabetical list of all names used in this model:\n" );
	ptr = model_name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		rt_log( "%s\n" , ptr->name );
		ptr = ptr->nright;
	}
}

struct name_tree *
Search_names( root , name , found )
struct name_tree *root;
char *name;
int *found;
{
	struct name_tree *ptr;

	*found = 0;

	ptr = root;
	if( !ptr )
		return( (struct name_tree *)NULL );

	while( 1 )
	{
		int diff;

		diff = strcmp( name , ptr->name );
		if( diff == 0 )
		{
			*found = 1;
			return( ptr );
		}
		else if( diff > 0 )
		{
			if( ptr->nright )
				ptr = ptr->nright;
			else
				return( ptr );
		}
		else if( diff < 0 )
		{
			if( ptr->nleft )
				ptr = ptr->nleft;
			else
				return( ptr );
		}
	}
}


struct name_tree *
Search_ident( root , reg_id , el_id , found )
struct name_tree *root;
int reg_id;
int el_id;
int *found;
{
	struct name_tree *ptr;

	*found = 0;

	ptr = root;
	if( !ptr )
		return( (struct name_tree *)NULL );

	while( 1 )
	{
		int diff;

		diff = reg_id -  ptr->region_id;
		if( diff == 0 )
			diff = el_id - ptr->element_id;

		if( diff == 0 )
		{
			*found = 1;
			return( ptr );
		}
		else if( diff > 0 )
		{
			if( ptr->rright )
				ptr = ptr->rright;
			else
				return( ptr );
		}
		else if( diff < 0 )
		{
			if( ptr->rleft )
				ptr = ptr->rleft;
			else
				return( ptr );
		}
	}
}

void
Insert_name( root , name )
struct name_tree *root;
char *name;
{
	struct name_tree *ptr;
	struct name_tree *new_ptr;
	int found;
	int diff;

	ptr = Search_names( root , name , &found );

	if( found )
	{
		rt_log( "Insert_name: %s already in name tree\n" , name );
		return;
	}

	new_ptr = (struct name_tree *)rt_malloc( sizeof( struct name_tree ) , "Insert_name: new_ptr" );

	strncpy( new_ptr->name , name , NAMESIZE );
	new_ptr->name[NAMESIZE] = '\0';
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->region_id = (-region_id);
	new_ptr->element_id = 0;

	if( !root )
	{
		root = new_ptr;
		return;
	}

	diff = strncmp( name , ptr->name , NAMESIZE );
	if( diff > 0 )
	{
		if( ptr->nright )
		{
			rt_log( "Insert_name: ptr->nright not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nright = new_ptr;
	}
	else
	{
		if( ptr->nleft )
		{
			rt_log( "Insert_name: ptr->nleft not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nleft = new_ptr;
	}
}

void
Insert_region_name( name , reg_id , el_id )
char *name;
int reg_id;
int el_id;
{
	struct name_tree *nptr_model,*rptr_model;
	struct name_tree *nptr_sect,*rptr_sect;
	struct name_tree *new_ptr;
	int foundn,foundr;
	int diff;

	rptr_model = Search_ident( model_name_root , reg_id , el_id , &foundr );
	nptr_model = Search_names( model_name_root , name , &foundn );

	if( foundn && foundr )
		return;

	if( foundn != foundr )
	{
		rt_log( "Insert_region_name: name %s ident %d element %d\n\tfound name is %d\n\tfound ident is %d\n",
			name, reg_id, el_id, foundn, foundr );
		List_names();
		rt_bomb( "\tCannot insert new node\n" );
	}

	/* Add to tree for this section */
	new_ptr = (struct name_tree *)rt_malloc( sizeof( struct name_tree ) , "Insert_region_name: new_ptr" );
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->region_id = reg_id;
	new_ptr->element_id = el_id;
	strncpy( new_ptr->name , name , NAMESIZE );
	new_ptr->name[NAMESIZE] = '\0';

	if( !sect_name_root )
		sect_name_root = new_ptr;
	else
	{
		rptr_sect = Search_ident( sect_name_root , reg_id , el_id , &foundr );
		nptr_sect = Search_names( sect_name_root , name , &foundn );

		diff = strncmp( name , nptr_sect->name , NAMESIZE );

		if( diff > 0 )
		{
			if( nptr_sect->nright )
			{
				rt_log( "Insert_region_name: nptr_sect->nright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_sect->nright = new_ptr;
		}
		else
		{
			if( nptr_sect->nleft )
			{
				rt_log( "Insert_region_name: nptr_sect->nleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_sect->nleft = new_ptr;
		}


		diff = reg_id - rptr_sect->region_id;

		if( diff == 0 )
			diff = el_id - rptr_sect->element_id;

		if( diff > 0 )
		{
			if( rptr_sect->rright )
			{
				rt_log( "Insert_region_name: rptr_sect->rright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_sect->rright = new_ptr;
		}
		else
		{
			if( rptr_sect->rleft )
			{
				rt_log( "Insert_region_name: rptr_sect->rleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_sect->rleft = new_ptr;
		}
	}

	/* Add to tree for entire model */
	new_ptr = (struct name_tree *)rt_malloc( sizeof( struct name_tree ) , "Insert_region_name: new_ptr" );
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->region_id = reg_id;
	new_ptr->element_id = el_id;
	strncpy( new_ptr->name , name , NAMESIZE );
	new_ptr->name[NAMESIZE] = '\0';

	if( !model_name_root )
		model_name_root = new_ptr;
	else
	{
		diff = strncmp( name , nptr_model->name , NAMESIZE );

		if( diff > 0 )
		{
			if( nptr_model->nright )
			{
				rt_log( "Insert_region_name: nptr_model->nright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_model->nright = new_ptr;
		}
		else
		{
			if( nptr_model->nleft )
			{
				rt_log( "Insert_region_name: nptr_model->nleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_model->nleft = new_ptr;
		}


		diff = reg_id - rptr_model->region_id;

		if( diff == 0 )
			diff = el_id - rptr_model->element_id;

		if( diff > 0 )
		{
			if( rptr_model->rright )
			{
				rt_log( "Insert_region_name: rptr_model->rright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rright = new_ptr;
		}
		else
		{
			if( rptr_model->rleft )
			{
				rt_log( "Insert_region_name: rptr_model->rleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rleft = new_ptr;
		}
	}
}

void
make_comp_group()
{
	struct wmember g_head;
	struct name_tree *ptr;
	char name[NAMESIZE+1];

	RT_LIST_INIT( &g_head.l );

	nmg_tbl( &stack , TBL_RST , (long *)NULL );

	ptr = sect_name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		if( mk_addmember( ptr->name , &g_head , WMOP_UNION ) == (struct wmember *)NULL )
		{
			rt_log( "make_comp_group: Could not add %s to group for ident %d\n" , ptr->name , ptr->region_id );
			break;
		}
		ptr = ptr->nright;
	}

	if( RT_LIST_NON_EMPTY( &g_head.l ) )
	{
		if( name_name[0] )
			strcpy( name , name_name );
		else
		{
			sprintf( name , "comp_%d" , region_id );
			make_unique_name( name );
			Insert_name( model_name_root , name );
		}

		mk_lfcomb( fdout , name , &g_head , 0 );
		add_to_series( name , region_id );
	}
}

void
do_groups()
{
	int group_no;
	struct wmember head_all;

	RT_LIST_INIT( &head_all.l );

	for( group_no=0 ; group_no < 11 ; group_no++ )
	{
		char name[NAMESIZE];

		if( RT_LIST_IS_EMPTY( &group_head[group_no].l ) )
			continue;

		sprintf( name , "%dxxx_series" , group_no );
		mk_lfcomb( fdout , name , &group_head[group_no] , 0 );

		if( mk_addmember( name , &head_all , WMOP_UNION ) == (struct wmember *)NULL )
			rt_log( "do_groups: mk_addmember failed to add %s to group all\n" , name );
	}

	if( RT_LIST_NON_EMPTY( &head_all.l ) )
		mk_lfcomb( fdout , "all" , &head_all , 0 );
}

void
add_to_series( name , reg_id )
char *name;
int reg_id;
{
	int group_no;

	group_no = reg_id / 1000;

	if( group_id < 0 || group_id > 10 )
	{
		rt_log( "add_to_series: region (%s) not added, illegal group number %d, region_id=$d\n" ,
			name , group_id , reg_id );
		return;
	}

	if( mk_addmember( name , &group_head[group_id] , WMOP_UNION ) == (struct wmember *)NULL )
		rt_log( "add_to_series: mk_addmember failed for region %s\n" , name );
}

void
do_name()
{
	int i,j;
	int g_id;
	int c_id;
	int len;
	char comp_name[25];
	char tmp_name[25];

	if( pass )
		return;

	strncpy( field , &line[8] , 8 );
	g_id = atoi( field );

	if( g_id != group_id )
	{
		rt_log( "$NAME card for group %d in section for group %d ignored\n" , g_id , group_id );
		rt_log( "%s\n" , line );
		return;
	}

	strncpy( field , &line[16] , 8 );
	c_id = atoi( field );

	if( c_id != comp_id )
	{
		rt_log( "$NAME card for component %d in section for component %d ignored\n" , c_id , comp_id );
		rt_log( "%s\n" , line );
		return;
	}

	strncpy( comp_name , &line[56] , 24 );

	/* eliminate trailing blanks */
	i = 26;
	while(  --i >= 0 && isspace( comp_name[i] ) )
		comp_name[i] = '\0';

	/* copy comp_name to tmp_name while replacing white space with "_" */
	i = (-1);
	j = (-1);

	/* copy */
	while( comp_name[++i] != '\0' )
	{
		if( isspace( comp_name[i] ) || comp_name[i] == '/' )
		{
			if( j == (-1) || tmp_name[j] != '_' )
				tmp_name[++j] = '_';
		}
		else
			tmp_name[++j] = comp_name[i];
	}
	tmp_name[++j] = '\0';

	len = strlen( tmp_name );
	if( len <= NAMESIZE )
		strncpy( name_name , tmp_name , NAMESIZE );
	else
		strncpy( name_name , &tmp_name[len-NAMESIZE] , NAMESIZE );

	/* reserve this name for group name */
	make_unique_name( name_name );
	Insert_name( model_name_root , name_name );

	name_count = 0;
}

char *
find_region_name( g_id , c_id , el_id , type )
int g_id;
int c_id;
int el_id;
char type;
{
	struct name_tree *ptr;
	int reg_id;
	int found;

	reg_id = g_id * 1000 + c_id;

	ptr = Search_ident( model_name_root , reg_id , el_id , &found );

	if( found )
		return( ptr->name );
	else
		return( (char *)NULL );
}

void
make_unique_name( name )
char *name;
{
	struct name_tree *ptr;
	char append[10];
	int append_len;
	int len;
	int found;

	/* make a unique name from what we got off the $NAME card */

	len = strlen( name );

	ptr = Search_names( model_name_root , name , &found );
	while( found )
	{
		sprintf( append , "_%d" , name_count );
		name_count++;
		append_len = strlen( append );

		if( len + append_len <= NAMESIZE )
			strcat( name , append );
		else
			strcpy( &name[NAMESIZE-append_len] , append );

		ptr = Search_names( model_name_root , name , &found );
	}
}

void
make_region_name( name , g_id , c_id , element_id , type )
char *name;
int g_id;
int c_id;
int element_id;
char type;
{
	int r_id;
	int found;
	char *tmp_name;
	struct name_tree *ptr;

	r_id = g_id * 1000 + c_id;

	tmp_name = find_region_name( g_id , c_id , element_id , type );
	if( tmp_name )
	{
		if( !pass )
			rt_log( "make_region_name: Name for region with g_id=%d, c_id=%d, el_id=%d, and type %c already exists (%s)\n",
				g_id, c_id, element_id, type , tmp_name );
		strncpy( name , tmp_name , NAMESIZE );
		return;
	}

	/* create a new name */
	if( name_name[0] )
		strncpy( name , name_name , NAMESIZE );
	else if( element_id < 0 && type == CLINE )
		sprintf( name , "%d.j.%d.r" , r_id , joint_no++ );
	else
		sprintf( name , "%d.%d.%c.r" , r_id , element_id , type );

	make_unique_name( name );

	Insert_region_name( name , r_id , element_id );
}

void
make_solid_name( name , type , element_id , c_id , g_id , inner )
char *name;
char type;
int element_id;
int c_id;
int g_id;
int inner;
{
	get_solid_name( name , type , element_id , c_id , g_id , inner );

	Insert_name( model_name_root , name );
}

void
get_solid_name( name , type , element_id , c_id , g_id , inner )
char *name;
char type;
int element_id;
int c_id;
int g_id;
int inner;
{
	int reg_id;

	reg_id = g_id * 1000 + c_id;

	sprintf( name , "%d.%d.%c%d" , reg_id , element_id , type , inner );
}

void
do_grid()
{
	int grid_no;
	fastf_t x,y,z;

	if( !pass )	/* not doing geometry yet */
		return;

	strncpy( field , &line[8] , 8 );
	grid_no = atoi( field );

	if( grid_no < 1 )
	{
		rt_log( "ERROR: grid id number = %d\n" , grid_no );
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
		grid_pts = (point_t *)rt_realloc( (char *)grid_pts , grid_size * sizeof( point_t ) , "fast4-g: grid_pts" );
	}

	VSET( grid_pts[grid_no] , x*25.4 , y*25.4 , z*25.4 );
	if( grid_no > max_grid_no )
		max_grid_no = grid_no;
}

void
make_cline_regions()
{
	struct cline *cline_ptr;
	struct nmg_ptbl points;
	struct wmember head;
	char name[NAMESIZE];
	int sph_no;

	RT_LIST_INIT( &head.l );
	nmg_tbl( &points , TBL_INIT , (long *)NULL );

	/* make a list of all the endpoints */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		/* Add endpoints to points list */
		nmg_tbl( &points , TBL_INS_UNIQUE , (long *)cline_ptr->pt1 );
		nmg_tbl( &points , TBL_INS_UNIQUE , (long *)cline_ptr->pt2 );

		
		cline_ptr = cline_ptr->next;
	}

	/* Now build cline objects */
	for( sph_no=0 ; sph_no < NMG_TBL_END( &points ) ; sph_no++ )
	{
		int pt_no;
		int line_no;
		struct nmg_ptbl lines;
		fastf_t sph_radius=0.0;
		fastf_t sph_inner_radius=0.0;

		nmg_tbl( &lines , TBL_INIT , (long *)NULL );

		pt_no = (int)NMG_TBL_GET( &points , sph_no );

		/* get list of clines that touch this point */
		cline_ptr = cline_root;
		while( cline_ptr )
		{
			if( cline_ptr->pt1 == pt_no || cline_ptr->pt2 == pt_no )
			{
				/* get outer radius for sphere */
				if( cline_ptr->radius > sph_radius )
					sph_radius = cline_ptr->radius;

				/* get inner radius for sphere */
				if( cline_ptr->thick > 0.0 &&
				    (cline_ptr->radius - cline_ptr->thick > sph_inner_radius ) )
					sph_inner_radius =  cline_ptr->radius - cline_ptr->thick;

				nmg_tbl( &lines , TBL_INS , (long *)cline_ptr );
			}

			cline_ptr = cline_ptr->next;
		}

		if( NMG_TBL_END( &lines ) > 1 )
		{
			/* make a joint where CLINE's meet */

			/* make sphere solid at cline joint */
			sprintf( name , "%d.%d.j0" , region_id , pt_no );
			Insert_name( model_name_root , name );
			mk_sph( fdout , name , grid_pts[pt_no] , sph_radius );

			/* Union sphere */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				rt_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
				nmg_tbl( &lines , TBL_FREE , (long *)NULL );
				break;
			}

			/* subtract inner sphere if required */
			if( sph_inner_radius > 0.0 && sph_inner_radius < sph_radius )
			{
				sprintf( name , "%d.%d.j1" , region_id , pt_no );
				Insert_name( model_name_root , name );
				mk_sph( fdout , name , grid_pts[pt_no] , sph_inner_radius );

				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
					nmg_tbl( &lines , TBL_FREE , (long *)NULL );
					break;
				}
			}

			/* subtract all outer cylinders that touch this point */
			for( line_no=0 ; line_no < NMG_TBL_END( &lines ) ; line_no++ )
			{
				cline_ptr = (struct cline *)NMG_TBL_GET( &lines , line_no );

				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for line at joint %s\n" , name );
					nmg_tbl( &lines , TBL_FREE , (long *)NULL );
					break;
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* now make the region */
			if( RT_LIST_NON_EMPTY( &head.l ) )
				MK_REGION( fdout , &head , group_id , comp_id , -pt_no , CLINE )
		}

		/* make regions for all CLINE elements that start at this pt_no */
		for( line_no=0 ; line_no < NMG_TBL_END( &lines ) ; line_no++ )
		{
			int i;
			cline_ptr = (struct cline *)NMG_TBL_GET( &lines , line_no ) ;
			if( cline_ptr->pt1 != pt_no )
				continue;

			/* make name for outer cylinder */
			get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );

			/* start region */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				rt_log( "make_cline_regions: mk_addmember failed for outer solid %s\n" , name );
				break;
			}
			cline_ptr->made = 1;

			if( cline_ptr->thick > 0.0 )
			{
				/* subtract inside cylinder */
				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 1 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
					break;
				}
			}

			/* subtract outside cylinders of any other clines that use this grid point */
			for( i=0 ; i < NMG_TBL_END( &lines ) ; i++ )
			{
				struct cline *ptr2;

				ptr2 = (struct cline *)NMG_TBL_GET( &lines , i );

				if( ptr2 != cline_ptr &&
				    ptr2->pt1 == pt_no || ptr2->pt2 == pt_no )
				{
					get_solid_name( name , CLINE , ptr2->element_id , comp_id , group_id , 0 );
					if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					{
						rt_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
						break;
					}
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* make the region */
			MK_REGION( fdout , &head , group_id , comp_id , cline_ptr->element_id , CLINE );
		}
		nmg_tbl( &lines , TBL_FREE , (long *)NULL );
	}

	nmg_tbl( &points , TBL_FREE , (long *)NULL );

	/* free the linked list of cline pointers */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		struct cline *ptr;

		ptr = cline_ptr;
		cline_ptr = cline_ptr->next;

		rt_free( (char *)ptr , "make_cline_regions: cline_ptr" );
	}

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

}

void
do_sphere()
{
	int element_id;
	int center_pt;
	fastf_t thick;
	fastf_t radius;
	fastf_t inner_radius;
	char name[NAMESIZE];
	struct wmember sphere_region;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CSPHERE );
		return;
	}

	strncpy( field , &line[24] , 8 );
	center_pt = atoi( field );

	strncpy( field , &line[56] , 8 );
	thick = atof( field );

	strncpy( field , &line[64] , 8 );
	radius = atof( field );
	if( radius <= 0.0 )
	{
		rt_log( "do_sphere: illegal radius (%f), skipping sphere\n" , radius );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( center_pt < 1 || center_pt > max_grid_no )
	{
		rt_log( "do_sphere: illegal grid number for center point %d, skipping sphere\n" , center_pt );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	RT_LIST_INIT( &sphere_region.l );

	make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 0 );
	mk_sph( fdout , name , grid_pts[center_pt] , radius );

	if( mk_addmember( name ,  &sphere_region , WMOP_UNION ) == (struct wmember *)NULL )
	{
		rt_log( "do_sphere: Error in adding %s to sphere region\n" , name );
		rt_bomb( "do_sphere" );
	}

	inner_radius = radius - thick;
	if( thick > 0.0 && inner_radius <= 0.0 )
	{
		rt_log( "do_sphere: illegal thickness (%f), skipping inner sphere\n" , thick );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 1 );
	mk_sph( fdout , name , grid_pts[center_pt] , inner_radius );

	if( mk_addmember( name , &sphere_region , WMOP_SUBTRACT ) == (struct wmember *)NULL )
	{
		rt_log( "do_sphere: Error in subtracting %s from sphere region\n" , name );
		rt_bomb( "do_sphere" );
	}

	/* subtract any holes for this component */
	Subtract_holes( &sphere_region , comp_id , group_id );

	MK_REGION( fdout , &sphere_region , group_id , comp_id , element_id , CSPHERE )
}

/*	cleanup from previous component and start a new one */
void
do_section( final )
int final;
{

	if( pass )
	{
		make_cline_regions();
		make_comp_group();
		List_names();
		Free_tree();
	}

	if( !final )
	{
		strncpy( field , &line[8] , 8 );
		group_id = atoi( field );

		strncpy( field , &line[16] , 8 );
		comp_id = atoi( field );

		if( comp_id > 999 )
		{
			rt_log( "Illegal component id number %d, changed to 999\n" , comp_id );
			comp_id = 999;
		}

		region_id = group_id * 1000 + comp_id;

		strncpy( field , &line[24] , 8 );
		mode = atoi( field );
		if( mode != 1 && mode != 2 )
		{
			rt_log( "Illegal mode (%d) for group %d component %d, using volume mode\n",
				mode, group_id, comp_id );
			mode = 2;
		}

		name_name[0] = '\0';
	}
}

void
do_vehicle()
{
	if( !pass )
		return;

	strncpy( vehicle , &line[8] , 16 );
	mk_id_units( fdout , vehicle , "in" );
}

void
Add_to_clines( element_id , pt1 , pt2 , thick , radius )
int element_id;
int pt1;
int pt2;
fastf_t thick;
fastf_t radius;
{
	struct cline *cline_ptr;

	cline_ptr = (struct cline *)rt_malloc( sizeof( struct cline ) , "Add_to_clines: cline_ptr" );

	cline_ptr->pt1 = pt1;
	cline_ptr->pt2 = pt2;
	cline_ptr->element_id = element_id;
	cline_ptr->made = 0;
	cline_ptr->thick = thick;
	cline_ptr->radius = radius;
	cline_ptr->next = (struct cline *)NULL;

	if( cline_last_ptr == (struct cline *)NULL )
		cline_root = cline_ptr;
	else
		cline_last_ptr->next = cline_ptr;

	cline_last_ptr = cline_ptr;
}

void
do_cline()
{
	int element_id;
	int pt1,pt2;
	fastf_t thick;
	fastf_t radius;
	vect_t height;
	char name[NAMESIZE];

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CLINE );
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );
	if( pt1 < 1 || pt1 > max_grid_no )
	{
		rt_log( "Illegal grid point (%d) in CLINE, skipping\n", pt1 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );
	if( pt2 < 1 || pt2 > max_grid_no )
	{
		rt_log( "Illegal grid point in CLINE (%d), skipping\n", pt2 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( pt1 == pt2 )
	{
		rt_log( "Ilegal grid points in CLINE ( %d and %d ), skipping\n", pt1 , pt2 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	strncpy( field , &line[56] , 8 );
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	radius = atof( field ) * 25.4;

	VSUB2( height , grid_pts[pt2] , grid_pts[pt1] );

	make_solid_name( name , CLINE , element_id , comp_id , group_id , 0 );
	mk_rcc( fdout , name , grid_pts[pt1] , height , radius );

	if( thick > radius )
	{
		rt_log( "ERROR: CLINE thickness (%f) greater than radius (%f)\n", thick, radius );
		rt_log( "\tnot making inner cylinder\n" );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		thick = 0.0;
	}
	else if( thick > 0.0 )
	{
		make_solid_name( name , CLINE , element_id , comp_id , group_id , 1 );
		mk_rcc( fdout , name , grid_pts[pt1] , height , radius-thick );
	}

	Add_to_clines( element_id , pt1 , pt2 , thick , radius );
}

void
do_ccone1()
{
	int element_id;
	int pt1,pt2;
	fastf_t thick;
	int c1,c2;
	int end1,end2;
	vect_t height;
	fastf_t r1,r2;
	char outer_name[NAMESIZE+1];
	char inner_name[NAMESIZE+1];
	char reg_name[NAMESIZE+1];
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( outer_name , group_id , comp_id , element_id , CCONE1 );
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , c1 );
			rt_bomb( "CCONE1\n" );
		}
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[56] , 8 );
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	r1 = atof( field ) * 25.4;

	strncpy( field , &line[72] , 8 );
	c1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE1\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		rt_log( "WARNING: CCONE1 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
	}

	strncpy( field , &line[8] , 8 );
	r2 = atof( field ) * 25.4;

	strncpy( field , &line[16] , 8 );
	end1 = atoi( field );

	strncpy( field , &line[24] , 8 );
	end2 = atoi( field );

	if( r1 < 0.0 || r2 < 0.0 )
	{
		rt_log( "ERROR: CCONE1 has illegal radii, %f and %f\n" , r1/25.4 , r2/25.4 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		rt_log( "\tCCONE1 solid ignored\n" );
		return;
	}

	if( mode == PLATE_MODE )
	{
		if( thick <= 0.0 )
		{
			rt_log( "ERROR: Plate mode CCONE1 has illegal thickness (%f)\n" , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 solid ignored\n" );
			return;
		}

		if( r1-thick <= SQRT_SMALL_FASTF && r2-thick <= SQRT_SMALL_FASTF )
		{
			rt_log( "ERROR: Plate mode CCONE1 has too large thickness (%f)\n" , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 solid ignored\n" );
			return;
		}
	}

	if( pt1 < 1 || pt1 > max_grid_no || pt2 < 1 || pt2 > max_grid_no || pt1 == pt2 )
	{
		rt_log( "ERROR: CCONE1 has illegal grid points ( %d and %d)\n" , pt1 , pt2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		rt_log( "\tCCONE1 solid ignored\n" );
		return;
	}

	/* BRL_CAD doesn't allow zero radius, so use a very small radius */
	if( r1 < SQRT_SMALL_FASTF )
		r1 = SQRT_SMALL_FASTF;
	if( r2 < SQRT_SMALL_FASTF )
		r2 = SQRT_SMALL_FASTF;

	/* make outside TGC */
	VSUB2( height , grid_pts[pt2] , grid_pts[pt1] );
	make_solid_name( outer_name , CCONE1 , element_id , comp_id , group_id , 0 );
	mk_trc_h( fdout , outer_name , grid_pts[pt1] , height , r1 , r2 );

	RT_LIST_INIT( &r_head.l );
	if( mk_addmember( outer_name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "CCONE1: mk_addmember failed\n" );

	if( mode == PLATE_MODE )
	{
		/* make inside TGC */

		point_t base;
		point_t top;
		vect_t inner_height;
		fastf_t inner_r1,inner_r2;
		fastf_t length;
		vect_t height_dir;

		length = MAGNITUDE( height );
		VSCALE( height_dir , height , 1.0/length );

		if( end1 == END_OPEN )
		{
			inner_r1 = r1 - thick;
			VMOVE( base , grid_pts[pt1] );
		}
		else
		{
			inner_r1 = r1 + thick * (r2 - r1)/length - thick;
			VJOIN1( base , grid_pts[pt1] , thick , height_dir );
		}

		if( inner_r1 < SQRT_SMALL_FASTF )
		{
			fastf_t dist_to_new_base;

			inner_r1 = SQRT_SMALL_FASTF;
			dist_to_new_base = length * ( thick + inner_r1 - r1)/(r2 - r1);

			VJOIN1( base , grid_pts[pt1] , dist_to_new_base , height_dir );
		}

		if( end2 == END_OPEN )
		{
			inner_r2 = r2 - thick;
			VMOVE( top , grid_pts[pt2] );
		}
		else
		{
			inner_r2 = r1 + (length - thick)*(r2 - r1)/length - thick;
			VJOIN1( top , grid_pts[pt2] , -thick , height_dir );
		}

		if( inner_r2 < SQRT_SMALL_FASTF )
		{
			fastf_t dist_to_new_top;

			inner_r2 = SQRT_SMALL_FASTF;
			dist_to_new_top = length * ( thick + inner_r1 - r1)/(r2 - r1);

			VJOIN1( top , grid_pts[pt1] , dist_to_new_top , height );
		}

		VSUB2( inner_height , top , base );
		if( VDOT( inner_height , height ) < 0.0 )
		{
			rt_log( "ERROR: CCONE1 height (%f) too small for thickness (%f)\n" , length/25.4 , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 inner solid ignored\n" );
		}
		else
		{
			/* make inner tgc */

			make_solid_name( inner_name , CCONE1 , element_id , comp_id , group_id , 1 );
			mk_trc_h( fdout , inner_name , base , inner_height , inner_r1 , inner_r2 );

			if( mk_addmember( inner_name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				rt_bomb( "CCONE1: mk_addmember failed\n" );
		}
	}

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE1 )
}

void
do_ccone2()
{
	int element_id;
	int pt1,pt2;
	int c1,c2;
	fastf_t ro1,ro2,ri1,ri2;
	vect_t height;
	char name[NAMESIZE+1];
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CCONE2 );
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , c1 );
			rt_bomb( "CCONE2\n" );
		}
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[64] , 8 );
	ro1 = atof( field ) * 25.4;

	strncpy( field , &line[72] , 8 );
	c1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CCONE2\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE2\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		rt_log( "WARNING: CCONE2 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
	}

	strncpy( field , &line[8] , 8 );
	ro2 = atof( field ) * 25.4;

	strncpy( field , &line[16] , 8 );
	ri1 = atof( field ) * 25.4;

	strncpy( field , &line[24] , 8 );
	ri2 = atof( field ) * 25.4;

	if( pt1 == pt2 )
	{
		rt_log( "ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0 )
	{
		rt_log( "ERROR: CCONE2 has illegal radii %f %f %f %f\n" , ro1, ro2, ri1, ri2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < SQRT_SMALL_FASTF )
		ro1 = SQRT_SMALL_FASTF;

	if( ro2 < SQRT_SMALL_FASTF )
		ro2 = SQRT_SMALL_FASTF;

	if( ri1 < SQRT_SMALL_FASTF )
		ri1 = SQRT_SMALL_FASTF;

	if( ri2 < SQRT_SMALL_FASTF )
		ri2 = SQRT_SMALL_FASTF;

	RT_LIST_INIT( &r_head.l );

	VSUB2( height , grid_pts[pt2] , grid_pts[pt1] );

	make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 0 );
	mk_trc_h( fdout , name , grid_pts[pt1] , height , ro1 , ro2 );

	if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "mk_addmember failed!\n" );

	make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 1 );
	mk_trc_h( fdout , name , grid_pts[pt1] , height , ri1 , ri2 );

	if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
		rt_bomb( "mk_addmember failed!\n" );

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE2 )
}

void
Add_holes( gr , comp , ptr )
int gr;
int comp;
struct hole_list *ptr;
{
	struct holes *hole_ptr;

	if( !hole_root )
	{
		hole_root = (struct holes *)rt_malloc( sizeof( struct holes ) , "Add_holes: hole_root" );
		hole_root->group = gr;
		hole_root->component = comp;
		hole_root->holes = ptr;
		hole_root->next = (struct holes *)NULL;
		return;
	}

	hole_ptr = hole_root;
	while( hole_ptr &&
	    hole_ptr->group != gr &&
	    hole_ptr->component != comp &&
	    hole_ptr->next )
		hole_ptr = hole_ptr->next;

	if( !hole_ptr )
		rt_bomb( "ERROR: Add_holes fell off end of list\n" );

	if( hole_ptr->group == gr && hole_ptr->component == comp )
	{
		struct hole_list *list;

		if( !hole_ptr->holes )
			hole_ptr->holes = ptr;
		else
		{
			list = hole_ptr->holes;
			while( list->next )
				list = list->next;
			list->next = ptr;
		}
	}

	hole_ptr->next = (struct holes *)rt_malloc( sizeof( struct holes ) , "Add_holes: hole_ptr->next" );
	hole_ptr = hole_ptr->next;
	hole_ptr->group = gr;
	hole_ptr->component = comp;
	hole_ptr->holes = ptr;
	hole_ptr->next = (struct holes *)NULL;
}

void
do_hole()
{
	struct hole_list *list_ptr;
	struct hole_list *list_start;
	int group, comp;
	int igrp, icmp;
	int s_len;
	int col;

	if( !pass )
		return;

	s_len = strlen( line );
	if( s_len > 80 )
		s_len = 80;

	strncpy( field , &line[8] , 8 );
	group = atoi( field );

	strncpy( field , &line[16] , 8 );
	comp = atoi( field );

	list_start = (struct hole_list *)NULL;
	list_ptr = (struct hole_list *)NULL;
	col = 24;
	while( col < s_len )
	{
		strncpy( field , &line[col] , 8 );
		igrp = atoi( field );

		col += 8;
		if( col >= s_len )
			break;

		strncpy( field , &line[col] , 8 );
		icmp = atoi( field );

		if( list_ptr )
		{
			list_ptr->next = (struct hole_list *)rt_malloc( sizeof( struct hole_list ) , "do_hole: list_ptr" );
			list_ptr = list_ptr->next;
		}
		else
		{
			list_ptr = (struct hole_list *)rt_malloc( sizeof( struct hole_list ) , "do_hole: list_ptr" );
			list_start = list_ptr;
		}

		list_ptr->group = igrp;
		list_ptr->component = icmp;
		list_ptr->next = (struct hole_list *)NULL;

		col += 8;
	}

	Add_holes( group , comp , list_start );
}

int
getline()
{
	int len;

	if( fgets( line , LINELEN , fdin ) == (char *)NULL )
		return( 0 );

	len = strlen( line );
	if( line[len-1] == '\n' )
		line[len-1] = '\0';

	return( 1 );
}

void
Process_input( pass_number )
int pass_number;
{

	if( pass_number != 0 && pass_number != 1 )
	{
		rt_log( "Process_input: illegal pass number %d\n" , pass_number );
		rt_bomb( "Process_input" );
	}

	pass = pass_number;
	while( getline() )
	{

		if( !strncmp( line , "VEHICLE" , 7 ) )
			do_vehicle();
		else if( !strncmp( line , "HOLE" , 4 ) )
			do_hole();
		else if( !strncmp( line , "WALL" , 4 ) )
			rt_log( "\twall\n" );
		else if( !strncmp( line , "SECTION" , 7 ) )
			do_section( 0 );
		else if( !strncmp( line , "$NAME" , 5 ) )
			do_name();
		else if( !strncmp( line , "$COMMENT" , 8 ) )
			;
		else if( !strncmp( line , "GRID" , 4 ) )
			do_grid();
		else if( !strncmp( line , "CLINE" , 5 ) )
			do_cline();
		else if( !strncmp( line , "CHEX1" , 5 ) )
			rt_log( "\tchex1\n" );
		else if( !strncmp( line , "CHEX2" , 5 ) )
			rt_log( "\tchex2\n" );
		else if( !strncmp( line , "CTRI" , 4 ) )
			rt_log( "\tctri\n" );
		else if( !strncmp( line , "CQUAD" , 5 ) )
			rt_log( "\tcquad\n" );
		else if( !strncmp( line , "CCONE1" , 6 ) )
			do_ccone1();
		else if( !strncmp( line , "CCONE2" , 6 ) )
			do_ccone2();
		else if( !strncmp( line , "CSPHERE" , 7 ) )
			do_sphere();
		else if( !strncmp( line , "ENDDATA" , 7 ) )
		{
			do_section( 1 );
			break;
		}
		else
			rt_log( "ERROR: skipping unrecognized data type\n%s\n" , line );
	}
}

main( argc , argv )
int argc;
char *argv[];
{
	int i;

	if( argc != 3 )
	{
		rt_log( "Usage: %s fastgen4_file brlcad_file.g\n" , argv[0] );
		exit( 1 );
	}

	if( (fdin=fopen( argv[1] , "r" )) == (FILE *)NULL )
	{
		rt_log( "Cannot open FASTGEN4 file (%s)\n" , argv[1] );
		perror( "fast4-g" );
		exit( 1 );
	}

	if( (fdout=fopen( argv[2] , "w" )) == (FILE *)NULL )
	{
		rt_log( "Cannot open file for output (%s)\n" , argv[2] );
		perror( "fast4-g" );
		exit( 1 );
	}

	grid_size = GRID_BLOCK;
	grid_pts = (point_t *)rt_malloc( grid_size * sizeof( point_t ) , "fast4-g: grid_pts" );

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

	sect_name_root = (struct name_tree *)NULL;
	model_name_root = (struct name_tree *)NULL;

	hole_root = (struct holes *)NULL;

	name_name[0] = '\0';
	name_count = 0;

	nmg_tbl( &stack , TBL_INIT , (long *)NULL );

	for( i=0 ; i<11 ; i++ )
		RT_LIST_INIT( &group_head[i].l );

	Process_input( 0 );

	rewind( fdin );

	Process_input( 1 );

	/* make groups */
	do_groups();

	List_holes();
}
