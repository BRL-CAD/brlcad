/*
 *		P R O E - B R L
 *
 *  This code uses the Pro/Toolkit from Pro/Engineer to add a menu item in Pro/E
 *  to convert models to the BRL-CAD ".asc" format (actually a tcl script).
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
 *	This software is Copyright (C) 2001 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#include <math.h>
#include <string.h>
#include "ProToolkit.h"
#include "ProUtil.h"
#include "ProMessage.h"
#include "ProMenuBar.h"
#include "ProMode.h"
#include "ProMdl.h"
#include "pd_prototype.h"
#include "ProPart.h"
#include "ProSolid.h"
#include "ProAsmcomppath.h"
#include "prodev_light.h"

static wchar_t  MSGFIL[] = {'u','s','e','r','m','s','g','.','t','x','t','\0'};

static double proe_to_brl_conv=25.4;	/* inches to mm */

static double max_error=1.5;	/* (mm) maximimum allowable error in facetized approximation */
static double tol_dist=0.005;	/* (mm) minimum distance between two distinct vertices */
static double angle_cntrl=0.5;	/* angle control for tessellation ( 0.0 - 1.0 ) */
static double local_tol=0.0;	/* tolerance in Pro/E units */
static double local_tol_sq=0.0;	/* tolerance squared */

static int reg_id = 1000;	/* region ident number (incremented with each part) */

static ProVector *part_verts=NULL;	/* list of vertices for current part */
static int max_vert=0;			/* number of vertices currently malloced */
static int curr_vert=0;			/* number of vertices currently being used */

#define VERT_BLOCK 512			/* numer of vertices to malloc per call */

static ProTriangle *part_tris=NULL;	/* list of triangles for current part */
static int max_tri=0;			/* number of triangles currently malloced */
static int curr_tri=0;			/* number of triangles currently being used */

#define TRI_BLOCK 512			/* number of triangles to malloc per call */

static FILE *outfp=NULL;		/* output file */

static ProCharName curr_part_name;	/* current part name */
static ProCharName curr_asm_name;	/* current assembly name */
static ProCharLine astr;		/* buffer for Pro/E output messages */

static ProMdl *done;			/* array of objects already output */
static int curr_done=0;			/* number of objects already output */
static int max_done=0;			/* current size of above array */

#define DONE_BLOCK 512			/* number of slots to malloc when above array gets full */


/* declaration of functions passed to the feature visit routine */
static ProError assembly_comp( ProFeature *feat, ProError status, ProAppData app_data );
static ProError assembly_filter( ProFeature *feat, ProAppData *data );

/* structure to hold info about a member of the current asssembly
 * this structure is created during feature visit
 */
struct asm_member {
	ProCharName name;
	ProMatrix xform;
	ProMdlType type;
	struct asm_member *next;
};

/* structure to hold info about current assembly
 * members are added during feature visit
 */
struct asm_head {
	ProCharName name;
	ProMdl model;
	struct asm_member *members;
};

struct empty_parts {
	char *name;
	struct empty_parts *next;
};

static struct empty_parts *empty_parts_root=NULL;

/* structure to make vertex searching fast
 * Each leaf represents a vertex, and has an index into the
 * part_verts array.
 * Each node is a cutting plane at the "cut_val" on the "coord" (0, 1, or 2) axis.
 * All vertices with "coord" value less than the "cut_val" are in the "lower"
 * subtree, others are in the "higher".
 */
union vert_tree {
	char type;
	struct vert_leaf {
		char type;
		int index;
	} vleaf;
	struct vert_node {
		char type;
		double cut_val;
		int coord;
		union vert_tree *higher, *lower;
	} vnode;
} *vert_root=NULL;

/* types for the above "vert_tree" */
#define VERT_LEAF	'l'
#define VERT_NODE	'n'


/* routine to free the "vert_tree"
 * called after each part is output
 */
void
free_vert_tree( union vert_tree *ptr )
{
	if( !ptr )
		return;

	if( ptr->type == VERT_NODE ) {
		free_vert_tree( ptr->vnode.higher );
		free_vert_tree( ptr->vnode.lower );
	}

	free( (char *)ptr );
}

