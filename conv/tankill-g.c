/*
 *			T A N K I L L - G
 *
 *  Program to convert the UK TANKILL format to BRL-CAD.
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The US Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
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

#define START_ARRAY_SIZE	64
#define ARRAY_BLOCK_SIZE	64

struct tankill_verts
{
	point_t coord;
	struct vertex *vp;
};

struct comp_idents
{
	int ident;
	int no_of_solids;
	struct comp_idents *next;
} *id_root;

/*	W R I T E _ S H E L L _ A S _ P O L Y S O L I D
 *
 *	This routine take an NMG shell and writes it out to the file
 *	out_fp as a polysolid with the indicated name.  Obviously,
 *	the shell should be a 3-manifold (winged edge).
 *	since polysolids may only have up to 5 vertices per face,
 *	any face with a loop of more than 5 vertices is triangulated
 *	using "nmg_triangulate_face" prior to output.
 */
void
write_shell_as_polysolid( FILE *out_fp , char *name , struct shell *s )
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	point_t verts[5];
	int count_npts;
	int max_count;
	int i;

	NMG_CK_SHELL( s );

	mk_polysolid( out_fp , name );

	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		/* only do OT_SAME faces */
		if( fu->orientation != OT_SAME )
			continue;

		/* count vertices in loops */
		max_count = 0;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			count_npts = 0;
			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				count_npts++;

			if( count_npts > max_count )
				max_count = count_npts;
		}

		/* if any loop has more than 5 vertices, triangulate the face */
		if( max_count > 5 )
			nmg_triangulate_face( fu );

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			count_npts = 0;
			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				for( i=0 ; i<3 ; i++ )
					verts[count_npts][i] = eu->vu_p->v_p->vg_p->coord[i];
				count_npts++;
			}

			if( mk_fpoly( out_fp , count_npts , verts ) )
				rt_log( "write_shell_as_polysolid: mk_fpoly failed for object %s\n" , name );
		}
	}
}

/*	Adds another solid to the list of solids for each component code number.
 *	Returns the number of solids in this component (including the one just added)
 */
static int
Add_solid( int comp_code_num )
{
	struct comp_idents *ptr;

	/* if list is empty, start one */
	if( id_root == NULL )
	{
		id_root = (struct comp_idents *)rt_malloc( sizeof( struct comp_idents ) , "tankill-g: idents list" );
		id_root->next = (struct comp_idents *)NULL;
		id_root->ident = comp_code_num;
		id_root->no_of_solids = 1;
		return( 1 );
	}
	else
	{
		/* look for an entry for this component code number */
		ptr = id_root;
		while( ptr->next != (struct comp_idents *)NULL && ptr->ident != comp_code_num )
			ptr = ptr->next;

		/* if found one, just increment the number of solids */
		if( ptr->ident == comp_code_num )
		{
			ptr->no_of_solids++;
			return( ptr->no_of_solids );
		}
		else
		{
			/* make a new entry for this component */
			ptr->next = (struct comp_idents *)rt_malloc( sizeof( struct comp_idents ) , "tankill-g: idents list " );
			ptr = ptr->next;
			ptr->next = NULL;
			ptr->ident = comp_code_num;
			ptr->no_of_solids = 1;
			return( 1 );
		}
	}
}

/*	T A N K I L L - G
 *
 *	Converts "tankill" format geometry to BRLCAD model
 */

static char *usage="Usage: tankill-g [-p] [-t tolerance] [-i input_tankill_file] [-o output_brlcad_model]\n\
	where tolerance is the minimum distance (mm) between distinct vertices\n";

