/*                            I D . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file id.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
