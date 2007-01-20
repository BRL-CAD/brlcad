/*                        S T R S O L . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2007 United States Government as represented by
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
/** @file strsol.c
 *
 *  Library for writing strsol solids to MGED databases.
 *  Assumes that some of the structure of such databases are known
 *  by the calling routines.
 *
 *
 *  Note that routines which are passed point_t or vect_t or mat_t
 *  parameters (which are call-by-address) must be VERY careful to
 *  leave those parameters unmodified (eg, by scaling), so that the
 *  calling routine is not surprised.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	BVLD/VMB Advanced Computer Systems Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

#if 0
/*
 *			M K _ S T R S O L
 *
 *  This routine is not intended for general use.
 *  It exists primarily to support the converter ASC2G and to
 *  permit the rapid development of new string solids.
 */
int
mk_strsol( fp, name, string_solid, string_arg )
FILE		*fp;
const char	*name;
const char	*string_solid;
const char	*string_arg;
{
	union record	rec[DB_SS_NGRAN];

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)rec, sizeof(rec) );
	rec[0].ss.ss_id = DBID_STRSOL;
	NAMEMOVE( name, rec[0].ss.ss_name );
	strncpy( rec[0].ss.ss_keyword, string_solid, sizeof(rec[0].ss.ss_keyword)-1 );
	strncpy( rec[0].ss.ss_args, string_arg, DB_SS_LEN-1 );

	if( fwrite( (char *)rec, sizeof(rec), 1, fp ) != 1 )
		return -1;
	return 0;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