main( int argc , char *argv[] )
{
	register int c;
	int i;
	int vert1,vert2;
	int vert_no;
	int no_of_verts;
	int comp_code;
	int array_size=START_ARRAY_SIZE;
	int surr_code;	/* not useful */
	float x,y,z;
	struct tankill_verts *verts;
	struct vertex **face_verts[3];
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl faces;
	struct rt_tol tol;
	struct wmember reg_head;
	struct comp_idents *ptr;
	char name[NAMESIZE+1];
	char *input_file;
	FILE *in_fp,*out_fp;
	int polysolids;
	int group_len[100];
	int all_len=0;

	/* Set defaults */

        /* XXX These need to be improved */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	in_fp = stdin;
	out_fp = stdout;
	polysolids = 0;
	input_file = (char *)NULL;

	/* get command line arguments */
	while ((c = getopt(argc, argv, "pt:i:o:")) != EOF)
	{
		switch( c )
		{
			case 'p': /* choose polysolid output */
				polysolids = 1;
				break;
			case 't': /* tolerance */
				tol.dist = atof( optarg );
				tol.dist_sq = tol.dist * tol.dist;
				break;
			case 'i': /* input file name */
				if( (in_fp = fopen( optarg , "r" )) == NULL )
				{
					fprintf( stderr , "Cannot open %s\n" , optarg );
					perror( "tankill-g" );
					rt_bomb( "Cannot open input file" );
				}
				input_file = rt_malloc( sizeof( optarg ) +1 , "tankill-g: input file name" );
				strcpy( input_file , optarg );
				break;
			case 'o': /* output file name */
				if( (out_fp = fopen( optarg , "w" )) == NULL )
				{
					fprintf( stderr , "Cannot open %s\n" , optarg );
					perror( "tankill-g" );
					rt_bomb( "Cannot open output file\n" );
				}
				break;
			default:
				rt_bomb( usage );
				break;
		}
	}


	id_root = (struct comp_idents *)NULL;

	/* make a table for the outward pointing faceuses (for nmg_gluefaces) */
	nmg_tbl( &faces , TBL_INIT , NULL );

	if( input_file == (char *)NULL )
		mk_id( out_fp , "Conversion from TANKILL" );
	else
		mk_id( out_fp , input_file );

	/* make some space to store the vertices */
	verts = (struct tankill_verts *)rt_malloc( array_size*sizeof( struct tankill_verts ) , "tankill-g: verts array " );

	/* read the number of vertices to expect */
	while( fscanf( in_fp , "%d" , &no_of_verts ) != EOF )
	{
		/* make a new shell */
		m = nmg_mm();
		r = nmg_mrsv( m );
		s = RT_LIST_FIRST( shell , &r->s_hd );

		/* make sure there is enough room */
		if( no_of_verts > array_size )
		{
			while( array_size < no_of_verts )
				array_size += ARRAY_BLOCK_SIZE;
		        verts = (struct tankill_verts *)rt_realloc( (char *)verts , array_size*sizeof( struct tankill_verts ) , "tankill-g: vertex array" );
		}

		/* read the component code number */
		if( fscanf( in_fp , "%d" , &comp_code ) == EOF )
		{
			fprintf( stderr , "Unexpected EOF\n" );
			break;
		}

		/* read the surroundings code (I think this is like the space code in GIFT/FASTGEN) */
		if( fscanf( in_fp , "%d " , &surr_code ) == EOF )
		{
			fprintf( stderr , "Unexpected EOF\n" );
			break;
		}

		/* read the vertices into our structure and set the nmg vertex pointer to NULL */
		for( vert_no=0 ; vert_no < no_of_verts ; vert_no++ )
		{
			if( fscanf( in_fp , "%6f %6f %6f" , &x , &y , &z ) == EOF )
			{
				fprintf( stderr , "Unexpected EOF\n" );
				break;
			}
			VSET( verts[vert_no].coord , x , y , z );
			verts[vert_no].vp = (struct vertex *)NULL;
		}

		/* skip component codes of 1001 (these are not real components) */
		if( comp_code == 1001 )
			continue;

		/* now start making faces */
		vert_no = 0;
		vert1 = 0;
		while( vert_no < no_of_verts - 2 )
		{
			/* skip combinations that won't make a face */
			if( rt_3pts_collinear( verts[vert_no].coord , verts[vert_no+1].coord , verts[vert_no+2].coord , &tol ) )
				vert_no++;
			else if( !rt_3pts_distinct( verts[vert_no].coord , verts[vert_no+1].coord , verts[vert_no+2].coord , &tol ) )
				vert_no++;
			else
			{
				/* put next three vertices in an array for nmg_cface */
				for( i=0 ; i<3 ; i++ )
					face_verts[i] = &verts[i+vert_no].vp;

				/* make a face */
				fu = nmg_cmface( s , face_verts , 3 );

				/* save the face in a table */
				nmg_tbl( &faces , TBL_INS , (long *)fu );

				/* make sure any duplicate vertices get the same vertex pointer */
				for( ; vert1 < vert_no+3 ; vert1++ )
				{
					for( vert2=vert1+1 ; vert2 < no_of_verts ; vert2++ )
					{
						if( verts[vert2].vp )
							continue;
						if( VEQUAL( verts[vert1].coord , verts[vert2].coord ) )
							verts[vert2].vp = verts[vert1].vp;
					}
				}
				vert_no++;
			}
		}

		/* assign geometry */
		for( i=0 ; i<no_of_verts ; i++ )
		{
			if( (verts[i].vp != (struct vertex *)NULL) &&
				(verts[i].vp->vg_p == (struct vertex_g *)NULL) )
					nmg_vertex_gv( verts[i].vp , verts[i].coord );
		}

		/* calculate plane equations for faces */
		for (RT_LIST_FOR(s, shell, &r->s_hd))
		{
		    NMG_CK_SHELL( s );
		    for (RT_LIST_FOR(fu, faceuse, &s->fu_hd))
		    {
		        NMG_CK_FACEUSE( fu );
		        if( fu->orientation == OT_SAME )
		        {
		                if( nmg_fu_planeeqn( fu , &tol ) )
		                        rt_log( "Failed to calculate plane eqn\n" );
		        }
		    }
		}

		/* look for edges that need to be broken apart for sharing
		 *
		 * Like so:
		 *    *------->*-------->*--------->*
		 *    *<----------------------------*
		 *
		 * bottom edge needs to be broken into three to share with
		 * upper edge
		 */

		s = RT_LIST_FIRST( shell , &r->s_hd );
		(void)nmg_break_long_edges( s , &tol );

		/* glue all the faces together */
		nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

		/* re-initialize the face list */
		nmg_tbl( &faces , TBL_RST , NULL );

		/* Calculate bounding boxes */
		nmg_region_a( r );

		/* fix the normals */
		s = RT_LIST_FIRST( shell , &r->s_hd );
		nmg_fix_normals( s );

		/* make a name for this solid */
		sprintf( name , "s.%d.%d" , comp_code , Add_solid( comp_code ) );

		/* write the solid to the brlcad database */
		if( polysolids )
			write_shell_as_polysolid( out_fp , name , s );
		else
		{
			int loop_count;

			/* simplify the structure as much as possible before writing */
			nmg_shell_coplanar_face_merge( s , &tol , 1 );
			nmg_simplify_shell( s );

			/* write it out */
			mk_nmg( out_fp , name , m );
		}

		/* kill the nmg model */
		nmg_km( m );

	}

	/* free the memory for the face list */
	nmg_tbl( &faces , TBL_FREE , NULL );

	/* Make regions */
	ptr = id_root;
	while( ptr != NULL )
	{
		struct wmember *wmem;

		RT_LIST_INIT( &reg_head.l );

		/* make linked list of members */
		for( i=0 ; i<ptr->no_of_solids ; i++ )
		{
			/* recreate the name of each solid */
			sprintf( name , "s.%d.%d" , ptr->ident , i+1 );
			wmem = mk_addmember( name , &reg_head , WMOP_UNION );
		}

		/* make the region name */
		sprintf( name , "r.%d" , ptr->ident );

		/* make the region */
		if (mk_lrcomb( out_fp , name , &reg_head , 1 , (char *)NULL ,
			(char *)NULL , (char *)NULL , ptr->ident , 0 ,
			1 , 100 , 0 ) )
		{
			rt_bomb( "tankill: Error in freeing region memory" );
		}
		ptr = ptr->next;
	}

	/* Make groups based on ident numbers */
	for( i=0 ; i<100 ; i++ )
	{
		struct wmember *wmem;

		RT_LIST_INIT( &reg_head.l );

		group_len[i] = 0;
		ptr = id_root;
		while( ptr != NULL )
		{
			if( ptr->ident/100 == i )
			{
				/* make the region name */
				sprintf( name , "r.%d" , ptr->ident );

				wmem = mk_addmember( name , &reg_head , WMOP_UNION );
				group_len[i]++;
			}
			ptr = ptr->next;
		}

		if( group_len[i] )
		{
			/* make a group name */
			sprintf( name , "%02dXX_codes" , i );

			/* make the group */
			if( mk_lcomb( out_fp , name , &reg_head , 0, (char *)0, (char *)0, (char *)0, 0 ) )
			{
				rt_bomb( "tankill: Error in freeing region memory" );
			}
		}
	}

	/* Make next higher level groups */
	for( i=0 ; i<10 ; i++ )
	{
		struct wmember *wmem;
		int do_group;
		int k;

		RT_LIST_INIT( &reg_head.l );

		do_group = 0;
		for( k=i*10 ; k<(i+1)*10 ; k++ )
		{
			if( group_len[k] )
			{
				do_group = 1;

				/* make group name */
				sprintf( name , "%02dXX_codes" , k );
				wmem = mk_addmember( name , &reg_head , WMOP_UNION );
			}
		}
		if( do_group )
		{
			/* make the group */
			sprintf( name , "%dXXX_codes" , i );
			if( mk_lcomb( out_fp , name , &reg_head , 0, (char *)0, (char *)0, (char *)0, 0 ) )
			{
				rt_bomb( "tankill: Error in freeing region memory" );
			}
		}
	}

	/* Make top level group "all" */

	RT_LIST_INIT( &reg_head.l );

	for( i=0 ; i<10 ; i++ )
	{
		struct wmember *wmem;
		int do_group;
		int k;

		do_group = 0;
		for( k=i*10 ; k<(i+1)*10 ; k++ )
		{
			if( group_len[k] )
			{
				do_group = 1;
				break;
			}
		}
		if( do_group )
		{
			/* make the group */
			sprintf( name , "%dXXX_codes" , i );

			if( (wmem = mk_addmember( name , &reg_head , WMOP_UNION ) ) == WMEMBER_NULL )
				rt_log( "mk_admember failed for %s\n" , name );
			all_len++;
		}
	}
	if( all_len )
	{
		if( mk_lcomb( out_fp , "all" , &reg_head , 0, (char *)0, (char *)0, (char *)0, 0 ) )
			rt_bomb( "tankill: Error in freeing region memory" );
	}
}
