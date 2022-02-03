/*                        O B J E C T S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @addtogroup ged_objects
 *
 * Geometry EDiting Library Database Generic Object Functions.
 *
 * These are functions that operate on database objects.
 */
/** @{ */
/** @file ged/objects.h */

#ifndef GED_OBJECTS_H
#define GED_OBJECTS_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/** Check if the object is a combination */
#define	GED_CHECK_COMB(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_COMB) == 0) { \
	int ged_check_comb_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_check_comb_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a combination", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_CHECK_EXISTS(_gedp, _name, _noisy, _flags) \
    if (db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy)) != RT_DIR_NULL) { \
	int ged_check_exists_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_check_exists_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s already exists.", (_name)); \
	} \
	return (_flags); \
    }

/** Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_gedp, _flags) \
    if ((_gedp)->ged_wdbp->dbip->dbi_read_only) { \
	int ged_check_read_only_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_check_read_only_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "Sorry, this database is READ-ONLY."); \
	} \
	return (_flags); \
    }

/** Check if the object is a region */
#define	GED_CHECK_REGION(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_REGION) == 0) { \
	int ged_check_region_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_check_region_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a region.", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** add a new directory entry to the currently open database */
#define GED_DB_DIRADD(_gedp, _dp, _name, _laddr, _len, _dirflags, _ptr, _flags) \
    if (((_dp) = db_diradd((_gedp)->ged_wdbp->dbip, (_name), (_laddr), (_len), (_dirflags), (_ptr))) == RT_DIR_NULL) { \
	int ged_db_diradd_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_db_diradd_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to add %s to the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_DB_LOOKUP(_gedp, _dp, _name, _noisy, _flags) \
    if (((_dp) = db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy))) == RT_DIR_NULL) { \
	int ged_db_lookup_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_db_lookup_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to find %s in the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Get internal representation */
#define GED_DB_GET_INTERNAL(_gedp, _intern, _dp, _mat, _resource, _flags) \
    if (rt_db_get_internal((_intern), (_dp), (_gedp)->ged_wdbp->dbip, (_mat), (_resource)) < 0) { \
	int ged_db_get_internal_quiet = (_flags) & BRLCAD_QUIET; \
	if (!ged_db_get_internal_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read failure."); \
	} \
	return (_flags); \
    }

/** Put internal representation */
#define GED_DB_PUT_INTERNAL(_gedp, _dp, _intern, _resource, _flags) \
    if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, (_intern), (_resource)) < 0) { \
       int ged_db_put_internal_quiet = (_flags) & BRLCAD_QUIET; \
       if (!ged_db_put_internal_quiet) { \
           bu_vls_printf((_gedp)->ged_result_str, "Database write failure."); \
       } \
       return (_flags); \
    }

__END_DECLS

#endif /* GED_OBJECTS_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
