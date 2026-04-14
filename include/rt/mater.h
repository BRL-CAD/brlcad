/*                          M A T E R . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @file mater.h
 *
 * @brief Region-ID-based material/color table management for BRL-CAD databases.
 *
 * BRL-CAD supports a region-ID color table: a sorted, non-overlapping list of
 * @c struct @c mater entries that map integer region-ID ranges to RGB colors.
 * This table is stored per-database (in @c db_i) and is used by the ray tracer
 * and display logic to assign colors to regions based on their region ID when no
 * explicit color attribute is present on the object itself.
 *
 * The table is populated when the database is opened (from @c _GLOBAL attributes
 * for v5 databases, or from @c ID_COLORTAB records for v4 databases) and is
 * freed when the database is closed.
 *
 * Typical usage pattern:
 *  - At database load time, @c db_mater_add() is called for each stored color
 *    record, which calls @c db_mater_insert() to build the sorted in-memory list.
 *  - At ray-trace / display time, @c db_mater_color_region() walks the table to
 *    apply an override color to a region whose ID falls within a mapped range.
 *  - To inspect or serialize the current table, use @c db_mater_head() to obtain
 *    the list head and iterate via @c mt_forw, or call @c db_mater_to_vls() for
 *    a string representation suitable for storing in a @c _GLOBAL attribute.
 *  - @c db_mater_free() releases all in-memory table entries (called by
 *    @c db_close()).
 */

#ifndef RT_MATER_H
#define RT_MATER_H

#include "common.h"
#include "rt/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

struct region; /* forward declaration */
struct db_i;   /* forward declaration */

/**
 * Per-region material and shading information carried on a @c struct @c region.
 *
 * This is separate from the region-ID color table (see @c struct @c mater
 * below); it stores per-region shader parameters and color as parsed from
 * object attributes.
 */
struct mater_info {
    float       ma_color[3];    /**< @brief explicit color:  0..1  */
    float       ma_temperature; /**< @brief positive ==> degrees Kelvin */
    char        ma_color_valid; /**< @brief non-0 ==> ma_color is non-default */
    char        ma_cinherit;    /**< @brief color: DB_INH_LOWER / DB_INH_HIGHER */
    char        ma_minherit;    /**< @brief mater: DB_INH_LOWER / DB_INH_HIGHER */
    char        *ma_shader;     /**< @brief shader name & parms */
};
#define RT_MATER_INFO_INIT_ZERO { VINIT_ZERO, 0.0, 0, 0, 0, NULL }
/* From MGED initial tree state */
#define RT_MATER_INFO_INIT_IDN { {1.0, 0.0, 0.0} , -1.0, 0, 0, 0, NULL }

/**
 * One entry in the per-database region-ID-to-color mapping table.
 *
 * Entries are kept in a singly-linked list sorted by ascending @c mt_low value
 * with no overlapping ranges.  Use @c db_mater_insert() to add entries in a
 * way that preserves this invariant.
 */
struct mater {
    long		mt_low;		/**< @brief lower bound of region ID range (inclusive) */
    long		mt_high;	/**< @brief upper bound of region ID range (inclusive) */
    unsigned char	mt_r;		/**< @brief red component (0-255) */
    unsigned char	mt_g;		/**< @brief green component (0-255) */
    unsigned char	mt_b;		/**< @brief blue component (0-255) */
    b_off_t		mt_daddr;	/**< @brief database address for v4 record updating */
    struct mater	*mt_forw;	/**< @brief next entry in the sorted list */
};
#define MATER_NULL	((struct mater *)0)
#define MATER_NO_ADDR	((b_off_t)0)		/**< @brief sentinel: entry has no backing database record */


/**
 * Apply the region-ID color table to a region.
 *
 * If @p regp->reg_regionid falls within a mapped range in the table associated
 * with @p dbip, the region's @c reg_mater.ma_color and @c ma_color_valid fields
 * are updated accordingly.  Has no effect if the region ID does not match any
 * entry.
 *
 * @param dbip   database whose material table is consulted
 * @param regp   region to color
 */
RT_EXPORT extern void db_mater_color_region(struct db_i *dbip, struct region *regp);

/**
 * Construct a @c struct @c mater entry from raw record data and insert it into
 * the database's material table.
 *
 * This is the primary loader function called from database scan routines (e.g.,
 * @c db_scan() for v4, @c db5_import_color_table() for v5) to populate the
 * in-memory table from stored records.
 *
 * @param dbip   database whose material table receives the new entry
 * @param low    lower bound of the region ID range
 * @param hi     upper bound of the region ID range
 * @param r      red component (0-255)
 * @param g      green component (0-255)
 * @param b      blue component (0-255)
 * @param addr   backing database address (@c MATER_NO_ADDR if none)
 */
