/*
 *  			S O L I D . H
 *
 *	Solids structure definition
 *
 *  Author -
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

#include "solid.h"

extern struct solid	FreeSolid;	/* Head of freelist */
extern struct solid	HeadSolid;	/* Head of doubly linked solid tab */
extern int		ndrawn;
