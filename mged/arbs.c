/*
 *			A R B S . C
 *
 *	f_3ptarb()	finds arb8 defined by 3 points, 2 coordinates of
 *			a 4th point, and a thickness
 *
 *	f_rfarb()	finds arb8 defined by rot & fallback angles, one point,
 *			2 coordinates of the 3 remaining points, and a thickness
 *
 *  Author -
 *	Keith A. Applin
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

#include <signal.h>
#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./dm.h"

extern int 	numargs;
extern char	*cmd_args[];
extern int	newargs;
extern int	args;
extern int	argcnt;
extern char	**promp;

static union record record;
static int	cgarbs();

#define XCOORD 0
#define YCOORD 1
#define ZCOORD 2

char *p_arb3pt[] = {
	"Enter X, Y, Z for point 1: ",
	"Enter Y, Z: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 2: ",
	"Enter Y, Z: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 3: ",
	"Enter Y, Z: ",
	"Enter Z: "
};

/*
 *
 *	F _ 3 P T A R B ( ) :	finds arb8 given.....
 *				1.  3 points of one face
 *				2.  2 coordinates of the 4th point in that face
 *				3.  thickness
 *
 */
void
f_3ptarb( argc, argv )
int	argc;
char	**argv;
{
	int i, solve;
	vect_t	vec1;
	vect_t	vec2;
	fastf_t	pt4[2], length, thick;
	vect_t	norm;
	fastf_t	ndotv;
	struct directory *dp;

	args = numargs;
	argcnt = 0;

	/* interupts */
	(void)signal( SIGINT, sig2 );

	for(i=0; i<24; i++) {
		record.s.s_values[i] = 0.0;
	}

	/* get the arb name */
	while( args < 2 ) {
		(void)printf("Enter name for this arb: ");
		argcnt = getcmd(args);
		args += argcnt;
	}
	if( db_lookup( dbip, cmd_args[1], LOOKUP_QUIET) != DIR_NULL ) {
		(void)printf("%s:  already exists\n",cmd_args[1]);
		return;
	}

	if( (int)strlen(cmd_args[1]) >= NAMESIZE ) {
		(void)printf("Names are limited to %d charscters\n",NAMESIZE-1);
		return;
	}
	NAMEMOVE( cmd_args[1], record.s.s_name );

	/* read the three points */
	promp = &p_arb3pt[0];
	while( args < 11 ) {
		(void)printf("%s", promp[args-2]);
		if( (argcnt = getcmd(args)) < 0 )
			return;
		args += argcnt;
	}

	/* preliminary calculations to check input so far */
	for(i=0; i<3; i++) {
		vec1[i] = atof(cmd_args[(i+2)]) - atof(cmd_args[(i+5)]);
		vec2[i] = atof(cmd_args[(i+2)]) - atof(cmd_args[(i+8)]);
	}
	VCROSS(norm, vec1, vec2);
	length = MAGNITUDE( norm );
	if(length == 0.0) {
		(void)printf("points are colinear\n");
		return;
	}
	VSCALE(norm, norm, 1.0/length);

	for(i=0; i<3; i++) {
		/* the three given vertices */
		record.s.s_values[i] = atof( cmd_args[(i+2)] ) * local2base;
		record.s.s_values[(i+3)] = atof( cmd_args[(i+5)] ) * local2base;
		record.s.s_values[(i+6)] = atof( cmd_args[(i+8)] ) * local2base;
	}

coordin:  	/* sent here to input coordinate to find again */

	(void)printf("Enter coordinate to solve for (x, y, or z): ");
	argcnt = getcmd(args);
	switch( *cmd_args[args] ) {

		case 'x':
			if(norm[0] == 0.0) {
				(void)printf("X not unique in this face\n");
				args += argcnt;
				goto coordin;
			}
			solve = XCOORD;
			args += argcnt;
			(void)printf("Enter the Y, Z coordinate values: ");
			argcnt = getcmd(args);
			pt4[0] = atof( cmd_args[args] ) * local2base;
			pt4[1] = atof( cmd_args[args+1] ) * local2base;
			args += argcnt;
			if( argcnt == 1 ) {
				(void)printf("Enter the Z coordinate value: ");
				argcnt = getcmd(args);
				pt4[1] = atof( cmd_args[args] ) * local2base;
				args += argcnt;
			}
		break;

		case 'y':
			if(norm[1] == 0.0) {
				(void)printf("Y not unique in this face\n");
				args += argcnt;
				goto coordin;
			}
			solve = YCOORD;
			args += argcnt;
			(void)printf("Enter the X, Z coordinate values: ");
			argcnt = getcmd(args);
			pt4[0] = atof( cmd_args[args] ) * local2base;
			pt4[1] = atof( cmd_args[args+1] ) * local2base;
			args += argcnt;
			if( argcnt == 1 ) {
				(void)printf("Enter the Z coordinate value: ");
				argcnt = getcmd(args);
				pt4[1] = atof( cmd_args[args] ) * local2base;
				args += argcnt;
			}
		break;

		case 'z':
			if(norm[2] == 0.0) {
				(void)printf("Z not unique in this face\n");
				args += argcnt;
				goto coordin;
			}
			solve = ZCOORD;
			args += argcnt;
			(void)printf("Enter the X, Y coordinate values: ");
			argcnt = getcmd(args);
			pt4[0] = atof( cmd_args[args] ) * local2base;
			pt4[1] = atof( cmd_args[args+1] ) * local2base;
			args += argcnt;
			if( argcnt == 1 ) {
				(void)printf("Enter the Y coordinate value: ");
				argcnt = getcmd(args);
				pt4[1] = atof( cmd_args[args] ) * local2base;
				args += argcnt;
			}
		break;

		default:
			(void)printf("coordinate must be x, y, or z\n");
			args += argcnt;
			goto coordin;
	}


thickagain:
	(void)printf("Enter thickness for this arb: ");
	argcnt = getcmd(args);
	if( (thick = (atof( cmd_args[args] ))) == 0.0 ) {
		(void)printf("thickness = 0.0\n");
		args += argcnt;
		goto thickagain;
	}
	args += argcnt;
	thick *= local2base;

	record.s.s_id = ID_SOLID;
	record.s.s_type = GENARB8;
	record.s.s_cgtype = ARB8;

	ndotv = VDOT( &record.s.s_values[0], norm );

	switch( solve ) {

		case XCOORD:
			/* solve for x-coord of 4th point */
			record.s.s_values[10] = pt4[0];	/* y-coord */
			record.s.s_values[11] = pt4[1]; 	/* z-coord */
			record.s.s_values[9] =  ( ndotv
						- norm[1] * record.s.s_values[10]
						- norm[2] * record.s.s_values[11])
						/ norm[0];	/* x-coord */
		break;

		case YCOORD:
			/* solve for y-coord of 4th point */
			record.s.s_values[9] = pt4[0];	/* x-coord */
			record.s.s_values[11] = pt4[1]; 	/* z-coord */
			record.s.s_values[10] =  ( ndotv
						- norm[0] * record.s.s_values[9]
						- norm[2] * record.s.s_values[11])
						/ norm[1];	/* y-coord */
		break;

		case ZCOORD:
			/* solve for z-coord of 4th point */
			record.s.s_values[9] = pt4[0];	/* x-coord */
			record.s.s_values[10] = pt4[1]; 	/* y-coord */
			record.s.s_values[11] =  ( ndotv
						- norm[0] * record.s.s_values[9]
						- norm[1] * record.s.s_values[10])
						/ norm[2];	/* z-coord */
		break;

		default:
			(void)printf("bad coordinate to solve for\n");
			return;

	}

	/* calculate the remaining 4 vertices */
	for(i=0; i<3; i++) {
		record.s.s_values[(i+12)] = record.s.s_values[i]
						+ (thick * norm[i]);
		record.s.s_values[(i+15)] = record.s.s_values[(i+3)]
						+ (thick * norm[i]);
		record.s.s_values[(i+18)] = record.s.s_values[(i+6)]
						+ (thick * norm[i]);
		record.s.s_values[(i+21)] = record.s.s_values[(i+9)]
						+ (thick * norm[i]);
	}

	/* convert to vector notation */
	for(i=3; i<=21; i+=3) {
		VSUB2(&record.s.s_values[i], &record.s.s_values[i],
			&record.s.s_values[0]);
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, record.s.s_name, -1, 0, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )  {
	    	ALLOC_ERR_return;
	    }
	if( db_put( dbip,  dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;

	/* draw the "made" solid */
	f_edit( 2, cmd_args );	/* depends on name being in cmd_args[1] */
}



char *p_rfin[] = {
	"Enter X, Y, Z of the known point: ",
	"Enter Y,Z: ",
	"Enter Z: "
};

/*	F _ R F A R B ( ) :	finds arb8 given.....
 *
 *		1. one point
 *		2. 2 coordinates of 3 other points
 *		3. rot and fallback angles
 *		4. thickness
 */
void
f_rfarb( argc, argv )
int	argc;
char	**argv;
{
	struct directory *dp;
	int	i;
	int	solve[3];
	fastf_t	pt[3][2];
	fastf_t	thick, rota, fba;
	vect_t	norm;
	fastf_t	ndotv;

	/* interupts */
	(void)signal( SIGINT, sig2 );

	args = numargs;
	argcnt = 0;

	for(i=0; i<24; i++) {
		record.s.s_values[i] = 0.0;
	}

	/* get the arb name */
	while( args < 2 ) {
		(void)printf("Enter name for this arb: ");
		argcnt = getcmd(args);
		args += argcnt;
	}
	if( db_lookup( dbip, cmd_args[1], LOOKUP_QUIET) != DIR_NULL ) {
		(void)printf("%s:  already exists\n",cmd_args[1]);
		return;
	}

	if( (int)strlen(cmd_args[1]) >= NAMESIZE ) {
		(void)printf("Names are limited to %d charscters\n",NAMESIZE-1);
		return;
	}
	NAMEMOVE( cmd_args[1], record.s.s_name );

	/* read the known point */
	promp = &p_rfin[0];
	while( args < 5 ) {
		(void)printf("%s", promp[args-2]);
		if( (argcnt = getcmd(args)) < 0 )
			return;
		args += argcnt;
	}
	for(i=0; i<3; i++) {
		record.s.s_values[i] = atof(cmd_args[i+2]) * local2base;
	}

	(void)printf("Enter ROTATION angle (deg): ");
	argcnt = getcmd(args);
	rota = atof( cmd_args[args] ) * degtorad;
	args += argcnt;
	(void)printf("Enter FALL BACK angle (deg): ");
	argcnt = getcmd(args);
	fba = atof( cmd_args[args] ) * degtorad;
	args += argcnt;

	/* calculate plane defined by these angles */
	norm[0] = cos(fba) * cos(rota);
	norm[1] = cos(fba) * sin(rota);
	norm[2] = sin(fba);

	for(i=0; i<3; i++) {
coordagain:	/* sent here to redo a point */
		(void)printf("POINT %d...",i+2);
		(void)printf("Enter coordinate to solve for (x, y, or z): ");
		argcnt = getcmd(args);
		switch( *cmd_args[args] ) {

			case 'x':
				if(norm[0] == 0.0) {
					(void)printf("X not unique in this face\n");
					args += argcnt;
					goto coordagain;
				}
				solve[i] = XCOORD;
				args += argcnt;
				(void)printf("Enter the Y, Z coordinate values: ");
				argcnt = getcmd(args);
				pt[i][0] = atof( cmd_args[args] ) * local2base;
				pt[i][1] = atof( cmd_args[args+1] ) * local2base;
				args += argcnt;
				if( argcnt == 1 ) {
					(void)printf("Enter the Z coordinate value: ");
					argcnt = getcmd(args);
					pt[i][1] = atof( cmd_args[args] ) * local2base;
					args += argcnt;
				}
			break;

			case 'y':
				if(norm[1] == 0.0) {
					(void)printf("Y not unique in this face\n");
					args += argcnt;
					goto coordagain;
				}
				solve[i] = YCOORD;
				args += argcnt;
				(void)printf("Enter the X, Z coordinate values: ");
				argcnt = getcmd(args);
				pt[i][0] = atof( cmd_args[args] ) * local2base;
				pt[i][1] = atof( cmd_args[args+1] ) * local2base;
				args += argcnt;
				if( argcnt == 1 ) {
					(void)printf("Enter the Z coordinate value: ");
					argcnt = getcmd(args);
					pt[i][1] = atof( cmd_args[args] ) * local2base;
					args += argcnt;
				}
			break;

			case 'z':
				if(norm[2] == 0.0) {
					(void)printf("Z not unique in this face\n");
					args += argcnt;
					goto coordagain;
				}
				solve[i] = ZCOORD;
				args += argcnt;
				(void)printf("Enter the X, Y coordinate values: ");
				argcnt = getcmd(args);
				pt[i][0] = atof( cmd_args[args] ) * local2base;
				pt[i][1] = atof( cmd_args[args+1] ) * local2base;
				args += argcnt;
				if( argcnt == 1 ) {
					(void)printf("Enter the Y coordinate value: ");
					argcnt = getcmd(args);
					pt[i][1] = atof( cmd_args[args] ) * local2base;
					args += argcnt;
				}
			break;

			default:
				(void)printf("coordinate must be x, y, or z\n");
				args += argcnt;
				goto coordagain;
		}
	}

thckagain:
	(void)printf("Enter thickness for this arb: ");
	argcnt = getcmd(args);
	if( (thick = (atof( cmd_args[args] ))) == 0.0 ) {
		(void)printf("thickness = 0.0\n");
		args += argcnt;
		goto thckagain;
	}
	args += argcnt;
	thick *= local2base;

	record.s.s_id = ID_SOLID;
	record.s.s_type = GENARB8;
	record.s.s_cgtype = ARB8;

	ndotv = VDOT( &record.s.s_values[0], norm );

	/* calculate the unknown coordinate for points 2,3,4 */
	for(i=0; i<3; i++) {

		switch( solve[i] ) {
			case XCOORD:
				record.s.s_values[3*i+4] = pt[i][0];
				record.s.s_values[3*i+5] = pt[i][1];
				record.s.s_values[3*i+3] = ( ndotv
					- norm[1] * record.s.s_values[3*i+4]
					- norm[2] * record.s.s_values[3*i+5])
					/ norm[0];
			break;
			case YCOORD:
				record.s.s_values[3*i+3] = pt[i][0];
				record.s.s_values[3*i+5] = pt[i][1];
				record.s.s_values[3*i+4] = ( ndotv
					- norm[0] * record.s.s_values[3*i+3]
					- norm[2] * record.s.s_values[3*i+5])
					/ norm[1];
			break;
			case ZCOORD:
				record.s.s_values[3*i+3] = pt[i][0];
				record.s.s_values[3*i+4] = pt[i][1];
				record.s.s_values[3*i+5] = ( ndotv
					- norm[0] * record.s.s_values[3*i+3]
					- norm[1] * record.s.s_values[3*i+4])
					/ norm[2];
			break;

			default:
			return;
		}
	}

	/* calculate the remaining 4 vertices */
	for(i=0; i<3; i++) {
		record.s.s_values[(i+12)] = record.s.s_values[i]
						+ (thick * norm[i]);
		record.s.s_values[(i+15)] = record.s.s_values[(i+3)]
						+ (thick * norm[i]);
		record.s.s_values[(i+18)] = record.s.s_values[(i+6)]
						+ (thick * norm[i]);
		record.s.s_values[(i+21)] = record.s.s_values[(i+9)]
						+ (thick * norm[i]);
	}

	/* convert to vector notation */
	for(i=3; i<=21; i+=3) {
		VSUB2(&record.s.s_values[i], &record.s.s_values[i],
			&record.s.s_values[0]);
	}

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, record.s.s_name, -1, 0, DIR_SOLID)) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0  )  {
	    	ALLOC_ERR_return;
	}
	if( db_put( dbip,  dp, &record, 0, 1 ) < 0 )  WRITE_ERR_return;

	/* draw the "made" solid */
	f_edit( 2, cmd_args );	/* depends on name being in cmd_args[1] */
}

