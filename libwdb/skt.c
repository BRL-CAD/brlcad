/*
 *			S K T . C
 *
 * Support for sketches
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char skt_RCSid[] = "@(#)$Header$ (BRL)";
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

int
mk_sketch( fp, name, skt )
FILE *fp;
char *name;
struct rt_sketch_internal *skt;
{
	struct rt_db_internal intern;
	int ret;

	RT_SKETCH_CK_MAGIC( skt );

	ret = mk_export_fwrite( fp, name, (genptr_t)skt, ID_SKETCH );

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_ptr = (genptr_t)skt;
	intern.idb_type = ID_SKETCH;

	rt_sketch_ifree( &intern );

	return( ret );
}
