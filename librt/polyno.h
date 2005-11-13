/*
 *			P O L Y N O . H
 *
 *	Definitions for handling polynomial equations
 *
 *  Author -
 *	Jeff Hanes
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */
#ifndef POLYNO_H
#define POLYNO_H seen

#define		MAXP			4	/* Maximum Poly Order */

#define		Abs( a )		((a) >= 0 ? (a) : -(a))
#define 	Max( a, b )		((a) > (b) ? (a) : (b))

/* error return value for 'polyMul' */
#define		PM_NULL			((poly *)0)

/* polynomial data type */
typedef  struct {
	int		dgr;
	double		cf[MAXP+1];
}  poly;


/* library functions in polylib.c */
extern poly	*polyAdd(), *polySub(), *polyMul(), *polyScal();
extern void	quadratic(), synDiv(), prntpoly(), pr_poly();
extern int	quartic(), cubic();

#endif /* POLYNO_H */