/* ------------------------------ old way ------------------------------ */

static void	move(), points(), vectors();
static int	redoarb();

static union record input;		/* Holds an object file record */

/* TYPE_ARB()	returns specific ARB type of record rec.  The record rec
 *		is also rearranged to "standard" form.
 */
type_arb( rec )
union record *rec;
{
	int i;
	static int uvec[8], svec[11];

	if( rec->s.s_type != GENARB8 )
		return( 0 );

	input = *rec;		/* copy */

	/* convert input record to points */
	points();

	if( (i = cgarbs(uvec, svec)) == 0 )
		return(0);

	if( redoarb(uvec, svec, i) == 0 )
		return( 0 );

	/* convert to vectors in the rec record */
	VMOVE(&rec->s.s_values[0], &input.s.s_values[0]);
	for(i=3; i<=21; i+=3) {
		VSUB2(&rec->s.s_values[i], &input.s.s_values[i], &input.s.s_values[0]);
	}

	return( input.s.s_cgtype );

}

#define NO	0
#define YES	1
	
/*
 * C G A R B S :   determines COMGEOM arb types from GED general arbs
 */
static int
cgarbs( uvec, svec )
register int *uvec;	/* array of unique points */
register int *svec;	/* array of like points */
{
	register int i,j;
	static int numuvec, unique, done;
	static int si;

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO)
			svec[si] = i;
		for(j=i+1; j<8; j++) {
			int tmp;
			vect_t vtmp;

			VSUB2( vtmp, &input.s.s_values[i*3], &input.s.s_values[j*3] );

			if( fabs(vtmp[0]) > 0.0001) tmp = 0;
			else 	if( fabs(vtmp[1]) > 0.0001) tmp = 0;
			else 	if( fabs(vtmp[2]) > 0.0001) tmp = 0;
			else tmp = 1;

			if( tmp ) {
				if( done == NO )
					svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}

	if( si > 2 && si < 6 ) 
		svec[0] = si - 1;
	if( si > 6 )
		svec[1] = si - 5;
	for(i=1; i<=svec[1]; i++)
		svec[svec[0]+1+i] = svec[5+i];
	for(i=svec[0]+svec[1]+2; i<11; i++)
		svec[i] = -1;
	/* find the unique points */
	numuvec = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES )
			uvec[numuvec++] = j;
	}

	/* put comgeom solid typpe into s_cgtype */
	switch( numuvec ) {

	case 8:
		input.s.s_cgtype = ARB8;  /* ARB8 */
		break;

	case 6:
		input.s.s_cgtype = ARB7;	/* ARB7 */
		break;

	case 4:
		if(svec[0] == 2)
			input.s.s_cgtype = ARB6;	/* ARB6 */
		else
			input.s.s_cgtype = ARB5;	/* ARB5 */
		break;

	case 2:
		input.s.s_cgtype = ARB4;	/* ARB4 */
		break;

	default:
		(void)printf("solid: %s  bad number of unique vectors (%d)\n",
			input.s.s_name, numuvec);
		return(0);
	}
	return( numuvec );
}

