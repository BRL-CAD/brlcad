/*
 *			G E D _ T Y P E S . H
 *
 *  Data type declarations for GED
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

/*
 * type definition for new "C" declaration:  "homog_t".
 *
 * This is to make declaring Homogeneous Transform matrices easier.
 */
typedef	float	mat_t[4*4];
typedef	float	*matp_t;
typedef	float	vect_t[4];
typedef	float	*vectp_t;

#define X	0
#define Y	1
#define Z	2
#define H	3

#define ALPHA	0
#define BETA	1
#define GAMMA	2
