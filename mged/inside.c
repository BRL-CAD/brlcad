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

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

extern int	args;		/* total number of args available */
extern int	argcnt;		/* holder for number of args added later */
extern int	newargs;	/* number of args from getcmd() */
extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */
extern char	**promp;	/* pointer to a pointer to a char */

void		arb_center();

static int	nface, np, nm, mp;
static fastf_t	thick[6];
static vect_t	pc;
static union record newrec;		/* new record to find inside solid */

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


/*	F _ I N S I D E ( ) :	control routine...reads all data
 */
void
f_inside()
{
	register int i;
	register struct directory *dp;
	mat_t newmat;
	int type, p1, p2, p3;

	(void)signal( SIGINT, sig2);	/* allow interrupts */

	/* don't use any arguments entered initially 
	 * will explicitly ask for data as needed
	 */
	args = numargs;
	argcnt = 0;

	/* SCHEME:
	 *	if in solid edit, use "edited" solid as newrec
	 *	if in object edit, use "key" solid as newrec
	 *	else get solid name to use as newrec
	 */

	if( state == ST_S_EDIT ) {
		/* solid edit mode, use edited solid for newrec */
		newrec = es_rec;
		/* apply es_mat editing to parameters */
		MAT4X3PNT( &newrec.s.s_values[0], es_mat, &es_rec.s.s_values[0]);
		for(i=1; i<8; i++) {
			MAT4X3VEC(	&newrec.s.s_values[i*3],
					es_mat,
					&es_rec.s.s_values[i*3] );
		}

		if(newrec.s.s_type == GENARB8) {
			/* must find new plane equations to account for
			 *	any editing in the es_mat matrix
			 */
			calc_planes( &newrec.s, newrec.s.s_cgtype );
		}

		(void)printf("Outside solid: ");
		for(i=0; i <= illump->s_last; i++) {
			(void)printf("/%s",illump->s_path[i]->d_namep);
		}
		(void)printf("\n");
	}

	else
	if( state == ST_O_EDIT ) {
		/* object edit mode */
		if( illump->s_Eflag ) {
			(void)printf("Cannot find inside of a processed (E'd) region\n");
			return;
		}
		/* use the solid at bottom of path (key solid) for newrec */
		newrec = es_rec;
		/* apply es_mat and modelchanges editing to parameters */
		mat_mul(newmat, modelchanges, es_mat);
		MAT4X3PNT( &newrec.s.s_values[0], newmat, &es_rec.s.s_values[0]);
		for(i=1; i<8; i++) {
			MAT4X3VEC(	&newrec.s.s_values[i*3],
					newmat,
					&es_rec.s.s_values[i*3] );
		}

		if(newrec.s.s_type == GENARB8) {
			/* must find new plane equations to account for
			 *	any editing done in the path to this solid
			 */
			calc_planes( &newrec.s, newrec.s.s_cgtype );
		}

		(void)printf("Outside solid: ");
		for(i=0; i <= illump->s_last; i++) {
			(void)printf("/%s",illump->s_path[i]->d_namep);
		}
		(void)printf("\n");
	}

	else {
		/* Not doing any editing....ask for outside solid */
		(void)printf("Enter name of outside solid: ");
		argcnt = getcmd(args);
		if( (dp = db_lookup( dbip,  cmd_args[args], LOOKUP_NOISY )) == DIR_NULL )  
			return;
		args += argcnt;
		if( db_get( dbip,  dp, &newrec, 0 , 1) < 0 )  READ_ERR_return;

		if(newrec.u_id != ID_SOLID) {
			(void)printf("%s: NOT a solid\n",dp->d_namep);
			return;
		}

		if( (type = newrec.s.s_cgtype) < 0 )
			type *= -1;
		if( type == BOX || type == RPP )
			type = ARB8;
		if( type == RAW ) {
			VMOVE(&newrec.s.s_values[3], &newrec.s.s_values[9]);
			VMOVE(&newrec.s.s_values[6], &newrec.s.s_values[21]);
			VMOVE(&newrec.s.s_values[9], &newrec.s.s_values[12]);
			VMOVE(&newrec.s.s_values[12], &newrec.s.s_values[3]);
			VMOVE(&newrec.s.s_values[15], &newrec.s.s_values[6]);
			VMOVE(&newrec.s.s_values[18], &newrec.s.s_values[18]);
			VMOVE(&newrec.s.s_values[21], &newrec.s.s_values[15]);
		}
		newrec.s.s_cgtype = type;

		if( newrec.s.s_type == GENARB8 ) {
			/* find the comgeom arb type */
			if( (type = type_arb( &newrec )) == 0 ) {
				(void)printf("%s: BAD ARB\n",newrec.s.s_name);
				return;
			}
			newrec.s.s_cgtype = type;

			/* find the plane equations */
			calc_planes( &newrec.s, type );
		}
	}

	/* newrec is now loaded with the outside solid data */


	/* get the inside solid name */
	argcnt=0;
	while( !argcnt ){		/* no command yet */
		(void)printf("Enter name of the inside solid: ");
		argcnt = getcmd(args);
	}
	if( db_lookup( dbip,  cmd_args[args], LOOKUP_QUIET ) != DIR_NULL ) {
		aexists( cmd_args[args] );
		return;
	}
	if( (int)strlen(cmd_args[args]) >= NAMESIZE )  {
		(void)printf("Names are limited to %d characters\n", NAMESIZE-1);
		return;
	}
	NAMEMOVE( cmd_args[args], newrec.s.s_name );

	args += argcnt;

	/* get thicknesses and calculate parameters for newrec */
	switch( newrec.s.s_type ) {

		case GENARB8:
			nface = 6;
			np = newrec.s.s_cgtype;
			nm = np - 4;
			mp = 8;

			/* point notation */
			for(i=3; i<=21; i+=3) {
				VADD2(	&newrec.s.s_values[i],
					&newrec.s.s_values[i],
					&newrec.s.s_values[0] );
			}

			switch( newrec.s.s_cgtype ) {

				case ARB8:
					promp = p_arb8;
				break;

				case ARB7:
					promp = p_arb7;
				break;

				case ARB6:
					promp = p_arb6;
					nface = 5;
					mp = 6;
					VMOVE( &newrec.s.s_values[15],
						&newrec.s.s_values[18] );
				break;

				case ARB5:
					promp = p_arb5;
					nface = 5;
					mp = 6;
				break;

				case ARB4:
					promp = p_arb4;
					nface = mp = 4;
					VMOVE( &newrec.s.s_values[9],
						&newrec.s.s_values[12] );
				break;
			}

			for(i=0; i<nface; i++) {
				(void)printf("%s",promp[i]);
				if( (argcnt = getcmd( args )) < 0 )
					return;
				thick[i] = atof(cmd_args[args]) * local2base;
				args += argcnt;
			}

			if( arbin() )
				return;

			/* back to vector notation */
			for(i=3; i<=21; i+=3) {
				VSUB2(	&newrec.s.s_values[i],
					&newrec.s.s_values[i],
					&newrec.s.s_values[0] );
			}
		break;

		case GENTGC:
			promp = p_tgcin;
			for(i=0; i<3; i++) {
				(void)printf("%s",promp[i]);
				if( (argcnt = getcmd(args)) < 0 )
					return;
				thick[i] = atof( cmd_args[args] ) * local2base;
				args += argcnt;
			}

			if( tgcin() )
				return;
		break;

		case GENELL:
			(void)printf("Enter desired thickness: ");
			if( (argcnt = getcmd(args)) < 0 )
				return;
			thick[0] = atof( cmd_args[args] ) * local2base;

			if( ellgin() )
				return;
		break;

		case TOR:
			(void)printf("Enter desired thickness: ");
			if( (argcnt = getcmd(args)) < 0 )
				return;
			thick[0] = atof( cmd_args[args] ) * local2base;

			if( torin() )
				return;
		break;

		default:
			(void)printf("Cannot find inside for this record (%c) type\n",newrec.s.s_type);
			return;
	}

	/* don't allow interrupts while we update the database! */
	(void)signal( SIGINT, SIG_IGN);
 
	/* Add to in-core directory */
	if( (dp = db_diradd( dbip,  newrec.s.s_name, -1, 0, DIR_SOLID )) == DIR_NULL ||
	    db_alloc( dbip, dp, 1 ) < 0 )  {
	    	ALLOC_ERR_return;
	}
	if( db_put( dbip, dp, &newrec, 0, 1 ) < 0 )  WRITE_ERR_return;

	/* Draw the new solid */
	{
		char	*arglist[3];
		arglist[0] = "e";
		arglist[1] = newrec.s.s_name;
		f_edit( 2, arglist );
	}
}