/*
 *  R E D O A R B :   rearranges arbs to be GIFT compatible
 */
static int
redoarb( uvec, svec, numvec )
register int *uvec, *svec;
int numvec;
{
	register int i, j;
	static int prod;

	switch( input.s.s_cgtype ) {

	case ARB8:
		/* do nothing */
		break;

	case ARB7:
		/* arb7 vectors: 0 1 2 3 4 5 6 4 */
		switch( svec[2] ) {
			case 0:
				/* 0 = 1, 3, or 4 */
				if(svec[3] == 1)
					move(4,7,6,5,1,4,3,1);
				if(svec[3] == 3)
					move(4,5,6,7,0,1,2,0);
				if(svec[3] == 4)
					move(1,2,6,5,0,3,7,0);
				break;
			case 1:
				/* 1 = 2 or 5 */
				if(svec[3] == 2)
					move(0,4,7,3,1,5,6,1);
				if(svec[3] == 5)
					move(0,3,7,4,1,2,6,1);
				break;
			case 2:
				/* 2 = 3 or 6 */
				if(svec[3] == 3)
					move(6,5,4,7,2,1,0,2);
				if(svec[3] == 6)
					move(3,0,4,7,2,1,5,2);
				break;
			case 3:
				/* 3 = 7 */
				move(2,1,5,6,3,0,4,3);
				break;
			case 4:
				/* 4 = 5 */
				/* if 4 = 7  do nothing */
				if(svec[3] == 5)
					move(1,2,3,0,5,6,7,5);
				break;
			case 5:
				/* 5 = 6 */
				move(2,3,0,1,6,7,4,6);
				break;
			case 6:
				/* 6 = 7 */
				move(3,0,1,2,7,4,5,7);
				break;
			default:
				(void)printf("redoarb: %s - bad arb7\n",
					input.s.s_name);
				return( 0 );
			}
			break;    	/* end of ARB7 case */

		case ARB6:
			/* arb6 vectors:  0 1 2 3 4 4 6 6 */

			prod = 1;
			for(i=0; i<numvec; i++)
				prod = prod * (uvec[i] + 1);
			switch( prod ) {
			case 24:
				/* 0123 unique */
				/* 4=7 and 5=6  OR  4=5 and 6=7 */
				if(svec[3] == 7)
					move(3,0,1,2,4,4,5,5);
				else
					move(0,1,2,3,4,4,6,6);
				break;
			case 1680:
				/* 4567 unique */
				/* 0=3 and 1=2  OR  0=1 and 2=3 */
				if(svec[3] == 3)
					move(7,4,5,6,0,0,1,1);
				else
					move(4,5,6,7,0,0,2,2);
				break;
			case 160:
				/* 0473 unique */
				/* 1=2 and 5=6  OR  1=5 and 2=6 */
				if(svec[3] == 2)
					move(0,3,7,4,1,1,5,5);
				else
					move(4,0,3,7,1,1,2,2);
				break;
			case 672:
				/* 3267 unique */
				/* 0=1 and 4=5  OR  0=4 and 1=5 */
				if(svec[3] == 1)
					move(3,2,6,7,0,0,4,4);
				else
					move(7,3,2,6,0,0,1,1);
				break;
			case 252:
				/* 1256 unique */
				/* 0=3 and 4=7  OR 0=4 and 3=7 */
				if(svec[3] == 3)
					move(1,2,6,5,0,0,4,4);
				else
					move(5,1,2,6,0,0,3,3);
				break;
			case 60:
				/* 0154 unique */
				/* 2=3 and 6=7  OR  2=6 and 3=7 */
				if(svec[3] == 3)
					move(0,1,5,4,2,2,6,6);
				else
					move(5,1,0,4,2,2,3,3);
				break;
			default:
				(void)printf("redoarb: %s: bad arb6\n",
					input.s.s_name);
				return( 0 );
			}
			break; 		/* end of ARB6 case */

		case ARB5:
			/* arb5 vectors:  0 1 2 3 4 4 4 4 */
			prod = 1;
			for(i=2; i<6; i++)
				prod = prod * (svec[i] + 1);
			switch( prod ) {
			case 24:
				/* 0=1=2=3 */
				move(4,5,6,7,0,0,0,0);
				break;
			case 1680:
				/* 4=5=6=7 */
				/* do nothing */
				break;
			case 160:
				/* 0=3=4=7 */
				move(1,2,6,5,0,0,0,0);
				break;
			case 672:
				/* 2=3=7=6 */
				move(0,1,5,4,2,2,2,2);
				break;
			case 252:
				/* 1=2=5=6 */
				move(0,3,7,4,1,1,1,1);
				break;
			case 60:
				/* 0=1=5=4 */
				move(3,2,6,7,0,0,0,0);
				break;
			default:
				(void)printf("redoarb: %s: bad arb5\n",
					input.s.s_name);
				return( 0 );
			}
			break;		/* end of ARB5 case */

		case ARB4:
			/* arb4 vectors:  0 1 2 0 4 4 4 4 */
			j = svec[6];
			if( svec[0] == 2 )
				j = svec[4];
			move(uvec[0],uvec[1],svec[2],uvec[0],j,j,j,j);
			break;

		default:
			(void)printf("redoarb %s: unknown arb type (%d)\n",
				input.s.s_name,input.s.s_cgtype);
			return( 0 );
	}

	return( 1 );
}


