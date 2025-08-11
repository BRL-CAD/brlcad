/*                        D A T A B A S E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @addtogroup ged_database
 *
 * Geometry EDiting Library Database Level Functions.
 *
 */
/** @{ */
/** @file ged/database.h */

#ifndef GED_DATABASE_H
#define GED_DATABASE_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/** Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_gedp, _flags) \
    if ((_gedp) == GED_NULL || (_gedp)->dbip == DBI_NULL) { \
	int ged_check_database_open_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_database_open_quiet) { \
	    if ((_gedp) != GED_NULL) { \
		bu_vls_trunc((_gedp)->ged_result_str, 0); \
		bu_vls_printf((_gedp)->ged_result_str, "A database is not open!"); \
	    } else {\
		bu_log("A database is not open!\n"); \
	    } \
	} \
	return (_flags); \
    }

/**
 * Checks that each directory in the supplied path actually has the subdirectories
 * that are implied by the path.
 */
GED_EXPORT extern int ged_path_validate(struct ged *gedp, const struct db_full_path * const path);

/**
 * Return a count of objects displayed
 */
GED_EXPORT extern size_t ged_who_argc(struct ged *gedp);

/**
 * Return a list of names of objects displayed
 */
GED_EXPORT extern int ged_who_argv(struct ged *gedp, char **start, const char **end);


/* defined in copy.c */
GED_EXPORT extern int ged_dbcopy(struct ged *from_gedp,
                                struct ged *to_gedp,
                                const char *from,
                                const char *to,
				int fflag);

__END_DECLS

#endif /* GED_DATABASE_H */

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
