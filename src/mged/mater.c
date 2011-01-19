/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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
 * Code to deal with establishing and maintaining the tables which map
 * region ID codes into worthwhile material information (colors and
 * outboard database "handles").
 *
 * It is expected that entries on this mater list will be sorted in
 * strictly ascending order, with no overlaps (ie, monotonicly
 * increasing).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "dg.h"

#include "./mged.h"
#include "./mged_dm.h"


/*
 * C O L O R _ P U T R E C
 *
 * Used to create a database record and get it written out to a
 * granule.  In some cases, storage will need to be allocated. v4 db
 * only.
 */
void
color_putrec(struct mater *mp)
{
    struct directory dir;
    union record rec;

    if (dbip == DBI_NULL)
	return;

    if (dbip->dbi_read_only)
	return;

    if (db_version(dbip) > 4) {
	bu_log("color_putrec does not work on db5 or later databases");
	return;
    }

    rec.md.md_id = ID_MATERIAL;
    rec.md.md_low = mp->mt_low;
    rec.md.md_hi = mp->mt_high;
    rec.md.md_r = mp->mt_r;
    rec.md.md_g = mp->mt_g;
    rec.md.md_b = mp->mt_b;

    /* Fake up a directory entry for db_* routines */
    dir.d_namep = "color_putrec";
    dir.d_magic = RT_DIR_MAGIC;
    dir.d_flags = 0;
    if (mp->mt_daddr == MATER_NO_ADDR) {
	/* Need to allocate new database space */
	if (db_alloc(dbip, &dir, 1) < 0) ALLOC_ERR_return;
	mp->mt_daddr = dir.d_addr;
    } else {
	dir.d_addr = mp->mt_daddr;
	dir.d_len = 1;
    }
    if (db_put(dbip, &dir, &rec, 0, 1) < 0) WRITE_ERR_return;
}


/*
 * C O L O R _ Z A P R E C
 *
 * Used to release database resources occupied by a material record.
 */
void
color_zaprec(struct mater *mp)
{
    struct directory dir;

    if (dbip == DBI_NULL)
	return;

    if (dbip->dbi_read_only || mp->mt_daddr == MATER_NO_ADDR)
	return;
    dir.d_magic = RT_DIR_MAGIC;
    dir.d_namep = "color_zaprec";
    dir.d_len = 1;
    dir.d_addr = mp->mt_daddr;
    dir.d_flags = 0;
    if (db_delete(dbip, &dir) != 0)
	DELETE_ERR_return("color_zaprec");
    mp->mt_daddr = MATER_NO_ADDR;
}


/*
 * C O L O R _ S O L T A B
 *
 * Pass through the solid table and set pointer to appropriate
 * mater structure.
 */
void
color_soltab(void)
{
    ged_color_soltab(&gedp->ged_gdp->gd_headDisplay);
    update_views = 1;		/* re-write control list with new colors */
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