static void
move( p0, p1, p2, p3, p4, p5, p6, p7 )
int p0, p1, p2, p3, p4, p5, p6, p7;
{
	register int i, j;
	static int	pts[8];
	static fastf_t	copy[24];

	pts[0] = p0 * 3;
	pts[1] = p1 * 3;
	pts[2] = p2 * 3;
	pts[3] = p3 * 3;
	pts[4] = p4 * 3;
	pts[5] = p5 * 3;
	pts[6] = p6 * 3;
	pts[7] = p7 * 3;

	/* copy of the record */
	for(i=0; i<=21; i+=3) {
		VMOVE(&copy[i], &input.s.s_values[i]);
	}

	for(i=0; i<8; i++) {
		j = pts[i];
		VMOVE(&input.s.s_values[i*3], &copy[j]);
	}
}

static void
vectors()
{
	register int i;

	for(i=3; i<=21; i+=3) {
		VSUB2(&input.s.s_values[i],&input.s.s_values[i],&input.s.s_values[0]);
	}
}



static void
points()
{
	register int i;

	for(i=3; i<=21; i+=3) {
		VADD2(&input.s.s_values[i],&input.s.s_values[i],&input.s.s_values[0]);
	}
}

/* ------------------------------ new way ------------------------------ */
/* XXX This should move to librt/g_arb.c at some point */