/* routine to add a part or assembly to the list of objects already putput */
void
add_to_done( ProMdl model )
{
	if( curr_done >= max_done ) {
		max_done += DONE_BLOCK;
		done = (ProMdl *)realloc( done, sizeof( ProMdl ) * max_done );
	}

	done[curr_done] = model;
	curr_done++;
}


/* routine to check if an object has already been output */
int
already_done( ProMdl model )
{
	int i;

	for( i=0 ; i<curr_done ; i++ ) {
		if( done[i] == model ) {
			return( 1 );
		}
	}

	return( 0 );
}

void
add_to_empty_list( char *name )
{
	struct empty_parts *ptr;
	int found=0;

	if( empty_parts_root == NULL ) {
		empty_parts_root = (struct empty_parts *)malloc( sizeof( struct empty_parts ) );
		ptr = empty_parts_root;
	} else {
		ptr = empty_parts_root;
		while( !found && ptr->next ) {
			if( !strcmp( name, ptr->name ) ) {
				found = 1;
				break;
			}
			ptr = ptr->next;
		}
		if( !found ) {
			ptr->next = (struct empty_parts *)malloc( sizeof( struct empty_parts ) );
			ptr = ptr->next;
		}
	}

	ptr->next = NULL;
	ptr->name = (char *)strdup( name );
}

void
kill_empty_parts()
{
	struct empty_parts *ptr;

	ptr = empty_parts_root;
	while( ptr ) {
		fprintf( outfp, "set combs [find %s]\n", ptr->name );
		fprintf( outfp, "foreach comb $combs {\n\trm $comb %s\n}\n", ptr->name );
		ptr = ptr->next;
	}
}

void
free_empty_parts()
{
	struct empty_parts *ptr, *prev;

	ptr = empty_parts_root;
	while( ptr ) {
		prev = ptr;
		ptr = ptr->next;
		free( prev->name );
		free( prev );
	}
}

/* routine to check for bad triangles
 * only checks for triangles with duplicate vertices
 */
