/*                         E D A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2009 United States Government as represented by
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
/** @file edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "rtgeom.h"
#include "ged.h"
#include "db.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"

void	ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb), old_ext4to6();

extern struct rt_db_internal	es_int;
extern struct rt_db_internal	es_int_orig;

/*
 *  			E D I T A R B
 *
 *  An ARB edge is moved by finding the direction of
 *  the line containing the edge and the 2 "bounding"
 *  planes.  The new edge is found by intersecting the
 *  new line location with the bounding planes.  The
 *  two "new" planes thus defined are calculated and the
 *  affected points are calculated by intersecting planes.
 *  This keeps ALL faces planar.
 *
 */

int
editarb( vect_t pos_model )
{
    static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    short *edptr;		/* pointer to arb edit array */
    short *final;		/* location of points to redo */
    static int i;
    const int *iptr;
    struct rt_arb_internal *arb;

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC( arb );

    /* set the pointer */
    switch ( es_type ) {

	case ARB4:
	    edptr = &earb4[es_menu][0];
	    final = &earb4[es_menu][16];
	    break;

	case ARB5:
	    edptr = &earb5[es_menu][0];
	    final = &earb5[es_menu][16];
	    if (es_edflag == PTARB) {
		edptr = &earb5[8][0];
		final = &earb5[8][16];
	    }
	    break;

	case ARB6:
	    edptr = &earb6[es_menu][0];
	    final = &earb6[es_menu][16];
	    if (es_edflag == PTARB) {
		i = 9;
		if (es_menu == 4)
		    i = 8;
		edptr = &earb6[i][0];
		final = &earb6[i][16];
	    }
	    break;

	case ARB7:
	    edptr = &earb7[es_menu][0];
	    final = &earb7[es_menu][16];
	    if (es_edflag == PTARB) {
		edptr = &earb7[11][0];
		final = &earb7[11][16];
	    }
	    break;

	case ARB8:
	    edptr = &earb8[es_menu][0];
	    final = &earb8[es_menu][16];
	    break;

	default:
	    Tcl_AppendResult(interp, "edarb: unknown ARB type\n", (char *)NULL);

	    return(1);
    }

    /* do the arb editing */

    if ( es_edflag == PTARB ) {
	/* moving a point - not an edge */
	VMOVE( arb->pt[es_menu], pos_model );
	edptr += 4;
    } else if ( es_edflag == EARB ) {
	vect_t	edge_dir;

	/* moving an edge */
	pt1 = *edptr++;
	pt2 = *edptr++;
	/* direction of this edge */
	if ( newedge ) {
	    /* edge direction comes from edgedir() in pos_model */
	    VMOVE( edge_dir, pos_model );
	    VMOVE(pos_model, arb->pt[pt1]);
	    newedge = 0;
	} else {
	    /* must calculate edge direction */
	    VSUB2(edge_dir, arb->pt[pt2], arb->pt[pt1]);
	}
	if (MAGNITUDE(edge_dir) == 0.0)
	    goto err;
	/* bounding planes bp1, bp2 */
	bp1 = *edptr++;
	bp2 = *edptr++;

	/* move the edge */
/*
  bu_log("moving edge: %d%d  bound planes: %d %d\n", pt1+1, pt2+1, bp1+1, bp2+1);
*/
	if ( mv_edge(pos_model, bp1, bp2, pt1, pt2, edge_dir) )
	    goto err;
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9) {	/* special flag --> redo all the planes */
	struct bu_vls error_msg;

	bu_vls_init(&error_msg);
	if (rt_arb_calc_planes(&error_msg, arb, es_type, es_peqn, &mged_tol)) {
	    Tcl_AppendResult( interp, bu_vls_addr(&error_msg), (char *)0);
	    bu_vls_free(&error_msg);
	    goto err;
	}
	bu_vls_free(&error_msg);
    }

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1, p2, p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;
/*
  bu_log("redo plane %d with points %d %d %d\n", newp+1, p1+1, p2+1, p3+1);
*/
	    if ( bn_mk_plane_3pts( es_peqn[newp], arb->pt[p1], arb->pt[p2],
				   arb->pt[p3], &mged_tol ) )
		goto err;

	    /* next plane */
	    if ( (newp = *edptr++) == -1 || newp == 8 )
		break;
	}
    }
    if (newp == 8) {
	/* special...redo next planes using pts defined in faces */
	for (i=0; i<3; i++) {
	    if ( (newp = *edptr++) == -1 )
		break;
	    iptr = &rt_arb_faces[es_type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;
/*
  bu_log("REdo plane %d with points %d %d %d\n", newp+1, p1+1, p2+1, p3+1);
*/
	    if ( bn_mk_plane_3pts( es_peqn[newp], arb->pt[p1], arb->pt[p2],
				   arb->pt[p3], &mged_tol ))
		goto err;
	}
    }

    /* the changed planes are all redone
     *	push necessary points back into the planes
     */
    edptr = final;	/* point to the correct location */
    for (i=0; i<2; i++) {
	if ( (p1 = *edptr++) == -1 )
	    break;
	/* intersect proper planes to define vertex p1 */
/*
  bu_log("intersect: type=%d   point = %d\n", es_type, p1+1);
*/
	if ( rt_arb_3face_intersect( arb->pt[p1], (const plane_t *)es_peqn, es_type, p1*3 ))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must
     *	recalculate plane 2 = 456
     */
    if (es_type == ARB7 && es_edflag == PTARB) {
/*
  bu_log("redo plane 2 == 5, 6, 7 for ARB7\n");
*/
	if (  bn_mk_plane_3pts( es_peqn[2], arb->pt[4], arb->pt[5], arb->pt[6], &mged_tol ))
	    goto err;
    }

    /* carry along any like points */
    switch ( es_type ) {
	case ARB8:
	    break;

	case ARB7:
	    VMOVE( arb->pt[7], arb->pt[4] )
		break;

	case ARB6:
	    VMOVE( arb->pt[5], arb->pt[4] );
	    VMOVE( arb->pt[7], arb->pt[6] );
	    break;

	case ARB5:
	    for ( i=5; i<8; i++ )
		VMOVE( arb->pt[i], arb->pt[4] )
		    break;

	case ARB4:
	    VMOVE( arb->pt[3], arb->pt[0] );
	    for ( i=5; i<8; i++ )
		VMOVE( arb->pt[i], arb->pt[4] )
		    break;
    }

    return(0);		/* OK */

 err:
    /* Error handling */
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "cannot move edge: %d%d\n", pt1+1, pt2+1);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    es_edflag = IDLE;

    return(1);		/* BAD */
}