#define NO	0
#define YES	1
	
/*
 *			R T _ A R B _ G E T _ C G T Y P E
 *
 * C G A R B S :   determines COMGEOM arb types from GED general arbs
 *
 *  Inputs -
 *
 *  Returns -
 *	#	Number of distinct edge vectors
 *		(Number of entries in uvec array)
 *
 *  Implicit returns -
 *	*cgtype		Comgeom type (number range 4..8;  ARB4 .. ARB8).
 *	uvec[8]
 *	svec[11]
 *			Entries [0] and [1] are special
 */
int
rt_arb_get_cgtype( cgtype, arb, tol, uvec, svec )
int			*cgtype;
struct rt_arb_internal	*arb;
struct rt_tol		*tol;
register int *uvec;	/* array of unique points */
register int *svec;	/* array of like points */
{
	register int i,j;
	int	numuvec, unique, done;
	int	si;

	RT_ARB_CK_MAGIC(arb);
	RT_CK_TOL(tol);

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO)
			svec[si] = i;
		for(j=i+1; j<8; j++) {
			int tmp;
			vect_t vtmp;

			VSUB2( vtmp, arb->pt[i], arb->pt[j] );

			if( fabs(vtmp[0]) > tol->dist) tmp = 0;
			else 	if( fabs(vtmp[1]) > tol->dist) tmp = 0;
			else 	if( fabs(vtmp[2]) > tol->dist) tmp = 0;
			else tmp = 1;

			if( tmp ) {
				if( done == NO )
					svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}

	if( si > 2 && si < 6 ) 
		svec[0] = si - 1;
	if( si > 6 )
		svec[1] = si - 5;
	for(i=1; i<=svec[1]; i++)
		svec[svec[0]+1+i] = svec[5+i];
	for(i=svec[0]+svec[1]+2; i<11; i++)
		svec[i] = -1;

	/* find the unique points */
	numuvec = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES )
			uvec[numuvec++] = j;
	}

	/* Figure out what kind of ARB this is */
	switch( numuvec ) {

	case 8:
		*cgtype = ARB8;		/* ARB8 */
		break;

	case 6:
		*cgtype = ARB7;		/* ARB7 */
		break;

	case 4:
		if(svec[0] == 2)
			*cgtype = ARB6;	/* ARB6 */
		else
			*cgtype = ARB5;	/* ARB5 */
		break;

	case 2:
		*cgtype = ARB4;		/* ARB4 */
		break;

	default:
		rt_log("rt_arb_get_cgtype: bad number of unique vectors (%d)\n",
			numuvec);
		return(0);
	}
