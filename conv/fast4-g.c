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
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <errno.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

#define		NAMESIZE	16	/* from db.h */

#define		LINELEN		128	/* Length of char array for input line */

#define		REGION_LIST_BLOCK	256	/* initial length of array of region ids to process */

static char	line[LINELEN+1];		/* Space for input line */
static FILE	*fdin;			/* Input FASTGEN4 file pointer */
static FILE	*fdout;			/* Output BRL-CAD file pointer */
static FILE	*fd_plot=NULL;		/* file for plot output */
static int	grid_size;		/* Number of points that will fit in current grid_pts array */
static int	max_grid_no=0;		/* Maximum grid number used */
static int	mode=0;			/* Plate mode (1) or volume mode (2) */
static int	group_id=(-1);		/* Group identification number from SECTION card */
static int	comp_id=(-1);		/* Component identification number from SECTION card */
static int	region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static int	joint_no=0;		/* Count of CLINE joints (sph solids created to connect CLINES) */
static char	field[9];		/* Space for storing one field from an input line */
static char	vehicle[17];		/* Title for BRLCAD model from VEHICLE card */
static char	name_name[NAMESIZE+1];	/* Component name built from $NAME card */
static int	name_count;		/* Count of number of times this name_name has been used */
static int	pass;			/* Pass number (0 -> only make names, 1-> do geometry ) */
static int	bot=0;			/* Flag: >0 -> There are BOT's in current component */
static int	warnings=0;		/* Flag: >0 -> Print warning messages */
static int	debug=0;		/* Debug flag */
static int	rt_debug=0;		/* rt_g.debug */
static int	quiet=0;		/* flag to not blather */
static int	comp_count=0;		/* Count of components in FASTGEN4 file */
static int	do_skips=0;		/* flag indicating that not all components will be processed */
static int	*region_list;		/* array of region_ids to be processed */
static int	region_list_len=0;	/* actual length of the malloc'd region_list array */
static int	do_plot=0;		/* flag indicating plot file should be created */
static struct cline    *cline_last_ptr; /* Pointer to last element in linked list of clines */
static struct wmember  group_head[11];	/* Lists of regions for groups */
static struct wmember  hole_head;	/* List of regions used as holes (not solid parts of model) */
static struct bu_ptbl stack;		/* Stack for traversing name_tree */

static int		*faces;		/* one triplet per face indexing three grid points */
static fastf_t		*thickness;	/* thickness of each face */
static char		*facemode;	/* mode for each face */
static int		face_size=0;	/* actual length of above arrays */
static int		face_count=0;	/* number of faces in above arrays */

static int	*int_list;		/* Array of integers */
static int	int_list_count=0;	/* Number of ints in above array */
static int	int_list_length=0;	/* Length of int_list array */
#define		INT_LIST_BLOCK	256	/* Number of int_list array slots to allocate */

static char	*usage="Usage:\n\tfast4-g [-dwq] [-c component_list] [-o plot_file] [-b BU_DEBUG_FLAG] [-x RT_DEBUG_FLAG] fastgen4_bulk_data_file output.g\n\
	d - print debugging info\n\
	q - quiet mode (don't say anyhing except error messages\n\
	w - print warnings about creating default names\n\
	c - process only the listed region ids, may be a list (3001,4082,5347) or a range (2314-3527)\n\
	o - create a 'plot_file' containing a libplot3 plot file of all CTRI and CQUAD elements processed\n\
	b - set LIBBU debug flag\n\
	x - set RT debug flag\n";

RT_EXTERN( fastf_t mat_determinant , (mat_t matrix ) );

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

/* convenient macro for building regions */
#define		MK_REGION( fp , headp , g_id , c_id , e_id , type ) \
			{\
				int r_id; \
				char name[NAMESIZE+1]; \
				r_id = g_id * 1000 + c_id; \
				make_region_name( name , g_id , c_id , e_id , type ); \
				if( debug ) \
					bu_log( "Making region: %s\n", name ); \
				mk_lrcomb( fp , name , headp , 1 ,\
					(char *)NULL, (char *)NULL, (unsigned char *)NULL, r_id, 0, 0, 0, 0 ); \
			}

#define	PUSH( ptr )	bu_ptbl_ins( &stack , (long *)ptr )
#define POP( structure , ptr )	{ if( BU_PTBL_END( &stack ) == 0 ) \
				ptr = (struct structure *)NULL; \
			  else \
			  { \
			  	ptr = (struct structure *)BU_PTBL_GET( &stack , BU_PTBL_END( &stack )-1 ); \
			  	bu_ptbl_rm( &stack , (long *)ptr ); \
			  } \
			}

#define	NHEX_FACES	12
int hex_faces[12][3]={
	{ 0 , 1 , 4 }, /* 1 */
	{ 1 , 5 , 4 }, /* 2 */
	{ 1 , 2 , 5 }, /* 3 */
	{ 2 , 6 , 5 }, /* 4 */
	{ 2 , 3 , 6 }, /* 5 */
	{ 3 , 7 , 6 }, /* 6 */
	{ 3 , 0 , 7 }, /* 7 */
	{ 0 , 4 , 7 }, /* 8 */
	{ 4 , 6 , 7 }, /* 9 */
	{ 4 , 5 , 6 }, /* 10 */
	{ 0 , 1 , 2 }, /* 11 */
	{ 0 , 2 , 3 }  /* 12 */
};

struct cline
{
	int pt1,pt2;
	int element_id;
	int made;
	fastf_t thick;
	fastf_t radius;
	struct cline *next;
} *cline_root;

#define	NAME_TREE_MAGIC	0x55555555
#define CK_TREE_MAGIC( ptr )	\
	{\
		if( !ptr )\
			bu_log( "ERROR: Null name_tree pointer, file=%s, line=%d\n", __FILE__, __LINE__ );\
		else if( ptr->magic != NAME_TREE_MAGIC )\
			bu_log( "ERROR: bad name_tree pointer (x%x), file=%s, line=%d\n", ptr, __FILE__, __LINE__ );\
	}

struct name_tree
{
	long magic;
	int region_id;
	int element_id;		/* > 0  -> normal fastgen4 element id
				 * < 0  -> CLINE element (-pt1)
				 * == 0 -> component name */
	int in_comp_group;	/* > 0 -> region already in a component group */
	char name[NAMESIZE+1];
	struct name_tree *nleft,*nright,*rleft,*rright;
} *name_root;

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
	int type;
	struct hole_list *holes;
	struct holes *next;
} *hole_root;

#define HOLE 1
#define WALL 2

point_t *grid_pts;

int
is_a_hole( id )
int id;
{
	struct holes *hole_ptr;
	struct hole_list *ptr;

	hole_ptr = hole_root;

	while( hole_ptr )
	{
		if( hole_ptr->type == HOLE )
		{
			ptr = hole_ptr->holes;
			while( ptr )
			{
				if( (ptr->group * 1000 + ptr->component) == id )
					return( 1 );
				ptr = ptr->next;
			}
		}
		hole_ptr = hole_ptr->next;
	}
	return( 0 );
}

void
add_to_holes( name, reg_id )
char *name;
int reg_id;
{
	if( mk_addmember( name , &hole_head , WMOP_UNION ) == (struct wmember *)NULL )
		bu_log( "add_to_holes: mk_addmember failed for region %s\n" , name );
}

void
plot_tri( pt1, pt2, pt3 )
int pt1, pt2, pt3;
{
	pdv_3move( fd_plot, grid_pts[pt1] );
	pdv_3cont( fd_plot, grid_pts[pt2] );
	pdv_3cont( fd_plot, grid_pts[pt3] );
	pdv_3cont( fd_plot, grid_pts[pt1] );
}

void
Check_names()
{
	struct name_tree *ptr;

	if( !name_root )
		return;

	bu_ptbl_reset( &stack );

	CK_TREE_MAGIC( name_root )
	/* ident order */
	ptr = name_root;
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

		/* visit node */
		CK_TREE_MAGIC( ptr )
		ptr = ptr->rright;
	}

	/* alpabetical order */
	ptr = name_root;
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

		/* visit node */
		CK_TREE_MAGIC( ptr )
		ptr = ptr->nright;
	}
}

void
insert_int( in )
int in;
{
	int i;

	for( i=0 ; i<int_list_count ; i++ )
	{
		if( int_list[i] == in )
			return;
	}

	if( int_list_count == int_list_length )
	{
		if( int_list_length == 0 )
			int_list = (int *)bu_malloc( INT_LIST_BLOCK*sizeof( int ) , "insert_id: int_list" );
		else
			int_list = (int *)bu_realloc( (char *)int_list , (int_list_length + INT_LIST_BLOCK)*sizeof( int ) , "insert_id: int_list" );
		int_list_length += INT_LIST_BLOCK;
	}

	int_list[int_list_count] = in;
	int_list_count++;

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in insert_int\n" );
}