/* finds inside arbs */
int
arbin()
{
	int i;

	/* find reference point (pc[3]) to find direction of normals */
	arb_center(pc, newrec.s.s_values, np);

	/* new face planes for the desired thicknesses */
	for(i=0; i<nface; i++) {
		if( (es_peqn[i][3] - VDOT(pc, &es_peqn[i][0])) > 0.0 )
			thick[i] *= -1.0;
		es_peqn[i][3] += thick[i];
	}

	/* find the new vertices by intersecting the new face planes */
	for(i=0; i<8; i++) {
		if( intersect(np, i*3, i, &newrec.s) ) {
			(void)printf("cannot find inside arb\n");
			/* restore es_peqn for anyone else */
			for(i=0; i<nface; i++)
				es_peqn[i][3] -= thick[i];
			return(1);
		}
	}

	/* restore es_peqn for anyone else */
	for(i=0; i<nface; i++)
		es_peqn[i][3] -= thick[i];
	return(0);
}

/* 
 *	A R B _ C E N T E R
 *
 * Find the center point for the arb whose values are in the s array,
 * with the given number of verticies.  Return the point in cpt.
 * WARNING: The s array is dbfloat_t's not fastf_t's.
 */
void
arb_center( cpt, s, npoints )
vect_t cpt;
dbfloat_t s[];
int npoints;
{
	int i,j,k;
	fastf_t temp;

	for(i=0; i<3; i++) {
		temp = 0.0;
		for(j=0; j<npoints; j++) {
			k = j * 3 + i;
			temp += s[k];
		}
		cpt[i] = temp / (fastf_t)npoints;
	}
}