#if 0
	printf("uvec: ");
	for(j=0; j<8; j++) printf("%d, ", uvec[j]);
	printf("\nsvec: ");
	for(j=0; j<11; j++ ) printf("%d, ", svec[j]);
	printf("\n");
#endif
	return( numuvec );
}

/*
 *			R T _ A R B _ P T M O V E
 *
 *  Note:  arbo and arbi must not point to same structure!
 */
static void
rt_arb_ptmove( arbo, arbi, p0, p1, p2, p3, p4, p5, p6, p7 )
struct rt_arb_internal	*arbo;
struct rt_arb_internal	*arbi;
int			p0, p1, p2, p3, p4, p5, p6, p7;
{

	RT_ARB_CK_MAGIC( arbo );
	RT_ARB_CK_MAGIC( arbi );

	VMOVE( arbo->pt[0], arbi->pt[p0] );
	VMOVE( arbo->pt[1], arbi->pt[p1] );
	VMOVE( arbo->pt[2], arbi->pt[p2] );
	VMOVE( arbo->pt[3], arbi->pt[p3] );
	VMOVE( arbo->pt[4], arbi->pt[p4] );
	VMOVE( arbo->pt[5], arbi->pt[p5] );
	VMOVE( arbo->pt[6], arbi->pt[p6] );
	VMOVE( arbo->pt[7], arbi->pt[p7] );
}

