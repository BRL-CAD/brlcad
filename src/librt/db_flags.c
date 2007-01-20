/*                     D B _ F L A G S . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2007 United States Government as represented by
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

/** @addtogroup db4 */

/*@{*/
/** @file db_flags.c
 *
 *  Given an internal representation of a geometry object, there are
 *  particular directory flags associated with it (at least for
 *  geometric objects) that we may need to obtain.  The directory
 *  flags are mostly based on the major and minor type of the object
 *  so these routines consolidate that logic.
 *
 *  Functions
 *    db_flags_internal - given an rt_db_internal, return the flags
 *    db_flags_raw_internal - given a db5_raw_internal, return flags
 *
 *  Authors -
 *      Christopher Sean Morrison
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */

#include "common.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 *  D B _ F L A G S _ I N T E R N A L
 *
 * Given the internal form of a database object, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
int
db_flags_internal(const struct rt_db_internal *intern)
{
	const struct rt_comb_internal	*comb;

	RT_CK_DB_INTERNAL(intern);

	if( intern->idb_type != ID_COMBINATION )
		return DIR_SOLID;

	comb = (struct rt_comb_internal *)intern->idb_ptr;
	RT_CK_COMB(comb);

	if( comb->region_flag )
		return DIR_COMB | DIR_REGION;
	return DIR_COMB;
}


/* XXX - should use in db5_diradd() */
/**
 * d b _ f l a g s _ r a w _ i n t e r n a l
 *
 * Given a database object in "raw" internal form, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
int
db_flags_raw_internal(const struct db5_raw_internal *raw)
{
    struct bu_attribute_value_set avs;

    if (raw->major_type != DB5_MAJORTYPE_BRLCAD) {
	return DIR_NON_GEOM;
    }
    if (raw->minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	if (raw->attributes.ext_buf) {
	    bu_avs_init_empty(&avs);
	    if (db5_import_attributes(&avs, &raw->attributes) < 0) {
		/* could not load attributes, so presume not a region */
		return DIR_COMB;
	    }
	    if (avs.count == 0) {
		return DIR_COMB;
	    }
	    if (bu_avs_get( &avs, "region" ) != NULL) {
		return DIR_COMB|DIR_REGION;
	    }
	}
	return DIR_COMB;
    }

    /* anything else is a solid? */
    return DIR_SOLID;
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