int
bad_triangle( int v1, int v2, int v3, ProVector *vertices )
{
	double dist;
	double coord;
	int i;

	if( v1 == v2 || v2 == v3 || v1 == v3 )
		return( 1 );

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vertices[v1][i] - vertices[v2][i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vertices[v2][i] - vertices[v3][i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vertices[v1][i] - vertices[v3][i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	return( 0 );
}

/* routine to add a vertex to the current list of part vertices */
int
Add_vert( ProVector vertex )
{
	int i;
	union vert_tree *ptr, *prev=NULL, *new_leaf, *new_node;
	ProVector diff;

	/* look for this vertex already in the list */
	ptr = vert_root;
	while( ptr ) {
		if( ptr->type == VERT_NODE ) {
			prev = ptr;
			if( vertex[ptr->vnode.coord] >= ptr->vnode.cut_val ) {
				ptr = ptr->vnode.higher;
			} else {
				ptr = ptr->vnode.lower;
			}
		} else {
			diff[0] = fabs( vertex[0] - part_verts[ptr->vleaf.index][0] ); 
			diff[1] = fabs( vertex[1] - part_verts[ptr->vleaf.index][1] ); 
			diff[2] = fabs( vertex[2] - part_verts[ptr->vleaf.index][2] ); 
			if( (diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2]) <= local_tol_sq ) {
				/* close enough, use this vertex again */
				return( ptr->vleaf.index );
			}
			break;
		}
	}

	/* add this vertex to the list */
	if( curr_vert >= max_vert ) {
		/* allocate more memory for vertices */
		max_vert += VERT_BLOCK;

		part_verts = (ProVector *)realloc( part_verts, sizeof( ProVector ) * max_vert );
		if( !part_verts ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
						"Failed to allocate memory for part vertices" );
			fprintf( stderr, "Failed to allocate memory for part vertices\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			exit( 1 );
		}
	}

	part_verts[curr_vert][0] = vertex[0];
	part_verts[curr_vert][1] = vertex[1];
	part_verts[curr_vert][2] = vertex[2];

	/* add to the tree also */
	new_leaf = (union vert_tree *)malloc( sizeof( union vert_tree ) );
	new_leaf->vleaf.type = VERT_LEAF;
	new_leaf->vleaf.index = curr_vert++;
	if( !vert_root ) {
		/* first vertex, it becomes the root */
		vert_root = new_leaf;
	} else if( ptr && ptr->type == VERT_LEAF ) {
		/* search above ended at a leaf, need to add a node above this leaf and the new leaf */
		new_node = (union vert_tree *)malloc( sizeof( union vert_tree ) );
		new_node->vnode.type = VERT_NODE;

		/* select the cutting coord based on the biggest difference */
		if( diff[0] >= diff[1] && diff[0] >= diff[2] ) {
			new_node->vnode.coord = 0;
		} else if( diff[1] >= diff[2] && diff[1] >= diff[0] ) {
			new_node->vnode.coord = 1;
		} else if( diff[2] >= diff[1] && diff[2] >= diff[0] ) {
			new_node->vnode.coord = 2;
		}

		/* set the cut value to the mid value between the two vertices */
		new_node->vnode.cut_val = (vertex[new_node->vnode.coord] +
					   part_verts[ptr->vleaf.index][new_node->vnode.coord]) * 0.5;

		/* set the node "lower" nad "higher" pointers */
		if( vertex[new_node->vnode.coord] >= part_verts[ptr->vleaf.index][new_node->vnode.coord] ) {
			new_node->vnode.higher = new_leaf;
			new_node->vnode.lower = ptr;
		} else {
			new_node->vnode.higher = ptr;
			new_node->vnode.lower = new_leaf;
		}

		if( ptr == vert_root ) {
			/* if the above search ended at the root, redefine the root */
			vert_root =  new_node;
		} else {
			/* set the previous node to point to our new one */
			if( prev->vnode.higher == ptr ) {
				prev->vnode.higher = new_node;
			} else {
				prev->vnode.lower = new_node;
			}
		}
	} else if( ptr && ptr->type == VERT_NODE ) {
		/* above search ended at a node, just add the new leaf */
		prev = ptr;
		if( vertex[prev->vnode.coord] >= prev->vnode.cut_val ) {
			if( prev->vnode.higher ) {
				exit(1);
			}
			prev->vnode.higher = new_leaf;
		} else {
			if( prev->vnode.lower ) {
				exit(1);
			}
			prev->vnode.lower = new_leaf;
		}
	} else {
		fprintf( stderr, "*********ERROR********\n" );
	}

	/* return the index into the vertex array */
	return( new_leaf->vleaf.index );
}

/* routine to add a new triangle to the current part */
void
add_triangle( int v1, int v2, int v3 )
{
	if( curr_tri >= max_tri ) {
		/* allocate more memory for triangles */
		max_tri += TRI_BLOCK;
		part_tris = (ProTriangle *)realloc( part_tris, sizeof( ProTriangle ) * max_tri );
		if( !part_tris ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
						"Failed to allocate memory for part triangles" );
			fprintf( stderr, "Failed to allocate memory for part triangles\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			exit( 1 );
		}
	}

	/* fill in triangle info */
	part_tris[curr_tri][0] = v1;
	part_tris[curr_tri][1] = v2;
	part_tris[curr_tri][2] = v3;

	/* increment count */
	curr_tri++;
}

/* routine to output a part as a BRL-CAD region with one BOT solid
 * The region will have the name from Pro/E.
 * The solid will have the same name with "s." prefix.
 *
 *	returns:
 *		0 - OK
 *		1 - Failure
 *		2 - empty part, nothing output
 */
int
output_part( ProMdl model )
{
	ProName name;
	Pro_surf_props props;
	char sol_name[PRO_NAME_SIZE + 30];
	ProSurfaceTessellationData *tess=NULL;
	ProError status;
	char str[PRO_NAME_SIZE + 1];
	int surf_no;
	int ret=0;

	/* if this part has already been output, do not do it again */
	if( already_done( model ) )
		return( 0 );

	/* let user know we are doing something */
	sprintf( astr, "Processing Part: %s", curr_part_name );
	(void)ProMessageDisplay( MSGFIL, "USER_INFO", astr );
	ProMessageClear();
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );

	/* can get bounding box of a solid using "ProSolidOutlineGet"
	 * may want to use this to implement relative facetization talerance
	 */

	/* tessellate part */
	status = ProPartTessellate( ProMdlToPart(model), max_error/proe_to_brl_conv,
			   angle_cntrl, PRO_B_TRUE, &tess  );

	if( status != PRO_TK_NO_ERROR ) {
		/* Failed!!! */

		sprintf( astr, "Failed to tessellate part (%s)", ProWstringToString( str, name ) );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ret = 1;
	} else if( !tess ) {
		/* not a failure, just an empty part */

		sprintf( astr, "%s has no surfaces, ignoring", curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		add_to_empty_list( curr_part_name );
		ret = 2;
	} else {
		/* output the triangles */
		int surface_count;

		status = ProArraySizeGet( (ProArray)tess, &surface_count );
		if( status != PRO_TK_NO_ERROR ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Failed to get array size" );
			ProMessageClear();
			fprintf( stderr, "Failed to get array size\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			ret = 1;
		} else if( surface_count < 1 ) {
			sprintf( astr, "%s has no surfaces, ignoring", curr_part_name );
			(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
			ProMessageClear();
			fprintf( stderr, "%s\n", astr );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			ret = 2;
		} else {
			int i;
			int v1, v2, v3;
			int surfno;
			int vert_no;
			int stat;
			ProName material;

			curr_vert = 0;
			curr_tri = 0;
			free_vert_tree(vert_root);
			vert_root = (union vert_tree *)NULL;

			/* add all vertices and triangles to our lists */
			for( surfno=0 ; surfno<surface_count ; surfno++ ) {
				for( i=0 ; i<tess[surfno].n_facets ; i++ ) {
					vert_no = tess[surfno].facets[i][0];
					v1 = Add_vert( tess[surfno].vertices[vert_no] );
					vert_no = tess[surfno].facets[i][1];
					v2 = Add_vert( tess[surfno].vertices[vert_no] );
					vert_no = tess[surfno].facets[i][2];
					v3 = Add_vert( tess[surfno].vertices[vert_no] );
					if( bad_triangle( v1, v2, v3, part_verts ) ) {
						continue;
					}
					add_triangle( v1, v2, v3 );
				}
			}

			/* actually output the part */
			/* first the BOT solid with a made-up name */
			sprintf( sol_name, "s.%s", curr_part_name );
			fprintf( outfp, "put %s bot mode volume orient no V { ", sol_name );
			for( i=0 ; i<curr_vert ; i++ ) {
				fprintf( outfp, " {%.12e %.12e %.12e}",
					 part_verts[i][0] * proe_to_brl_conv,
					 part_verts[i][1] * proe_to_brl_conv,
					 part_verts[i][2] * proe_to_brl_conv );
			}
			fprintf( outfp, " } F {" );
			for( i=0 ; i<curr_tri ; i++ ) {
				fprintf( outfp, " {%d %d %d}", part_tris[i][0],
					 part_tris[i][1], part_tris[i][2] ); 
			}
			fprintf( outfp, " }\n" );

			/* get the surface properties for the part
			 * and create a region using the actual part name
			 */
			stat = prodb_get_surface_props( model, SEL_3D_PART, -1, 0, &props );
			if( stat == PRODEV_SURF_PROPS_NOT_SET ) {
				/* no surface properties */
				fprintf( outfp, "put %s comb region yes id %d los 100 GIFTmater 1 tree { l %s }\n",
					 curr_part_name, reg_id, sol_name );
			} else if( stat == PRODEV_SURF_PROPS_SET ) {
				/* use the colors, ... that was set in Pro/E */
				fprintf( outfp, "put %s comb region yes id %d los 100 GIFTmater 1 rgb {%d %d %d} shader {plastic {",
					 curr_part_name,
					 reg_id,
					 (int)(props.color_rgb[0]*255.0),
					 (int)(props.color_rgb[1]*255.0),
					 (int)(props.color_rgb[2]*255.0) );
				if( props.transparency != 0.0 ) {
					fprintf( outfp, " tr %g", props.transparency );
				}
				if( props.shininess != 1.0 ) {
					fprintf( outfp, " sh %d", (int)(props.shininess * 18 + 2.0) );
				}
				if( props.diffuse != 0.3 ) {
					fprintf( outfp, " di %g", props.diffuse );
				}
				if( props.highlite != 0.7 ) {
					fprintf( outfp, " sp %g", props.highlite );
				}
				fprintf( outfp, "} }" );
				fprintf( outfp, " tree { l %s }\n", sol_name );
			} else {
				/* something is wrong, but just ignore the missing properties */
				fprintf( stderr, "Error getting surface properties for %s\n",
					 curr_part_name );
				fprintf( outfp, "put %s comb region yes id %d los 100 GIFTmater 1 tree { l %s }\n",
					 curr_part_name, reg_id, sol_name );
			}

			/* if the part has a material, add it as an attribute */
			status = ProPartMaterialNameGet( ProMdlToPart(model), material );
			if( status == PRO_TK_NO_ERROR ) {
				fprintf( outfp, "attr set %s material_name {%s}\n",
					 curr_part_name,
					 ProWstringToString( str, material ) ); 
			}

			/* increment the region id */
			reg_id++;
		}
	}


	/* free the tessellation memory */
	ProPartTessellationFree( &tess );
	tess = NULL;

	/* add this part to the list of objects already output */
	add_to_done( model );

	return( ret );
}

/* routine to free the memory associated with our assembly info */
void
free_assem( struct asm_head *curr_assem )
{
	struct asm_member *ptr, *tmp;

	ptr = curr_assem->members;
	while( ptr ) {
		tmp = ptr;
		ptr = ptr->next;
		free( (char *)tmp );
	}
}

/* routine to list assembly info (for debugging) */
void
list_assem( struct asm_head *curr_asm )
{
	struct asm_member *ptr;

	fprintf( stderr, "Assembly %s:\n", curr_asm->name );
	ptr = curr_asm->members;
	while( ptr ) {
		fprintf( stderr, "\t%s\n", ptr->name );
		ptr = ptr->next;
	}
}

/* routine to output an assembly as a BRL-CAD combination
 * The combination will have the Pro/E name with a ".c" suffix.
 * Cannot just use the Pro/E name, because assembly can have the same name as a part.
 */
void
output_assembly( ProMdl model )
{
	ProError status;
	struct asm_head curr_assem;
	struct asm_member *member;
	int member_count=0;
	int i, j, k;

	/* do not output this assembly more than once */
	if( already_done( model ) )
		return;

	/* let the user know we are doing something */
	sprintf( astr, "Processing Assembly: %s", curr_part_name );
	(void)ProMessageDisplay( MSGFIL, "USER_INFO", astr );
	ProMessageClear();
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );

	/* everything starts out in "curr_part_name", copy name to "curr_asm_name" */
	strcpy( curr_asm_name, curr_part_name );

	/* start filling in assembly info */
	strcpy( curr_assem.name, curr_part_name );
	curr_assem.members = NULL;
	curr_assem.model = model;

	/* use feature visit to get info about assembly members.
	 * also calls output functions for members (parts or assemblies)
	 */
	status = ProSolidFeatVisit( ProMdlToPart(model),
				    assembly_comp,
				    (ProFeatureFilterAction)assembly_filter,
				    (ProAppData)&curr_assem );

	/* output the accumulated assembly info */
	fprintf( outfp, "put %s.c comb region no tree ", curr_assem.name );

	/* count number of members */
	member = curr_assem.members;
	while( member ) {
		member_count++;
		member = member->next;
	}

	/* output the "tree" */
	for( i=1 ; i<member_count ; i++ ) {
		fprintf( outfp, "{u ");
	}

	member = curr_assem.members;
	i = 0;
	while( member ) {
		/* output the member name */
		if( member->type == PRO_MDL_ASSEMBLY ) {
			fprintf( outfp, "{l %s.c", member->name );
		} else {
			fprintf( outfp, "{l %s", member->name );
		}

		/* if there is an xform matrix, put it here */
		if( is_non_identity( member->xform ) ) {
			fprintf( outfp, " {" );
			for( j=0 ; j<4 ; j++ ) {
				for( k=0 ; k<4 ; k++ ) {
					if( k == 3 && j < 3 ) {
						fprintf( outfp, " %.12e",
						     member->xform[k][j] * proe_to_brl_conv );
					} else {
						fprintf( outfp, " %.12e",
							 member->xform[k][j] );
					}
				}
			}
			fprintf( outfp, "}" );
		}
		if( i ) {
			fprintf( outfp, "}} " );
		} else {
			fprintf( outfp, "} " );
		}
		member = member->next;
		i++;
	}
	fprintf( outfp, "\n" );

	/* add this assembly to th elist of already output objects */
	add_to_done( model );

	/* free the memory associated with this assembly */
	free_assem( &curr_assem );
}

/* routine that is called by feature visit for each assembly member
 * the "app_data" is the head of the assembly info for this assembly
 */
ProError
assembly_comp( ProFeature *feat, ProError status, ProAppData app_data )
{
	ProIdTable id_table;
	ProMdl model;
	ProAsmcomppath comp_path;
	ProMatrix xform;
	ProMdlType type;
	ProName name;
	struct asm_head *curr_assem = (struct asm_head *)app_data;
	struct asm_member *member, *prev=NULL;
	int i, j;

	/* get the model associated with this member */
	status = ProAsmcompMdlGet( feat, &model );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get model for component %s",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* and get its name */
	if( ProMdlNameGet( model, name ) != PRO_TK_NO_ERROR ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
					"Could not get name for part!!" );
		ProMessageClear();
		fprintf( stderr, "Could not get name for part" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		strcpy( curr_part_name, "noname" );
	} else {
		/* this should never happen, everything must have a name */
		(void)ProWstringToString( curr_part_name, name );
	}

	/* the next two Pro/Toolkit calls are the only way I could find to get
	 * the transformation matrix to apply to this member.
	 * this call is creating a path from the assembly to this particular member
	 * (assembly/member)
	 */
	id_table[0] = feat->id;
	status = ProAsmcomppathInit( (ProSolid)curr_assem->model, id_table, 1, &comp_path );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get path from %s to %s (aborting)", curr_asm_name,
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* this call accumulates the xform matrix along the path created above */
	status = ProAsmcomppathTrfGet( &comp_path, PRO_B_TRUE, xform );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get transformation matrix %s/%s",
			 curr_asm_name, curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* add this member to our assembly info */
	prev = NULL;
	member = curr_assem->members;
	if( member ) {
		while( member->next ) {
			prev = member;
			member = member->next;
		}
		member->next = (struct asm_member *)malloc( sizeof( struct asm_member ) );
		prev = member;
		member = member->next;
	} else {
		curr_assem->members = (struct asm_member *)malloc(
					 sizeof( struct asm_member ) );
		member = curr_assem->members;
	}

	if( !member ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
					"memory allocation for member failed" );
		ProMessageClear();
		fprintf( stderr, "memory allocation for member failed\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_GENERAL_ERROR );
	}
	member->next = NULL;

	/* capture its name */
	strcpy( member->name, curr_part_name );

	/* copy xform matrix */
	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			member->xform[i][j] = xform[i][j];
		}
	}

	/* get the model for this member */
	status = ProAsmcompMdlGet( feat, &model );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get model for component %s",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* get its type (part or assembly are the only ones that should make it here) */
	status = ProMdlTypeGet( model, &type );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get type for component %s",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* remember the type */
	member->type = type;

	/* output this member */
	switch( type ) {
	case PRO_MDL_ASSEMBLY:
		output_assembly( model );
		break;
	case PRO_MDL_PART:
		if( output_part( model ) == 2 ) {
			/* part had no solid parts, eliminate from the assembly */
			if( prev ) {
				prev->next = NULL;
			} else {
				curr_assem->members = NULL;
			}
			free( (char *)member );
		}
		break;
	}

	return( PRO_TK_NO_ERROR );
}

/* this routine is a filter for the feature visit routine
 * selects only "component" items (should be only parts and assemblies)
 */
ProError
assembly_filter( ProFeature *feat, ProAppData *data )
{
	ProFeattype type;
	ProError status;

	status = ProFeatureTypeGet( feat, &type );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "In assembly_filter, cannot get feature type for feature %d",
			 feat->id );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_CONTINUE );
	}

	if( type != PRO_FEAT_COMPONENT ) {
		return( PRO_TK_CONTINUE );
	}

	return( PRO_TK_NO_ERROR );
}