/* finds inside of tgc */
int
tgcin()
{
	fastf_t	mag[5], nmag[5];
	vect_t	unitH, unitA, unitB;
	vect_t	hwork;
	fastf_t	aa[2], ab[2], ba[2], bb[2];
	fastf_t	dt, h1, h2, ht, dtha, dthb;
	fastf_t	s, d4, d5, ctan, t3;
	register int i, j, k;
	double ratio;

	thick[3] = thick[2];

	for(i=0; i<5; i++) {
		j = (i+1) * 3;
		mag[i] = MAGNITUDE( &newrec.s.s_values[j] );
	}
	if( (ratio = (mag[2] / mag[1])) < .8 )
		thick[3] = thick[3] / (1.016447 * pow(ratio, .071834));
	VSCALE(unitH, &newrec.s.s_values[3], 1.0/mag[0]);
	VSCALE(unitA, &newrec.s.s_values[6], 1.0/mag[1]);
	VSCALE(unitB, &newrec.s.s_values[9], 1.0/mag[2]);

	VCROSS(hwork, unitA, unitB);
	if( (dt = VDOT(hwork, unitH)) == 0.0 ) {
		(void)printf("BAD cylinder\n");
		return(1);
	}
	h1 = thick[0] / dt;
	h2 = thick[1] / dt;
	if( (ht = dt * mag[0]) == 0.0 ) {
		(void)printf("Cannot find the inside cylinder\n");
		return(1);
	}
	dtha = VDOT(unitA, &newrec.s.s_values[3]);
	dthb = VDOT(unitB, &newrec.s.s_values[3]);
	s = 1.0;
	for(k=0; k<2; k++) {
		if( k )
			s = -1.0;
		d4 = s * dtha + mag[3];
		d5 = s * dthb + mag[4];
		ctan = (mag[1] - d4) / ht;
		t3 = thick[2] * sqrt( (mag[1] - d4)*(mag[1] - d4) + ht*ht ) / ht;
		aa[k] = mag[1] - t3 - (thick[0]*ctan);
		ab[k] = d4 - t3 + (thick[1]*ctan);

		ctan = (mag[2] - d5) / ht;
		t3 = thick[3] * sqrt( (mag[2]-d5)*(mag[2]-d5) + ht*ht ) / ht;
		ba[k] = mag[2] - t3 - (thick[0]*ctan);
		bb[k] = d5 - t3 + (thick[1]*ctan);
	}

	nmag[0] = mag[0] - (h1+h2);
	nmag[1] = ( aa[0] + aa[1] ) * .5;
	nmag[2] = ( ba[0] + ba[1] ) * .5;
	nmag[3] = (ab[0] + ab[1]) * .5;
	nmag[4] = (bb[0] + bb[1]) * .5;
	VSCALE(&newrec.s.s_values[3], &newrec.s.s_values[3], nmag[0]/mag[0]);
	VSCALE(&newrec.s.s_values[6], &newrec.s.s_values[6], nmag[1]/mag[1]);
	VSCALE(&newrec.s.s_values[9], &newrec.s.s_values[9], nmag[2]/mag[2]);
	VSCALE(&newrec.s.s_values[12], &newrec.s.s_values[12], nmag[3]/mag[3]);
	VSCALE(&newrec.s.s_values[15], &newrec.s.s_values[15], nmag[4]/mag[4]);

	for( i=0; i<3; i++ ) 
		newrec.s.s_values[i] = newrec.s.s_values[i]
					 + unitH[i]*h1
		 + .5 * ( (aa[0] - aa[1])*unitA[i] + (ba[0] - ba[1])*unitB[i] );
	return(0);
}