void
Subtract_holes( head , comp_id , group_id )
struct wmember *head;
int comp_id;
int group_id;
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	if( debug )
		bu_log( "Subtract_holes( comp_id=%d, group_id=%d )\n" , comp_id , group_id );

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
				ptr = name_root;
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
					bu_log( "ERROR: Can't find holes to subtract for group %d, component %d\n" , group_id , comp_id );
					return;
				}

				bu_ptbl_reset( &stack );

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
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in subtract_holes\n" );
}

void
List_holes()
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	hole_ptr = hole_root;

	while( hole_ptr )
	{
		bu_log( "Holes for Group %d, Component %d:\n" , hole_ptr->group, hole_ptr->component );
		list_ptr = hole_ptr->holes;
		while( list_ptr )
		{
			bu_log( "\tgroup %d component %d\n" , list_ptr->group, list_ptr->component );
			list_ptr = list_ptr->next;
		}
		hole_ptr = hole_ptr->next;
	}
}

void
List_names()
{
	struct name_tree *ptr;

	bu_ptbl_reset( &stack );

	bu_log( "\nNames in ident order:\n" );
	ptr = name_root;
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

		bu_log( "%s %d %d\n" , ptr->name , ptr->region_id , ptr->element_id );
		ptr = ptr->rright;
	}

	bu_log( "\tAlphabetical list of names:\n" );
	ptr = name_root;
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

		bu_log( "%s\n" , ptr->name );
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

