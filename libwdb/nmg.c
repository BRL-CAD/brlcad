/*
 *			N M G . C
 *
 *  libwdb support for writing an NMG to disk.
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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ N M G
 *
 *  Caller is responsible for freeing the NMG, if desired.
 *
 *  Returns -
 *	<0	error
 *	 0	OK
 */
int
mk_nmg( filep, name, m )
FILE		*filep;
char		*name;
struct model	*m;
{
	NMG_CK_MODEL( m );

	return mk_export_fwrite( filep, name, (genptr_t)&m, ID_NMG );
}
