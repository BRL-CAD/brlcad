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
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

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
mk_id( fp, title )
FILE		*fp;
CONST char	*title;
{
	return db_fwrite_ident( fp, title, 1.0 );
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
mk_id_units( fp, title, units )
FILE		*fp;
CONST char	*title;
register CONST char	*units;
{
	return db_fwrite_ident( fp, title, bu_units_conversion(units) );
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
 *  In the v5 database, the conversion factor should be stored intact.
 *
 *  Returns -
 *	<0	error
 *	0	success
 */
int
mk_id_editunits( fp, title, local2mm )
FILE		*fp;
CONST char	*title;
double		local2mm;
{
	return db_fwrite_ident( fp, title, local2mm );
}