/*
 *			R T _ A R B _ R E D O
 *
 *  R E D O A R B :   rearranges arbs to be GIFT compatible
 *
 *  Note:  arbo and arbi must not point to same structure!
 *
 *  Returns -
 *	0	FAIL
 *	1	OK
 *
 *  Implicit returns -
 *	arb->pt[] array reorganized.
 */
static int
rt_arb_redo( arbo, arbi, uvec, svec, numvec, cgtype )
struct rt_arb_internal	*arbo;
struct rt_arb_internal	*arbi;
register int		*uvec;
register int		*svec;
int			numvec;
int			cgtype;
{
	register int i, j;
	int	prod;

	RT_ARB_CK_MAGIC( arbo );
	RT_ARB_CK_MAGIC( arbi );

	/* By default, let output be the input */
	*arbo = *arbi;		/* struct copy */

	switch( cgtype ) {

	case ARB8:
		/* do nothing */
		break;

	case ARB7:
		/* arb7 vectors: 0 1 2 3 4 5 6 4 */
		switch( svec[2] ) {
		case 0:
			/* 0 = 1, 3, or 4 */
			if(svec[3] == 1)
				rt_arb_ptmove( arbo, arbi,4,7,6,5,1,4,3,1);
			else if(svec[3] == 3)
				rt_arb_ptmove( arbo, arbi,4,5,6,7,0,1,2,0);
			else if(svec[3] == 4)
				rt_arb_ptmove( arbo, arbi,1,2,6,5,0,3,7,0);
			break;
		case 1:
			/* 1 = 2 or 5 */
			if(svec[3] == 2)
				rt_arb_ptmove( arbo, arbi,0,4,7,3,1,5,6,1);
			else if(svec[3] == 5)
				rt_arb_ptmove( arbo, arbi,0,3,7,4,1,2,6,1);
			break;
		case 2:
			/* 2 = 3 or 6 */
			if(svec[3] == 3)
				rt_arb_ptmove( arbo, arbi,6,5,4,7,2,1,0,2);
			else if(svec[3] == 6)
				rt_arb_ptmove( arbo, arbi,3,0,4,7,2,1,5,2);
			break;
		case 3:
			/* 3 = 7 */
			rt_arb_ptmove( arbo, arbi,2,1,5,6,3,0,4,3);
			break;
		case 4:
			/* 4 = 5 */
			/* if 4 = 7  do nothing */
			if(svec[3] == 5)
				rt_arb_ptmove( arbo, arbi,1,2,3,0,5,6,7,5);
			break;
		case 5:
			/* 5 = 6 */
			rt_arb_ptmove( arbo, arbi,2,3,0,1,6,7,4,6);
			break;
		case 6:
			/* 6 = 7 */
			rt_arb_ptmove( arbo, arbi,3,0,1,2,7,4,5,7);
			break;
		default:
			rt_log("rt_arb_redo: bad arb7\n");
			return( 0 );
		}
		break;    	/* end of ARB7 case */

	case ARB6:
		/* arb6 vectors:  0 1 2 3 4 4 6 6 */
		prod = 1;
		for(i=0; i<numvec; i++)
			prod = prod * (uvec[i] + 1);

		switch( prod ) {
		case 24:
			/* 0123 unique */
			/* 4=7 and 5=6  OR  4=5 and 6=7 */
			if(svec[3] == 7)
				rt_arb_ptmove( arbo, arbi,3,0,1,2,4,4,5,5);
			else
				rt_arb_ptmove( arbo, arbi,0,1,2,3,4,4,6,6);
			break;
		case 1680:
			/* 4567 unique */
			/* 0=3 and 1=2  OR  0=1 and 2=3 */
			if(svec[3] == 3)
				rt_arb_ptmove( arbo, arbi,7,4,5,6,0,0,1,1);
			else
				rt_arb_ptmove( arbo, arbi,4,5,6,7,0,0,2,2);
			break;
		case 160:
			/* 0473 unique */
			/* 1=2 and 5=6  OR  1=5 and 2=6 */
			if(svec[3] == 2)
				rt_arb_ptmove( arbo, arbi,0,3,7,4,1,1,5,5);
			else
				rt_arb_ptmove( arbo, arbi,4,0,3,7,1,1,2,2);
			break;
		case 672:
			/* 3267 unique */
			/* 0=1 and 4=5  OR  0=4 and 1=5 */
			if(svec[3] == 1)
				rt_arb_ptmove( arbo, arbi,3,2,6,7,0,0,4,4);
			else
				rt_arb_ptmove( arbo, arbi,7,3,2,6,0,0,1,1);
			break;
		case 252:
			/* 1256 unique */
			/* 0=3 and 4=7  OR 0=4 and 3=7 */
			if(svec[3] == 3)
				rt_arb_ptmove( arbo, arbi,1,2,6,5,0,0,4,4);
			else
				rt_arb_ptmove( arbo, arbi,5,1,2,6,0,0,3,3);
			break;
		case 60:
			/* 0154 unique */
			/* 2=3 and 6=7  OR  2=6 and 3=7 */
			if(svec[3] == 3)
				rt_arb_ptmove( arbo, arbi,0,1,5,4,2,2,6,6);
			else
				rt_arb_ptmove( arbo, arbi,5,1,0,4,2,2,3,3);
			break;
		default:
			rt_log("rt_arb_redo: bad arb6\n");
			return( 0 );
		}
		break; 		/* end of ARB6 case */

	case ARB5:
		/* arb5 vectors:  0 1 2 3 4 4 4 4 */
		prod = 1;
		for(i=2; i<6; i++)
			prod = prod * (svec[i] + 1);

		switch( prod ) {
		case 24:
			/* 0=1=2=3 */
			rt_arb_ptmove( arbo, arbi,4,5,6,7,0,0,0,0);
			break;
		case 1680:
			/* 4=5=6=7 */
			/* do nothing */
			break;
		case 160:
			/* 0=3=4=7 */
			rt_arb_ptmove( arbo, arbi,1,2,6,5,0,0,0,0);
			break;
		case 672:
			/* 2=3=7=6 */
			rt_arb_ptmove( arbo, arbi,0,1,5,4,2,2,2,2);
			break;
		case 252:
			/* 1=2=5=6 */
			rt_arb_ptmove( arbo, arbi,0,3,7,4,1,1,1,1);
			break;
		case 60:
			/* 0=1=5=4 */
			rt_arb_ptmove( arbo, arbi,3,2,6,7,0,0,0,0);
			break;
		default:
			rt_log("rt_arb_redo: bad arb5\n");
			return( 0 );
		}
		break;		/* end of ARB5 case */

	case ARB4:
		/* arb4 vectors:  0 1 2 0 4 4 4 4 */
		j = svec[6];
		if( svec[0] == 2 )
			j = svec[4];
		rt_arb_ptmove( arbo, arbi,uvec[0],uvec[1],svec[2],uvec[0],j,j,j,j);
		break;

	default:
		rt_log("rt_arb_redo: unknown arb type (%d)\n",
			cgtype);
		return( 0 );
	}
	return( 1 );
}

