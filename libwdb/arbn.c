/*
 *			A R B N . C
 *
 *  libwdb support for writing an ARBN.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ A R B N
 *
 *  Caller is responsible for freeing eqn[]
 *
 *  Returns -
 *	<0	error
 *	 0	OK
 */
int
mk_arbn( filep, name, neqn, eqn )
FILE	*filep;
char	*name;
int	neqn;
plane_t	*eqn;
{
	struct rt_arbn_internal	arbn;

	if( neqn <= 0 )  return(-1);

	arbn.magic = RT_ARBN_INTERNAL_MAGIC;
	arbn.neqn = neqn;
	arbn.eqn = eqn;

	return mk_export_fwrite( filep, name, (genptr_t)&arbn, ID_ARBN );
}