/* finds inside of torus */
int
torin()
{
	fastf_t	mr1, mr2, mnewr2, msum, mdif, nmsum, nmdif;


	if( thick[0] == 0.0 )
		return(0);

	mr1 = MAGNITUDE( &newrec.s.s_values[6] );
	mr2 = MAGNITUDE( &newrec.s.s_values[3] );
	if( thick[0] < 0 ) {
		if( (mr2 - thick[0]) > (mr1 + .01) ) {
			(void)printf("cannot do: r2 > r1\n");
			return(1);
		}
	}
	if( thick[0] >= mr2 ) {
		(void)printf("cannot do: r2 <= 0\n");
		return(1);
	}

	mnewr2 = mr2 - thick[0];
	VSCALE(&newrec.s.s_values[3],&newrec.s.s_values[3],mnewr2/mr2);
	msum = MAGNITUDE( &newrec.s.s_values[18] );
	mdif = MAGNITUDE( &newrec.s.s_values[12] );
	nmsum = mr1 + mnewr2;
	nmdif = mr1 - mnewr2;
	VSCALE(&newrec.s.s_values[12],&newrec.s.s_values[12],nmdif/mdif);
	VSCALE(&newrec.s.s_values[15],&newrec.s.s_values[15],nmdif/mdif);
	VSCALE(&newrec.s.s_values[18],&newrec.s.s_values[18],nmsum/msum);
	VSCALE(&newrec.s.s_values[21],&newrec.s.s_values[21],nmsum/msum);
	return(0);
}


/* finds inside ellg */
int
ellgin()
{
	int i, j, k, order[3];
	fastf_t mag[3], nmag[3];
	fastf_t ratio;

	if( thick[0] == 0.0 ) 
		return(0);

	for(i=0; i<3; i++) {
		j = (i + 1) * 3;
		mag[i] = MAGNITUDE( &newrec.s.s_values[j] );
		order[i] = i;
		thick[i] = thick[0];
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
		if( (nmag[i] = mag[i] - thick[i]) <= 0.0 )
			(void)printf("Warning: new vector length <= 0 \n");
		j = (i+1) * 3;
		VSCALE(&newrec.s.s_values[j], &newrec.s.s_values[j],
			nmag[i]/mag[i]);
	}
	return(0);
}
