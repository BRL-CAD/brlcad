/*                       D B _ A T T R . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file db_attr.h
 *
 */

#ifndef RT_DB_ATTR_H
#define RT_DB_ATTR_H

#include "common.h"
#include "bu/avs.h"
#include "rt/defines.h"

__BEGIN_DECLS


struct rt_comb_internal;      /* forward declaration */


/* db5_attr.c */
/**
 * Define standard attribute types in BRL-CAD geometry.  (See the
 * attributes manual page.)  These should be a collective enumeration
 * starting from 0 and increasing without any gaps in the numbers so
 * db5_standard_attribute() can be used as an index-based iterator.
 */
enum {
    ATTR_REGION = 0,
    ATTR_REGION_ID,
    ATTR_MATERIAL_ID,
    ATTR_MATERIAL_NAME,
    ATTR_AIR,
    ATTR_LOS,
    ATTR_COLOR,
    ATTR_SHADER,
    ATTR_INHERIT,
    ATTR_TIMESTAMP, /* a binary attribute */
    ATTR_NULL
};

/* Enum to characterize status of attributes */
enum {
    ATTR_STANDARD = 0,
    ATTR_USER_DEFINED,
    ATTR_UNKNOWN_ORIGIN
};

struct db5_attr_ctype {
    int attr_type;    /* ID from the main attr enum list */
    int is_binary;   /* false for ASCII attributes; true for binary attributes */
    int attr_subtype; /* ID from attribute status enum list */

    /* names should be specified with alphanumeric characters
     * (lower-case letters, no white space) and will act as unique
     * keys to an object's attribute list */
    const char *name;         /* the "standard" name */
    const char *description;
    const char *examples;
    /* list of alternative names for this attribute: */
    const char *aliases;
    /* property name */
    const char *property;
    /* a longer description for lists outside a table */
    const char *long_description;
};

RT_EXPORT extern const struct db5_attr_ctype db5_attr_std[];

/* Container for holding user-registered attributes */
struct db5_registry{
    void *internal; /**< @brief implementation-specific container for holding information*/
};

/**
 * Initialize a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_init(struct db5_registry *registry);

/**
 * Free a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_free(struct db5_registry *registry);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_create(struct db5_registry *registry,
				     int attr_type,
				     int is_binary,
				     int attr_subtype,
				     const char *name,
				     const char *description,
				     const char *examples,
				     const char *aliases,
				     const char *property,
				     const char *long_description);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_register(struct db5_registry *registry,
				       struct db5_attr_ctype *attribute);

/**
 * De-register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_deregister(struct db5_registry *registry,
					 const char *name);

/**
 * Look to see if a specific attribute is registered
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype *db5_attr_get(struct db5_registry *registry,
						     const char *name);

/**
 * Get an array of pointers to all registered attributes
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype **db5_attr_dump(struct db5_registry *registry);


/**
 * Function returns the string name for a given standard attribute
 * index.  Index values returned from db5_standardize_attribute()
 * correspond to the names returned from this function, returning the
 * "standard" name.  Callers may also iterate over all names starting
 * with an index of zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute(int idx);


/**
 * Function returns the string definition for a given standard
 * attribute index.  Index values returned from
 * db5_standardize_attribute_def() correspond to the definition
 * returned from this function, returning the "standard" definition.
 * Callers may also iterate over all names starting with an index of
 * zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute_def(int idx);

/**
 * Function for recognizing various versions of the DB5 standard
 * attribute names that have been used - returns the attribute type of
 * the supplied attribute name, or -1 if it is not a recognized
 * variation of the standard attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_is_standard_attribute(const char *attrname);

/**
 * Ensures that an attribute set containing standard attributes with
 * non-standard/old/deprecated names gets the standard name added.  It
 * will update the first non-standard name encountered, but will leave
 * any subsequent matching attributes found unmodified if they have
 * different values.  Such "conflict" attributes must be resolved
 * manually.
 *
 * Returns the number of conflicting attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern size_t db5_standardize_avs(struct bu_attribute_value_set *avs);

/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_standardize_attribute(const char *attr);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_attr_to_comb(struct rt_comb_internal *comb, const struct bu_attribute_value_set *avs, struct directory *dp);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_comb_to_attr(struct bu_attribute_value_set *avs, const struct rt_comb_internal *comb);

/* Convenience macros */
#define ATTR_STD(attr) db5_standard_attribute(db5_standardize_attribute(attr))


