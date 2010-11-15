/*                          B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brep.c
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
brep( entityno )
    int entityno;
{

    int		sol_num;		/* IGES solid type number */
    int		shell_de;		/* Directory sequence number for a shell */
    int		orient;			/* Orientation of shell */
    int		*void_shell_de;		/* Directory sequence number for an void shell */
    int		*void_orient;		/* Orientation of void shell */
    int		num_of_voids;		/* Number of inner void shells */
    struct model	*m;			/* NMG model */
    struct nmgregion *r;			/* NMG region */
    struct shell	**void_shells;		/* List of void shells */
    struct shell	*s_outer;		/* Outer shell */
    struct iges_vertex_list *v_list;
    struct iges_vertex_list *v_list_tmp;
    struct iges_edge_list	*e_list;
    struct iges_edge_list	*e_list_tmp;
    int		i;
    int		mk_nmg_executed_flag = 0;	/* Boolean indicating if mk_nmg was executed */

    /* Acquiring Data */

    if ( dir[entityno]->param <= pstart )
    {
	bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
		dir[entityno]->direct, dir[entityno]->name );
	return 0;
    }

    Readrec( dir[entityno]->param );
    Readint( &sol_num, "" );
    Readint( &shell_de, "" );
    Readint( &orient, "" );
    Readint( &num_of_voids, "" );

    if ( num_of_voids )
    {
	void_shell_de = (int *)bu_calloc( num_of_voids, sizeof( int ), "BREP: void shell DE's" );
	void_orient = (int *)bu_calloc( num_of_voids, sizeof( int ), "BREP: void shell orients" );
	void_shells = (struct shell **)bu_calloc( num_of_voids, sizeof( struct shell *), "BREP: void shell pointers" );
	for ( i=0; i<num_of_voids; i++ )
	{
	    Readint( &void_shell_de[i], "" );
	    Readint( &void_orient[i], "" );
	}
    }
    else {
	void_shell_de = NULL;
	void_orient = NULL;
	void_shells = NULL;
    }

    /* start building */
    m = nmg_mmr();
    r = BU_LIST_FIRST( nmgregion, &m->r_hd );

    /* Put outer shell in region */
    if ( (s_outer=Get_outer_shell( r, (shell_de - 1)/2 )) == (struct shell *)NULL )
	goto err;

    /* Put voids in */
    for ( i=0; i<num_of_voids; i++ )
    {
	if ( (void_shells[i]=Add_inner_shell( r, (void_shell_de[i] - 1)/2))
	     == (struct shell *)NULL )
	    goto err;
    }

    /* orient loops */
    Orient_loops( r );

    /* orient shells */
    nmg_fix_normals( s_outer, &tol );
    for ( i=0; i<num_of_voids; i++ )
    {
	nmg_fix_normals( void_shells[i], &tol );
	nmg_invert_shell( void_shells[i] );
    }

    if ( do_bots )
    {
	/* Merge all shells into one */
	for ( i=0; i<num_of_voids; i++ )
	    nmg_js( s_outer, void_shells[i], &tol );

	/* write out BOT */
	if ( mk_bot_from_nmg( fdout, dir[entityno]->name, s_outer ) )
	    goto err;
    }
    else
    {
	/* Compute "geometry" for region and shell */
	nmg_region_a( r, &tol );

	/* Write NMG solid */
	mk_nmg_executed_flag = 1;
	if ( mk_nmg( fdout, dir[entityno]->name, m ) )
	    goto err;
    }

    if ( num_of_voids )
    {
	bu_free( (char *)void_shell_de, "BREP: void shell DE's" );
	bu_free( (char *)void_orient, "BREP: void shell orients" );
	bu_free( (char *)void_shells, "brep: void shell list" );
    }

    v_list = vertex_root;
    while ( v_list != NULL )
    {
	v_list_tmp = v_list->next;
	bu_free( (char *)v_list->i_verts, "brep: iges_vertex" );
	bu_free( (char *)v_list, "brep: vertex list" );
	v_list = v_list_tmp;
    }
    vertex_root = NULL;

    e_list = edge_root;
    while ( e_list != NULL )
    {
	e_list_tmp = e_list->next;
	bu_free( (char *)e_list->i_edge, "brep:iges_edge" );
	bu_free( (char *)e_list, "brep: edge list" );
	e_list = e_list_tmp;
    }
    edge_root = NULL;

    /* perform nmg kill model if make nmg was not executed since
     * make nmg will have already performed an nmg kill model
     */
    if (!mk_nmg_executed_flag) {
        nmg_km(m);
    }

    return 1;

    err :
	if ( num_of_voids )
	{
	    bu_free( (char *)void_shell_de, "BREP: void shell DE's" );
	    bu_free( (char *)void_orient, "BREP: void shell orients" );
	    bu_free( (char *)void_shells, "brep: void shell list" );
	}

    /* perform nmg kill model if make nmg was not executed since
     * make nmg will have already performed an nmg kill model
     */
    if (!mk_nmg_executed_flag) {
        nmg_km(m);
    }

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
