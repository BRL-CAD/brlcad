/*                        O B J E C T S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
 * These are functions that either operate on groups or are not
 * specific to a single primitive type.
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
	int ged_check_comb_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_comb_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a combination", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_CHECK_EXISTS(_gedp, _name, _noisy, _flags) \
    if (db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy)) != RT_DIR_NULL) { \
	int ged_check_exists_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_exists_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s already exists.", (_name)); \
	} \
	return (_flags); \
    }

/** Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_gedp, _flags) \
    if ((_gedp)->ged_wdbp->dbip->dbi_read_only) { \
	int ged_check_read_only_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_read_only_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "Sorry, this database is READ-ONLY."); \
	} \
	return (_flags); \
    }

/** Check if the object is a region */
#define	GED_CHECK_REGION(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_REGION) == 0) { \
	int ged_check_region_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_region_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a region.", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** add a new directory entry to the currently open database */
#define GED_DB_DIRADD(_gedp, _dp, _name, _laddr, _len, _dirflags, _ptr, _flags) \
    if (((_dp) = db_diradd((_gedp)->ged_wdbp->dbip, (_name), (_laddr), (_len), (_dirflags), (_ptr))) == RT_DIR_NULL) { \
	int ged_db_diradd_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_diradd_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to add %s to the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_DB_LOOKUP(_gedp, _dp, _name, _noisy, _flags) \
    if (((_dp) = db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy))) == RT_DIR_NULL) { \
	int ged_db_lookup_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_lookup_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to find %s in the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Get internal representation */
#define GED_DB_GET_INTERNAL(_gedp, _intern, _dp, _mat, _resource, _flags) \
    if (rt_db_get_internal((_intern), (_dp), (_gedp)->ged_wdbp->dbip, (_mat), (_resource)) < 0) { \
	int ged_db_get_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_get_internal_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read failure."); \
	} \
	return (_flags); \
    }

/** Put internal representation */
#define GED_DB_PUT_INTERNAL(_gedp, _dp, _intern, _resource, _flags) \
    if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, (_intern), (_resource)) < 0) { \
	int ged_db_put_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_put_internal_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database write failure."); \
	} \
	return (_flags); \
    }

/**
 * Adjust object's attribute(s)
 */
GED_EXPORT extern int ged_adjust(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set, get, show, remove or append to attribute values for the specified object.
 * The arguments for "set" and "append" subcommands are attribute name/value pairs.
 * The arguments for "get", "rm", and "show" subcommands are attribute names.
 * The "set" subcommand sets the specified attributes for the object.
 * The "append" subcommand appends the provided value to an existing attribute,
 * or creates a new attribute if it does not already exist.
 * The "get" subcommand retrieves and displays the specified attributes.
 * The "rm" subcommand deletes the specified attributes.
 * The "show" subcommand does a "get" and displays the results in a user readable format.
 */
GED_EXPORT extern int ged_attr(struct ged *gedp, int argc, const char *argv[]);

/**
 * List attributes (brief).
 */
GED_EXPORT extern int ged_cat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set combination color.
 */
GED_EXPORT extern int ged_comb_color(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or extend combination w/booleans.
 */
GED_EXPORT extern int ged_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or extend a combination using standard notation.
 */
GED_EXPORT extern int ged_comb_std(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get comb's members.
 */
GED_EXPORT extern int ged_combmem(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy a database object
 */
GED_EXPORT extern int ged_copy(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy an 'evaluated' path solid
 */
GED_EXPORT extern int ged_copyeval(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy the matrix from one combination's arc to another.
 */
GED_EXPORT extern int ged_copymat(struct ged *gedp, int argc, const char *argv[]);


/**
 * Edit region ident codes.
 */
GED_EXPORT extern int ged_edcodes(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit combination.
 */
GED_EXPORT extern int ged_edcomb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit objects, by using subcommands.
 */
GED_EXPORT extern int ged_edit(struct ged *gedp, int argc, const char *argv[]);

/**
 * Checks to see if the specified database object exists.
 */
GED_EXPORT extern int ged_exists(struct ged *gedp, int argc, const char *argv[]);

/**
 * returns form for objects of type "type"
 */
GED_EXPORT extern int ged_form(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get object attributes
 */
GED_EXPORT extern int ged_get(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get combination information
 */
GED_EXPORT extern int ged_get_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a group
 */
GED_EXPORT extern int ged_group(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the "hidden" flag for the specified objects so they do not
 * appear in an "ls" command output
 */
GED_EXPORT extern int ged_hide(struct ged *gedp, int argc, const char *argv[]);

/**
 * Add instance of obj to comb
 */
GED_EXPORT extern int ged_instance(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save/keep the specified objects in the specified file
 */
GED_EXPORT extern int ged_keep(struct ged *gedp, int argc, const char *argv[]);

/**
 * List attributes of regions within a group/combination.
 */
GED_EXPORT extern int ged_lc(struct ged *gedp, int argc, const char *argv[]);

/**
 * List object information, verbose.
 */
GED_EXPORT extern int ged_list(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make a unique object name.
 */
GED_EXPORT extern int ged_make_name(struct ged *gedp, int argc, const char *argv[]);

/**
 * Modify material information.
 */
GED_EXPORT extern int ged_mater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Globs expression against database objects, does not return tokens that match nothing
 */
GED_EXPORT extern int ged_match(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set combination attributes
 */
GED_EXPORT extern int ged_put_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a database object
 */
GED_EXPORT extern int ged_put(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit region/comb
 */
GED_EXPORT extern int ged_red(struct ged *gedp, int argc, const char *argv[]);

/**
 * Remove members from a combination
 */
GED_EXPORT extern int ged_remove(struct ged *gedp, int argc, const char *argv[]);

/**
 * Unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output
 */
GED_EXPORT extern int ged_unhide(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write material properties to a file for specified combination(s).
 */
GED_EXPORT extern int ged_wmater(struct ged *gedp, int argc, const char *argv[]);





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