/**
 * Convert the on-disk encoding into a handy easy-to-use
 * bu_attribute_value_set structure.
 *
 * Take advantage of the readonly_min/readonly_max capability so that
 * we don't have to bu_strdup() each string, but can simply point to
 * it in the provided buffer *ap.  Important implication: don't free
 * *ap until you're done with this avs.
 *
 * The upshot of this is that bu_avs_add() and bu_avs_remove() can be
 * safely used with this *avs.
 *
 * Returns -
 * >0 count of attributes successfully imported
 * -1 Error, mal-formed input
 */
RT_EXPORT extern int db5_import_attributes(struct bu_attribute_value_set *avs,
					   const struct bu_external *ap);

/**
 * Encode the attribute-value pair information into the external
 * on-disk format.
 *
 * The on-disk encoding is:
 *
 *   name1 NULL value1 NULL ... nameN NULL valueN NULL NULL
 *
 * For binary attributes the on-disk encoding is:
 *
 *   bname1 NULL uchar valuelen1 comment1 NULL bvalue1 NULL ...
 *     bnameN NULL uchar valuelenN commentN NULL bvalueN NULL NULL
 *
 * 'ext' is initialized on behalf of the caller.
 */
RT_EXPORT extern void db5_export_attributes(struct bu_external *ap,
					    const struct bu_attribute_value_set *avs);



/* lookup directory entries based on attributes */

/**
 * lookup directory entries based on directory flags (dp->d_flags) and
 * attributes the "dir_flags" arg is a mask for the directory flags
 * the *"avs" is an attribute value set used to select from the
 * objects that *pass the flags mask. if "op" is 1, then the object
 * must have all the *attributes and values that appear in "avs" in
 * order to be *selected. If "op" is 2, then the object must have at
 * least one of *the attribute/value pairs from "avs".
 *
 * dir_flags are in the form used in struct directory (d_flags)
 *
 * for op:
 * 1 -> all attribute name/value pairs must be present and match
 * 2 -> at least one of the name/value pairs must be present and match
 *
 * returns a ptbl list of selected directory pointers.  An empty list
 * means nothing met the requirements and a NULL return means something
 * went wrong.
 */
RT_EXPORT extern struct bu_ptbl *db_lookup_by_attr(struct db_i *dbip,
						   int dir_flags,
						   struct bu_attribute_value_set *avs,
						   int op);
/**
 * Put the old region-id-color-table into the global object.  A null
 * attribute is set if the material table is empty.
 *
 * Returns -
 * <0 error
 * 0 OK
 */
RT_EXPORT extern int db5_put_color_table(struct db_i *dbip);
RT_EXPORT extern int db5_update_ident(struct db_i *dbip,
				      const char *title,
				      double local2mm);


/**
 * Update an arbitrary number of attributes on a given database
 * object.  For efficiency, this is done without looking at the object
 * body at all.
 *
 * Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attributes(struct directory *dp,
					   struct bu_attribute_value_set *avsp,
					   struct db_i *dbip);

/**
 * A convenience routine to update the value of a single attribute.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attribute(const char *obj_name,
					  const char *aname,
					  const char *value,
					  struct db_i *dbip);

/**
 * Replace the attributes of a given database object.
 *
 * For efficiency, this is done without looking at the object body at
 * all.  Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_replace_attributes(struct directory *dp,
					    struct bu_attribute_value_set *avsp,
					    struct db_i *dbip);

/**
 *
 * Get attributes for an object pointed to by *dp
 *
 * returns:
 * 0 - all is well
 * <0 - error
 */
RT_EXPORT extern int db5_get_attributes(const struct db_i *dbip,
					struct bu_attribute_value_set *avs,
					const struct directory *dp);



__END_DECLS

#endif /*RT_DB_ATTR_H*/

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