/*  MV_EDGE:
 *	Moves an arb edge (end1, end2) with bounding
 *	planes bp1 and bp2 through point "thru".
 *	The edge has (non-unit) slope "dir".
 *	Note that the fact that the normals here point in rather than
 *	out makes no difference for computing the correct intercepts.
 *	After the intercepts are found, they should be checked against
 *	the other faces to make sure that they are always "inside".
 */
int
mv_edge(
    vect_t thru,
    int bp1, int bp2, int end1, int end2,
    const vect_t	dir)
{
    struct rt_arb_internal *arb;
    fastf_t	t1, t2;

    if ( bn_isect_line3_plane( &t1, thru, dir, es_peqn[bp1], &mged_tol ) < 0 ||
	 bn_isect_line3_plane( &t2, thru, dir, es_peqn[bp2], &mged_tol ) < 0 )  {
	Tcl_AppendResult(interp, "edge (direction) parallel to face normal\n", (char *)NULL);
	return( 1 );
    }

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC( arb );

    VJOIN1( arb->pt[end1], thru, t1, dir );
    VJOIN1( arb->pt[end2], thru, t2, dir );

    return( 0 );
}

/* Extrude command - project an arb face */
/* Format: extrude face distance	*/
int
f_extrude(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int i, j;
    static int face;
    static int pt[4];
    static int prod;
    static fastf_t dist;
    struct rt_arb_internal larb;	/* local copy of arb for new way */
    struct bu_vls error_msg;

    CHECK_DBI_NULL;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help extrude");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_S_EDIT, "Extrude" ) )
	return TCL_ERROR;

    if ( es_int.idb_type != ID_ARB8 )
    {
	Tcl_AppendResult(interp, "Extrude: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (es_type != ARB8 && es_type != ARB6 && es_type != ARB4) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ARB%d: extrusion of faces not allowed\n", es_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_ERROR;
    }

    face = atoi( argv[1] );

    /* get distance to project face */
    dist = atof( argv[2] );
    /* apply es_mat[15] to get to real model space */
    /* convert from the local unit (as input) to the base unit */
    dist = dist * es_mat[15] * local2base;

    memcpy((char *)&larb, (char *)es_int.idb_ptr, sizeof( struct rt_arb_internal ));

    if ( (es_type == ARB6 || es_type == ARB4) && face < 1000 ) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    }
    else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	 10	    125 for ARB6
     *	 72	    346 for ARB6
     * --- special case to make ARB6 from ARB4
     * ---   provides easy way to build ARB6's
     *        6	    123 for ARB4
     *	  8	    124 for ARB4
     *	 12	    134 for ARB4
     *	 24	    234 for ARB4
     */
    prod = 1;
    for ( i = 0; i <= 3; i++ )  {
	prod *= pt[i];
	if (es_type == ARB6 && pt[i] == 6)
	    pt[i]++;
	if (es_type == ARB4 && pt[i] == 4)
	    pt[i]++;
	pt[i]--;
	if ( pt[i] > 7 )  {
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    /* find plane containing this face */
    if ( bn_mk_plane_3pts( es_peqn[6], larb.pt[pt[0]], larb.pt[pt[1]],
			   larb.pt[pt[2]], &mged_tol ) )
    {
	Tcl_AppendResult(interp, "face: ", argv[1], " is not a plane\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* get normal vector of length == dist */
    for ( i = 0; i < 3; i++ )
	es_peqn[6][i] *= dist;

    /* protrude the selected face */
    switch ( prod )  {

	case 24:   /* protrude face 1234 */
	    if (es_type == ARB6) {
		Tcl_AppendResult(interp, "ARB6: extrusion of face ", argv[1],
				 " not allowed\n", (char *)NULL);
		return TCL_ERROR;
	    }
	    if (es_type == ARB4)
		goto a4toa6;	/* extrude face 234 of ARB4 to make ARB6 */

	    for ( i = 0; i < 4; i++ )  {
		j = i + 4;
		VADD2( larb.pt[j], larb.pt[i], es_peqn[6] );
	    }
	    break;

	case 6:		/* extrude ARB4 face 123 to make ARB6 */
	case 8:		/* extrude ARB4 face 124 to make ARB6 */
	case 12:	/* extrude ARB4 face 134 to Make ARB6 */
    a4toa6:
	    ext4to6(pt[0], pt[1], pt[2], &larb);
	    es_type = ARB6;
	    sedit_menu();
	    break;

	case 1680:   /* protrude face 5678 */
	    for ( i = 0; i < 4; i++ )  {
		j = i + 4;
		VADD2( larb.pt[i], larb.pt[j], es_peqn[6] );
	    }
	    break;

	case 60:   /* protrude face 1256 */
	case 10:   /* extrude face 125 of ARB6 */
	    VADD2( larb.pt[3], larb.pt[0], es_peqn[6] );
	    VADD2( larb.pt[2], larb.pt[1], es_peqn[6] );
	    VADD2( larb.pt[7], larb.pt[4], es_peqn[6] );
	    VADD2( larb.pt[6], larb.pt[5], es_peqn[6] );
	    break;

	case 672:   /* protrude face 4378 */
	case 72:	/* extrude face 346 of ARB6 */
	    VADD2( larb.pt[0], larb.pt[3], es_peqn[6] );
	    VADD2( larb.pt[1], larb.pt[2], es_peqn[6] );
	    VADD2( larb.pt[5], larb.pt[6], es_peqn[6] );
	    VADD2( larb.pt[4], larb.pt[7], es_peqn[6] );
	    break;

	case 252:   /* protrude face 2367 */
	    VADD2( larb.pt[0], larb.pt[1], es_peqn[6] );
	    VADD2( larb.pt[3], larb.pt[2], es_peqn[6] );
	    VADD2( larb.pt[4], larb.pt[5], es_peqn[6] );
	    VADD2( larb.pt[7], larb.pt[6], es_peqn[6] );
	    break;

	case 160:   /* protrude face 1548 */
	    VADD2( larb.pt[1], larb.pt[0], es_peqn[6] );
	    VADD2( larb.pt[5], larb.pt[4], es_peqn[6] );
	    VADD2( larb.pt[2], larb.pt[3], es_peqn[6] );
	    VADD2( larb.pt[6], larb.pt[7], es_peqn[6] );
	    break;

	case 120:
	case 180:
	    Tcl_AppendResult(interp, "ARB6: extrusion of face ", argv[1],
			     " not allowed\n", (char *)NULL);
	    return TCL_ERROR;

	default:
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
    }

    /* redo the plane equations */
    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, &larb, es_type, es_peqn, &mged_tol))
    {
	Tcl_AppendResult(interp, bu_vls_addr(&error_msg),
			 "Cannot calculate new plane equations for faces\n",
			 (char *)NULL);
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* copy local copy back to original */
    memcpy((char *)es_int.idb_ptr, (char *)&larb, sizeof( struct rt_arb_internal ));

    /* draw the updated solid */
    replot_editing_solid();
    update_views = 1;

    return TCL_OK;
}

/* define an arb8 using rot fb angles to define a face */
/* Format: a name rot fb	*/
int
f_arbdef(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct directory *dp;
    struct rt_db_internal	internal;
    struct rt_arb_internal	*arb;
    int i, j;
    fastf_t rota, fb;
    vect_t norm1, norm2, norm3;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help arb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
	aexists( argv[1] );
	return TCL_ERROR;
    }

    /* get rotation angle */
    rota = atof( argv[2] ) * degtorad;

    /* get fallback angle */
    fb = atof( argv[3] ) * degtorad;

    BU_GETSTRUCT( arb, rt_arb_internal );
    RT_INIT_DB_INTERNAL( &internal );
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &rt_functab[ID_ARB8];
    internal.idb_ptr = (genptr_t)arb;
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* put vertex of new solid at center of screen */
    VSET(arb->pt[0], -view_state->vs_gvp->gv_center[MDX], -view_state->vs_gvp->gv_center[MDY], -view_state->vs_gvp->gv_center[MDZ]);

    /* calculate normal vector defined by rot, fb */
    norm1[0] = cos(fb) * cos(rota);
    norm1[1] = cos(fb) * sin(rota);
    norm1[2] = sin(fb);

    /* find two perpendicular vectors which are perpendicular to norm */
    j = 0;
    for ( i = 0; i < 3; i++ )  {
	if ( fabs(norm1[i]) < fabs(norm1[j]) )
	    j = i;
    }
    VSET( norm2, 0.0, 0.0, 0.0 );
    norm2[j] = 1.0;
    VCROSS( norm3, norm2, norm1 );
    VCROSS( norm2, norm3, norm1 );

    /* create new rpp 20x20x2 */
    /* the 20x20 faces are in rot, fb plane */
    VUNITIZE( norm2 );
    VUNITIZE( norm3 );
    VJOIN1( arb->pt[1], arb->pt[0], 508.0, norm2 );
    VJOIN1( arb->pt[3], arb->pt[0], -508.0, norm3 );
    VJOIN2( arb->pt[2], arb->pt[0], 508.0, norm2, -508.0, norm3 );
    for ( i=0; i<4; i++ )
	VJOIN1( arb->pt[i+4], arb->pt[i], -50.8, norm1 );

    /* no interrupts */
    (void)signal( SIGINT, SIG_IGN );

    if ( (dp=db_diradd( dbip, argv[1], -1L, 0, DIR_SOLID, (genptr_t)&internal.idb_type)) == DIR_NULL )
    {
	rt_db_free_internal(&internal);
	Tcl_AppendResult(interp, "Cannot add ", argv[1], " to directory\n", (char *)NULL );
	return TCL_ERROR;
    }

    if ( rt_db_put_internal( dp, dbip, &internal, &rt_uniresource ) < 0 )
    {
	rt_db_free_internal(&internal);
	TCL_WRITE_ERR_return;
    }

    {
	const char *av[3];

	av[0] = "e";
	av[1] = argv[1]; /* depends on solid name being in argv[1] */
	av[2] = NULL;

	/* draw the "made" solid */
	return cmd_draw( clientData, interp, 2, av );
    }
}

/* Mirface command - mirror an arb face */
/* Format: mirror face axis	*/
int
f_mirface(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int i, j, k;
    static int face;
    static int pt[4];
    static int prod;
    static vect_t work;
    struct rt_arb_internal *arb;
    struct rt_arb_internal larb;	/* local copy of solid */
    struct bu_vls error_msg;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help mirface");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_S_EDIT, "Mirface" ) )
	return TCL_ERROR;

    if ( es_int.idb_type != ID_ARB8 )
    {
	Tcl_AppendResult(interp, "Mirface: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC( arb );

    if (es_type != ARB8 && es_type != ARB6) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ARB%d: mirroring of faces not allowed\n", es_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }
    face = atoi( argv[1] );
    if ( face > 9999 || (face < 1000 && es_type != ARB6) ) {
	Tcl_AppendResult(interp, "ERROR: ", argv[1], " bad face\n", (char *)NULL);
	return TCL_ERROR;
    }
    /* check which axis */
    k = -1;
    if ( strcmp( argv[2], "x" ) == 0 )
	k = 0;
    if ( strcmp( argv[2], "y" ) == 0 )
	k = 1;
    if ( strcmp( argv[2], "z" ) == 0 )
	k = 2;
    if ( k < 0 ) {
	Tcl_AppendResult(interp, "axis must be x, y or z\n", (char *)NULL);
	return TCL_ERROR;
    }

    work[0] = work[1] = work[2] = 1.0;
    work[k] = -1.0;

    /* make local copy of arb */
    memcpy((char *)&larb, (char *)arb, sizeof( struct rt_arb_internal ));

    if (es_type == ARB6 && face < 1000) {
  	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    }
    else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	 10	    125 for ARB6
     *	 72	    346 for ARB6
     */
    prod = 1;
    for ( i = 0; i <= 3; i++ )  {
	prod *= pt[i];
	pt[i]--;
	if ( pt[i] > 7 )  {
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    /* mirror the selected face */
    switch ( prod )  {

	case 24:   /* mirror face 1234 */
	    if (es_type == ARB6) {
		Tcl_AppendResult(interp, "ARB6: mirroring of face ", argv[1],
				 " not allowed\n", (char *)NULL);
		return TCL_ERROR;
	    }
	    for ( i = 0; i < 4; i++ )  {
		j = i + 4;
		VELMUL( larb.pt[j], larb.pt[i], work );
	    }
	    break;

	case 1680:   /* mirror face 5678 */
	    for ( i = 0; i < 4; i++ )  {
		j = i + 4;
		VELMUL( larb.pt[i], larb.pt[j], work );
	    }
	    break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
	    VELMUL( larb.pt[3], larb.pt[0], work );
	    VELMUL( larb.pt[2], larb.pt[1], work );
	    VELMUL( larb.pt[7], larb.pt[4], work );
	    VELMUL( larb.pt[6], larb.pt[5], work );
	    break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
	    VELMUL( larb.pt[0], larb.pt[3], work );
	    VELMUL( larb.pt[1], larb.pt[2], work );
	    VELMUL( larb.pt[5], larb.pt[6], work );
	    VELMUL( larb.pt[4], larb.pt[7], work );
	    break;

	case 252:   /* mirror face 2367 */
	    VELMUL( larb.pt[0], larb.pt[1], work );
	    VELMUL( larb.pt[3], larb.pt[2], work );
	    VELMUL( larb.pt[4], larb.pt[5], work );
	    VELMUL( larb.pt[7], larb.pt[6], work );
	    break;

	case 160:   /* mirror face 1548 */
	    VELMUL( larb.pt[1], larb.pt[0], work );
	    VELMUL( larb.pt[5], larb.pt[4], work );
	    VELMUL( larb.pt[2], larb.pt[3], work );
	    VELMUL( larb.pt[6], larb.pt[7], work );
	    break;

	case 120:
	case 180:
	    Tcl_AppendResult(interp, "ARB6: mirroring of face ", argv[1],
			     " not allowed\n", (char *)NULL);
	    return TCL_ERROR;
	default:
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
    }

    /* redo the plane equations */
    bu_vls_init(&error_msg);
    if (rt_arb_calc_planes(&error_msg, &larb, es_type, es_peqn, &mged_tol)) {
	Tcl_AppendResult( interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* copy to original */
    memcpy((char *)arb, (char *)&larb, sizeof( struct rt_arb_internal ));

    /* draw the updated solid */
    replot_editing_solid();
    view_state->vs_flag = 1;

    return TCL_OK;
}

/* Edgedir command:  define the direction of an arb edge being moved
 *	Format:  edgedir deltax deltay deltaz
 OR  edgedir rot fb
*/

int
f_edgedir(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int i;
    vect_t slope;
    fastf_t rot, fb;

    if (argc < 3 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help edgedir");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_S_EDIT, "Edgedir" ) )
	return TCL_ERROR;

    if ( es_edflag != EARB ) {
	Tcl_AppendResult(interp, "Not moving an ARB edge\n", (char *)NULL);
	return TCL_ERROR;
    }

    if ( es_int.idb_type != ID_ARB8 )
    {
	Tcl_AppendResult(interp, "Edgedir: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* set up slope -
     *	if 2 values input assume rot, fb used
     *	else assume delta_x, delta_y, delta_z
     */
    if ( argc == 3 ) {
	rot = atof( argv[1] ) * degtorad;
	fb = atof( argv[2] ) * degtorad;
	slope[0] = cos(fb) * cos(rot);
	slope[1] = cos(fb) * sin(rot);
	slope[2] = sin(fb);
    }
    else {
	for (i=0; i<3; i++) {
	    /* put edge slope in slope[] array */
	    slope[i] = atof( argv[i+1] );
	}
    }

    if (MAGNITUDE(slope) == 0) {
	Tcl_AppendResult(interp, "BAD slope\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* get it done */
    newedge = 1;
    editarb( slope );
#if 1
    sedit();
    return TCL_OK;
#else
    sedraw++;

    return TCL_ERROR;
#endif
}


/*	EXT4TO6():	extrudes face pt1 pt2 pt3 of an ARB4 "distance"
 *			to produce ARB6
 */
void
ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb)
{
    point_t pts[8];
    int i;

    VMOVE(pts[0], arb->pt[pt1]);
    VMOVE(pts[1], arb->pt[pt2]);
    VMOVE(pts[4], arb->pt[pt3]);
    VMOVE(pts[5], arb->pt[pt3]);

    /* extrude "distance" to get remaining points */
    VADD2(pts[2], pts[1], &es_peqn[6][0]);
    VADD2(pts[3], pts[0], &es_peqn[6][0]);
    VADD2(pts[6], pts[4], &es_peqn[6][0]);
    VMOVE(pts[7], pts[6]);

    /* copy to the original record */
    for ( i=0; i<8; i++ )
	VMOVE(arb->pt[i], pts[i])

	    }

/* Permute command - permute the vertex labels of an ARB
 * Format: permute tuple	*/

/*
 *	     Minimum and maximum tuple lengths
 *     ------------------------------------------------
 *	Solid	# vertices needed	# vertices
 *	type	 to disambiguate	in THE face
 *     ------------------------------------------------
 *	ARB4		3		    3
 *	ARB5		2		    4
 *	ARB6		2		    4
 *	ARB7		1		    4
 *	ARB8		3		    4
 *     ------------------------------------------------
 */
int
f_permute(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    /*
     *	1) Why were all vars declared static?
     *	2) Recompute plane equations?
     */
    int 	vertex, i, k;
    int			arglen;
    int			face_size;	/* # vertices in THE face */
    char		**p;
    struct rt_arb_internal	*arb;
    struct rt_arb_internal	larb;		/* local copy of solid */
    struct rt_arb_internal	tarb;		/* temporary copy of solid */
    static int		min_tuple_size[9] = {0, 0, 0, 0, 3, 2, 2, 1, 3};
    /*
     *			The Permutations
     *
     *	     Each permutation is encoded as an 8-character string,
     *	where the ith character specifies which of the current vertices
     *	(1 through n for an ARBn) should assume the role of vertex i.
     *	Wherever the internal representation of the ARB as an ARB8
     *	stores a redundant copy of a vertex, the string contains a '*'.
     */
    static char 	*perm4[4][7] =
	{
	    {"123*4***", "124*3***", "132*4***", "134*2***", "142*3***",
	     "143*2***", 0},
	    {"213*4***", "214*3***", "231*4***", "234*1***", "241*3***",
	     "243*1***", 0},
	    {"312*4***", "314*2***", "321*4***", "324*1***", "341*2***",
	     "342*1***", 0},
	    {"412*3***", "413*2***", "421*3***", "423*1***", "431*2***",
	     "432*1***", 0}
	};
    static char 	*perm5[5][3] =
	{
	    {"12345***", "14325***", 0},
	    {"21435***", "23415***", 0},
	    {"32145***", "34125***", 0},
	    {"41235***", "43215***", 0},
	    {0, 0, 0}
	};
    static char 	*perm6[6][3] =
	{
	    {"12345*6*", "15642*3*", 0},
	    {"21435*6*", "25631*4*", 0},
	    {"34126*5*", "36524*1*", 0},
	    {"43216*5*", "46513*2*", 0},
	    {"51462*3*", "52361*4*", 0},
	    {"63254*1*", "64153*2*", 0}
	};
    static char		*perm7[7][2] =
	{
	    {"1234567*", 0},
	    {0, 0},
	    {0, 0},
	    {"4321576*", 0},
	    {0, 0},
	    {"6237514*", 0},
	    {"7326541*", 0}
	};
    static char		*perm8[8][7] =
	{
	    {"12345678", "12654378", "14325876", "14852376",
	     "15624873", "15842673", 0},
	    {"21436587", "21563487", "23416785", "23761485",
	     "26513784", "26731584", 0},
	    {"32147658", "32674158", "34127856", "34872156",
	     "37624851", "37842651", 0},
	    {"41238567", "41583267", "43218765", "43781265",
	     "48513762", "48731562", 0},
	    {"51268437", "51486237", "56218734", "56781234",
	     "58416732", "58761432", 0},
	    {"62157348", "62375148", "65127843", "65872143",
	     "67325841", "67852341", 0},
	    {"73268415", "73486215", "76238514", "76583214",
	     "78436512", "78563412", 0},
	    {"84157326", "84375126", "85147623", "85674123",
	     "87345621", "87654321", 0}
	};
    static int	vert_loc[] =
	{
	    /*		-----------------------------
	     *		   Array locations in which
	     *		   the vertices are stored
	     *		-----------------------------
	     *		1   2   3   4   5   6   7   8
	     *		-----------------------------
	     * ARB4 */	0,  1,  2,  4, -1, -1, -1, -1,
	     /* ARB5 */	0,  1,  2,  3,  4, -1, -1, -1,
	     /* ARB6 */	0,  1,  2,  3,  4,  6, -1, -1,
	     /* ARB7 */	0,  1,  2,  3,  4,  5,  6, -1,
	     /* ARB8 */	0,  1,  2,  3,  4,  5,  6,  7
	};
#define		ARB_VERT_LOC(n, v)	vert_loc[((n) - 4) * 8 + (v) - 1]

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help permute");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(ST_S_EDIT, "Permute"))
	return TCL_ERROR;

    if ( es_int.idb_type != ID_ARB8 )
    {
	Tcl_AppendResult(interp, "Permute: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    if ((es_type < 4) || (es_type > 8))
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "Permute: es_type=%d\nThis shouldn't happen\n", es_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC( arb );

    /* make a local copy of the solid */
    memcpy((char *)&larb, (char *)arb, sizeof( struct rt_arb_internal ));

    /*
     *	Find the encoded form of the specified permutation,
     *	if it exists
     */
    arglen = strlen(argv[1]);
    if (arglen < min_tuple_size[es_type])
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ERROR: tuple '%s' too short to disambiguate ARB%d face\n",
		      argv[1], es_type);
	bu_vls_printf(&tmp_vls, "Need at least %d vertices\n", min_tuple_size[es_type]);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }
    face_size = (es_type == 4) ? 3 : 4;
    if (arglen > face_size)
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ERROR: tuple '%s' length exceeds ARB%d face size of %d\n",
		      argv[1], es_type, face_size);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }
    vertex = argv[1][0] - '1';
    if ((vertex < 0) || (vertex >= es_type))
    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ERROR: invalid vertex %c\n", argv[1][0]);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_ERROR;
    }
    p = (es_type == 4) ? perm4[vertex] :
	(es_type == 5) ? perm5[vertex] :
	(es_type == 6) ? perm6[vertex] :
	(es_type == 7) ? perm7[vertex] : perm8[vertex];
    for (;; ++p)
    {
	if (*p == 0)
	{
	    Tcl_AppendResult(interp, "ERROR: invalid vertex tuple: '",
			     argv[1], "'\n", (char *)NULL);
	    return TCL_ERROR;
	}
	if (strncmp(*p, argv[1], arglen) == 0)
	    break;
    }

    /*
     *	Collect the vertices in the specified order
     */
    for (i = 0; i < 8; ++i)
    {
	char	buf[2];

	if ((*p)[i] == '*')
	    continue;
	sprintf(buf, "%c", (*p)[i]);
	k = atoi(buf);
	VMOVE( tarb.pt[i], larb.pt[ARB_VERT_LOC(es_type, k)]);
    }

#if 0
    bu_log("After collection...\n");
    for (i = 0; i < 8; i++ )
    {
	char	string[1024] = {0};

	sprintf(string, "vertex %d", i + 1);
	VPRINT(string, tarb.pt[i]);
    }
    bu_log("...\n");
#endif

    /*
     *	Reinstall the permuted vertices back into the temporary buffer,
     *	copying redundant vertices as necessay
     *
     *		-------+-------------------------
     *		 Solid |    Redundant storage
     *		  Type | of some of the vertices
     *		-------+-------------------------
     *		 ARB4  |    3=0, 5=6=7=4
     *		 ARB5  |    5=6=7=4
     *		 ARB6  |    5=4, 7=6
     *		 ARB7  |    7=4
     *		 ARB8  |
     *		-------+-------------------------
     */
    for (i = 0; i < 8; i++ )
    {
	VMOVE( larb.pt[i], tarb.pt[i]);
    }
    switch (es_type)
    {
	case ARB4:
	    VMOVE(larb.pt[3], larb.pt[0]);
	    /* break intentionally left out */
	case ARB5:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[6], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB6:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[6]);
	    break;
	case ARB7:
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB8:
	    break;
	default:
	{
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "%s: %d: This shouldn't happen\n", __FILE__, __LINE__);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	    return TCL_ERROR;
	}
    }

    /* copy back to original arb */
    memcpy((char *)arb, (char *)&larb, sizeof( struct rt_arb_internal ));

    /* draw the updated solid */
    replot_editing_solid();
    view_state->vs_flag = 1;

    return TCL_OK;
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
