/*
 *			M A T H T A B . H
 *
 *  Definitions to implement fast (but approximate) math library functions
 *  using table lookups.
 *
 *  Authors -
 *	Philip Dykstra
 *	Michael John Muuss
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

/* All these should become inline-macros */
extern double tab_sin();
extern double rand0to1();
extern double rand_half();
