/*
 *			D I R . C
 *
 * Ray Tracing program, GED database directory manager.
 *
 *  Functions -
 *	rt_dirbuild	Read GED database, build directory
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
 */
#ifndef lint
static char RCSdir[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ D I R B U I L D
 *
 *  Builds a directory of the object names.
 *
 *  Allocate and initialize information for this
 *  instance of an RT model database.
 *
 * Returns -
 *	(struct rt_i *)	Success
 *	RTI_NULL	Fatal Error
 */
struct rt_i *
rt_dirbuild(filename, buf, len)
char	*filename;
char	*buf;
int	len;
{
	register struct rt_i	*rtip;
	register struct db_i	*dbip;		/* Database instance ptr */

	if( RT_LIST_FIRST( rt_list, &rt_g.rtg_vlfree ) == 0 )  {
		RT_LIST_INIT( &rt_g.rtg_vlfree );
	}

	if( (dbip = db_open( filename, "r" )) == DBI_NULL )
	    	return( RTI_NULL );		/* FAIL */

	if( db_scan( dbip, (int (*)())db_diradd, 1 ) < 0 )
	    	return( RTI_NULL );		/* FAIL */

	GETSTRUCT( rtip, rt_i );
	rtip->rti_magic = RTI_MAGIC;
	RT_LIST_INIT( &(rtip->rti_headsolid) );
	rtip->rti_dbip = dbip;
	rtip->needprep = 1;

	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );
	VSETALL( rtip->rti_inf_box.bn.bn_min, -0.1 );
	VSETALL( rtip->rti_inf_box.bn.bn_max,  0.1 );
	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;

	if( buf != (char *)NULL )
		strncpy( buf, dbip->dbi_title, len );

	return( rtip );				/* OK */
}