/*
 *			R T _ A R B _ S T D _ T Y P E
 *
 *  Given an ARB in internal form, return it's specific ARB type,
 *  and reorganize the points to be in GIFT "standard" order.
 *
 *  Set tol.dist = 0.0001 to obtain past behavior.
 *
 *  Returns -
 *	0	Error in input ARB
 *	4	ARB4
 *	5	ARB5
 *	6	ARB6
 *	7	ARB7
 *	8	ARB8
 *
 *  Implicit return -
 *	rt_arb_internal pt[] array reorganized into GIFT "standard" order.
 */
int
rt_arb_std_type( ip, tol )
struct rt_db_internal	*ip;
struct rt_tol		*tol;
{
	struct rt_arb_internal	*arb;
	struct rt_arb_internal	arbo;
	int i;
	int uvec[8], svec[11];
	int	nedge;
	int	cgtype = 0;

	RT_CK_DB_INTERNAL(ip);
	RT_CK_TOL(tol);

	if( ip->idb_type != ID_ARB8 )  rt_bomb("rt_arb_std_type: not ARB!\n");

	arb = (struct rt_arb_internal *)ip->idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if( (nedge = rt_arb_get_cgtype( &cgtype, arb, tol, uvec, svec )) == 0 )
		return(0);

	arbo.magic = RT_ARB_INTERNAL_MAGIC;
	if( rt_arb_redo( &arbo, arb, uvec, svec, nedge, cgtype) == 0 )
		return( 0 );

	*arb = arbo;		/* struct copy of reorganized arb */
	return( cgtype );
}


/* 
 *			R T _ A R B _ C E N T R O I D
 *
 * Find the center point for the arb whose values are in the s array,
 * with the given number of verticies.  Return the point in center_pt.
 * WARNING: The s array is dbfloat_t's not fastf_t's.
 */
void
rt_arb_centroid( center_pt, arb, npoints )
point_t			center_pt;
struct rt_arb_internal	*arb;
int			npoints;
{
	register int	j;
	fastf_t		div;
	point_t		sum;

	RT_ARB_CK_MAGIC(arb);

	VSETALL(sum, 0);

	for( j=0; j < npoints; j++ )  {
		VADD2( sum, sum, arb->pt[j] );
	}
	div = 1.0 / npoints;
	VSCALE( center_pt, sum, div );
}
