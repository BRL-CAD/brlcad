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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "externs.h"
#include "./solid.h"
#include <signal.h>

extern int numargs;   			/* number of arguments */
extern int args;       			/* total number of args available */
extern int argcnt;     			/* holder for number of args added later */
extern char *cmd_args[];    		/* array of pointers to args */

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

void	f_facedef();

static void	get_pleqn(), get_rotfb(), get_nupnt();
void	calc_pnts();
static int	get_3pts();

/*			F _ F A C E D E F ( )
 * Redefines one of the defining planes for a GENARB8. Finds
 * which plane to redefine and gets input, then shuttles the process over to
 * one of four functions before calculating new vertices.
 */
void
f_facedef()
{
	short int 	i,face,prod,plane;
	static vect_t 	tempvec;	/* because MAT4X3PNT,MAT4X3VEC corrupts the vector */
	struct rt_tol	tol;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	(void)signal( SIGINT, sig2 );		/* allow interrupts */
	if( state != ST_S_EDIT ){
		(void)printf("Facedef: must be in solid edit\n");
		return;
	}
	else if( es_rec.s.s_type != GENARB8 ){
		(void)printf("Facedef: type must be GENARB8\n");
		return;
	}	

	/* apply es_mat editing to parameters,put into es_rec.s */
	VMOVE( tempvec, &es_rec.s.s_values[0] );
	MAT4X3PNT( &es_rec.s.s_values[0], es_mat, tempvec );
	for(i=1; i<8; i++){
		VMOVE( tempvec, &es_rec.s.s_values[i*3] );
		MAT4X3VEC( 	&es_rec.s.s_values[i*3],
				es_mat,
				tempvec			);
	}

	/* find new planes to account for any editing */
	calc_planes( &es_rec.s, es_rec.s.s_cgtype );

	/* get face, initialize args and argcnt */
	face = atoi( cmd_args[1] );
	args = numargs;
	argcnt = 0;
	
	/* use product of vertices to distinguish faces */
	for(i=0,prod=1;i<4;i++)
		if( face > 0 ){
			prod *= face%10;
			face /= 10;
		}

	switch( prod ){
		case    6:			/* face  123 of arb4 */
		case   24:plane=0;		/* face 1234 of arb8 */
						/* face 1234 of arb7 */
						/* face 1234 of arb6 */
						/* face 1234 of arb5 */
			  if(es_rec.s.s_cgtype==ARB4 && prod==24)
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
			  if(es_rec.s.s_cgtype==ARB5)
				plane=4; 	/* face  145 of arb5 */
			  break;
		case   12:			/* face  134 of arb4 */
		case   10:			/* face  125 of arb6 */
		case  252:plane=3;		/* face 2376 of arb8 */
						/* face 2376 of arb7 */
			  if(es_rec.s.s_cgtype==ARB5)
				plane=1; 	/* face  125 of arb5 */
		 	  break;
		case   72:               	/* face  346 of arb6 */
		case   60:plane=4;	 	/* face 1265 of arb8 */
						/* face 1265 of arb7 */
			  if(es_rec.s.s_cgtype==ARB5)
				plane=3; 	/* face  345 of arb5 */
			  break;
		case  420:			/* face 4375 of arb7 */
		case  672:plane=5;		/* face 4378 of arb8 */
			  break;
		default:
			  (void)printf("bad face\n");
			  return;
	}

	while( args < 3 ){

		/* menu of choices for plane equation definition */
		(void)printf("\ta   planar equation\n");
		(void)printf("\tb   3 points\n");
		(void)printf("\tc   rot,fb angles + fixed pt\n");
		(void)printf("\td   same plane thru fixed pt\n");
		(void)printf("\tq   quit\n\n");

		(void)printf("Enter form of new face definition: ");
		argcnt = getcmd(args);
		args += argcnt;
	}

	switch( *cmd_args[2] ){
		case 'a': 
			/* special case for arb7, because of 2 4-pt planes meeting */
			if( es_rec.s.s_cgtype == ARB7 )
				if( plane!=0 && plane!=3 ){
					(void)printf("Facedef: can't redefine that arb7 plane\n");
					return;
				}
			while( args < 7 ){  	/* total # of args under this option */
				(void)printf("%s",p_pleqn[args-3]);
				if( (argcnt = getcmd(args)) < 0 ){
					(void)printf("Facedef: input bad\n");
					return;
				}
				args += argcnt;
			}
			get_pleqn( es_peqn[plane], &cmd_args[3] );
			break;
		case 'b': 
			/* special case for arb7, because of 2 4-pt planes meeting */
			if( es_rec.s.s_cgtype == ARB7 )
				if( plane!=0 && plane!=3 ){
					(void)printf("Facedef: can't redefine that arb7 plane\n");
					return;
				}
			while( args < 12 ){           /* total # of args under this option */
				(void)printf("%s %d: ", p_3pts[(args-3)%3] ,args/3);
				if( (argcnt = getcmd(args)) < 0 ){
					(void)printf("Facedef: input bad\n");
					return;
				}
				args += argcnt;
			}
			if( get_3pts( es_peqn[plane], &cmd_args[3], &tol) ){
				/* clean up array es_peqn for anyone else */
				calc_planes( &es_rec.s, es_rec.s.s_cgtype );
				return;				/* failure */
			}
			break;
		case 'c': 
			/* special case for arb7, because of 2 4-pt planes meeting */
			if( es_rec.s.s_cgtype == ARB7 && (plane != 0 && plane != 3) ) {
				while( args < 5 ){ 	/* total # of args under this option */
					(void)printf("%s",p_rotfb[args-3]);
					if( (argcnt = getcmd(args)) < 0){
						(void)printf("Facedef: input bad\n");
						return;
					}
					args += argcnt;
				}
				cmd_args[5] = "v5";
				(void)printf("Fixed point is vertex five.\n");
			}
			else while( args < 8 ){         /* total # of args under this option */	
				if( args > 5 && cmd_args[5][0] == 'v' )       /* vertex point given,stop */
					break;
				(void)printf("%s",p_rotfb[args-3]);
				if( (argcnt = getcmd(args)) < 0 ){
					(void)printf("Facedef: input bad\n");
					return;
				}
				args += argcnt;
			}
			get_rotfb(es_peqn[plane], &cmd_args[3], &es_rec.s);
			break;
		case 'd': 
			/* special case for arb7, because of 2 4-pt planes meeting */
			if( es_rec.s.s_cgtype == ARB7 )
				if( plane!=0 && plane!=3 ){
					(void)printf("Facedef: can't redefine that arb7 plane\n");
					return;
				}
			while( args < 6 ){  	/* total # of args under this option */
				(void)printf("%s",p_nupnt[args-3]);
				if( (argcnt = getcmd(args)) < 0 ){
					(void)printf("Facedef: input bad\n");
					return;
				}
				args += argcnt;
			}
			get_nupnt(es_peqn[plane], &cmd_args[3]);
			break;
		case 'q': 
			return;
		default:  
			(void)printf("Facedef: not an option\n");
			return;
	}

	/* find all vertices, put in vector notation */
	calc_pnts( &es_rec.s, es_rec.s.s_cgtype );

	/* go back to before es_mat changes */
	VMOVE( tempvec, &es_rec.s.s_values[0] );
	MAT4X3PNT( &es_rec.s.s_values[0], es_invmat, tempvec );
	for(i=1; i<8; i++){
		VMOVE( tempvec, &es_rec.s.s_values[i*3] );
		MAT4X3VEC(	&es_rec.s.s_values[i*3],
				es_invmat,
				tempvec			);
	}

	/* draw the new solid */
	replot_editing_solid();

	/* update display information */
	dmaflag = 1;
	return;				/* everything OK */
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
 *  Gets three definite points from the array cmd_args[]
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
get_rotfb(plane, argv, s_recp)
plane_t	plane;
char	*argv[];
struct solidrec *s_recp;
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
		if(temp > 0){
			VADD2( &pt[0], &s_recp->s_values[0], &s_recp->s_values[temp*3] );
			plane[3]= VDOT(&plane[0], pt);
		} else {
			plane[3]= VDOT(&plane[0],&s_recp->s_values[0]);
		}
	} else {				         /* definite point given */
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

/* 			C A L C _ P N T S (  )
 * Takes the array es_peqn[] and intersects the planes to find the vertices
 * of a GENARB8.  The vertices are stored in the solid record 'old_srec' which
 * is of type 'type'.  If intersect fails, the points (in vector notation) of
 * 'old_srec' are used to clean up the array es_peqn[] for anyone else. The
 * vertices are put in 'old_srec' in vector notation.  This is an analog to
 * calc_planes().
 */
void
calc_pnts( old_srec, type )
struct solidrec *old_srec;
int type;
{
	struct solidrec temp_srec;
	short int i;

	/* find new points for entire solid */
	for(i=0; i<8; i++){
		/* use temp_srec until we know intersect doesn't fail */
		if( intersect(type,i*3,i,&temp_srec) ){
			(void)printf("Intersection of planes fails\n");
			/* clean up array es_peqn for anyone else */
			calc_planes( old_srec, type );
			return;				/* failure */
		}
	}

	/* back to vector notation */
	VMOVE( &old_srec->s_values[0], &temp_srec.s_values[0] );
	for(i=3; i<=21; i+=3){
		VSUB2(	&old_srec->s_values[i],
			&temp_srec.s_values[i],
			&temp_srec.s_values[0]  );
	}
	return;						/* success */
}
