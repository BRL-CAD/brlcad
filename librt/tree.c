/*
 *			T R E E
 *
 * Ray Tracing program, GED tree tracer.
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "db.h"
#include "debug.h"

extern struct soltab *HeadSolid;
extern struct functab functab[];

static char idmap[] = {
	ID_NULL,	/* undefined, 0 */
	ID_NULL,	/*  RPP	1 axis-aligned rectangular parallelopiped */
	ID_NULL,	/* BOX	2 arbitrary rectangular parallelopiped */
	ID_NULL,	/* RAW	3 right-angle wedge */
	ID_NULL,	/* ARB4	4 tetrahedron */
	ID_NULL,	/* ARB5	5 pyramid */
	ID_NULL,	/* ARB6	6 extruded triangle */
	ID_NULL,	/* ARB7	7 weird 7-vertex shape */
	ID_NULL,	/* ARB8	8 hexahedron */
	ID_NULL,	/* ELL	9 ellipsoid */
	ID_NULL,	/* ELL1	10 another ellipsoid ? */
	ID_NULL,	/* SPH	11 sphere */
	ID_NULL,	/* RCC	12 right circular cylinder */
	ID_NULL,	/* REC	13 right elliptic sylinder */
	ID_NULL,	/* TRC	14 truncated regular cone */
	ID_NULL,	/* TEC	15 truncated elliptic cone */
	ID_TOR,		/* TOR	16 toroid */
	ID_NULL,	/* TGC	17 truncated general cone */
	ID_TGC,		/* GENTGC 18 supergeneralized TGC; internal form */
	ID_ELL,		/* GENELL 19: V,A,B,C */
	ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
	ID_ARS,		/* ARS 21: arbitrary triangular-surfaced polyhedron */
	ID_NULL,	/* ARSCONT 22: extension record type for ARS solid */
	ID_NULL		/* n+1 */
};

get_tree(node)
char *node;
{
	union record rec;

	/* For now, a very primitive function */

	rec.s.s_id = GENELL;
	VSET( &rec.s.s_values[0], 0,3,0 );	/*  V */
	VSET( &rec.s.s_values[3], 2.5,0,0 );	/*  A */
	VSET( &rec.s.s_values[6], 0,0.75,0 );	/*  B */
	VSET( &rec.s.s_values[9], 0,0,1 );	/*  C */
	add_solid( &rec, "Ell" );

	rec.s.s_id = GENELL;
	VSET( &rec.s.s_values[0], 4,3,0 );	/*  V */
	VSET( &rec.s.s_values[3], 1,0,0 );	/*  A */
	VSET( &rec.s.s_values[6], 0,1,0 );	/*  B */
	VSET( &rec.s.s_values[9], 0,0,1 );	/*  C */
	add_solid( &rec, "Near Sphere" );

	rec.s.s_id = GENELL;
	VSET( &rec.s.s_values[0], 4,3,-5 );	/*  V */
	VSET( &rec.s.s_values[3], 1,0,0 );	/*  A */
	VSET( &rec.s.s_values[6], 0,1,0 );	/*  B */
	VSET( &rec.s.s_values[9], 0,0,1 );	/*  C */
	add_solid( &rec, "Far Sphere" );

	rec.s.s_id = GENARB8;
	/* set Z=0 to explode the formulas */
#define Z 1
	VSET( &rec.s.s_values[0*3], -2,-1,Z);	/* Abs V */
	VSET( &rec.s.s_values[1*3], 4,0,0);	/* Relative to V ... */
	VSET( &rec.s.s_values[2*3], 4,2,0);
	VSET( &rec.s.s_values[3*3], 0,2,0);

#define H 4
	VSET( &rec.s.s_values[4*3], 2,1,H);
	VSET( &rec.s.s_values[5*3], 2,1,H);
	VSET( &rec.s.s_values[6*3], 2,1,H);
	VSET( &rec.s.s_values[7*3], 2,1,H);
	add_solid( &rec, "Flat Arb" );
#undef Z
#undef H

	rec.s.s_id = GENARB8;
	/* set Z=0 to explode the formulas */
#define Z 1
	VSET( &rec.s.s_values[0*3], 0,-3,Z);	/* Abs V */
	VSET( &rec.s.s_values[1*3], 4,0,0);	/* Relative to V ... */
	VSET( &rec.s.s_values[2*3], 4,2,0);
	VSET( &rec.s.s_values[3*3], 0,2,0);

#define H 0.5
	VSET( &rec.s.s_values[4*3], 2,1,H);
	VSET( &rec.s.s_values[5*3], 2,1,H);
	VSET( &rec.s.s_values[6*3], 2,1,H);
	VSET( &rec.s.s_values[7*3], 2,1,H);
#undef Z
#undef H
	add_solid( &rec, "Flat Arb" );
}

static
add_solid( rec, name )
union record *rec;
char *name;
{
	register struct soltab *stp;
	mat_t	mat;

	GETSTRUCT(stp, soltab);
	stp->st_id = idmap[rec->u_id];
	stp->st_name = name;
	stp->st_specific = (int *)0;
	mat_idn( mat);

	if( functab[stp->st_id].ft_prep( &rec->s, stp, mat ) )  {
		/* Error, solid no good */
		free(stp);
		return;		/* continue */
	}

	/* For now, just link them all onto the same list */
	stp->st_forw = HeadSolid;
	HeadSolid = stp;

	if(debug&DEBUG_SOLIDS)  {
		printf("-------------- %s -------------\n", stp->st_name);
		VPRINT("Bound Sph CENTER", stp->st_center);
		printf("Bound Sph Rad**2 = %f\n", stp->st_radsq);
		functab[stp->st_id].ft_print( stp );
	}
}