RT_EXPORT extern void db_mater_add(struct db_i *dbip,
				   int low,
				   int hi,
				   int r,
				   int g,
				   int b,
				   b_off_t addr);

/**
 * Insert a @c struct @c mater entry into a database's material table, maintaining
 * sorted order and resolving any overlaps with existing entries.
 *
 * Ownership of @p newp is transferred to the table; do not free it after
 * calling this function.
 *
 * @param dbip   database whose material table is modified
 * @param newp   entry to insert (must be heap-allocated)
 */
RT_EXPORT extern void db_mater_insert(struct db_i *dbip, struct mater *newp);

/**
 * Serialize the database's material table to a @c bu_vls string.
 *
 * Each entry is written as @c "{low high r g b} " and appended to @p str.
 * The resulting string is suitable for storage in the @c regionid_colortable
 * attribute of a v5 @c _GLOBAL object.
 *
 * @param str    output string (appended to; caller must initialize)
 * @param dbip   database whose material table is serialized
 */
RT_EXPORT extern void db_mater_to_vls(struct bu_vls *str, struct db_i *dbip);

/**
 * Return the head of the material table list for a database.
 *
 * The returned pointer is the first entry of the sorted list; iterate using
 * @c mt_forw to traverse all entries.  Returns @c MATER_NULL if the table is
 * empty.
 *
 * @param dbip   database to query
 * @return pointer to first @c struct @c mater entry, or @c MATER_NULL
 */
RT_EXPORT extern struct mater *db_mater_head(struct db_i *dbip);

/**
 * Replace the material table list head for a database.
 *
 * This is a low-level setter.  It does @e not free the old list; call
 * @c db_mater_free() first if the old entries should be released.  Typically
 * used when swapping in a pre-built list (e.g., after @c db_mater_dup()).
 *
 * @param dbip    database to update
 * @param newmat  new list head (may be @c MATER_NULL to clear the table)
 */
RT_EXPORT extern void db_mater_set_head(struct db_i *dbip, struct mater *newmat);

/**
 * Return a deep copy of the material table for a database.
 *
 * All entries are duplicated; the caller owns the returned list and is
 * responsible for freeing it (e.g., by iterating and calling @c bu_free(), or
 * by inserting into another database's table via @c db_mater_set_head() and
 * letting @c db_mater_free() handle cleanup at close time).
 *
 * @param dbip   database whose material table is duplicated
 * @return head of the new list, or @c MATER_NULL if the table is empty
 */
RT_EXPORT extern struct mater *db_mater_dup(struct db_i *dbip);

/**
 * Free all entries in the material table of a database.
 *
 * After this call the table is empty (@c db_mater_head() returns
 * @c MATER_NULL).  Called automatically by @c db_close().
 *
 * @param dbip   database whose material table is freed
 */
RT_EXPORT extern void db_mater_free(struct db_i *dbip);

/**
 * @name Deprecated global material API (use db_mater_* instead)
 *
 * The following functions operated on a single process-wide (global) material
 * table and are retained for backward compatibility only.  New code must use
 * the per-database @c db_mater_*() functions above.
 *
 * @deprecated since 7.42 - use db_mater_head(), db_mater_set_head(),
 *             db_mater_dup(), db_mater_free(), db_mater_insert(),
 *             db_mater_add(), db_mater_to_vls(), and db_mater_color_region()
 *             instead.
 * @{
 */

/** @deprecated use db_mater_head() */
DEPRECATED RT_EXPORT extern struct mater *rt_material_head(void);

/** @deprecated use db_mater_set_head() */
DEPRECATED RT_EXPORT extern void rt_new_material_head(struct mater *newmat);

/** @deprecated use db_mater_dup() */
DEPRECATED RT_EXPORT extern struct mater *rt_dup_material_head(void);

/** @deprecated use db_mater_free() */
DEPRECATED RT_EXPORT extern void rt_color_free(void);

/** @deprecated use db_mater_insert() */
DEPRECATED RT_EXPORT extern void rt_insert_color(struct mater *newp);

/** @deprecated use db_mater_add() */
DEPRECATED RT_EXPORT extern void rt_color_addrec(int low, int hi, int r, int g, int b, b_off_t addr);

/** @deprecated use db_mater_to_vls() */
DEPRECATED RT_EXPORT extern void rt_vls_color_map(struct bu_vls *str);

/** @deprecated use db_mater_color_region() */
DEPRECATED RT_EXPORT extern void rt_region_color_map(struct region *regp);

/** @} */

__END_DECLS

#endif /* RT_MATER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