char *
find_nmg_region_name( g_id, c_id )
int g_id;
int c_id;
{
	int reg_id;
	struct name_tree *ptr;

	reg_id = g_id*1000 + c_id;

	ptr = name_root;
	if( !ptr )
		return( (char *)NULL );

	while( 1 )
	{
		int diff;

		diff = reg_id - ptr->region_id;
		if( diff == 0 )
		{
			int len;

			len = strlen( ptr->name );
			if( !strcmp( &ptr->name[len-4] , ".n.r" ) )
				return( ptr->name );
			else
			{
				if( ptr->rright )
					ptr = ptr->rright;
				else
					return( (char *)NULL );
			}
		}
		else if( diff > 0 )
		{
			if( ptr->rright )
				ptr = ptr->rright;
			else
				return( (char *)NULL );
		}
		else if( diff < 0 )
		{
			if( ptr->rleft )
				ptr = ptr->rleft;
			else
				return( (char *)NULL );
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
Delete_name( root , name )
struct name_tree **root;
char *name;
{
	struct name_tree *ptr,*parent,*ptr2;
	int r_id, e_id;
	int found;
	int diff;

	/* first delete from name portion of tree */
	ptr = *root;
	parent = (struct name_tree *)NULL;
	found = 0;

	while( 1 )
	{
		diff = strcmp( name , ptr->name );
		if( diff == 0 )
		{
			found = 1;
			break;
		}
		else if( diff > 0 )
		{
			if( ptr->nright )
			{
				parent = ptr;
				ptr = ptr->nright;
			}
			else
				break;
		}
		else if( diff < 0 )
		{
			if( ptr->nleft )
			{
				parent = ptr;
				ptr = ptr->nleft;
			}
			else
				break;
		}
	}

	if( !found )
		return;

	r_id = ptr->region_id;
	e_id = ptr->element_id;

	if( parent == (struct name_tree *)NULL )
	{
		if( ptr->nright )
		{
			*root = ptr->nright;
			ptr2 = *root;
			while( ptr2->nleft )
				ptr2 = ptr2->nleft;
			ptr2->nleft = ptr->nleft;

			ptr2 = *root;
			while( ptr2->rleft )
				ptr2 = ptr2->rleft;
			ptr2->rleft = ptr->rleft;
		}
		else if( ptr->nleft )
		{
			*root = ptr->nleft;
			ptr2 = *root;
			while( ptr2->nright )
				ptr2 = ptr2->nright;
			ptr2->nright = ptr->nright;
			ptr2 = *root;
			while( ptr2->rright )
				ptr2 = ptr2->rright;
			ptr2->rright = ptr->rright;
		}
		else
		{
			/* This was the only name in the tree */
			*root = (struct name_tree *)NULL;
		}
		bu_free( (char *)ptr , "Delete_name: ptr" );
		if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
			bu_log( "ERROR: bu_mem_barriercheck failed in Delete_name\n" );
		return;
	}
	else
	{
		if( parent->nright == ptr )
		{
			if( ptr->nleft )
			{
				parent->nright = ptr->nleft;
				ptr2 = ptr->nleft;
				while( ptr2->nright )
					ptr2 = ptr2->nright;
				ptr2->nright = ptr->nright;
			}
			else
				parent->nright = ptr->nright;
		}
		else if( parent->nleft == ptr )
		{
			if( ptr->nright )
			{
				parent->nleft = ptr->nright;
				ptr2 = ptr->nright;
				while( ptr2->nleft )
					ptr2 = ptr2->nleft;
				ptr2->nleft = ptr->nleft;
			}
			else
				parent->nleft = ptr->nleft;
		}
	}


	/* now delete from ident prtion of tree */
	ptr = *root;
	parent = (struct name_tree *)NULL;
	found = 0;

	while( 1 )
	{
		diff = r_id - ptr->region_id;
		if( diff == 0 )
			diff = e_id - ptr->element_id;

		if( diff == 0 )
		{
			found = 1;
			break;
		}
		else if( diff > 0 )
		{
			if( ptr->rright )
			{
				parent = ptr;
				ptr = ptr->rright;
			}
			else
				break;
		}
		else if( diff < 0 )
		{
			if( ptr->rleft )
			{
				parent = ptr;
				ptr = ptr->rleft;
			}
			else
				break;
		}
	}

	if( !found )
	{
		bu_log( "name (%s) deleted from name tree, but not found in ident tree!!\n" , name );
		rt_bomb( "Delete_name\n" );
	}

	if( !parent )
	{
		bu_log( "name (%s) is root of ident tree, but not name tree!!\n" , name );
		rt_bomb( "Delete_name\n" );
	}


	if( parent->rright == ptr )
	{
		if( ptr->rleft )
		{
			parent->rright = ptr->rleft;
			ptr2 = ptr->rleft;
			while( ptr2->rright )
				ptr2 = ptr2->rright;
			ptr2->rright = ptr->rright;
		}
		else
			parent->rright = ptr->rright;
	}
	else if( parent->rleft == ptr )
	{
		if( ptr->rright )
		{
			parent->rleft = ptr->rright;
			ptr2 = ptr->rright;
			while( ptr2->rleft )
				ptr2 = ptr2->rleft;
			ptr2->rleft = ptr->rleft;
		}
		else
			parent->rleft = ptr->rleft;
	}
	bu_free( (char *)ptr , "Delete_name: ptr" );
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in Delete_name\n" );
	Check_names();
}

void
Insert_name( root , name )
struct name_tree **root;
char *name;
{
	struct name_tree *ptr;
	struct name_tree *new_ptr;
	int found;
	int diff;

	ptr = Search_names( *root , name , &found );

	if( found )
	{
		bu_log( "Insert_name: %s already in name tree\n" , name );
		return;
	}

	new_ptr = (struct name_tree *)bu_malloc( sizeof( struct name_tree ) , "Insert_name: new_ptr" );

	strncpy( new_ptr->name , name , NAMESIZE+1 );
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->region_id = (-region_id);
	new_ptr->in_comp_group = 0;
	new_ptr->element_id = 0;
	new_ptr->magic = NAME_TREE_MAGIC;

	if( !*root )
	{
		*root = new_ptr;
		return;
	}

	diff = strncmp( name , ptr->name , NAMESIZE );
	if( diff > 0 )
	{
		if( ptr->nright )
		{
			bu_log( "Insert_name: ptr->nright not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nright = new_ptr;
	}
	else
	{
		if( ptr->nleft )
		{
			bu_log( "Insert_name: ptr->nleft not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nleft = new_ptr;
	}
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in Insert_name\n" );
}

void
Insert_region_name( name , reg_id , el_id )
char *name;
int reg_id;
int el_id;
{
	struct name_tree *nptr_model,*rptr_model;
	struct name_tree *new_ptr;
	int foundn,foundr;
	int diff;

	if( debug )
		bu_log( "Insert_region_name( name=%s, reg_id=%d, el_id=%d\n" , name, reg_id, el_id );

	rptr_model = Search_ident( name_root , reg_id , el_id , &foundr );
	nptr_model = Search_names( name_root , name , &foundn );

	if( foundn && foundr )
		return;

	if( foundn != foundr )
	{
		bu_log( "Insert_region_name: name %s ident %d element %d\n\tfound name is %d\n\tfound ident is %d\n",
			name, reg_id, el_id, foundn, foundr );
		List_names();
		rt_bomb( "\tCannot insert new node\n" );
	}

	/* Add to tree for entire model */
	new_ptr = (struct name_tree *)bu_malloc( sizeof( struct name_tree ) , "Insert_region_name: new_ptr" );
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->region_id = reg_id;
	new_ptr->element_id = el_id;
	new_ptr->in_comp_group = 0;
	strncpy( new_ptr->name , name , NAMESIZE+1 );
	new_ptr->magic = NAME_TREE_MAGIC;

	if( !name_root )
		name_root = new_ptr;
	else
	{
		diff = strncmp( name , nptr_model->name , NAMESIZE );

		if( diff > 0 )
		{
			if( nptr_model->nright )
			{
				bu_log( "Insert_region_name: nptr_model->nright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_model->nright = new_ptr;
		}
		else
		{
			if( nptr_model->nleft )
			{
				bu_log( "Insert_region_name: nptr_model->nleft not null\n" );
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
				bu_log( "Insert_region_name: rptr_model->rright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rright = new_ptr;
		}
		else
		{
			if( rptr_model->rleft )
			{
				bu_log( "Insert_region_name: rptr_model->rleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rleft = new_ptr;
		}
	}
	Check_names();
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in Insert_region_name\n" );
}

char *
find_region_name( g_id , c_id , el_id )
int g_id;
int c_id;
int el_id;
{
	struct name_tree *ptr;
	int reg_id;
	int found;

	reg_id = g_id * 1000 + c_id;

	if( debug )
		bu_log( "find_region_name( g_id=%d, c_id=%d, el_id=%d ), reg_id=%d\n" , g_id, c_id, el_id, reg_id );

	ptr = Search_ident( name_root , reg_id , el_id , &found );

	if( found )
		return( ptr->name );
	else
		return( (char *)NULL );
}

void
make_unique_name( name )
char *name;
{
	char append[10];
	int append_len;
	int len;
	int found;

	/* make a unique name from what we got off the $NAME card */

	len = strlen( name );

	(void)Search_names( name_root , name , &found );
	while( found )
	{
		sprintf( append , "_%d" , name_count );
		name_count++;
		append_len = strlen( append );

		if( len + append_len < NAMESIZE )
			strcat( name , append );
		else
		{
			strcpy( &name[NAMESIZE-append_len] , append );
			name[NAMESIZE] = '\0';
		}

		(void)Search_names( name_root , name , &found );
	}
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in make_unique_name\n" );
}

void
add_to_series( name , reg_id )
char *name;
int reg_id;
{
	if( group_id < 0 || group_id > 10 )
	{
		bu_log( "add_to_series: region (%s) not added, illegal group number %d, region_id=$d\n" ,
			name , group_id , reg_id );
		return;
	}

	if( mk_addmember( name , &group_head[group_id] , WMOP_UNION ) == (struct wmember *)NULL )
		bu_log( "add_to_series: mk_addmember failed for region %s\n" , name );
}

void
make_comp_group()
{
	struct wmember g_head;
	struct name_tree *ptr;
	char name[NAMESIZE+1];

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in make_comp_group\n" );

	BU_LIST_INIT( &g_head.l );

	bu_ptbl_reset( &stack );

	ptr = name_root;
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

		if(ptr->region_id == region_id && ptr->element_id && !ptr->in_comp_group )
		{
			if( mk_addmember( ptr->name , &g_head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				bu_log( "make_comp_group: Could not add %s to group for ident %d\n" , ptr->name , ptr->region_id );
				break;
			}
			ptr->in_comp_group = 1;
		}
		ptr = ptr->nright;
	}

	if( BU_LIST_NON_EMPTY( &g_head.l ) )
	{
		char *tmp_name;

		if( (tmp_name=find_region_name( group_id , comp_id , 0 )) )
			strcpy( name , tmp_name );
		else
		{
			sprintf( name , "comp_%d" , region_id );
			make_unique_name( name );
			if( warnings )
				bu_log( "Creating default name (%s) for group %d component %d\n",
						name , group_id , comp_id );
			Insert_name( &name_root , name );
		}

		mk_lfcomb( fdout , name , &g_head , 0 );
		if( !is_a_hole( region_id ) )
			add_to_series( name , region_id );
		else
			add_to_holes( name, region_id );
	}
}

void
do_groups()
{
	int group_no;
	struct wmember head_all;

	if( debug )
		bu_log( "do_groups\n" );

	BU_LIST_INIT( &head_all.l );

	for( group_no=0 ; group_no < 11 ; group_no++ )
	{
		char name[NAMESIZE+1];

		if( BU_LIST_IS_EMPTY( &group_head[group_no].l ) )
			continue;

		sprintf( name , "%dxxx_series" , group_no );
		mk_lfcomb( fdout , name , &group_head[group_no] , 0 );

		if( mk_addmember( name , &head_all , WMOP_UNION ) == (struct wmember *)NULL )
			bu_log( "do_groups: mk_addmember failed to add %s to group all\n" , name );
	}

	if( BU_LIST_NON_EMPTY( &head_all.l ) )
		mk_lfcomb( fdout , "all" , &head_all , 0 );

	if( BU_LIST_NON_EMPTY( &hole_head.l ) )
		mk_lfcomb( fdout , "holes" , &hole_head , 0 );
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

	if( debug )
		bu_log( "do_name: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	g_id = atoi( field );

	if( g_id != group_id )
	{
		bu_log( "$NAME card for group %d in section for group %d ignored\n" , g_id , group_id );
		bu_log( "%s\n" , line );
		return;
	}

	strncpy( field , &line[16] , 8 );
	c_id = atoi( field );

	if( c_id != comp_id )
	{
		bu_log( "$NAME card for component %d in section for component %d ignored\n" , c_id , comp_id );
		bu_log( "%s\n" , line );
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
	name_name[NAMESIZE] = '\0';

	/* reserve this name for group name */
	make_unique_name( name_name );
	Insert_region_name( name_name , region_id , 0 );

	name_count = 0;
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in do_name\n" );
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
	char *tmp_name;

	r_id = g_id * 1000 + c_id;

	if( debug )
		bu_log( "make_region_name( g_id=%d, c_id=%d, element_id=%d, type=%c )\n" , g_id, c_id, element_id, type );

	tmp_name = find_region_name( g_id , c_id , element_id );
	if( tmp_name )
	{
		strncpy( name , tmp_name , NAMESIZE+1 );
		return;
	}

	/* create a new name */
	if( name_name[0] )
		strncpy( name , name_name , NAMESIZE+1 );
	else if( element_id < 0 && type == CLINE )
		sprintf( name , "%d.j.%d.r" , r_id , joint_no++ );
	else
		sprintf( name , "%d.%d.%c.r" , r_id , element_id , type );

	make_unique_name( name );

	Insert_region_name( name , r_id , element_id );
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
make_solid_name( name , type , element_id , c_id , g_id , inner )
char *name;
char type;
int element_id;
int c_id;
int g_id;
int inner;
{
	get_solid_name( name , type , element_id , c_id , g_id , inner );

	Insert_name( &name_root , name );
}

void
do_grid()
{
	int grid_no;
	fastf_t x,y,z;

	if( !pass )	/* not doing geometry yet */
		return;

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
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
	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed at end of do_grid\n" );
}

void
make_cline_regions()
{
	struct cline *cline_ptr;
	struct wmember head;
	char name[NAMESIZE+1];
	int sph_no;

	if( debug )
		bu_log( "make_cline_regions\n" );

	BU_LIST_INIT( &head.l );

	/* make a list of all the endpoints */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		/* Add endpoints to points list */
		insert_int( cline_ptr->pt1 );
		insert_int( cline_ptr->pt2 );
		
		cline_ptr = cline_ptr->next;
	}

	/* Now build cline objects */
	for( sph_no=0 ; sph_no < int_list_count ; sph_no++ )
	{
		int pt_no;
		int line_no;
		struct bu_ptbl lines;
		fastf_t sph_radius=0.0;
		fastf_t sph_inner_radius=0.0;

		bu_ptbl_init( &lines , 64, " &lines ");

		pt_no = int_list[sph_no];

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

				bu_ptbl_ins( &lines , (long *)cline_ptr );
			}

			cline_ptr = cline_ptr->next;
		}

		if( BU_PTBL_END( &lines ) > 1 )
		{
			/* make a joint where CLINE's meet */

			/* make sphere solid at cline joint */
			sprintf( name , "%d.%d.j0" , region_id , pt_no );
			Insert_name( &name_root , name );
			mk_sph( fdout , name , grid_pts[pt_no] , sph_radius );

			/* Union sphere */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				bu_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
				bu_ptbl_free( &lines );
				break;
			}

			/* subtract inner sphere if required */
			if( sph_inner_radius > 0.0 && sph_inner_radius < sph_radius )
			{
				sprintf( name , "%d.%d.j1" , region_id , pt_no );
				Insert_name( &name_root , name );
				mk_sph( fdout , name , grid_pts[pt_no] , sph_inner_radius );

				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					bu_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
					bu_ptbl_free( &lines );
					break;
				}
			}

			/* subtract all outer cylinders that touch this point */
			for( line_no=0 ; line_no < BU_PTBL_END( &lines ) ; line_no++ )
			{
				cline_ptr = (struct cline *)BU_PTBL_GET( &lines , line_no );

				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					bu_log( "make_cline_regions: mk_addmember failed for line at joint %s\n" , name );
					bu_ptbl_free( &lines );
					break;
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* now make the region */
			if( BU_LIST_NON_EMPTY( &head.l ) )
				MK_REGION( fdout , &head , group_id , comp_id , -pt_no , CLINE )
		}
		else
		{
			char *bad_name;

			/* no need for a region here
			 * but we need to remove this region from the name tree
			 */
			bad_name = find_region_name( group_id , comp_id , -pt_no );
			if( bad_name )
				Delete_name( &name_root , bad_name );
		}

		/* make regions for all CLINE elements that start at this pt_no */
		for( line_no=0 ; line_no < BU_PTBL_END( &lines ) ; line_no++ )
		{
			int i;
			cline_ptr = (struct cline *)BU_PTBL_GET( &lines , line_no ) ;
			if( cline_ptr->pt1 != pt_no )
				continue;

			/* make name for outer cylinder */
			get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );

			/* start region */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				bu_log( "make_cline_regions: mk_addmember failed for outer solid %s\n" , name );
				break;
			}
			cline_ptr->made = 1;

			if( cline_ptr->thick > 0.0 )
			{
				/* subtract inside cylinder */
				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 1 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					bu_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
					break;
				}
			}

			/* subtract outside cylinders of any other clines that use this grid point */
			for( i=0 ; i < BU_PTBL_END( &lines ) ; i++ )
			{
				struct cline *ptr2;

				ptr2 = (struct cline *)BU_PTBL_GET( &lines , i );

				if( ptr2 != cline_ptr &&
				    ptr2->pt1 == pt_no || ptr2->pt2 == pt_no )
				{
					get_solid_name( name , CLINE , ptr2->element_id , comp_id , group_id , 0 );
					if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					{
						bu_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
						break;
					}
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* make the region */
			MK_REGION( fdout , &head , group_id , comp_id , cline_ptr->element_id , CLINE );
		}
		bu_ptbl_free( &lines );
	}

	int_list_count = 0;

	/* free the linked list of cline pointers */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		struct cline *ptr;

		ptr = cline_ptr;
		cline_ptr = cline_ptr->next;

		bu_free( (char *)ptr , "make_cline_regions: cline_ptr" );
	}

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in make_cline_regions\n" );

}

void
do_sphere()
{
	int element_id;
	int center_pt;
	fastf_t thick;
	fastf_t radius;
	fastf_t inner_radius;
	char name[NAMESIZE+1];
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
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	radius = atof( field ) * 25.4;
	if( radius <= 0.0 )
	{
		bu_log( "do_sphere: illegal radius (%f), skipping sphere\n" , radius );
		bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( center_pt < 1 || center_pt > max_grid_no )
	{
		bu_log( "do_sphere: illegal grid number for center point %d, skipping sphere\n" , center_pt );
		bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	BU_LIST_INIT( &sphere_region.l );

	make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 0 );
	mk_sph( fdout , name , grid_pts[center_pt] , radius );

	if( mk_addmember( name ,  &sphere_region , WMOP_UNION ) == (struct wmember *)NULL )
	{
		bu_log( "do_sphere: Error in adding %s to sphere region\n" , name );
		rt_bomb( "do_sphere" );
	}

	if( mode == PLATE_MODE )
	{
		inner_radius = radius - thick;
		if( thick > 0.0 && inner_radius <= 0.0 )
		{
			bu_log( "do_sphere: illegal thickness (%f), skipping inner sphere\n" , thick );
			bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 1 );
		mk_sph( fdout , name , grid_pts[center_pt] , inner_radius );

		if( mk_addmember( name , &sphere_region , WMOP_SUBTRACT ) == (struct wmember *)NULL )
		{
			bu_log( "do_sphere: Error in subtracting %s from sphere region\n" , name );
			rt_bomb( "do_sphere" );
		}
	}

	/* subtract any holes for this component */
	Subtract_holes( &sphere_region , comp_id , group_id );

	MK_REGION( fdout , &sphere_region , group_id , comp_id , element_id , CSPHERE )
}

void
make_nmg_name( name , r_id )
char *name;
int r_id;
{
	sprintf( name , "nmg.%d" , r_id );

	make_unique_name( name );
}

void
do_vehicle()
{
	if( pass )
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

	cline_ptr = (struct cline *)bu_malloc( sizeof( struct cline ) , "Add_to_clines: cline_ptr" );

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

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in Add_to_clines\n" );
}

void
do_cline()
{
	int element_id;
	int pt1,pt2;
	fastf_t thick;
	fastf_t radius;
	vect_t height;
	char name[NAMESIZE+1];
	char name2[NAMESIZE+1];
	struct wmember r_head;

	if( debug )
		bu_log( "do_cline: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );
	if( pass && (pt1 < 1 || pt1 > max_grid_no) )
	{
		bu_log( "Illegal grid point (%d) in CLINE, skipping\n", pt1 );
		bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );
	if( pass && (pt2 < 1 || pt2 > max_grid_no) )
	{
		bu_log( "Illegal grid point in CLINE (%d), skipping\n", pt2 );
		bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( pt1 == pt2 )
	{
		bu_log( "Ilegal grid points in CLINE ( %d and %d ), skipping\n", pt1 , pt2 );
		bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CLINE );
		return;
	}

	strncpy( field , &line[56] , 8 );
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	radius = atof( field ) * 25.4;

	VSUB2( height , grid_pts[pt2] , grid_pts[pt1] );

	make_solid_name( name , CLINE , element_id , comp_id , group_id , 0 );
	mk_cline( fdout , name , grid_pts[pt1] , height , radius, thick );

	BU_LIST_INIT( &r_head.l );
	if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		bu_bomb( "CLINE: mk_addmember failed\n" );

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CLINE )

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
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( outer_name , group_id , comp_id , element_id , CCONE1 );
		if( !getline() )
		{
			bu_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
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
		bu_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE1\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		bu_log( "WARNING: CCONE1 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
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
		bu_log( "ERROR: CCONE1 has illegal radii, %f and %f\n" , r1/25.4 , r2/25.4 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		bu_log( "\tCCONE1 solid ignored\n" );
		return;
	}

	if( mode == PLATE_MODE )
	{
		if( thick <= 0.0 )
		{
			bu_log( "ERROR: Plate mode CCONE1 has illegal thickness (%f)\n" , thick/25.4 );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			bu_log( "\tCCONE1 solid ignored\n" );
			return;
		}

		if( r1-thick <= SQRT_SMALL_FASTF && r2-thick <= SQRT_SMALL_FASTF )
		{
			bu_log( "ERROR: Plate mode CCONE1 has too large thickness (%f)\n" , thick/25.4 );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			bu_log( "\tCCONE1 solid ignored\n" );
			return;
		}
	}

	if( pt1 < 1 || pt1 > max_grid_no || pt2 < 1 || pt2 > max_grid_no || pt1 == pt2 )
	{
		bu_log( "ERROR: CCONE1 has illegal grid points ( %d and %d)\n" , pt1 , pt2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		bu_log( "\tCCONE1 solid ignored\n" );
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

	BU_LIST_INIT( &r_head.l );
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
		fastf_t sin_ang;
		fastf_t slant_len;
		fastf_t r1a,r2a;
		vect_t height_dir;

		length = MAGNITUDE( height );
		VSCALE( height_dir , height , 1.0/length );
		slant_len = sqrt( length*length + (r2 - r1)*(r2 - r1) );

		sin_ang = length/slant_len;

		if( end1 == END_OPEN )
		{
			r1a = r1;
			inner_r1 = r1 - thick/sin_ang;
			VMOVE( base , grid_pts[pt1] );
		}
		else
		{
			r1a = r1 + (r2 - r1)*thick/length;
			inner_r1 = r1a - thick/sin_ang;
			VJOIN1( base , grid_pts[pt1] , thick , height_dir );
		}

		if( inner_r1 < 0.0 )
		{
			fastf_t dist_to_new_base;

			dist_to_new_base = inner_r1 * length/(r1 - r2 );
			inner_r1 = SQRT_SMALL_FASTF;
			VJOIN1( base , base , dist_to_new_base , height_dir );
		}
		else if( inner_r1 < SQRT_SMALL_FASTF )
			inner_r1 = SQRT_SMALL_FASTF;

		if( end2 == END_OPEN )
		{
			r2a = r2;
			inner_r2 = r2 - thick/sin_ang;
			VMOVE( top , grid_pts[pt2] );
		}
		else
		{
			r2a = r2 + (r1 - r2)*thick/length;
			inner_r2 = r2a - thick/sin_ang;
			VJOIN1( top , grid_pts[pt2] , -thick , height_dir );
		}

		if( inner_r2 < 0.0 )
		{
			fastf_t dist_to_new_top;

			dist_to_new_top = inner_r2 * length/(r2 - r1 );
			inner_r2 = SQRT_SMALL_FASTF;
			VJOIN1( top , top , -dist_to_new_top , height_dir );
		}
		else if( inner_r2 < SQRT_SMALL_FASTF )
			inner_r2 = SQRT_SMALL_FASTF;

		VSUB2( inner_height , top , base );
		if( VDOT( inner_height , height ) < 0.0 )
		{
			bu_log( "ERROR: CCONE1 height (%f) too small for thickness (%f)\n" , length/25.4 , thick/25.4 );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			bu_log( "\tCCONE1 inner solid ignored\n" );
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
			bu_log( "Unexpected EOF while reading continuation card for CCONE2\n" );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
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
		bu_log( "Unexpected EOF while reading continuation card for CCONE2\n" );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE2\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		bu_log( "WARNING: CCONE2 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
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
		bu_log( "ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0 )
	{
		bu_log( "ERROR: CCONE2 has illegal radii %f %f %f %f\n" , ro1, ro2, ri1, ri2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < SQRT_SMALL_FASTF )
		ro1 = SQRT_SMALL_FASTF;

	if( ro2 < SQRT_SMALL_FASTF )
		ro2 = SQRT_SMALL_FASTF;

	BU_LIST_INIT( &r_head.l );

	VSUB2( height , grid_pts[pt2] , grid_pts[pt1] );

	make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 0 );
	mk_trc_h( fdout , name , grid_pts[pt1] , height , ro1 , ro2 );

	if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "mk_addmember failed!\n" );

	if( ri1 > 0.0 || ri2 > 0.0 )
	{
		if( ri1 < SQRT_SMALL_FASTF )
			ri1 = SQRT_SMALL_FASTF;

		if( ri2 < SQRT_SMALL_FASTF )
			ri2 = SQRT_SMALL_FASTF;

		make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 1 );
		mk_trc_h( fdout , name , grid_pts[pt1] , height , ri1 , ri2 );

		if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
			rt_bomb( "mk_addmember failed!\n" );
	}

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE2 )
}

void
do_ccone3()
{
	int element_id;
	int pt1, pt2, pt3, pt4, c1, c2, i;
	char name[NAMESIZE+1];
	fastf_t ro[4], ri[4], dot2, dot3, len03, len01, len12, len23;
	vect_t diff, diff2, diff3, diff4;
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CCONE3 );
		if( !getline() )
		{
			bu_log( "Unexpected EOF while reading continuation card for CCONE3\n" );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_bomb( "CCONE2\n" );
		}
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[40] , 8 );
	pt3 = atoi( field );

	strncpy( field , &line[48] , 8 );
	pt4 = atoi( field );

	strncpy( field, &line[72], 8 );

	if( !getline() )
	{
		bu_log( "Unexpected EOF while reading continuation card for CCONE3\n" );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE3\n" );
	}

	if( strncmp( field, line, 8 ) )
	{
		bu_log( "WARNING: CCONE3 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
	}

	for( i=0 ; i<4 ; i++ )
	{
		strncpy( field, &line[8*(i+1)], 8 );
		ro[i] = atof( field ) * 25.4;
		if( ro[i] < 0.0 )
		{
			bu_log( "ERROR: CCONE3 has illegal radius %f\n", ro[i] );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			return;
		}
		if( !strcmp( field, "        " ) )
			ro[i] = -1.0;
	}

	for( i=0 ; i<4 ; i++ )
	{
		strncpy( field, &line[32 + 8*(i+1)], 8 );
		ri[i] = atof( field ) * 25.4;
		if( ri[i] < 0.0 )
		{
			bu_log( "ERROR: CCONE3 has illegal radius %f\n", ri[i] );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			return;
		}
		if( !strcmp( field, "        " ) )
			ri[i] = -1.0;
	}

	VSUB2( diff4, grid_pts[pt4], grid_pts[pt1] );
	VSUB2( diff3, grid_pts[pt3], grid_pts[pt1] );
	VSUB2( diff2, grid_pts[pt2], grid_pts[pt1] );
	dot3 = VDOT( diff4, diff3 );
	dot2 = VDOT( diff4, diff2 );

	len03 = MAGNITUDE( diff4 );
	len01 = MAGNITUDE( diff2 );
	len12 = MAGNITUDE( diff3 ) - len01;
	len23 = len03 - len01 - len12;

	for( i=0 ; i<4 ; i+=3 )
	{
		if( ro[i] ==-1.0 )
		{
			if( ri[i] == -1.0 )
			{
				bu_log( "ERROR: both inner and outer radii at g%d of a CCONE3 are undefined\n", i+1 );
				bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
					group_id, comp_id, element_id );
				return;
			}
			else
				ro[i] = ri[i];
			
		}
		else if( ri[i] == -1.0 )
			ri[i] = ro[i];
	}

	if( ro[1] == -1.0 )
	{
		if( ro[2] != -1.0 )
			ro[1] = ro[0] + (ro[2] - ro[0]) * len01 / (len01 + len12);
		else
			ro[1] = ro[0] + (ro[3] - ro[0]) * len01 / len03;
	}
	if( ro[2] == -1.0 )
	{
		if( ro[1] != -1.0 )
			ro[2] = ro[1] + (ro[3] - ro[1]) * len12 / (len12 + len23);
		else
			ro[2] = ro[0] + (ro[3] - ro[0]) * (len01 + len12) / len03;
	}
	if( ri[1] == -1.0 )
	{
		if( ri[2] != -1.0 )
			ri[1] = ri[0] + (ri[2] - ri[0]) * len01 / (len01 + len12);
		else
			ri[1] = ri[0] + (ri[3] - ri[0]) * len01 / len03;
	}
	if( ri[2] == -1.0 )
	{
		if( ri[1] != -1.0 )
			ri[2] = ri[1] + (ri[3] - ri[1]) * len12 / (len12 + len23);
		else
			ri[2] = ri[0] + (ri[3] - ri[0]) * (len01 + len12) / len03;
	}

	for( i=0 ; i<4 ; i++ )
	{
		if( ro[i] < SQRT_SMALL_FASTF )
			ro[i] = SQRT_SMALL_FASTF;
		if( ri[i] < SQRT_SMALL_FASTF )
			ri[i] = SQRT_SMALL_FASTF;
	}

	BU_LIST_INIT( &r_head.l );

	if( pt1 != pt2 )
	{
		VSUB2( diff, grid_pts[pt2], grid_pts[pt1] );

		/* make first cone */
		if( ro[0] != SQRT_SMALL_FASTF || ro[1] != SQRT_SMALL_FASTF )
		{
			make_solid_name( name, CCONE3, element_id, comp_id, group_id, 1 );
			mk_trc_h( fdout, name, grid_pts[pt1], diff, ro[0], ro[1] );
			if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
				bu_bomb( "mk_addmember failed!\n" );

			/* and the inner cone */
			if( ri[0] != SQRT_SMALL_FASTF || ri[1] != SQRT_SMALL_FASTF )
			{
				make_solid_name( name, CCONE3, element_id, comp_id, group_id, 11 );
				mk_trc_h( fdout, name, grid_pts[pt1], diff, ri[0], ri[1] );
				if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					bu_bomb( "mk_addmember failed!\n" );
			}

			/* subtract any holes for this component */
			Subtract_holes( &r_head , comp_id , group_id );
		}
	}

	if( pt2 != pt3 )
	{
		VSUB2( diff, grid_pts[pt3], grid_pts[pt2] );

		/* make second cone */
		if( ro[1] != SQRT_SMALL_FASTF || ro[2] != SQRT_SMALL_FASTF )
		{
			make_solid_name( name, CCONE3, element_id, comp_id, group_id, 2 );
			mk_trc_h( fdout, name, grid_pts[pt2], diff, ro[1], ro[2] );
			if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
				bu_bomb( "mk_addmember failed!\n" );

			/* and the inner cone */
			if( ri[1] != SQRT_SMALL_FASTF || ri[2] != SQRT_SMALL_FASTF )
			{
				make_solid_name( name, CCONE3, element_id, comp_id, group_id, 22 );
				mk_trc_h( fdout, name, grid_pts[pt2], diff, ri[1], ri[2] );
				if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					bu_bomb( "mk_addmember failed!\n" );
			}

			/* subtract any holes for this component */
			Subtract_holes( &r_head , comp_id , group_id );
		}
	}

	if( pt3 != pt4 )
	{
		VSUB2( diff, grid_pts[pt4], grid_pts[pt3] );

		/* make third cone */
		if( ro[2] != SQRT_SMALL_FASTF || ro[3] != SQRT_SMALL_FASTF )
		{
			make_solid_name( name, CCONE3, element_id, comp_id, group_id, 3 );
			mk_trc_h( fdout, name, grid_pts[pt3], diff, ro[2], ro[3] );
			if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
				bu_bomb( "mk_addmember failed!\n" );

			/* and the inner cone */
			if( ri[2] != SQRT_SMALL_FASTF || ri[3] != SQRT_SMALL_FASTF )
			{
				make_solid_name( name, CCONE3, element_id, comp_id, group_id, 33 );
				mk_trc_h( fdout, name, grid_pts[pt3], diff, ri[2], ri[3] );
				if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					bu_bomb( "mk_addmember failed!\n" );
			}

			/* subtract any holes for this component */
			Subtract_holes( &r_head , comp_id , group_id );
		}
	}
	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE3 )
}

void
Add_holes( type, gr , comp , ptr )
int type;
int gr;
int comp;
struct hole_list *ptr;
{
	struct holes *hole_ptr, *prev;
	struct hole_list *hptr;

	if( debug )
	{
		bu_log( "Adding holes for group %d, component %d:\n", gr, comp );
		hptr = ptr;
		while( hptr )
		{
			bu_log( "\t%d %d\n", hptr->group, hptr->component );
			hptr = hptr->next;
		}
	}

	if( do_skips )
	{
		if( !skip_region(gr*1000 + comp) )
		{
			/* add holes for this region to the list of regions to process */
			hptr = ptr;
			if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
				bu_log( "ERROR: bu_mem_barriercheck failed in Add_hole\n" );
			while( hptr )
			{
				if( do_skips == region_list_len )
				{
					region_list_len += REGION_LIST_BLOCK;
					region_list = (int *)bu_realloc( (char *)region_list, region_list_len*sizeof( int ), "region_list" );
					if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
						bu_log( "ERROR: bu_mem_barriercheck failed in Add_hole (after realloc)\n" );
				}
				region_list[do_skips++] = 1000*hptr->group + hptr->component;
				if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
					bu_log( "ERROR: bu_mem_barriercheck failed in Add_hole (after adding %d\n)\n", 1000*hptr->group + hptr->component );
				hptr = hptr->next;
			}
		}
	}

	if( !hole_root )
	{
		hole_root = (struct holes *)bu_malloc( sizeof( struct holes ) , "Add_holes: hole_root" );
		hole_root->group = gr;
		hole_root->component = comp;
		hole_root->type = type;
		hole_root->holes = ptr;
		hole_root->next = (struct holes *)NULL;
		return;
	}

	hole_ptr = hole_root;
	prev = hole_root;
	while( hole_ptr )
	{
		if( hole_ptr->group == gr &&
			hole_ptr->component == comp &&
			hole_ptr->type == type )
				break;
		prev = hole_ptr;
		hole_ptr = hole_ptr->next;
	}

	if( hole_ptr && hole_ptr->group == gr && hole_ptr->component == comp && hole_ptr->type == type )
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
	else
	{
		prev->next = (struct holes *)bu_malloc( sizeof( struct holes ) , "Add_holes: hole_ptr->next" );
		hole_ptr = prev->next;
		hole_ptr->group = gr;
		hole_ptr->component = comp;
		hole_ptr->type = type;
		hole_ptr->holes = ptr;
		hole_ptr->next = (struct holes *)NULL;
	}
}

void
do_hole_wall( type )
int type;
{
	struct hole_list *list_ptr;
	struct hole_list *list_start;
	int group, comp;
	int igrp, icmp;
	int s_len;
	int col;

	if( debug )
		bu_log( "do_hole_wall: %s\n" , line );

	if( pass )
		return;

	/* eliminate trailing blanks */
	s_len = strlen( line );
	while( isspace(line[--s_len] ) )
		line[s_len] = '\0';

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
			list_ptr->next = (struct hole_list *)bu_malloc( sizeof( struct hole_list ) , "do_hole_wall: list_ptr" );
			list_ptr = list_ptr->next;
		}
		else
		{
			list_ptr = (struct hole_list *)bu_malloc( sizeof( struct hole_list ) , "do_hole_wall: list_ptr" );
			list_start = list_ptr;
		}

		list_ptr->group = igrp;
		list_ptr->component = icmp;
		list_ptr->next = (struct hole_list *)NULL;

		col += 8;
	}

	Add_holes( type, group , comp , list_start );
}

int
getline()
{
	int len;

	bzero( (void *)line , LINELEN );

	if( fgets( line , LINELEN , fdin ) == (char *)NULL )
		return( 0 );

	len = strlen( line );
	if( line[len-1] == '\n' )
		line[len-1] = '\0';

	return( 1 );
}

void
Add_bot_face( pt1, pt2, pt3, thick, pos )
int pt1, pt2, pt3;
fastf_t thick;
int pos;
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
		faces = (int *)bu_realloc( (void *)faces,  face_size*3*sizeof( int ), "faces" );
		thickness = (fastf_t *)bu_realloc( (void *)thickness, face_size*sizeof( fastf_t ), "thickness" );
		facemode = (char *)bu_realloc( (void *)facemode, face_size*sizeof( char ), "facemode" );
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
}

void
do_tri()
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

	if( !pass )
		return;

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

	thick = 0.0;
	pos = 0;
	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( debug )
			bu_log( "\tplate mode: thickness = %f\n" , thick );

		thickness[face_count] = thick;
		facemode[face_count] = pos;
	}

	if( do_plot )
		plot_tri( pt1, pt2, pt3 );

	Add_bot_face( pt1, pt2, pt3, thick, pos );
}

void
do_quad()
{
	int element_id;
	int pt1,pt2,pt3,pt4;
	fastf_t thick;
	int pos;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( debug )
		bu_log( "do_quad: %s\n" , line );

	if( !bot )
		bot = element_id;

	if( !pass )
		return;

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

int
skip_region( id )
int id;
{
	int i;

	if( !do_skips )
		return( 0 );

	for( i=0 ; i<do_skips ; i++ )
	{
		if( id == region_list[i] )
			return( 0 );
	}

	return( 1 );
}

void
make_bot_object()
{
	int i;
	int max_pt=0, min_pt=999999;
	int num_vertices;
	struct bu_bitv *bv=NULL;
	int bot_mode;
	char name[NAMESIZE+1];
	struct wmember bot_region;
	int element_id=bot;
	int count;
	struct rt_bot_internal bot_ip;

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , BOT );
		return;
	}

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
	bot_ip.error_mode = 0;

	count = bot_vertex_fuse( &bot_ip );
	if( count )
		(void)bot_condense( &bot_ip );

	count = bot_face_fuse( &bot_ip );
	if( count )
		bu_log( "\t%d duplicate faces eliminated\n", count );

	BU_LIST_INIT( &bot_region.l );

	make_solid_name( name , BOT , element_id , comp_id , group_id , 0 );
	mk_bot( fdout, name, bot_mode, RT_BOT_UNORIENTED, 0, bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
		bot_ip.faces, bot_ip.thickness, bot_ip.face_mode );

	if( mode == PLATE_MODE )
	{
		bu_free( (char *)bot_ip.thickness, "BOT thickness" );
		bu_free( (char *)bot_ip.face_mode, "BOT face_mode" );
	}
	bu_free( (char *)bot_ip.vertices, "BOT vertices" );
	bu_free( (char *)bot_ip.faces, "BOT faces" );

	if( mk_addmember( name ,  &bot_region , WMOP_UNION ) == (struct wmember *)NULL )
	{
		bu_log( "make_bot_object: Error in adding %s to bot region\n" , name );
		rt_bomb( "make_bot_object" );
	}


	/* subtract any holes for this component */
	Subtract_holes( &bot_region , comp_id , group_id );

	MK_REGION( fdout , &bot_region , group_id , comp_id , element_id , BOT )
}

/*	cleanup from previous component and start a new one.
 *	This is called with final == 1 when ENDDATA is found
 */
void
do_section( final )
int final;
{

	if( debug )
		bu_log( "do_section(%d): %s\n", final , line );

	if( pass )	/* doing geometry */
	{
		if( region_id && !skip_region( region_id ) )
		{
			comp_count++;

			if( bot )
				make_bot_object();
			make_cline_regions();
			make_comp_group();

		}
		if( final && debug ) /* The ENDATA card has been found */
			List_names();
	}
	else if( bot )
	{
		char	name[NAMESIZE+1];

		make_region_name( name , group_id , comp_id , bot , NMG );
	}

	if( !final )
	{
		strncpy( field , &line[8] , 8 );
		group_id = atoi( field );

		strncpy( field , &line[16] , 8 );
		comp_id = atoi( field );

		region_id = group_id * 1000 + comp_id;

		if( skip_region( region_id ) ) /* do not process this component */
		{
			long section_start;

			/* skip to start of next section */
			section_start = ftell( fdin );
			if( getline() )
			{
				while( line[0] && strncmp( line, "SECTION" , 7 ) &&
						strncmp( line, "HOLE", 4 ) &&
						strncmp( line, "WALL", 4 ) &&
						strncmp( line, "VEHICLE", 7 ) )
				{
					section_start = ftell( fdin );
					if( !getline() )
						break;
				}
			}
			/* seek to start of the section */
			fseek( fdin, section_start, SEEK_SET );
			return;
		}

		if( pass && !quiet )
			bu_log( "Making component %s, group #%d, component #%d\n",
				name_name, group_id , comp_id );

		if( comp_id > 999 )
		{
			bu_log( "Illegal component id number %d, changed to 999\n" , comp_id );
			comp_id = 999;
		}

		strncpy( field , &line[24] , 8 );
		mode = atoi( field );
		if( mode != 1 && mode != 2 )
		{
			bu_log( "Illegal mode (%d) for group %d component %d, using volume mode\n",
				mode, group_id, comp_id );
			mode = 2;
		}

		if( pass )
			name_name[0] = '\0';

	}

	bot = 0;
	face_count = 0;
}

void
do_hex1()
{
	fastf_t thick=0.0;
	int pos;
	int pts[8];
	int element_id;
	int i;
	int cont1,cont2;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !bot )
		bot = element_id;

	if( !pass )
	{
		if( !getline() )
		{
			bu_log( "Unexpected EOF while reading continuation card for CHEX1\n" );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_bomb( "CHEX1\n" );
		}
		return;
	}

	if( faces == NULL )
	{
		faces = (int *)bu_malloc( GRID_BLOCK*3*sizeof( int ), "faces" );
		thickness = (fastf_t *)bu_malloc( GRID_BLOCK*sizeof( fastf_t ), "thickness" );
		facemode = (char *)bu_malloc( GRID_BLOCK*sizeof( char ), "facemode" );
		face_size = GRID_BLOCK;
		face_count = 0;
	}

	for( i=0 ; i<6 ; i++ )
	{
		strncpy( field , &line[24 + i*8] , 8 );
		pts[i] = atoi( field );
	}

	strncpy( field , &line[72] , 8 );
	cont1 = atoi( field );

	if( !getline() )
	{
		bu_log( "Unexpected EOF while reading continuation card for CHEX1\n" );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , cont1 );
		rt_bomb( "CHEX1\n" );
	}

	strncpy( field , line , 8 );
	cont2 = atoi( field );

	if( cont1 != cont2 )
	{
		bu_log( "Continuation card numbers do not match for CHEX1 element (%d vs %d)\n", cont1 , cont2 );
		bu_log( "\tskipping CHEX1 element: group_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	strncpy( field , &line[8] , 8 );
	pts[6] = atoi( field );

	strncpy( field , &line[16] , 8 );
	pts[7] = atoi( field );

	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;
		if( thick <= 0.0 )
		{
			bu_log( "do_hex1: illegal thickness (%f), skipping CHEX1 element\n" , thick );
			bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( pos == 0 )	/* use default */
			pos = POS_FRONT;

		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			bu_log( "do_hex1: illegal postion parameter (%d), must be 1 or 2, skipping CHEX1 element\n" , pos );
			bu_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}
	}
	else
	{
		pos =  POS_FRONT;
		thick = 0.0;
	}

	for( i=0 ; i<12 ; i++ )
		Add_bot_face( pts[hex_faces[i][0]], pts[hex_faces[i][1]], pts[hex_faces[i][2]], thick, pos );
}

void
do_hex2()
{
	int pts[8];
	int element_id;
	int i;
	int cont1,cont2;
	point_t points[8];
	char name[NAMESIZE+1];
	struct wmember head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		if( !getline() )
		{
			bu_log( "Unexpected EOF while reading continuation card for CHEX2\n" );
			bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_bomb( "CHEX2\n" );
		}
		return;
	}

	for( i=0 ; i<6 ; i++ )
	{
		strncpy( field , &line[24 + i*8] , 8 );
		pts[i] = atoi( field );
	}

	strncpy( field , &line[72] , 8 );
	cont1 = atoi( field );

	if( !getline() )
	{
		bu_log( "Unexpected EOF while reading continuation card for CHEX2\n" );
		bu_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , cont1 );
		rt_bomb( "CHEX2\n" );
	}

	strncpy( field , line , 8 );
	cont2 = atoi( field );

	if( cont1 != cont2 )
	{
		bu_log( "Continuation card numbers do not match for CHEX2 element (%d vs %d)\n", cont1 , cont2 );
		bu_log( "\tskipping CHEX2 element: group_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	strncpy( field , &line[8] , 8 );
	pts[6] = atoi( field );

	strncpy( field , &line[16] , 8 );
	pts[7] = atoi( field );

	for( i=0 ; i<8 ; i++ )
		VMOVE( points[i] , grid_pts[pts[i]] );

	make_solid_name( name , CHEX2 , element_id , comp_id , group_id , 0 );
	mk_arb8( fdout , name , &points[0][X] );

	BU_LIST_INIT( &head.l );

	if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "CHEX2: mk_addmember failed\n" );

	/* subtract any holes for this component */
	Subtract_holes( &head , comp_id , group_id );

	MK_REGION( fdout , &head , group_id , comp_id , element_id , CHEX1 )

	if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
		bu_log( "ERROR: bu_mem_barriercheck failed in Do_hex2\n" );
}

void
Process_hole_wall()
{
	if( debug )
		bu_log( "Process_hole_wall\n" );
	if( bu_debug & DEBUG_MEM_FULL )
		bu_prmem( "At start of Process_hole_wall:" );

	rewind( fdin );
	while( 1 )
	{
		if( !strncmp( line , "HOLE" , 4 ) )
			do_hole_wall( HOLE );
		else if( !strncmp( line , "WALL" , 4 ) )
			do_hole_wall( WALL );
		else if( !strncmp( line , "ENDDATA" , 7 ) )
			break;

		if( !getline() || !line[0] )
			break;
	}

	if( debug )
	{
		bu_log( "At end of Process_hole_wall:\n" );
		List_holes();
	}
}

int
Process_input( pass_number )
int pass_number;
{

	if( debug )
		bu_log( "\n\nProcess_input( pass = %d )\n" , pass_number );
	if( bu_debug & DEBUG_MEM_FULL )
		bu_prmem( "At start of Process_input:" );

	if( pass_number != 0 && pass_number != 1 )
	{
		bu_log( "Process_input: illegal pass number %d\n" , pass_number );
		rt_bomb( "Process_input" );
	}

	region_id = 0;
	pass = pass_number;
	if( !getline() || !line[0] )
		strcpy( line, "ENDDATA" );
	while( 1 )
	{
		if( !strncmp( line , "VEHICLE" , 7 ) )
			do_vehicle();
		else if( !strncmp( line , "HOLE" , 4 ) )
			;
		else if( !strncmp( line , "WALL" , 4 ) )
			;
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
			do_hex1();
		else if( !strncmp( line , "CHEX2" , 5 ) )
			do_hex2();
		else if( !strncmp( line , "CTRI" , 4 ) )
			do_tri();
		else if( !strncmp( line , "CQUAD" , 5 ) )
			do_quad();
		else if( !strncmp( line , "CCONE1" , 6 ) )
			do_ccone1();
		else if( !strncmp( line , "CCONE2" , 6 ) )
			do_ccone2();
		else if( !strncmp( line , "CCONE3" , 6 ) )
			do_ccone3();
		else if( !strncmp( line , "CSPHERE" , 7 ) )
			do_sphere();
		else if( !strncmp( line , "ENDDATA" , 7 ) )
		{
			do_section( 1 );
			break;
		}
		else
			bu_log( "ERROR: skipping unrecognized data type\n%s\n" , line );

		if( !getline() || !line[0] )
			strcpy( line, "ENDDATA" );
	}

	if( debug )
	{
		bu_log( "At pass %d:\n" , pass );
		List_names();
	}

	return( 0 );
}

void
list_regions( dbip, dp, ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t	ptr;
{
	struct directory	*dp2;
	struct rt_db_internal   internal, internal2;
	struct rt_comb_internal *comb, *comb2;
	union tree		*tree, *tree2;

	/* only process regions */
	if( !(dp->d_flags & DIR_REGION) )
		return;

	if( rt_db_get_internal( &internal, dp, dbip, NULL ) < 0 )
	{
		bu_log( "Failed to get internal representation of %s\n", dp->d_namep );
		bu_bomb( "rt_db_get_internal() Failed!!!\n" );
	}

	if( internal.idb_type != ID_COMBINATION )
	{
		bu_log( "%s is not a combination!!!!\n", dp->d_namep );
		bu_bomb( "Expecting a combination!!!!\n" );
	}

	comb = (struct rt_comb_internal *)internal.idb_ptr;
	if( !comb->region_flag )
	{
		bu_log( "%s is not a region!!!!\n", dp->d_namep );
		bu_bomb( "Expecting a region!!!!\n" );
	}
	RT_CK_COMB( comb );
	tree = comb->tree;

	if( tree->tr_op != MKOP(12) )
	{
		rt_db_free_internal( &internal );
		return;
	}

	/* only one element in tree */

	if( (dp2=db_lookup( dbip, tree->tr_l.tl_name, 0 )) == DIR_NULL )
	{
		bu_log( "Could not find %s\n", tree->tr_l.tl_name );
		rt_db_free_internal( &internal );
		return;
	}

	if( rt_db_get_internal( &internal2, dp2, dbip, NULL ) < 0 )
	{
		bu_log( "Failed to get internal representation of %s\n", dp2->d_namep );
		bu_bomb( "rt_db_get_internal() Failed!!!\n" );
	}

	if( internal2.idb_type != ID_COMBINATION )
	{
		rt_db_free_internal( &internal );
		rt_db_free_internal( &internal2 );
		return;
	}

	comb2 = (struct rt_comb_internal *)internal2.idb_ptr;
	RT_CK_COMB( comb2 );

	/* move the second tree into the first */
	tree2 = comb2->tree;
	comb->tree = tree2;
	if( rt_db_put_internal( dp, dbip, &internal ) < 0 )
	{
		bu_log( "Failed to write region %s\n", dp->d_namep );
		bu_bomb( "rt_db_put_internal() failed!!!\n" );
	}

	/* now kill the second combination */
	db_delete( dbip, dp2 );
	db_dirdelete( dbip, dp2 );

	db_free_tree( tree );

}

void
Post_process( output_file )
char *output_file;
{
	struct db_i *dbip;
	struct directory *dp;

	if ((dbip = db_open( output_file , "rw")) == DBI_NULL)
	{
		bu_log( "Cannot open %s, post processing not completed\n", output_file );
		return;
	}

	db_scan(dbip, (int (*)())db_diradd, 1, NULL);

	if( (dp=db_lookup( dbip, "all", 0 )) == DIR_NULL )
	{
		bu_log( "Cannot find group 'all' in model, post processing not completed\n" );
		db_close( dbip );
		return;
	}
	db_functree( dbip, dp, list_regions, 0, NULL );
}

void
make_region_list( str )
char *str;
{
	char *ptr, *ptr2;

	region_list = (int *)bu_calloc( REGION_LIST_BLOCK, sizeof( int ), "region_list" );
	region_list_len = REGION_LIST_BLOCK;
	do_skips = 0;

	ptr = strtok( str, "," );
	while( ptr )
	{
		if( (ptr2=strchr( ptr, '-')) )
		{
			int i, start, stop;

			*ptr2 = '\0';
			ptr2++;
			start = atoi( ptr );
			stop = atoi( ptr2 );
			if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
				bu_log( "ERROR: bu_mem_barriercheck failed in make_region_list\n" );
			for( i=start ; i<=stop ; i++ )
			{
				if( do_skips == region_list_len )
				{
					region_list_len += REGION_LIST_BLOCK;
					region_list = (int *)bu_realloc( (char *)region_list, region_list_len*sizeof( int ), "region_list" );
					if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
						bu_log( "ERROR: bu_mem_barriercheck failed in make_region_list (after realloc)\n" );
				}
				region_list[do_skips++] = i;
				if( rt_g.debug&DEBUG_MEM_FULL &&  bu_mem_barriercheck() )
					bu_log( "ERROR: bu_mem_barriercheck failed in make_region_list (after adding %d)\n", i );
			}
		}
		else
		{
			if( do_skips == region_list_len )
			{
				region_list_len += REGION_LIST_BLOCK;
				region_list = (int *)bu_realloc( (char *)region_list, region_list_len*sizeof( int ), "region_list" );
			}
			region_list[do_skips++] = atoi( ptr );
		}
		ptr = strtok( (char *)NULL, "," );
	}
}

main( argc , argv )
int argc;
char *argv[];
{
	int i;
	int c;
	char *output_file;
	char *plot_file=NULL;

	while( (c=getopt( argc , argv , "qo:c:dwx:b:X:" ) ) != EOF )
	{
		switch( c )
		{
			case 'q':	/* quiet mode */
				quiet = 1;
				break;
			case 'o':	/* output a plotfile of original FASTGEN4 elements */
				do_plot = 1;
				plot_file = optarg;
				break;
			case 'c':	/* convert only the specified components */
				make_region_list( optarg );
				break;
			case 'd':	/* debug option */
				debug = 1;
				break;
			case 'w':	/* print warnings */
				warnings = 1;
				break;
			case 'x':
				sscanf( optarg, "%x", &rt_debug );
				bu_debug = rt_debug;
				break;
			case 'b':
				sscanf( optarg, "%x", &bu_debug );
				break;
			default:
				bu_log( "Unrecognzed option (%c)\n", c );
				rt_bomb( usage );
				break;
		}
	}

	if( bu_debug & BU_DEBUG_MEM_CHECK )
		bu_log( "doing memory checking\n" );

	if( argc-optind != 2 )
		rt_bomb( usage );

	if( (fdin=fopen( argv[optind] , "r" )) == (FILE *)NULL )
	{
		bu_log( "Cannot open FASTGEN4 file (%s)\n" , argv[optind] );
		perror( "fast4-g" );
		exit( 1 );
	}

	if( (fdout=fopen( argv[optind+1] , "w" )) == (FILE *)NULL )
	{
		bu_log( "Cannot open file for output (%s)\n" , argv[optind+1] );
		perror( "fast4-g" );
		exit( 1 );
	}
	output_file = argv[optind+1];

	if( plot_file )
	{
		if( (fd_plot=fopen( plot_file, "w")) == NULL )
		{
			bu_log( "Cannot open plot file (%s)\n", plot_file );
			bu_bomb( usage );
		}
	}

	if( rt_g.debug )
	{
		bu_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
		bu_log("\n");
	}
	if( rt_g.NMG_debug )
	{
		bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
		bu_log("\n");
	}

	grid_size = GRID_BLOCK;
	grid_pts = (point_t *)bu_malloc( grid_size * sizeof( point_t ) , "fast4-g: grid_pts" );

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

	name_root = (struct name_tree *)NULL;

	hole_root = (struct holes *)NULL;

	name_name[0] = '\0';
	name_count = 0;

	vehicle[0] = '\0';

	bu_ptbl_init( &stack , 64, " &stack ");

	for( i=0 ; i<11 ; i++ )
		BU_LIST_INIT( &group_head[i].l );

	BU_LIST_INIT( &hole_head.l );

	Process_hole_wall();

	rewind( fdin );

	Process_input( 0 );

	rewind( fdin );

	/* Make an ID record if no vehicle card was found */
	if( !vehicle[0] )
		mk_id_units( fdout , argv[optind] , "in" );

	while( Process_input( 1 ) );

	/* make groups */
	do_groups();

	if( debug )
		List_holes();

	/* post process */
	fclose( fdout );
	Post_process( output_file );

	if( !quiet )
		bu_log( "%d components converted\n", comp_count );
}
