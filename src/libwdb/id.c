/*
 *			I D . C
 *
 *  An ID record must be the first granule in the database.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ I D
 *
 *  Make a database header (ID) record.
 */
int
mk_id(struct rt_wdb *fp, const char *title)
{
	return mk_id_editunits( fp, title, 1.0 );
}

/*
 *			M K _ I D _ U N I T S
 *
 *  Make a database header (ID) record, and note the
 *  user's preferred editing units (specified as a string).
 *
 *  Returns -
 *	<0	error
 *	0	success
 */
int
mk_id_units(struct rt_wdb *fp, const char *title, register const char *units)
{
	return mk_id_editunits( fp, title, bu_units_conversion(units) );
}

/*
 *			M K _ I D _ E D I T U N I T S
 *
 *  Make a database header (ID) record, and note the
 *  user's preferred editing units (specified as a conversion factor).
 *
 *  Note that the v4 database format offers only a limited number
 *  of choices for the preferred editing units.
 *  If the user is editing in unusual units (like 2.5feet), don't
 *  fail to create the database header.
 *
 *  In the v5 database, the conversion factor will be stored intact.
 *
 *  Note that the database-layer header record
 *  will have already been written by db_create().
 *  All we have to do here is update it.
 *
 *  Returns -
 *	<0	error
 *	0	success
 */
int
mk_id_editunits(
	struct rt_wdb *wdbp,
	const char *title,
	double local2mm )
{
	RT_CK_WDB(wdbp);
	return db_update_ident( wdbp->dbip, title, local2mm );
}
