/*
 *			I N S I D E 
 *
 *	Given an outside solid and desired thicknesses, finds
 *	an inside solid to produce those thicknesses.
 *
 * Functions -
 *	f_inside	reads all the input required
 *	arbin		finds inside of arbs
 *	tgcin		finds inside of tgcs
 *	ellgin		finds inside of ellgs
 *	torin		finds inside of tors
 *
 *  Authors -
 *	Keith A Applin
 *	Michael Markowski
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

BU_EXTERN( void nmg_invert_shell , ( struct shell *s , CONST struct bn_tol *tol ) );
BU_EXTERN( struct shell *nmg_extrude_shell , ( struct shell *s, fastf_t thick , int normal_ward , int approximate , CONST struct bn_tol *tol ) );

extern struct rt_db_internal	es_int;	/* from edsol.c */
extern struct bn_tol		mged_tol;	/* from ged.c */

extern char	**promp;	/* pointer to a pointer to a char */

static char *p_arb4[] = {
	"Enter thickness for face 123: ",
	"Enter thickness for face 124: ",
	"Enter thickness for face 234: ",
	"Enter thickness for face 134: ",
};

static char *p_arb5[] = {
	"Enter thickness for face 1234: ",
	"Enter thickness for face 125: ",
	"Enter thickness for face 235: ",
	"Enter thickness for face 345: ",
	"Enter thickness for face 145: ",
};

static char *p_arb6[] = {
	"Enter thickness for face 1234: ",
	"Enter thickness for face 2356: ",
	"Enter thickness for face 1564: ",
	"Enter thickness for face 125: ",
	"Enter thickness for face 346: ",
};

static char *p_arb7[] = {
	"Enter thickness for face 1234: ",
	"Enter thickness for face 567: ",
	"Enter thickness for face 145: ",
	"Enter thickness for face 2376: ",
	"Enter thickness for face 1265: ",
	"Enter thickness for face 3475: ",
};

static char *p_arb8[] = {
	"Enter thickness for face 1234: ",
	"Enter thickness for face 5678: ",
	"Enter thickness for face 1485: ",
	"Enter thickness for face 2376: ",
	"Enter thickness for face 1265: ",
	"Enter thickness for face 3478: ",
};

static char *p_tgcin[] = {
	"Enter thickness for base (AxB): ",
	"Enter thickness for top (CxD): ",
	"Enter thickness for side: ",
};

static char *p_rpcin[] = {
	"Enter thickness for front plate (contains V): ",
	"Enter thickness for back plate: ",
	"Enter thickness for top plate: ",
	"Enter thickness for body: ",
};

static char *p_rhcin[] = {
	"Enter thickness for front plate (contains V): ",
	"Enter thickness for back plate: ",
	"Enter thickness for top plate: ",
	"Enter thickness for body: ",
};

static char *p_epain[] = {
	"Enter thickness for top plate: ",
	"Enter thickness for body: ",
};

static char *p_ehyin[] = {
	"Enter thickness for top plate: ",
	"Enter thickness for body: ",
};

static char *p_etoin[] = {
	"Enter thickness for body: ",
};

static char *p_nmgin[] = {
	"Enter thickness for shell: ",
};

/*	F _ I N S I D E ( ) :	control routine...reads all data
 */
