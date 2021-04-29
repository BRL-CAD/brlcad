/*                       S T A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @addtogroup ged_state
 *
 * Geometry EDiting Library Object Selection Functions.
 *
 */
/** @{ */
/** @file ged/view/state.h */

#ifndef GED_VIEW_STATE_H
#define GED_VIEW_STATE_H

#include "common.h"
#include "bn/tol.h"
#include "rt/db_fullpath.h"
#include "rt/db_instance.h"
#include "ged/defines.h"

__BEGIN_DECLS

struct draw_update_data_t {
    struct db_i *dbip;
    struct db_full_path fp;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
};


/* Defined in vutil.c */
GED_EXPORT extern void ged_view_update(struct bview *gvp);

/**
 * Erase all currently displayed geometry and draw the specified object(s)
 */
GED_EXPORT extern int ged_blast(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_draw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_E(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare all regions with the given air code(s) for display
 */
GED_EXPORT extern int ged_eac(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase objects from the display.
 */
GED_EXPORT extern int ged_erase(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns how an object is being displayed
 */
GED_EXPORT extern int ged_how(struct ged *gedp, int argc, const char *argv[]);

/**
 * Illuminate/highlight database object.
 */
GED_EXPORT extern int ged_illum(struct ged *gedp, int argc, const char *argv[]);

/**
 * Configure Level of Detail drawing.
 */
GED_EXPORT extern int ged_lod(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the shaded mode.
 */
GED_EXPORT extern int ged_shaded_mode(struct ged *gedp, int argc, const char *argv[]);


/**
 * Recalculate plots for displayed objects.
 */
GED_EXPORT extern int ged_redraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * List the objects currently prepped for drawing
 */
GED_EXPORT extern int ged_who(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase all currently displayed geometry
 */
GED_EXPORT extern int ged_zap(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rubber band rectangle utility.
 */
GED_EXPORT extern int ged_rect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the keypoint
 */
GED_EXPORT extern int ged_keypoint(struct ged *gedp, int argc, const char *argv[]);


GED_EXPORT extern unsigned long long dl_name_hash(struct ged *gedp);

GED_EXPORT extern unsigned long long dl_update(struct ged *gedp);


__END_DECLS

#endif /* GED_VIEW_STATE_H */

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
