/*
 *			F A C E D E F . C
 *  Authors -
 *	Daniel C. Dender
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "db.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "./solid.h"
#include <signal.h>

extern struct rt_db_internal	es_int;	/* from edsol.c */

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

static void	get_pleqn(), get_rotfb(), get_nupnt();
static int	get_3pts();

/*
 *			F _ F A C E D E F
 *
 * Redefines one of the defining planes for a GENARB8. Finds
 * which plane to redefine and gets input, then shuttles the process over to
 * one of four functions before calculating new vertices.
 */
int
f_facedef(argc, argv)
int argc;
char **argv;
{
	short int 	i;
	int		face,prod,plane;
	struct rt_tol	tol;
	struct rt_db_internal	intern;
	struct rt_arb_internal	*arb;
	struct rt_arb_internal	*arbo;
	plane_t		planes[6];

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

#ifdef XMGED
	(void)signal( SIGINT, cur_sigint );		/* allow interrupts */
#else
	(void)signal( SIGINT, sig2);    /* allow interupts */
#endif

	if( state != ST_S_EDIT ){
		rt_log("Facedef: must be in solid edit mode\n");
		return CMD_BAD;
	}
	if( es_int.idb_type != ID_ARB8 )  {
		rt_log("Facedef: solid type must be ARB\n");
		return CMD_BAD;
	}	

	/* apply es_mat editing to parameters.  "new way" */
	transform_editing_solid( &intern, es_mat, &es_int, 0 );

	arb = (struct rt_arb_internal *)intern.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	/* find new planes to account for any editing */
	if( rt_arb_calc_planes( planes, arb, es_type, &tol ) < 0 )  {
		rt_log("Unable to determine plane equations\n");
		return CMD_BAD;
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
			  rt_log("bad face (product=%d)\n", prod);
			  return CMD_BAD;
	}

	if( argc < 3 ){

		/* menu of choices for plane equation definition */
		rt_log("\ta   planar equation\n");
		rt_log("\tb   3 points\n");
		rt_log("\tc   rot,fb angles + fixed pt\n");
		rt_log("\td   same plane thru fixed pt\n");
		rt_log("\tq   quit\n\n");

		rt_log("Enter form of new face definition: ");
		return CMD_MORE;
	}

	switch( argv[2][0] ){
	case 'a': 
		/* special case for arb7, because of 2 4-pt planes meeting */
		if( es_type == 7 )
			if( plane!=0 && plane!=3 ){
				rt_log("Facedef: can't redefine that arb7 plane\n");
				return CMD_BAD;
			}
		if( argc < 7 ){  	/* total # of args under this option */
			rt_log("%s",p_pleqn[argc-3]);
			return CMD_MORE;
		}
		get_pleqn( planes[plane], &argv[3] );
		break;
	case 'b': 
		/* special case for arb7, because of 2 4-pt planes meeting */
		if( es_type == 7 )
			if( plane!=0 && plane!=3 ){
				rt_log("Facedef: can't redefine that arb7 plane\n");
				return CMD_BAD;
			}
		if( argc < 12 ){           /* total # of args under this option */
			rt_log("%s %d: ", p_3pts[(argc-3)%3], argc/3);
			return CMD_MORE;
		}
		if( get_3pts( planes[plane], &argv[3], &tol) ){
			return CMD_BAD;			/* failure */
		}
		break;
	case 'c': 
		/* special case for arb7, because of 2 4-pt planes meeting */
		if( es_type == 7 && (plane != 0 && plane != 3) ) {
			if( argc < 5 ){ 	/* total # of args under this option */
				rt_log("%s",p_rotfb[argc-3]);
				return CMD_MORE;
			}
			argv[5] = "v5";
			rt_log("Fixed point is vertex five.\n");
		}
                                    /* total # of args under this option */
		else if( argc < 8 && (argc >= 5 ? argv[5][0] != 'v' : 1)) { 
			rt_log("%s",p_rotfb[argc-3]);
			return CMD_MORE;
		}
		get_rotfb(planes[plane], &argv[3], arb);
		break;
	case 'd': 
		/* special case for arb7, because of 2 4-pt planes meeting */
		if( es_type == 7 )
			if( plane!=0 && plane!=3 ){
				rt_log("Facedef: can't redefine that arb7 plane\n");
				return CMD_BAD;
			}
		if( argc < 6 ){  	/* total # of args under this option */
			rt_log("%s",p_nupnt[argc-3]);
			return CMD_MORE;
		}
		get_nupnt(planes[plane], &argv[3]);
		break;
	case 'q': 
		return CMD_OK;
	default:  
		rt_log("Facedef: '%s' is not an option\n", argv[2]);
		return CMD_BAD;
	}

	/* find all vertices from the plane equations */
	if( rt_arb_calc_points( arb, es_type, planes, &tol ) < 0 )  {
		rt_log("facedef:  unable to find points\n");
		return CMD_BAD;
	}
	/* Now have 8 points, which is the internal form of an ARB8. */

	/* Transform points back before es_mat changes */
	/* This is the "new way" */
	arbo = (struct rt_arb_internal *)es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arbo);

	for(i=0; i<8; i++){
		MAT4X3PNT( arbo->pt[i], es_invmat, arb->pt[i] );
	}
	rt_db_free_internal(&intern);

	/* draw the new solid */
	replot_editing_solid();
	return CMD_OK;				/* everything OK */
}


/*
 * 			G E T _ P L E Q N
 *
 * Gets the planar equation from the array argv[]
 * and puts the result into 'plane'.
 */
static void
get_pleqn( plane, argv )
plane_t	plane;
char	*argv[];
{
	int i;

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
get_3pts( plane, argv, tol)
plane_t		plane;
char		*argv[];
struct rt_tol	*tol;
{
	int i;
	point_t	a,b,c;

	for(i=0; i<3; i++)
		a[i] = atof(argv[0+i]) * local2base;
	for(i=0; i<3; i++)
		b[i] = atof(argv[3+i]) * local2base;
	for(i=0; i<3; i++)
		c[i] = atof(argv[6+i]) * local2base;

	if( rt_mk_plane_3pts( plane, a, b, c, tol ) < 0 )  {
		rt_log("Facedef: not a plane\n");
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
get_rotfb(plane, argv, arb)
plane_t	plane;
char	*argv[];
CONST struct rt_arb_internal	*arb;
{
	fastf_t rota, fb;
	short int i,temp;
	point_t		pt;

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
void
static get_nupnt(plane, argv)
plane_t	plane;
char	*argv[];
{
	int	i;
	point_t	pt;

	for(i=0; i<3; i++)
		pt[i] = atof(argv[i]) * local2base;
	plane[3] = VDOT(&plane[0], pt);
}
