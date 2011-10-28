/*                     D B _ F L A G S . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2011 United States Government as represented by
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
/** @addtogroup db4 */
/** @{ */
/** @file librt/db_flags.c
 *
 * Given an internal representation of a geometry object, there are
 * particular directory flags associated with it (at least for
 * geometric objects) that we may need to obtain.  The directory flags
 * are mostly based on the major and minor type of the object so these
 * routines consolidate that logic.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 * Given the internal form of a database object, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
int
db_flags_internal(const struct rt_db_internal *intern)
{
    const struct rt_comb_internal *comb;

    RT_CK_DB_INTERNAL(intern);

    if (intern->idb_type != ID_COMBINATION)
	return RT_DIR_SOLID;

    comb = (struct rt_comb_internal *)intern->idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag)
	return RT_DIR_COMB | RT_DIR_REGION;
    return RT_DIR_COMB;
}


/* XXX - should use in db5_diradd() */
/**
 * Given a database object in "raw" internal form, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
int
db_flags_raw_internal(const struct db5_raw_internal *raw)
{
    struct bu_attribute_value_set avs;

    if (raw->major_type != DB5_MAJORTYPE_BRLCAD) {
	return RT_DIR_NON_GEOM;
    }
    if (raw->minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	if (raw->attributes.ext_buf) {
	    bu_avs_init_empty(&avs);
	    if (db5_import_attributes(&avs, &raw->attributes) < 0) {
		/* could not load attributes, so presume not a region */
		return RT_DIR_COMB;
	    }
	    if (avs.count == 0) {
		return RT_DIR_COMB;
	    }
	    if (bu_avs_get(&avs, "region") != NULL) {
		return RT_DIR_COMB|RT_DIR_REGION;
	    }
	}
	return RT_DIR_COMB;
    }

    /* anything else is a solid? */
    return RT_DIR_SOLID;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
