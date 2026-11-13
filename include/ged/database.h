/*                        D A T A B A S E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#include <stdint.h>
#include "ged/defines.h"

struct ged_db_change_set;

/** Object-name classifications recorded in a database change set. */
enum ged_db_change_object_kind {
    GED_DB_CHANGE_OBJECT_ADDED = 0,
    GED_DB_CHANGE_OBJECT_MODIFIED,
    GED_DB_CHANGE_OBJECT_REMOVED
};

/** Aggregate properties of the recorded database changes. */
enum ged_db_change_flag {
    GED_DB_CHANGE_STRUCTURAL = 0x01,
    GED_DB_CHANGE_NREF_DIRTY = 0x02,
    GED_DB_CHANGE_REBUILD = 0x04
};

/** State categories refreshed by ged_db_change_reconcile(). */
enum ged_db_change_effect {
    GED_DB_CHANGE_EFFECT_NONE = 0x00,
    GED_DB_CHANGE_EFFECT_DATABASE = 0x01,
    GED_DB_CHANGE_EFFECT_VIEW = 0x02
};

__BEGIN_DECLS

/** Return non-zero when change tracking is available for @p gedp. */
GED_EXPORT extern int ged_db_change_available(struct ged *gedp);

/**
 * Copy retained database changes newer than @p after_revision.
 *
 * The returned set owns all strings and remains valid independently of later
 * reconciliation or acknowledgement.  An available tracker returns an empty
 * set when there are no matching changes; NULL indicates that tracking is not
 * available.  Destroy the result with ged_db_change_set_destroy().
 */
GED_EXPORT extern struct ged_db_change_set *
ged_db_change_set_create(struct ged *gedp, uint64_t after_revision);

/** Destroy a change set returned by ged_db_change_set_create(). */
GED_EXPORT extern void
ged_db_change_set_destroy(struct ged_db_change_set *changes);

/** Return the newest revision represented by @p changes, or zero if empty. */
GED_EXPORT extern uint64_t
ged_db_change_set_revision(const struct ged_db_change_set *changes);

/** Return the ORed ged_db_change_flag values represented by @p changes. */
GED_EXPORT extern unsigned int
ged_db_change_set_flags(const struct ged_db_change_set *changes);

/** Return the number of names in one object-change classification. */
GED_EXPORT extern size_t
ged_db_change_set_object_count(const struct ged_db_change_set *changes,
			       enum ged_db_change_object_kind kind);

/** Return a set-owned object name, or NULL for an invalid kind or index. */
GED_EXPORT extern const char *
ged_db_change_set_object_at(const struct ged_db_change_set *changes,
			    enum ged_db_change_object_kind kind,
			    size_t index);

/** Return the number of old-name/new-name rename pairs. */
GED_EXPORT extern size_t
ged_db_change_set_rename_count(const struct ged_db_change_set *changes);

/** Return a set-owned old name, or NULL for an invalid index. */
GED_EXPORT extern const char *
ged_db_change_set_rename_old_at(const struct ged_db_change_set *changes,
				size_t index);

/** Return a set-owned new name, or NULL for an invalid index. */
GED_EXPORT extern const char *
ged_db_change_set_rename_new_at(const struct ged_db_change_set *changes,
				size_t index);

/** Return the number of changed hierarchy paths. */
GED_EXPORT extern size_t
ged_db_change_set_path_count(const struct ged_db_change_set *changes);

/** Return a set-owned hierarchy path, or NULL for an invalid index. */
GED_EXPORT extern const char *
ged_db_change_set_path_at(const struct ged_db_change_set *changes,
			  size_t index);

/**
 * Reconcile every recorded revision not yet applied to GED state.
 *
 * Returns ORed ged_db_change_effect values, or zero when no work was needed
 * or change tracking is unavailable.  Reconciliation does not discard the
 * retained snapshots.
 */
GED_EXPORT extern unsigned int ged_db_change_reconcile(struct ged *gedp);

/**
 * Discard reconciled snapshots through @p revision.
 *
 * Unreconciled revisions are retained.  Returns non-zero when change tracking
 * is available, including when no snapshots qualify for removal.
 */
GED_EXPORT extern int
ged_db_change_acknowledge(struct ged *gedp, uint64_t revision);

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