int
f_inside(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	register int i;
	struct directory	*dp;
	struct directory	*outdp;
	mat_t newmat;
	int	cgtype;		/* cgtype ARB 4..8 */
	int	nface;
	fastf_t	thick[6];
	plane_t	planes[6];
	struct rt_db_internal	intern;
	char	*newname;
	int arg = 1;
	int status = TCL_OK;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	/* SCHEME:
	 *	if in solid edit, use "edited" solid
	 *	if in object edit, use "key" solid
	 *	else get solid name to use
	 */

	if( state == ST_S_EDIT ) {
	  /* solid edit mode */
	  /* apply es_mat editing to parameters */
	  transform_editing_solid( &intern, es_mat, &es_int, 0 );
	  outdp = illump->s_path[illump->s_last];

	  Tcl_AppendResult(interp, "Outside solid: ", (char *)NULL);
	  for(i=0; i <= illump->s_last; i++) {
	    Tcl_AppendResult(interp, "/", illump->s_path[i]->d_namep, (char *)NULL);
	  }
	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	}  else if( state == ST_O_EDIT ) {
	  /* object edit mode */
	  if( illump->s_Eflag ) {
	     Tcl_AppendResult(interp, "Cannot find inside of a processed (E'd) region\n",
			      (char *)NULL);
	     status = TCL_ERROR;
	     goto end;
	  }
	  /* use the solid at bottom of path (key solid) */
	  /* apply es_mat and modelchanges editing to parameters */
	  bn_mat_mul(newmat, modelchanges, es_mat);
	  transform_editing_solid( &intern, newmat, &es_int, 0 );
	  outdp = illump->s_path[illump->s_last];

	  Tcl_AppendResult(interp, "Outside solid: ", (char *)NULL);
	  for(i=0; i <= illump->s_last; i++) {
	    Tcl_AppendResult(interp, "/", illump->s_path[i]->d_namep, (char *)NULL);
	  }
	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	} else {
	  /* Not doing any editing....ask for outside solid */
	  if( argc < arg+1 ) {
	    Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter name of outside solid: ",
			     (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  if( (outdp = db_lookup( dbip,  argv[arg], LOOKUP_NOISY )) == DIR_NULL ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  ++arg;

	  if( rt_db_get_internal( &intern, outdp, dbip, bn_mat_identity ) < 0 ) {
	    (void)signal( SIGINT, SIG_IGN );
	    TCL_READ_ERR_return;
	  }
	}

	if( intern.idb_type == ID_ARB8 )  {
	  /* find the comgeom arb type, & reorganize */
	  int uvec[8],svec[8];

	  if( rt_arb_get_cgtype( &cgtype , intern.idb_ptr, &mged_tol , uvec , svec ) == 0 ) {
	    Tcl_AppendResult(interp, outdp->d_namep, ": BAD ARB\n", (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }

	  /* must find new plane equations to account for
	   * any editing in the es_mat matrix or path to this solid.
	   */
	  if( rt_arb_calc_planes( planes, intern.idb_ptr, cgtype, &mged_tol ) < 0 )  {
	    Tcl_AppendResult(interp, "rt_arb_calc_planes(", outdp->d_namep,
			     "): failed\n", (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	}
	/* "intern" is now loaded with the outside solid data */

	/* get the inside solid name */
	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter name of the inside solid: ",
			   (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}
	if( db_lookup( dbip, argv[arg], LOOKUP_QUIET ) != DIR_NULL ) {
	  aexists( argv[arg] );
	  status = TCL_ERROR;
	  goto end;
	}
	if( (int)strlen(argv[arg]) >= NAMESIZE )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "Names are limited to %d characters\n", NAMESIZE-1);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  status = TCL_ERROR;
	  goto end;
	}
	newname = argv[arg];
	++arg;

	/* get thicknesses and calculate parameters for newrec */
	switch( intern.idb_type )  {

	case ID_ARB8:
	    {
	    	struct rt_arb_internal *arb =
			(struct rt_arb_internal *)intern.idb_ptr;

		nface = 6;

		switch( cgtype ) {
		case 8:
			promp = p_arb8;
			break;

		case 7:
			promp = p_arb7;
			break;

		case 6:
			promp = p_arb6;
			nface = 5;
			VMOVE( arb->pt[5], arb->pt[6] );
			break;

		case 5:
			promp = p_arb5;
			nface = 5;
			break;

		case 4:
			promp = p_arb4;
			nface = 4;
			VMOVE( arb->pt[3], arb->pt[4] );
			break;
		}

		for(i=0; i<nface; i++) {
		  if( argc < arg+1 ) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
		    status = TCL_ERROR;
		    goto end;
		  }
		  thick[i] = atof(argv[arg]) * local2base;
		  ++arg;
		}

		if( arbin(&intern, thick, nface, cgtype, planes) ){
		  status = TCL_ERROR;
		  goto end;
		}
		break;
	    }

	case ID_TGC:
	  promp = p_tgcin;
	  for(i=0; i<3; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }

	  if( tgcin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_ELL:
	  if( argc < arg+1 ) {
	    Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter desired thickness: ", (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  thick[0] = atof( argv[arg] ) * local2base;
	  ++arg;

	  if( ellgin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_TOR:
	  if( argc < arg+1 ) {
	    Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter desired thickness: ", (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  thick[0] = atof( argv[arg] ) * local2base;
	  ++arg;

	  if( torin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_RPC:
	  promp = p_rpcin;
	  for (i = 0; i < 4; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }

	  if( rpcin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_RHC:
	  promp = p_rhcin;
	  for (i = 0; i < 4; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }

	  if( rhcin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_EPA:
	  promp = p_epain;
	  for (i = 0; i < 2; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }

	  if( epain(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_EHY:
	  promp = p_ehyin;
	  for (i = 0; i < 2; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }
	  
	  if( ehyin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_ETO:
	  promp = p_etoin;
	  for (i = 0; i < 1; i++) {
	    if( argc < arg+1 ) {
	      Tcl_AppendResult(interp, MORE_ARGS_STR, promp[i], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	    thick[i] = atof( argv[arg] ) * local2base;
	    ++arg;
	  }

	  if( etoin(&intern, thick) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	case ID_NMG:
	  promp = p_nmgin;
	  if( argc < arg+1 ) {
	    Tcl_AppendResult(interp, MORE_ARGS_STR, promp[0], (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  thick[0] = atof( argv[arg] ) * local2base;
	  ++arg;
	  if( nmgin( &intern , thick[0] ) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;

	default:
	  Tcl_AppendResult(interp, "Cannot find inside for '",
			   rt_functab[intern.idb_type].ft_name, "' solid\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	/* don't allow interrupts while we update the database! */
	(void)signal( SIGINT, SIG_IGN);
 
	/* Add to in-core directory */
	if( (dp = db_diradd( dbip,  newname, -1, 0, DIR_SOLID )) == DIR_NULL )  {
	  (void)signal( SIGINT, SIG_IGN );
	  TCL_ALLOC_ERR_return;
	}
	if( rt_db_put_internal( dp, dbip, &intern ) < 0 ) {
	  (void)signal( SIGINT, SIG_IGN );
	  TCL_WRITE_ERR_return;
	}

	/* Draw the new solid */
	{
		char	*arglist[3];
		arglist[0] = "e";
		arglist[1] = newname;
		arglist[2] = NULL;
		return f_edit(clientData, interp, 2, arglist );
	}
end:
	(void)signal( SIGINT, SIG_IGN );
	return status;
}



/* finds inside arbs */
int
arbin(ip, thick, nface, cgtype, planes)
struct rt_db_internal	*ip;
fastf_t	thick[6];
int	nface;
int	cgtype;		/* # of points, 4..8 */
plane_t	planes[6];
{
	struct rt_arb_internal	*arb = (struct rt_arb_internal *)ip->idb_ptr;
	point_t		center_pt;
	int		num_pts=8;	/* number of points to solve using rt_arb_3face_intersect */
	int		i;

	RT_ARB_CK_MAGIC(arb);

	/* find reference point (center_pt[3]) to find direction of normals */
	rt_arb_centroid( center_pt, arb, cgtype );

	/* move new face planes for the desired thicknesses
	 * don't do this yet for an arb7 */
	if( cgtype != 7 )
	{
		for(i=0; i<nface; i++) {
			if( (planes[i][3] - VDOT(center_pt, &planes[i][0])) > 0.0 )
				thick[i] *= -1.0;
			planes[i][3] += thick[i];
		}
	}

	if( cgtype == 5 ) 
		num_pts = 4;	/* use rt_arb_3face_intersect for first 4 points */
	else if( cgtype == 7 )
		num_pts = 0;	/* don't use rt_arb_3face_intersect for any points */

	/* find the new vertices by intersecting the new face planes */
	for(i=0; i<num_pts; i++) {
	  if( rt_arb_3face_intersect( arb->pt[i], planes, cgtype, i*3 ) < 0 )  {
	    Tcl_AppendResult(interp, "cannot find inside arb\n", (char *)NULL);
	    return(1);
	  }
	}

	/* The following is code for the special cases of arb5 and arb7
	 * These arbs have a vertex that is the intersection of four planes, and
	 * the inside solid may have a single vertex or an edge replacing this vertex
	 */
	if( cgtype == 5 )
	{
	  /* Here we are only concerned with the one vertex where 4 planes intersect
	   * in the original solid
	   */
	  point_t pt[4];
	  fastf_t dist0,dist1;
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  
	  /* calculate the four possible intersect points */
	  if( bn_mkpoint_3planes( pt[0] , planes[1] , planes[2] , planes[3] ) )
	    {
	      bu_vls_printf(&tmp_vls, "Cannot find inside arb5\n" );
	      bu_vls_printf(&tmp_vls, "Cannot find intersection of three planes for point 0:\n" );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[1] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[2] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[3] ) );
	      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	      bu_vls_free(&tmp_vls);
	      return( 1 );
	    }
	  if( bn_mkpoint_3planes( pt[1] , planes[2] , planes[3] , planes[4] ) )
	    {
	      bu_vls_printf(&tmp_vls, "Cannot find inside arb5\n" );
	      bu_vls_printf(&tmp_vls, "Cannot find intersection of three planes for point 1:\n" );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[2] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[3] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[4] ) );
	      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	      bu_vls_free(&tmp_vls);
	      return( 1 );
	    }
	  if( bn_mkpoint_3planes( pt[2] , planes[3] , planes[4] , planes[1] ) )
	    {
	      bu_vls_printf(&tmp_vls, "Cannot find inside arb5\n" );
	      bu_vls_printf(&tmp_vls, "Cannot find intersection of three planes for point 2:\n" );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[3] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[4] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[1] ) );
	      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	      bu_vls_free(&tmp_vls);
	      return( 1 );
	    }
	  if( bn_mkpoint_3planes( pt[3] , planes[4] , planes[1] , planes[2] ) )
	    {
	      bu_vls_printf(&tmp_vls, "Cannot find inside arb5\n" );
	      bu_vls_printf(&tmp_vls, "Cannot find intersection of three planes for point 3:\n" );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[4] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[1] ) );
	      bu_vls_printf(&tmp_vls, "\t%f %f %f %f\n" , V4ARGS( planes[2] ) );
	      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	      bu_vls_free(&tmp_vls);
	      return( 1 );
	    }
			
		if( bn_pt3_pt3_equal( pt[0] , pt[1] , &mged_tol ) )
		{
			/* if any two of the calculates intersection points are equal,
			 * then all four must be equal
			 */
			for( i=4 ; i<8 ; i++ )
				VMOVE( arb->pt[i] , pt[0] );

			return( 0 );
		}

		/* There will be an edge where the four planes come together
		 * Two edges of intersection have been calculated
		 *     pt[0]<->pt[2]
		 *     pt[1]<->pt[3]
		 * the one closest to the non-invloved plane (planes[0]) is the
		 * one we want
		 */

		dist0 = DIST_PT_PLANE( pt[0] , planes[0] );
		if( dist0 < 0.0 )
			dist0 = (-dist0);

		dist1 = DIST_PT_PLANE( pt[1] , planes[0] );
		if( dist1 < 0.0 )
			dist1 = (-dist1);

		if( dist0 < dist1 )
		{
			VMOVE( arb->pt[5] , pt[0] );
			VMOVE( arb->pt[6] , pt[0] );
			VMOVE( arb->pt[4] , pt[2] );
			VMOVE( arb->pt[7] , pt[2] );
		}
		else
		{
			VMOVE( arb->pt[4] , pt[3] );
			VMOVE( arb->pt[5] , pt[3] );
			VMOVE( arb->pt[6] , pt[1] );
			VMOVE( arb->pt[7] , pt[1] );
		}
	}
	else if( cgtype == 7 )
	{
		struct model *m;
		struct nmgregion *r;
		struct shell *s;
		struct faceuse *fu;
		struct rt_tess_tol ttol;
		struct bu_ptbl vert_tab;

		ttol.magic = RT_TESS_TOL_MAGIC;
		ttol.abs = mged_abs_tol;
		ttol.rel = mged_rel_tol;
		ttol.norm = mged_nrm_tol;

		/* Make a model to hold the inside solid */
		m = nmg_mm();

		/* get an NMG version of this arb7 */
		if( rt_functab[ip->idb_type].ft_tessellate( &r , m , ip , &ttol , &mged_tol ) )
		{
		  Tcl_AppendResult(interp, "Cannot tessellate arb7\n", (char *)NULL);
		  rt_functab[ip->idb_type].ft_ifree( ip );
		  return( 1 );
		}

		/* move face planes */
		for( i=0 ; i<nface ; i++ )
		{
			int found=0;

			/* look for the face plane with the same geometry as the arb7 planes */
			s = BU_LIST_FIRST( shell , &r->s_hd );
			for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				struct face_g_plane *fg;
				plane_t pl;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					continue;

				NMG_GET_FU_PLANE( pl , fu );
				if( bn_coplanar( planes[i] , pl , &mged_tol ) > 0 )
				{
					/* found the NMG face geometry that matches arb face i */
					found = 1;
					fg = fu->f_p->g.plane_p;
					NMG_CK_FACE_G_PLANE( fg );

					/* move the face by distance "thick[i]" */
					if( fu->f_p->flip )
						fg->N[3] += thick[i];
					else
						fg->N[3] -= thick[i];

					break;
				}
			}
			if( !found )
			{
			  struct bu_vls tmp_vls;

			  bu_vls_init(&tmp_vls);
			  bu_vls_printf(&tmp_vls,"Could not move face plane for arb7, face #%d\n",
					i );
			  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			  bu_vls_free(&tmp_vls);
			  nmg_km( m );
			  return( 1 );
			}
		}

		/* solve for new vertex geometry
		 * This does all the vertices
		 */
		bu_ptbl( &vert_tab , BU_PTBL_INIT , (long *)NULL );
		nmg_vertex_tabulate( &vert_tab , &m->magic );
		for( i=0 ; i<BU_PTBL_END( &vert_tab ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)BU_PTBL_GET( &vert_tab , i );
			NMG_CK_VERTEX( v );

			if( nmg_in_vert( v , 0 , &mged_tol ) )
			{
			  Tcl_AppendResult(interp, "Could not find coordinates for inside arb7\n",
					   (char *)NULL);
			  nmg_km( m );
			  bu_ptbl( &vert_tab , BU_PTBL_FREE , (long *)NULL );
			  return( 1 );
			}
		}
		bu_ptbl( &vert_tab , BU_PTBL_FREE , (long *)NULL );

		/* rebound model */
		nmg_rebound( m , &mged_tol );

		nmg_extrude_cleanup( s , 0 , &mged_tol );

		/* free old ip pointer */
		rt_db_free_internal( ip );

		/* put new solid in "ip" */
		ip->idb_type = ID_NMG;
		ip->idb_ptr = (genptr_t)m;
	}

	return(0);
}

/*	Calculates inside TGC
 *
 * thick[0] is thickness for base (AxB)
 * thick[1] is thickness for top (CxD)
 * thick[2] is thickness for side
 */
int
tgcin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[6];
{
	struct rt_tgc_internal	*tgc = (struct rt_tgc_internal *)ip->idb_ptr;
	vect_t norm;		/* unit vector normal to base */
	fastf_t normal_height;	/* height in direction normal to base */
	vect_t v,h,a,b,c,d;	/* parameters for inside TGC */
	point_t top;		/* vertex at top of inside TGC */
	fastf_t mag_a,mag_b,mag_c,mag_d; /* lengths of original semi-radii */
	fastf_t new_mag_a,new_mag_b,new_mag_c,new_mag_d; /* new lengths */
	vect_t unit_a,unit_b,unit_c,unit_d; /* unit vectors along semi radii */
	fastf_t ratio;

	RT_TGC_CK_MAGIC(tgc);

	VCROSS( norm, tgc->a, tgc->b )
	VUNITIZE( norm )

	normal_height = VDOT( norm, tgc->h );
	if( normal_height < 0.0 )
	{
		normal_height = (-normal_height);
		VREVERSE( norm, norm )
	}

	if( (thick[0] + thick[1]) >= normal_height )
	{
		Tcl_AppendResult(interp, "TGC shorter than base and top thicknesses\n", (char *)NULL);
		return( 1 );
	}

	mag_a = MAGNITUDE( tgc->a );
	mag_b = MAGNITUDE( tgc->b );
	mag_c = MAGNITUDE( tgc->c );
	mag_d = MAGNITUDE( tgc->d );

	if(( mag_a < VDIVIDE_TOL && mag_c < VDIVIDE_TOL ) ||
	   ( mag_b < VDIVIDE_TOL && mag_d < VDIVIDE_TOL ) )
	{
		Tcl_AppendResult(interp, "TGC is too small too create inside solid", (char *)NULL );
		return( 1 );
	}

	if( mag_a >= VDIVIDE_TOL )
		VSCALE( unit_a, tgc->a, 1.0/mag_a )
	else if( mag_c >= VDIVIDE_TOL )
		VSCALE( unit_a, tgc->c, 1.0/mag_c )

	if( mag_c >= VDIVIDE_TOL )
		VSCALE( unit_c, tgc->c, 1.0/mag_c )
	else if( mag_a >= VDIVIDE_TOL )
		VSCALE( unit_c, tgc->a, 1.0/mag_a )

	if( mag_b >= VDIVIDE_TOL )
		VSCALE( unit_b, tgc->b, 1.0/mag_b )
	else if( mag_d >= VDIVIDE_TOL )
		VSCALE( unit_b, tgc->d, 1.0/mag_d )

	if( mag_d >= VDIVIDE_TOL )
		VSCALE( unit_d, tgc->d, 1.0/mag_d )
	else if( mag_c >= VDIVIDE_TOL )
		VSCALE( unit_d, tgc->b, 1.0/mag_b )

	/* Calculate new vertex from base thickness */
	if( thick[0] != 0.0 )
	{
		/* calculate new vertex using similar triangles */
		ratio = thick[0]/normal_height;
		VJOIN1( v, tgc->v, ratio, tgc->h )

		/* adjust lengths of a and c to account for new vertex position */
		new_mag_a = mag_a + (mag_c - mag_a)*ratio;
		new_mag_b = mag_b + (mag_d - mag_b)*ratio;
	}
	else /* just copy the existing values */
	{
		VMOVE( v, tgc->v )
		new_mag_a = mag_a;
		new_mag_b = mag_b;
	}

	/* calculate new height vector */
	if( thick[1] != 0.0 )
	{
		/* calculate new height vector using simialr triangles */
		ratio = thick[1]/normal_height;
		VJOIN1( top, tgc->v, 1.0 - ratio, tgc->h )

		/* adjust lengths of c and d */
		new_mag_c = mag_c + (mag_a - mag_c)*ratio;
		new_mag_d = mag_d + (mag_b - mag_d)*ratio;
	}
	else /* just copy existing values */
	{
		VADD2( top, tgc->v, tgc->h )
		new_mag_c = mag_c;
		new_mag_d = mag_d;
	}

	/* calculate new height vector based on new vertex and top */
	VSUB2( h, top, v )

	if( thick[2] != 0.0 )	/* ther is a side thickness */
	{
		vect_t ctoa;	/* unit vector from tip of C to tip of A */
		vect_t dtob;	/* unit vector from tip of D to tip of B */
		point_t pt_a, pt_b, pt_c, pt_d;	/* points at tips of semi radii */
		fastf_t delta_ac, delta_bd;	/* radius change for thickness */
		fastf_t dot;	/* dot product */
		fastf_t ratio1,ratio2;

		if( (thick[2] >= new_mag_a || thick[2] >= new_mag_b) &&
		    (thick[2] >= new_mag_c || thick[2] >= new_mag_d) )
		{
			/* can't make a small enough TGC */
			Tcl_AppendResult(interp, "Side thickness too large\n", (char *)NULL );
			return( 1 );
		}

		/* approach this as two 2D problems. One is in the plane containing
		 * the a, h, and c vectors. The other is in the plane containing
		 * the b, h, and d vectors.
		 * In the ahc plane:
		 * Calculate the amount that both a and c must be changed to produce
		 * a normal thickness of thick[2]. Use the vector from tip of c to tip
		 * of a and the unit_a vector to get sine of angle that the normal
		 * side thickness makes with vector a (and so also with vector c).
		 * The amount vectors a and c must change is thick[2]/(cosine of that angle).
		 * Similar for the bhd plane.
		 */

		/* Calculate unit vectors from tips of c/d to tips of a/b */
		VJOIN1( pt_a, v, new_mag_a, unit_a )
		VJOIN1( pt_b, v, new_mag_b, unit_b )
		VJOIN2( pt_c, v, 1.0, h, new_mag_c, unit_c )
		VJOIN2( pt_d, v, 1.0, h, new_mag_d, unit_d )
		VSUB2( ctoa, pt_a, pt_c )
		VSUB2( dtob, pt_b, pt_d )
		VUNITIZE( ctoa )
		VUNITIZE( dtob )

		/* Calculate amount vectors a and c must change */
		dot = VDOT( ctoa, unit_a );
		delta_ac = thick[2]/sqrt( 1.0 - dot*dot );

		/* Calculate amount vectors d and d must change */
		dot = VDOT( dtob, unit_b );
		delta_bd = thick[2]/sqrt( 1.0 - dot*dot );

		if( (delta_ac > new_mag_a || delta_bd > new_mag_b) &&
		    (delta_ac > new_mag_c || delta_bd > new_mag_d) )
		{
			/* Can't make TGC small enough */
			Tcl_AppendResult(interp, "Side thickness too large\n", (char *)NULL );
			return( 1 );
		}

		/* Check if changes will make vectors a or d lengths negative */
		if( delta_ac >= new_mag_c || delta_bd >= new_mag_d )
		{
			/* top vertex (height) must move. Calculate similar triangle ratios */
			if( delta_ac >= new_mag_c )
				ratio1 = (new_mag_a - delta_ac)/(new_mag_a - new_mag_c);
			else
				ratio1 = 1.0;

			if( delta_bd >= new_mag_d )
				ratio2 = (new_mag_b - delta_bd)/(new_mag_b - new_mag_d);
			else
				ratio2 = 1.0;

			/* choose the smallest similar triangle for setting new top vertex */
			if( ratio1 < ratio2 )
				ratio = ratio1;
			else
				ratio = ratio2;

			if( ratio1 == ratio && ratio1 < 1.0 ) /* c vector must go to zero */
				new_mag_c = SQRT_SMALL_FASTF;
			else if( ratio1 > ratio && ratio < 1.0 )
			{
				/* vector d will go to zero, but vector c will not */

				/* calculate original length of vector c at new top vertex */
				new_mag_c = new_mag_c + (new_mag_a - new_mag_c)*( 1.0 - ratio);

				/* now just subtract delta */
				new_mag_c -= delta_ac;
			}
			else /* just change c vector length by delta */
				new_mag_c -= delta_ac;

			if( ratio2 == ratio && ratio2 < 1.0 ) /* vector d must go to zero */
				new_mag_d = SQRT_SMALL_FASTF;
			else if( ratio2 > ratio && ratio < 1.0 )
			{
				/* calculate vector length at new top vertex */
				new_mag_d = new_mag_d + (new_mag_b - new_mag_d)*(1.0 - ratio);

				/* now just subtract delta */
				new_mag_d -= delta_bd;
			}
			else /* just adjust length */
				new_mag_d -= delta_bd;

			VSCALE( h, h, ratio )
			new_mag_a -= delta_ac;
			new_mag_b -= delta_bd;
		}
		else if( delta_ac >= new_mag_a || delta_bd >= new_mag_b)
		{
			/* base vertex (v) must move */

			/* Calculate similar triangle ratios */
			if( delta_ac >= new_mag_a )
				ratio1 = (new_mag_c - delta_ac)/(new_mag_c - new_mag_a);
			else
				ratio1 = 1.0;

			if( delta_bd >= new_mag_b )
				ratio2 = (new_mag_d - delta_bd)/(new_mag_d - new_mag_b);
			else
				ratio2 = 1.0;

			/* select smallest triangle to set new base vertex */
			if( ratio1 < ratio2 )
				ratio = ratio1;
			else
				ratio = ratio2;

			if( ratio1 == ratio && ratio1 < 1.0 ) /* vector a must go to zero */
				new_mag_a = SQRT_SMALL_FASTF;
			else if( ratio1 > ratio && ratio < 1.0 )
			{
				/* calculate length of vector a if it were at new base location */
				new_mag_a = new_mag_c + (new_mag_a - new_mag_c)*ratio;

				/* now just subtract delta */
				new_mag_a -= delta_ac;
			}
			else /* just subtract delta */
				new_mag_a -= delta_ac;

			if( ratio2 == ratio && ratio2 < 1.0 ) /* vector b must go to zero */
				new_mag_b = SQRT_SMALL_FASTF;
			else if( ratio2 > ratio && ratio < 1.0 )
			{
				/* Calculate length of b if it were at new base vector */
				new_mag_b = new_mag_d + (new_mag_b - new_mag_d)*ratio;

				/* now just subtract delta */
				new_mag_b -= delta_bd;
			}
			else /* just subtract delta */
				new_mag_b -= delta_bd;

			/* adjust height vector using smallest similar triangle ratio */
			VJOIN1( v, v, 1.0-ratio, h )
			VSUB2( h, top, v )
			new_mag_c -= delta_ac;
			new_mag_d -= delta_bd;
		}
		else /* just change the vector lengths */
		{
			new_mag_a -= delta_ac;
			new_mag_b -= delta_bd;
			new_mag_c -= delta_ac;
			new_mag_d -= delta_bd;
		}
	}

	/* copy new values into the TGC */
	VMOVE( tgc->v, v )
	VMOVE( tgc->h, h)
	VSCALE( tgc->a, unit_a, new_mag_a )
	VSCALE( tgc->b, unit_b, new_mag_b )
	VSCALE( tgc->c, unit_c, new_mag_c )
	VSCALE( tgc->d, unit_d, new_mag_d )

	return( 0 );
}

/* finds inside of torus */
int
torin(ip, thick)
struct rt_db_internal	*ip;
fastf_t			thick[6];
{
	struct rt_tor_internal	*tor = (struct rt_tor_internal *)ip->idb_ptr;

	RT_TOR_CK_MAGIC(tor);
	if( thick[0] == 0.0 )
		return(0);

	if( thick[0] < 0 ) {
	  if( (tor->r_h - thick[0]) > (tor->r_a + .01) ) {
	    Tcl_AppendResult(interp, "cannot do: r2 > r1\n", (char *)NULL);
	    return(1);
	  }
	}
	if( thick[0] >= tor->r_h ) {
	  Tcl_AppendResult(interp, "cannot do: r2 <= 0\n", (char *)NULL);
	  return(1);
	}

	tor->r_h = tor->r_h - thick[0];
	return(0);
}


/* finds inside ellg */
int
ellgin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[6];
{
	struct rt_ell_internal	*ell = (struct rt_ell_internal *)ip->idb_ptr;
	int i, j, k, order[3];
	fastf_t mag[3], nmag[3];
	fastf_t ratio;

	if( thick[0] <= 0.0 ) 
		return(0);
	thick[2] = thick[1] = thick[0];	/* uniform thickness */

	RT_ELL_CK_MAGIC(ell);
	mag[0] = MAGNITUDE(ell->a);
	mag[1] = MAGNITUDE(ell->b);
	mag[2] = MAGNITUDE(ell->c);

	for(i=0; i<3; i++) {
		order[i] = i;
	}

	for(i=0; i<2; i++) {
		k = i + 1;
		for(j=k; j<3; j++) {
			if(mag[i] < mag[j])
				order[i] = j;
		}
	}

	if( (ratio = mag[order[1]] / mag[order[0]]) < .8 )
		thick[order[1]] = thick[order[1]]/(1.016447*pow(ratio,.071834));
	if( (ratio = mag[order[2]] / mag[order[1]]) < .8 )
		thick[order[2]] = thick[order[2]]/(1.016447*pow(ratio,.071834));

	for(i=0; i<3; i++) {
	  if( (nmag[i] = mag[i] - thick[i]) <= 0.0 ){
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Warning: new vector [%d] length <= 0 \n", i);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }
	}
	VSCALE(ell->a, ell->a, nmag[0]/mag[0]);
	VSCALE(ell->b, ell->b, nmag[1]/mag[1]);
	VSCALE(ell->c, ell->c, nmag[2]/mag[2]);
	return(0);
}

/* finds inside of rpc, not quite right - r needs to be smaller */
int
rpcin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[4];
{
	struct rt_rpc_internal	*rpc = (struct rt_rpc_internal *)ip->idb_ptr;
	fastf_t			b;
	vect_t			Bu, Hu, Ru;

	RT_RPC_CK_MAGIC(rpc);

	/* get unit coordinate axes */
	VMOVE( Bu, rpc->rpc_B );
	VMOVE( Hu, rpc->rpc_H );
	VCROSS( Ru, Hu, Bu );
	VUNITIZE( Bu );
	VUNITIZE( Hu );
	VUNITIZE( Ru );

	b = MAGNITUDE(rpc->rpc_B);
	VJOIN2( rpc->rpc_V, rpc->rpc_V, thick[0], Hu, thick[2], Bu );
	VSCALE( rpc->rpc_H, Hu, MAGNITUDE(rpc->rpc_H) - thick[0] - thick[1] );
	VSCALE( rpc->rpc_B, Bu, b - thick[2] - thick[3] );
#if 0
	bp = b - thick[2] - thick[3];
	rp = rpc->rpc_r - thick[3];	/* !!! ESTIMATE !!! */
	yp = rp * sqrt( (bp - thick[2])/bp );
	VSET( Norm,
		0.,
		2 * bp * yp/(rp * rp),
		-1.);
	VUNITIZE(Norm)
	th = thick[3] / Norm[Y];
	rpc->rpc_r -= th;
#endif
	rpc->rpc_r -= thick[3];

	return(0);
}

/* XXX finds inside of rhc, not quite right */
int
rhcin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[4];
{
	struct rt_rhc_internal	*rhc = (struct rt_rhc_internal *)ip->idb_ptr;
	vect_t			Bn, Hn, Bu, Hu, Ru;

	RT_RHC_CK_MAGIC(rhc);
	
	VMOVE( Bn, rhc->rhc_B );
	VMOVE( Hn, rhc->rhc_H );
	
	/* get unit coordinate axes */
	VMOVE( Bu, Bn );
	VMOVE( Hu, Hn );
	VCROSS( Ru, Hu, Bu );
	VUNITIZE( Bu );
	VUNITIZE( Hu );
	VUNITIZE( Ru );

	VJOIN2( rhc->rhc_V, rhc->rhc_V, thick[0], Hu, thick[2], Bu );
	VSCALE( rhc->rhc_H, Hu, MAGNITUDE(rhc->rhc_H) - thick[0] - thick[1] );
	VSCALE( rhc->rhc_B, Bu, MAGNITUDE(rhc->rhc_B) - thick[2] - thick[3] );
	rhc->rhc_r -= thick[3];

	return(0);
}

/* finds inside of epa, not quite right */
int
epain(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[2];
{
	struct rt_epa_internal	*epa = (struct rt_epa_internal *)ip->idb_ptr;
	vect_t			Hu;

	RT_EPA_CK_MAGIC(epa);
	
	VMOVE( Hu, epa->epa_H );
	VUNITIZE( Hu );

	VJOIN1( epa->epa_V, epa->epa_V, thick[0], Hu );
	VSCALE( epa->epa_H, Hu, MAGNITUDE(epa->epa_H) - thick[0] - thick[1] );
	epa->epa_r1 -= thick[1];
	epa->epa_r2 -= thick[1];

	return(0);
}

/* finds inside of ehy, not quite right, */
int
ehyin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[2];
{
	struct rt_ehy_internal	*ehy = (struct rt_ehy_internal *)ip->idb_ptr;
	vect_t			Hu;

	RT_EHY_CK_MAGIC(ehy);
	
	VMOVE( Hu, ehy->ehy_H );
	VUNITIZE( Hu );
	
	VJOIN1( ehy->ehy_V, ehy->ehy_V, thick[0], Hu );
	VSCALE( ehy->ehy_H, Hu, MAGNITUDE(ehy->ehy_H) - thick[0] - thick[1] );
	ehy->ehy_r1 -= thick[1];
	ehy->ehy_r2 -= thick[1];

	return(0);
}

/* finds inside of eto */
int
etoin(ip, thick)
struct rt_db_internal	*ip;
fastf_t	thick[1];
{
	fastf_t			c;
	struct rt_eto_internal	*eto = (struct rt_eto_internal *)ip->idb_ptr;

	RT_ETO_CK_MAGIC(eto);

	c = 1. - thick[0]/MAGNITUDE(eto->eto_C);
	VSCALE( eto->eto_C, eto->eto_C, c );
	eto->eto_rd -= thick[0];

	return(0);
}

/* find inside for NMG */
int
nmgin( ip , thick )
struct rt_db_internal	*ip;
fastf_t thick;
{
	struct model *m;
	struct nmgregion *r;

	if( ip->idb_type != ID_NMG )
		return( 1 );

	m = (struct model *)ip->idb_ptr;
	NMG_CK_MODEL( m );

	r = BU_LIST_FIRST( nmgregion ,  &m->r_hd );
	while( BU_LIST_NOT_HEAD( r , &m->r_hd ) )
	{
		struct nmgregion *next_r;
		struct shell *s;

		NMG_CK_REGION( r );

		next_r = BU_LIST_PNEXT( nmgregion , &r->l );

		s = BU_LIST_FIRST( shell , &r->s_hd );
		while( BU_LIST_NOT_HEAD( s , &r->s_hd ) )
		{
			struct shell *next_s;

			next_s = BU_LIST_PNEXT( shell , &s->l );

			(void)nmg_extrude_shell( s , thick , 0 , 0 , &mged_tol );

			s = next_s;
		}

		if( BU_LIST_IS_EMPTY( &r->s_hd ) )
			nmg_kr( r );

		r = next_r;
	}

	if( BU_LIST_IS_EMPTY( &m->r_hd ) )
	{
	  Tcl_AppendResult(interp, "No inside created\n", (char *)NULL);
	  nmg_km( m );
	  return( 1 );
	}
	else
	  return( 0 );
}
