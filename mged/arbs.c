
/*
 *			A R B S . C
 *
 *	f_3ptarb()	finds arb8 defined by 3 points, 2 coordinates of
 *			a 4th point, and a thickness
 *
 *	f_rfarb()	finds arb8 defined by rot & fallback angles, one point,
 *			2 coordinates of the 3 remaining points, and a thickness
 *
 */

#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include "./machine.h"
#include "../h/vmath.h"
#include "../h/db.h"
#include "ged.h"
#include "objdir.h"
#include <math.h>
#include "dm.h"

extern void aexists();
extern double atof();
extern char *strcat(), *strcpy();

double fabs();

extern int 	numargs;
extern char	*cmd_args[];
int		newargs;
int		args;
int		argcnt;
char		**promp;

static union record record;

#define MAXLINE	512
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

f_3ptarb(  )
{
	int i, solve;
	float vec1[3], vec2[3], pt4[2], length, thick;
	double norm[4];
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
	if( lookup(cmd_args[1], LOOKUP_QUIET) != DIR_NULL ) {
		(void)printf("%s:  already exists\n",cmd_args[1]);
		return;
	}

	if( strlen(cmd_args[1]) >= NAMESIZE ) {
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

	norm[3] =  record.s.s_values[0] * norm[0]
                 + record.s.s_values[1] * norm[1]
   		 + record.s.s_values[2] * norm[2];


	switch( solve ) {

		case XCOORD:
			/* solve for x-coord of 4th point */
			record.s.s_values[10] = pt4[0];	/* y-coord */
			record.s.s_values[11] = pt4[1]; 	/* z-coord */
			record.s.s_values[9] =  ( norm[3]
						- norm[1] * record.s.s_values[10]
						- norm[2] * record.s.s_values[11])
						/ norm[0];	/* x-coord */
		break;

		case YCOORD:
			/* solve for y-coord of 4th point */
			record.s.s_values[9] = pt4[0];	/* x-coord */
			record.s.s_values[11] = pt4[1]; 	/* z-coord */
			record.s.s_values[10] =  ( norm[3]
						- norm[0] * record.s.s_values[9]
						- norm[2] * record.s.s_values[11])
						/ norm[1];	/* y-coord */
		break;

		case ZCOORD:
			/* solve for z-coord of 4th point */
			record.s.s_values[9] = pt4[0];	/* x-coord */
			record.s.s_values[10] = pt4[1]; 	/* y-coord */
			record.s.s_values[11] =  ( norm[3]
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

	if( (dp = dir_add(record.s.s_name, -1, DIR_SOLID, 0)) == DIR_NULL )
		return;
	db_alloc( dp, 1 );
	db_putrec( dp, &record, 0 );
	drawHobj(dp, ROOT, 0, identity);
	dmp->dmr_colorchange();
	dmaflag = 1;
	return;
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
f_rfarb()
{
	struct directory *dp;
	int i, solve[3];
	float pt[3][2], thick, rota, fba;
	double norm[4];


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
	if( lookup(cmd_args[1], LOOKUP_QUIET) != DIR_NULL ) {
		(void)printf("%s:  already exists\n",cmd_args[1]);
		return;
	}

	if( strlen(cmd_args[1]) >= NAMESIZE ) {
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

	norm[3] =  record.s.s_values[0] * norm[0]
                 + record.s.s_values[1] * norm[1]
   		 + record.s.s_values[2] * norm[2];

	/* calculate the unknown coordinate for points 2,3,4 */
	for(i=0; i<3; i++) {

		switch( solve[i] ) {
			case XCOORD:
				record.s.s_values[3*i+4] = pt[i][0];
				record.s.s_values[3*i+5] = pt[i][1];
				record.s.s_values[3*i+3] = ( norm[3]
					- norm[1] * record.s.s_values[3*i+4]
					- norm[2] * record.s.s_values[3*i+5])
					/ norm[0];
			break;
			case YCOORD:
				record.s.s_values[3*i+3] = pt[i][0];
				record.s.s_values[3*i+5] = pt[i][1];
				record.s.s_values[3*i+4] = ( norm[3]
					- norm[0] * record.s.s_values[3*i+3]
					- norm[2] * record.s.s_values[3*i+5])
					/ norm[1];
			break;
			case ZCOORD:
				record.s.s_values[3*i+3] = pt[i][0];
				record.s.s_values[3*i+4] = pt[i][1];
				record.s.s_values[3*i+5] = ( norm[3]
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

	if( (dp = dir_add(record.s.s_name, -1, DIR_SOLID, 0)) == DIR_NULL )
		return;
	db_alloc( dp, 1 );
	db_putrec( dp, &record, 0 );
	drawHobj(dp, ROOT, 0, identity);
	dmp->dmr_colorchange();
	dmaflag = 1;
	return;
}




