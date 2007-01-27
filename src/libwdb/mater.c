/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mater.c
 *
 *  Interface for writing region-id-based color tables to the database.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "db.h"
#include "raytrace.h"
#include "wdb.h"
#include "mater.h"

/*
 *  Given that the color table has been built up by successive calls
 *  to rt_color_addrec(),  write it into the database.
 */
int
mk_write_color_table( struct rt_wdb *ofp )
{
	RT_CK_WDB(ofp);
	if( ofp->dbip->dbi_version <= 4 )  {
		register struct mater *mp;

		BU_ASSERT_LONG( mk_version, ==, 4 );

		for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
#if 0
			union record	record;
			record.md.md_id = ID_MATERIAL;
			record.md.md_flags = 0;
			record.md.md_low = mp->mt_low;
			record.md.md_hi = mp->mt_high;
			record.md.md_r = mp->mt_r;
			record.md.md_g = mp->mt_g;
			record.md.md_b = mp->mt_b;

			/* This record has no name field! */

/* XXX examine mged/mater.c: color_putrec() */

			/* Write out the record */
			(void)fwrite( (char *)&record, sizeof record, 1, ofpxx );
#else
	bu_log("mk_write_color_table(): not implemented for v4 database yet\n");
#endif
		}
	} else {
		return db5_put_color_table( ofp->dbip );
	}
	return 0;
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
