/*                          V I E W . H
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
/** @file rt/view.h
 *
 */

#ifndef RT_VIEW_H
#define RT_VIEW_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/hash.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "bv/defines.h"
#include "bv/lod.h"  // for libbv CACHE_* defines
#include "rt/defines.h"

__BEGIN_DECLS

// Key strings to use in combination with a name hash to look up data.
// Changes to any of these requires incrementing DB_CACHE_CURRENT_FORMAT.
#define CACHE_REGION_ID "rid"
#define CACHE_REGION_FLAG "rf"
#define CACHE_INHERIT_FLAG "if"
#define CACHE_COLOR "c"

// If BV_CACHE_CURRENT_FORMAT is incremented, this
// should be as well.
#define DB_CACHE_CURRENT_FORMAT 3

/**
 * Initialize drawing data such as color and region flag for all objects
 * in the cache.
 *
 * TODO - meshes are currently separate - make a queue for LoD work
 * on the big ones and just handle everything else here to simplify the API
 */
RT_EXPORT extern void db_cache_draw_init(struct db_i *dbip, int verbose);

/**
 * Update the cache contents to reflect the current state of the object
 * in dbip.  If there is no object present in dbip with that name, then
 * remove any cached data associated with that name from the cache.
 *
 * If no cache is present, or the attempt encounters a problem, return
 * BRLCAD_ERROR - else returns BRLCAD_OK;
 */
RT_EXPORT extern int db_cache_draw_update(struct db_i *dbip, const char *name);


/* Routines for managing the Drawing cache.  As many of the details as
 * possible of the cache should be private, but we do at least need ways to set
 * up, clear, update and load information. */

/**
 * Initialize the LoD cache, doing any necessary setup on disk.  If
 * being run for the first time this can be quite slow, so it is
 * recommended to run it in a background thread.
 */
RT_EXPORT extern void db_cache_mesh_init(struct db_i *dbip, int verbose);

/**
 * Update the cache contents to reflect the current state of the object
 * in dbip.  If there is no object present in dbip with that name, then
 * remove any cached data associated with that name from the cache.
 *
 * If no cache is present, or the attempt encounters a problem, return
 * BRLCAD_ERROR - else returns BRLCAD_OK;
 */
RT_EXPORT extern int db_cache_mesh_update(struct db_i *dbip, const char *name);

/**
 * Retrieve the bv_lod_mesh data from the dbip's cache associated with
 * the named object.  If no such data is present, returns NULL.
 */
RT_EXPORT extern struct bv_lod_mesh *db_cache_mesh_get(struct db_i *dbip, const char *name);



/**
 * NOTE: Normally, librt doesn't have a concept of a "display" of the geometry.
 * However for at least the plotting routines, view information is sometimes
 * needed to produce more intelligent output.  In those situations, the
 * application will generally pass in a bv structure.
 */

/**
 * Specifies a subset of a primitive's geometry as the target for an
 * operation.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection {
    void *obj; /**< @brief primitive-specific selection object */
};

/**
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_set {
    struct bu_ptbl selections; /**< @brief holds struct rt_selection */

    /** selection-object-specific routine that will free all memory
     *  associated with any of the stored selections
     */
    void (*free_selection)(struct rt_selection *);
};

/**
 * Stores selections associated with an object. There is an entry in
 * the selections table for each kind of selection (e.g. "active",
 * "option"). The table entries are sets to allow more than one
 * selection of the same type (e.g. multiple "option" selections).
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_object_selections {
    /** selection type -> struct rt_selection_set */
    struct bu_hash_tbl *sets;
};

/**
 * Analogous to a database query. Specifies how to filter and sort the
 * selectable components of a primitive in order to find the most
 * relevant selections for a particular application.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_query {
    point_t start;     /**< @brief start point of query ray */
    vect_t dir;        /**< @brief direction of query ray */

#define RT_SORT_UNSORTED         0
#define RT_SORT_CLOSEST_TO_START 1
    int sorting;
};

/**
 * Parameters of a translation applied to a selection.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_translation {
    fastf_t dx;
    fastf_t dy;
    fastf_t dz;
};

/**
 * Describes an operation that can be applied to a selection.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_operation {
#define RT_SELECTION_NOP         0
#define RT_SELECTION_TRANSLATION 1
    int type;
    union {
	struct rt_selection_translation tran;
    } parameters;
};

__END_DECLS

#endif /* RT_VIEW_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
