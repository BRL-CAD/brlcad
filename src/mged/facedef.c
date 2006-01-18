/*                       F A C E D E F . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
 */
/** @file facedef.c
 *  Authors -
 *	Daniel C. Dender
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "./mged_solid.h"

extern struct rt_db_internal	es_int;	/* from edsol.c */
extern struct bn_tol		mged_tol;		/* from ged.c */

char *p_rotfb[] = {
	"Enter rot, fb angles: ",
	"Enter fb angle: ",
	"Enter fixed vertex(v#) or point(X Y Z): ",
	"Enter Y, Z of point: ",
	"Enter Z of point: "
};

char *p_3pts[] = {
	"Enter X,Y,Z of point",
	"Enter Y,Z of point",
	"Enter Z of point"
};

char *p_pleqn[] = {
	"Enter A,B,C,D of plane equation: ",
	"Enter B,C,D of plane equation: ",
	"Enter C,D of plane equation: ",
	"Enter D of plane equation: "
};

char *p_nupnt[] = {
	"Enter X,Y,Z of fixed point: ",
	"Enter Y,Z of fixed point: ",
	"Enter Z of fixed point: "
};

static void	get_pleqn(fastf_t *plane, char **argv), get_rotfb(fastf_t *plane, char **argv, const struct rt_arb_internal *arb), get_nupnt(fastf_t *plane, char **argv);
static int	get_3pts(fastf_t *plane, char **argv, const struct bn_tol *tol);

/*
 *			F _ F A C E D E F
 *
 * Redefines one of the defining planes for a GENARB8. Finds
 * which plane to redefine and gets input, then shuttles the process over to
 * one of four functions before calculating new vertices.
 */