/* routine to check if xform is an identity */
int
is_non_identity( ProMatrix xform )
{
	int i, j;

	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			if( i == j ) {
				if( xform[i][j] != 1.0 )
					return( 1 );
			} else {
				if( xform[i][j] != 0.0 )
					return( 1 );
			}
		}
	}

	return( 0 );
}

/* routine to output the top level object that is currently displayed in Pro/E */
void
output_top_level_object( ProMdl model, ProMdlType type )
{
	ProName name;
	ProCharName top_level;

	/* get its name */
	if( ProMdlNameGet( model, name ) != PRO_TK_NO_ERROR ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
					"Could not get name for part!!" );
		ProMessageClear();
		fprintf( stderr, "Could not get name for part" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		strcpy( curr_part_name, "noname" );
	} else {
		(void)ProWstringToString( curr_part_name, name );
	}

	/* save name */
	strcpy( top_level, curr_part_name );

	/* output the object */
	if( type == PRO_MDL_PART ) {
		/* tessellate part and output triangles */
		output_part( model );
	} else if( type == PRO_MDL_ASSEMBLY ) {
		/* visit all members of assembly */
		output_assembly( model );
	} else {
		sprintf( astr, "Object %s is neither PART nor ASSEMBLY, skipping",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	}

	/* make a top level combination named "top", if there is not already one
	 * this combination contains the xform to rotate the model into BRL-CAD standard orientation
	 */
	fprintf( outfp, "if { [catch {get top} ret] } {\n" );
	if( type == PRO_MDL_ASSEMBLY ) {
		fprintf( outfp,
			 "\tput top comb region no tree {l %s.c {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}\n",
			 top_level );
	} else {
		fprintf( outfp,
			 "\tput top comb region no tree {l %s {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}\n",
			 top_level );
	}
	fprintf( outfp, "}\n" );
}


/* this is the driver routine for converting Pro/E to BRL-CAD */
int
proe_brl( uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data )
{
	ProError status;
	ProMdl model;
	ProMdlType type;
	int unit_subtype;
	wchar_t unit_name[PRO_NAME_SIZE];
	double proe_conv;
	wchar_t w_output_file[128];
	char output_file[128];
	double range[2];

	empty_parts_root = NULL;

	/* default output file name */
	strcpy( output_file, "proe.asc" );

	/* get the output file name */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_STRING",
				 "Enter name of file to receive output: ",
				 output_file );
	status = ProMessageStringRead( 127, w_output_file );
	if( status == PRO_TK_NO_ERROR ) {
		(void)ProWstringToString( output_file, w_output_file );
	} else if( status == PRO_TK_MSG_USER_QUIT) {
		return( 0 );
	}

	/* get starting ident number */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_INT",
				 "Enter staring ident number: ",
				 &reg_id );
	status = ProMessageIntegerRead( NULL, &reg_id );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* get the maximum allowed error */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				 "Enter maximum allowable error for tessellation (mm): ",
				 &max_error );
	range[0] = 0.0;
	range[1] = 500.0;
	status = ProMessageDoubleRead( range, &max_error );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* get the angle control */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter a value for angle control: ",
				  &angle_cntrl );
	range[0] = 0.0;
	range[1] = 1.0;
	status = ProMessageDoubleRead( range, &angle_cntrl );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* open output file */
	if( (outfp=fopen( output_file, "w" ) ) == NULL ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Cannot open output file" );
		ProMessageClear();
		fprintf( stderr, "Cannot open output file\n" );
		perror( "\t" );
		return( PRO_TK_GENERAL_ERROR );
	}

	/* get the curently displayed model in Pro/E */
	status = ProMdlCurrentGet( &model );
	if( status == PRO_TK_BAD_CONTEXT ) {
		(void)ProMessageDisplay(MSGFIL, "USER_NO_MODEL" );
		ProMessageClear();
		fprintf( stderr, "No model is displayed!!\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* get its type */
	status = ProMdlTypeGet( model, &type );
	if( status == PRO_TK_BAD_INPUTS ) {
		(void)ProMessageDisplay(MSGFIL, "USER_NO_TYPE" );
		ProMessageClear();
		fprintf( stderr, "Cannot get type of current model\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* can only do parts and assemblies, no drawings, etc */
	if( type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART ) {
		(void)ProMessageDisplay(MSGFIL, "USER_TYPE_NOT_SOLID" );
		ProMessageClear();
		fprintf( stderr, "Current model is not a solid object\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* get units, and adjust conversion factor */
	if( prodb_get_model_units( model, LENGTH_UNIT, &unit_subtype,
				   unit_name, &proe_conv ) ) {
		if( unit_subtype == UNIT_MM )
			proe_to_brl_conv = 1.0;
		else
			proe_to_brl_conv = proe_conv * 25.4;
	} else {
		ProMessageDisplay(MSGFIL, "USER_NO_UNITS" );
		ProMessageClear();
		fprintf( stderr, "No units specified, assuming inches\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		proe_to_brl_conv = 25.4;
	}

	/* adjust tolerance for Pro/E units */
	local_tol = tol_dist / proe_to_brl_conv;
	local_tol_sq = local_tol * local_tol;

	/* output the top level object
	 * this will recurse through the entire model
	 */
	output_top_level_object( model, type );

	/* kill any references to empty parts */
	kill_empty_parts();

	/* let user know we are done */
	ProMessageDisplay(MSGFIL, "USER_INFO", "Conversion complete" );

	/* free a bunch of stuff */
	if( done ) {
		free( (char *)done );
	}
	done = NULL;
	max_done = 0;
	curr_done = 0;

	if( part_tris ) {
		free( (char *)part_tris );
	}
	part_tris = NULL;

	if( part_verts ) {
		free( (char *)part_verts );
	}
	part_verts = NULL;

	free_vert_tree( vert_root );
	vert_root = NULL;

	max_vert = 0;
	max_tri = 0;

	free_empty_parts();

	fclose( outfp );

	return( 0 );
}

/* this routine determines whether the "proe-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
static uiCmdAccessState
proe_brl_access( uiCmdAccessMode access_mode )
{

	ProMode mode;
	ProError status;

	status = ProModeCurrentGet( &mode );
	if ( status != PRO_TK_NO_ERROR ) {
		return( ACCESS_UNAVAILABLE );
	}

	/* only allow our menu item to be used when parts or assemblies are displayed */
	if( mode == PRO_MODE_ASSEMBLY || mode == PRO_MODE_PART ) {
		return( ACCESS_AVAILABLE );
	} else {
		return( ACCESS_UNAVAILABLE );
	}
}

/* routine to add our menu item */
int
user_initialize( int argc, char *argv[], char *version, char *build, wchar_t errbuf[80] )
{
	ProError status;
	int i;
	uiCmdCmdId cmd_id;

	/* Pro/E says always check the size of w_char */
	status = ProWcharSizeVerify (sizeof (wchar_t), &i);
	if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
		sprintf(astr,"ERROR wchar_t Incorrect size (%d). Should be: %d",
			sizeof(wchar_t), i );
		status = ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		printf("%s\n", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return(-1);
	}

	/* add a command that calls our proe-brl routine */
	status = ProCmdActionAdd( "Proe-BRL", (uiCmdCmdActFn)proe_brl, uiProe2ndImmediate,
				  proe_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to add proe-brl action" );
		fprintf( stderr, "%s\n", astr);
		ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( -1 );
	}

	/* add a menu item that runs the new command */
	status = ProMenubarmenuPushbuttonAdd( "File", "Proe-BRL", "Proe-BRL", "Proe-BRL-HELP",
					      "File.psh_exit", PRO_B_FALSE, cmd_id, MSGFIL );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to add proe-brl menu button" );
		fprintf( stderr, "%s\n", astr);
		ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( -1 );
	}

	/* let user know we are here */
	ProMessageDisplay( MSGFIL, "OK" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );

	return( 0 );
}

/* required terminate routine, we do not use it */
void
user_terminate()
{
}






