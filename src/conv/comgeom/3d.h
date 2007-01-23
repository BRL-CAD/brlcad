/*                            3 D . H
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
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
 *
 */
/** @file 3d.h
 *
 * This header file describes the object file structure,
 * and some commonly used 3D vector math macros.
 *
 * Authors -
 *	Michael John Muuss
 *	Earl P Weaver
 *
 *	Ballistic Research Laboratory
 *	U. S. Army
 *	March, 1980
 *
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	March 81  CAS	Added ARS 'A' and 'B' record information
 *
 *	09/22/81  MJM	Changed m_ record to use matrix transformation
 */

#define NAMESIZE	16
#define NAMEMOVE(from,to)	strncpy(to,from,NAMESIZE)
extern char	*strncpy();

/*
 *		OBJECT FILE FORMAT
 *
 * All records are 128 bytes long, and are composed of one of 5 formats:
 *	A SOLID description
 *	A COMBINATION description (1 header, 1 member)
 *	A COMBINATION extention (2 members)
 *	An ARS 'A' (header) record
 *	An ARS 'B' (data) record
 */
union record  {
	char	u_id;		/* To differentiate SOLID vs COMB */
	char	u_size[128];	/* Total record size */
	struct solids  {
		char	s_id;
		char	s_type;
		char	s_name[16];
		short	s_num;		/* COMGEOM solid # temporary */
		float	s_values[24];
	}  s;
	struct combination  {
		char	c_id;
		char	c_flags;
		char	c_name[16];
		short	c_regionid;
		short	c_aircode;
		short	c_length;		/* in 128 byte granules */
		short	c_num;			/* region #, from COMGEOM */
		short	c_material;		/* material code */
		short  	c_los;			/* line of sight percentage */
		char	c_pad[128-30];
	}  c;
	struct members  {
		char	m_id;
		char	m_relation;		/* OR, UNION, DIFF */
		char	m_brname[16];		/* Name of this branch */
		char	m_instname[16];		/* Name of refered-to obj */
		short 	m_pad1;
		mat_t	m_mat;			/* Homogeneous Xform Matrix */
		short	m_num;			/* solid # ref, from COMGEOM */
		short	m_pad2;
	}  M;
	struct	ars_rec	{
		char	a_id;		/* A */
		char	a_type;
		char	a_name[16];
		short	a_m;		/* # curves */
		short	a_n;		/* # points per curve */
		short	a_curlen;	/* # granules per curve */
		short	a_totlen;	/* # granules per solid */
		short	a_pad;
		float	a_xmax;		/* max x coordinate */
		float	a_xmin;		/* min x coordinate */
		float	a_ymax;		/* max y coordinate */
		float	a_ymin;		/* min y coordinate */
		float	a_zmax;		/* max z coordinate */
		float	a_zmin;		/* min z coordinate */
	}    a;
	struct	ars_ext	{
		char	b_id;		/*   B   */
		char	b_type;
		short	b_n;        /* 1.1 1.2 1.3 2.1 2.2 2.3 */
		short	b_ngranule;
		short	b_pad;
		float	b_values[8*3];
	}   b;
};

/*
 * Record id types
 */
#define SOLID	'S'
#define COMB	'C'
#define MEMB	'M'
#define ARS_A	'A'
#define ARS_B	'B'

/*
 * for the m_relation fields
 */
#define UNION		'u'
#define SUBTRACT	'-'
#define INTERSECT	'+'

/*
 *	SOLID TYPES
 */
#define RPP	1
#define BOX	2
#define RAW	3
#define ARB4	4
#define ARB5	5
#define ARB6	6
#define ARB7	7
#define ARB8	8
#define ELL	9
#define ELL1	10
#define SPH	11
#define RCC	12
#define REC	13
#define TRC	14
#define TEC	15
#define TOR	16
#define TGC	17
#define GENTGC	18	/* Supergeneralized TGC; internal form */
#define GENELL	19	/* Ready for drawing ELL:  V,A,B,C */
#define GENARB8	20	/* Generalized ARB8: V, and 7 other vectors */
#define	ARS	21	/* arbitrary triangular surfaced polyhedron */
#define ARSCONT 22	/* extention record type for ARS solid */

/*
 *			V E C T O R   M A T H
 */

/* Set a vector pointer 'a' to have x,y,z elements b,c,d */
#define VSET(a,b,c,d)	*(a) = (b);\
			*((a)+1) = (c);\
			*((a)+2) = (d)

/* Transfer x,y,z from vector pointed at by 'b' to that pointed by 'a' */
#define VMOVE(a,b)	*(a) = *(b);\
			*((a)+1) = *((b)+1);\
			*((a)+2) = *((b)+2)

/* Add x,y,z in vectors 'b' and 'c', store in 'a' */
#define VADD2(a,b,c)	*(a) = *(b) + *(c);\
			*((a)+1) = *((b)+1) + *((c)+1);\
			*((a)+2) = *((b)+2) + *((c)+2)

/* Subtract x,y,z in vector 'c' from 'b', store in 'a' */
#define VSUB2(a,b,c)	*(a) = *(b) - *(c);\
			*((a)+1) = *((b)+1) - *((c)+1);\
			*((a)+2) = *((b)+2) - *((c)+2)

#define VADD3(a,b,c,d)	*(a) = *(b) + *(c) + *(d);\
			*((a)+1) = *((b)+1) + *((c)+1) + *((d)+1);\
			*((a)+2) = *((b)+2) + *((c)+2) + *((d)+2)

#define VADD4(a,b,c,d,e) *(a) = *(b) + *(c) + *(d) + *(e);\
			*((a)+1) = *((b)+1) + *((c)+1) + *((d)+1) + *((e)+1);\
			*((a)+2) = *((b)+2) + *((c)+2) + *((d)+2) + *((e)+2)

/* Scale all elements in 'b' by multiplication by scalar 'c', store in 'a' */
#define VSCALE(a,b,c)	*(a) = *(b) * (c);\
			*((a)+1) = *((b)+1) * (c);\
			*((a)+2) = *((b)+2) * (c)

/* Compose vector 'a' of:
 *	Vector 'b' plus
 *	scalar 'c' times vector 'd' plus
 *	scalar 'e' times vector 'f'
 */
#define VCOMPOSE(a,b,c,d,e,f)	\
	*(a+0) = *(b+0) + c * (*(d+0)) + e * (*(f+0));\
	*(a+1) = *(b+1) + c * (*(d+1)) + e * (*(f+1));\
	*(a+2) = *(b+2) + c * (*(d+2)) + e * (*(f+2))

/* Return scalar magnitude of vector pointed at by 'a' */
#define MAGNITUDE(a)	sqrt( MAGSQ( (a) ))
#define MAGSQ(a)	( *(a)*(*(a)) + *((a)+1)*(*((a)+1)) + *((a)+2)*(*((a)+2)) )

/*
 * Cross product:
 *	12-21, 20-02, 01-10
 *	b1*c2-b2*c1, b2c0-b0c2, b0c1-b1c0
 */
#define VCROSS( a, b, c)	\
	*(a+0) = *(b+1) * *(c+2) - *(b+2) * *(c+1);\
	*(a+1) = *(b+2) * *(c+0) - *(b+0) * *(c+2);\
	*(a+2) = *(b+0) * *(c+1) - *(b+1) * *(c+0)

#define VPRINT( a, b )	printf("%s (%f, %f, %f)\n", a, (b)[0], (b)[1], (b)[2] )

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
