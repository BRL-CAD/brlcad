/*                   D B _ I N T E R N A L . H
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
/** @file db_internal.h
 *
 */

#ifndef RT_DB_INTERNAL_H
#define RT_DB_INTERNAL_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "bu/magic.h"
#include "bu/avs.h"
#include "bn/mat.h"
#include "rt/defines.h"
#include "rt/resource.h"

__BEGIN_DECLS

struct rt_functab; /* forward declaration */

/**
 * A handle on the internal format of a BRL-CAD database object.
 *
 * TODO - right now, rt_db_internal doesn't encode any notion of data version,
 * which appears to be a major reason some APIs need to pass a dbip through in
 * addition to the rt_db_internal.  Should we add an idb_version entry here to
 * indicate idb_ptr data versioning?
 */
struct rt_db_internal {
    uint32_t            idb_magic;
    int                 idb_major_type;
    int                 idb_minor_type; /**< @brief ID_xxx */
    const struct rt_functab *idb_meth;  /**< @brief for ft_ifree(), etc. */
    void *              idb_ptr;
    struct bu_attribute_value_set idb_avs;
};
#define idb_type                idb_minor_type
#define RT_DB_INTERNAL_INIT(_p) { \
	(_p)->idb_magic = RT_DB_INTERNAL_MAGIC; \
	(_p)->idb_major_type = -1; \
	(_p)->idb_minor_type = -1; \
	(_p)->idb_meth = (const struct rt_functab *) ((void *)0); \
	(_p)->idb_ptr = ((void *)0); \
	bu_avs_init_empty(&(_p)->idb_avs); \
    }
#define RT_DB_INTERNAL_INIT_ZERO {RT_DB_INTERNAL_MAGIC, -1, -1, NULL, NULL, BU_AVS_INIT_ZERO}
#define RT_CK_DB_INTERNAL(_p) BU_CKMAG(_p, RT_DB_INTERNAL_MAGIC, "rt_db_internal")

/**
 * Get an object from the database, and convert it into its internal
 * (i.e., unserialized in-memory) representation.  Applies the
 * provided matrix transform only to the in-memory internal being
 * returned.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal(struct rt_db_internal   *ip,
					const struct directory  *dp,
					const struct db_i       *dbip,
					const mat_t             mat,
					struct resource         *resp);

/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.  On success only, the internal
 * representation is freed.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal(struct directory        *dp,
					struct db_i             *dbip,
					struct rt_db_internal   *ip,
					struct resource         *resp);

/**
 * Put an object in internal format out onto a file in external
 * format.  Used by LIBWDB.
 *
 * Can't really require a dbip parameter, as many callers won't have
 * one.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * 0 OK
 * <0 error
 */
RT_EXPORT extern int rt_fwrite_internal(FILE *fp,
					const char *name,
					const struct rt_db_internal *ip,
					double conv2mm);
RT_EXPORT extern void rt_db_free_internal(struct rt_db_internal *ip);

/**
 * Convert an object name to a rt_db_internal pointer
 *
 * Looks up the named object in the directory of the specified model,
 * obtaining a directory pointer.  Then gets that object from the
 * database and constructs its internal representation.  Returns
 * ID_NULL on error, otherwise returns the type of the object.
 */
RT_EXPORT extern int rt_db_lookup_internal(struct db_i *dbip,
					   const char *obj_name,
					   struct directory **dpp,
					   struct rt_db_internal *ip,
					   int noisy,
					   struct resource *resp);



/////////////////////////////////////////////////////////////////
// The following define an "editing" equivalent to rt_db_internal

#define RT_SOLID_EDIT_IDLE              0
#define RT_SOLID_EDIT_TRANS             1
#define RT_SOLID_EDIT_SCALE             2       /* Scale whole solid by scalar */
#define RT_SOLID_EDIT_ROT               3
#define RT_SOLID_EDIT_PSCALE            4       /* Scale one solid parameter by scalar */

struct rt_solid_edit_map;

struct rt_solid_edit {

    struct rt_solid_edit_map *m;

    // Optional logging of messages from editing code
    struct bu_vls *log_str;

    // Container to hold the intermediate state
    // of the object being edited (I think?)
    struct rt_db_internal es_int;

    // Tolerance for calculations
    const struct bn_tol *tol;
    struct bview *vp;

    // Primary variable used to identify specific editing operations
    int edit_flag;
    /* item/edit_mode selected from menu.  TODO - it seems like this
     * may be used to "specialize" edit_flag to narrow its scope to
     * specific operations - in which case we might be able to rename
     * it to something more general than "menu"... */
    int edit_menu;

    // Translate values used in XY mouse vector manipulation
    vect_t edit_absolute_model_tran;
    vect_t last_edit_absolute_model_tran;
    vect_t edit_absolute_view_tran;
    vect_t last_edit_absolute_view_tran;

    // Scale values used in XY mouse vector manipulation
    fastf_t edit_absolute_scale;

    // MGED wants to know if we're in solid rotate, translate or scale mode.
    // (TODO - why?) Rather than keying off of primitive specific edit op
    // types, have the ops set flags:
    int solid_edit_rotate;
    int solid_edit_translate;
    int solid_edit_scale;
    int solid_edit_pick;

    fastf_t es_scale;           /* scale factor */
    mat_t incr_change;          /* change(s) from last cycle */
    mat_t model_changes;        /* full changes this edit */
    fastf_t acc_sc_sol;         /* accumulate solid scale factor */
    mat_t acc_rot_sol;          /* accumulate solid rotations */

    int e_keyfixed;             /* keypoint specified by user? */
    point_t e_keypoint;         /* center of editing xforms */
    const char *e_keytag;       /* string identifying the keypoint */

    int e_mvalid;               /* e_mparam valid.  e_inpara must = 0 */
    vect_t e_mparam;            /* mouse input param.  Only when es_mvalid set */

    int e_inpara;               /* parameter input from keyboard flag.  1 == e_para valid.  e_mvalid must = 0 */
    vect_t e_para;              /* keyboard input parameter changes */

    mat_t e_invmat;             /* inverse of e_mat KAA */
    mat_t e_mat;                /* accumulated matrix of path */

    point_t curr_e_axes_pos;    /* center of editing xforms */
    point_t e_axes_pos;

    // Conversion factors
    double base2local;
    double local2base;

    // Trigger for view updating
    int update_views;

    // vlfree list
    struct bu_list *vlfree;

    /* Flag to trigger some primitive edit opts to use keypoint (and maybe other behaviors?) */
    int mv_context;

    /* Internal primitive editing information specific to primitive types. */
    void *ipe_ptr;

    /* User pointer */
    void *u_ptr;
};

/** Create and initialize an rt_solid_edit struct */
RT_EXPORT extern struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);

/** Free a rt_solid_edit struct */
RT_EXPORT extern void
rt_solid_edit_destroy(struct rt_solid_edit *s);

/* Functions for working with editing callback maps */
RT_EXPORT extern struct rt_solid_edit_map *
rt_solid_edit_map_create(void);
RT_EXPORT extern void
rt_solid_edit_map_destroy(struct rt_solid_edit_map *);
RT_EXPORT extern int
rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d);
RT_EXPORT extern int
rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode);
RT_EXPORT extern int
rt_solid_edit_map_sync(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im);



RT_EXPORT extern void
rt_get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, fastf_t *mat);



struct rt_solid_edit_menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_solid_edit *, int, int, int, void *);
    int menu_arg;
};




__END_DECLS

#endif /* RT_DB_INTERNAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