int
f_facedef(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	short int 	i;
	int		face,prod,plane;
	struct rt_db_internal	intern;
	struct rt_arb_internal	*arb;
	struct rt_arb_internal	*arbo;
	plane_t		planes[6];
	int status = TCL_OK;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help facedef");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
	else
	  return TCL_OK;

	if( state != ST_S_EDIT ){
	  Tcl_AppendResult(interp, "Facedef: must be in solid edit mode\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}
	if( es_int.idb_type != ID_ARB8 )  {
	   Tcl_AppendResult(interp, "Facedef: solid type must be ARB\n");
	   status = TCL_ERROR;
	   goto end;
	}

	/* apply es_mat editing to parameters.  "new way" */
	transform_editing_solid( &intern, es_mat, &es_int, 0 );

	arb = (struct rt_arb_internal *)intern.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	/* find new planes to account for any editing */
	if (rt_arb_calc_planes(interp, arb, es_type, planes, &mged_tol)) {
	  Tcl_AppendResult(interp, "Unable to determine plane equations\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	/* get face, initialize args and argcnt */
	face = atoi( argv[1] );

	/* use product of vertices to distinguish faces */
	for(i=0,prod=1;i<4;i++)  {
		if( face > 0 ){
			prod *= face%10;
			face /= 10;
		}
	}

	switch( prod ){
		case    6:			/* face  123 of arb4 */
		case   24:plane=0;		/* face 1234 of arb8 */
						/* face 1234 of arb7 */
						/* face 1234 of arb6 */
						/* face 1234 of arb5 */
			  if(es_type==4 && prod==24)
				plane=2; 	/* face  234 of arb4 */
			  break;
		case    8:			/* face  124 of arb4 */
		case  180: 			/* face 2365 of arb6 */
		case  210:			/* face  567 of arb7 */
		case 1680:plane=1;      	/* face 5678 of arb8 */
			  break;
		case   30:			/* face  235 of arb5 */
		case  120:			/* face 1564 of arb6 */
		case   20:      		/* face  145 of arb7 */
		case  160:plane=2;		/* face 1584 of arb8 */
			  if(es_type==5)
				plane=4; 	/* face  145 of arb5 */
			  break;
		case   12:			/* face  134 of arb4 */
		case   10:			/* face  125 of arb6 */
		case  252:plane=3;		/* face 2376 of arb8 */
						/* face 2376 of arb7 */
			  if(es_type==5)
				plane=1; 	/* face  125 of arb5 */
		 	  break;
		case   72:               	/* face  346 of arb6 */
		case   60:plane=4;	 	/* face 1265 of arb8 */
						/* face 1265 of arb7 */
			  if(es_type==5)
				plane=3; 	/* face  345 of arb5 */
			  break;
		case  420:			/* face 4375 of arb7 */
		case  672:plane=5;		/* face 4378 of arb8 */
			  break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "bad face (product=%d)\n", prod);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		    status = TCL_ERROR;
		    goto end;
		  }
	}

	if( argc < 3 ){
	  /* menu of choices for plane equation definition */
	  Tcl_AppendResult(interp,
			   "\ta   planar equation\n",
			   "\tb   3 points\n",
			   "\tc   rot,fb angles + fixed pt\n",
			   "\td   same plane thru fixed pt\n",
			   "\tq   quit\n\n",
			   MORE_ARGS_STR, "Enter form of new face definition: ", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	switch( argv[2][0] ){
	case 'a':
	  /* special case for arb7, because of 2 4-pt planes meeting */
	  if( es_type == 7 )
	    if( plane!=0 && plane!=3 ){
	      Tcl_AppendResult(interp, "Facedef: can't redefine that arb7 plane\n", (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	  if( argc < 7 ){  	/* total # of args under this option */
	    Tcl_AppendResult(interp, MORE_ARGS_STR, p_pleqn[argc-3], (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  get_pleqn( planes[plane], &argv[3] );
	  break;
	case 'b':
	  /* special case for arb7, because of 2 4-pt planes meeting */
	  if( es_type == 7 )
	    if( plane!=0 && plane!=3 ){
	      Tcl_AppendResult(interp, "Facedef: can't redefine that arb7 plane\n", (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	  if( argc < 12 ){           /* total # of args under this option */
	    struct bu_vls tmp_vls;

	     bu_vls_init(&tmp_vls);
	     bu_vls_printf(&tmp_vls, "%s%s %d: ", MORE_ARGS_STR, p_3pts[(argc-3)%3], argc/3);
	     Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	     bu_vls_free(&tmp_vls);
	     status = TCL_ERROR;
	     goto end;
	  }
	  if( get_3pts( planes[plane], &argv[3], &mged_tol) ){
	    status = TCL_ERROR;
	    goto end;
	  }
	  break;
	case 'c':
	  /* special case for arb7, because of 2 4-pt planes meeting */
	  if( es_type == 7 && (plane != 0 && plane != 3) ) {
	    if( argc < 5 ){ 	/* total # of args under this option */
	      Tcl_AppendResult(interp, MORE_ARGS_STR, p_rotfb[argc-3], (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }

	    argv[5] = "Release 6";
	    Tcl_AppendResult(interp, "Fixed point is vertex five.\n");
	  }
	  /* total # of as under this option */
	  else if( argc < 8 && (argc > 5 ? argv[5][0] != 'R' : 1)) {
	    Tcl_AppendResult(interp, MORE_ARGS_STR, p_rotfb[argc-3], (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  get_rotfb(planes[plane], &argv[3], arb);
	  break;
	case 'd':
	  /* special case for arb7, because of 2 4-pt planes meeting */
	  if( es_type == 7 )
	    if( plane!=0 && plane!=3 ){
	      Tcl_AppendResult(interp, "Facedef: can't redefine that arb7 plane\n", (char *)NULL);
	      status = TCL_ERROR;
	      goto end;
	    }
	  if( argc < 6 ){  	/* total # of args under this option */
	    Tcl_AppendResult(interp, MORE_ARGS_STR, p_nupnt[argc-3], (char *)NULL);
	    status = TCL_ERROR;
	    goto end;
	  }
	  get_nupnt(planes[plane], &argv[3]);
	  break;
	case 'q':
	  return TCL_OK;
	default:
	  Tcl_AppendResult(interp, "Facedef: '", argv[2], "' is not an option\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}

	/* find all vertices from the plane equations */
	if( rt_arb_calc_points( arb, es_type, planes, &mged_tol ) < 0 )  {
	  Tcl_AppendResult(interp, "facedef:  unable to find points\n", (char *)NULL);
	  status = TCL_ERROR;
	  goto end;
	}
	/* Now have 8 points, which is the internal form of an ARB8. */

	/* Transform points back before es_mat changes */
	/* This is the "new way" */
	arbo = (struct rt_arb_internal *)es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arbo);

	for(i=0; i<8; i++){
		MAT4X3PNT( arbo->pt[i], es_invmat, arb->pt[i] );
	}
	rt_db_free_internal(&intern, &rt_uniresource);

	/* draw the new solid */
	replot_editing_solid();

end:
	(void)signal( SIGINT, SIG_IGN );
	return status;
}


/*
 * 			G E T _ P L E Q N
 *
 * Gets the planar equation from the array argv[]
 * and puts the result into 'plane'.
 */
static void
get_pleqn(fastf_t *plane, char **argv)
{
	int i;

	if(dbip == DBI_NULL)
	  return;

	for(i=0; i<4; i++)
		plane[i]= atof(argv[i]);
	VUNITIZE( &plane[0] );
	plane[3] *= local2base;
	return;
}


/*
 * 			G E T _ 3 P T S
 *
 *  Gets three definite points from the array argv[]
 *  and finds the planar equation from these points.
 *  The resulting plane equation is stored in 'plane'.
 *
 *  Returns -
 *	 0	success
 *	-1	failure
 */
static int
get_3pts(fastf_t *plane, char **argv, const struct bn_tol *tol)
{
	int i;
	point_t	a,b,c;

	CHECK_DBI_NULL;

	for(i=0; i<3; i++)
		a[i] = atof(argv[0+i]) * local2base;
	for(i=0; i<3; i++)
		b[i] = atof(argv[3+i]) * local2base;
	for(i=0; i<3; i++)
		c[i] = atof(argv[6+i]) * local2base;

	if( bn_mk_plane_3pts( plane, a, b, c, tol ) < 0 )  {
	  Tcl_AppendResult(interp, "Facedef: not a plane\n", (char *)NULL);
	  return(-1);		/* failure */
	}
	return(0);			/* success */
}

/*
 * 			G E T _ R O T F B
 *
 * Gets information from the array argv[].
 * Finds the planar equation given rotation and fallback angles, plus a
 * fixed point. Result is stored in 'plane'. The vertices
 * pointed to by 's_recp' are used if a vertex is chosen as fixed point.
 */
static void
get_rotfb(fastf_t *plane, char **argv, const struct rt_arb_internal *arb)
{
	fastf_t rota, fb;
	short int i,temp;
	point_t		pt;

	if(dbip == DBI_NULL)
	  return;

	rota= atof(argv[0]) * degtorad;
	fb  = atof(argv[1]) * degtorad;

	/* calculate normal vector (length=1) from rot,fb */
	plane[0] = cos(fb) * cos(rota);
	plane[1] = cos(fb) * sin(rota);
	plane[2] = sin(fb);

	if( argv[2][0] == 'v' ){     	/* vertex given */
		/* strip off 'v', subtract 1 */
		temp = atoi(argv[2]+1) - 1;
		plane[3]= VDOT(&plane[0], arb->pt[temp]);
	} else {		         /* definite point given */
		for(i=0; i<3; i++)
			pt[i]=atof(argv[2+i]) * local2base;
		plane[3]=VDOT(&plane[0], pt);
	}
}

/*
 * 			G E T _ N U P N T
 *
 * Gets a point from the three strings in the 'argv' array.
 * The value of D of 'plane' is changed such that the plane
 * passes through the input point.
 */
static void
get_nupnt(fastf_t *plane, char **argv)
{
	int	i;
	point_t	pt;

	if(dbip == DBI_NULL)
	  return;

	for(i=0; i<3; i++)
		pt[i] = atof(argv[i]) * local2base;
	plane[3] = VDOT(&plane[0], pt);
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
