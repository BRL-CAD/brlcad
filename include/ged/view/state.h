/*                       S T A T E . H
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
/** @addtogroup ged_state
 *
 * Geometry EDiting Library Object Selection Functions.
 *
 *
 * TODO - both the drawn state and the selection state need a rethink, particularly
 * since they need to sync when an interface is visually indicating one or both
 * sets of information.
 *
 * There are three things we may want from either:
 *
 * 1.  "minimal" list of active entities - the shallowest paths that fully
 * express the set of active objects
 *
 * 2.  "input" list - what the user has actually specified to create the
 * current state
 *
 * 3.  "solids" list - the full paths to the solids that will be the sources of
 * the actual geometry shown in the scene.  A complication here is drawing
 * modes that evaluate the geometry, which will not have child solids but will
 * themselves be holding a temporary set of generated view data.
 *
 * 4.  "active" list - the set of all paths between specified/minimal and solids
 * that are implicit active
 *
 * Both #1 and #3 can be generated from #2, although how quickly this can be
 * done is a concern as hierarchies get large.  #3 is necessary for graphical
 * interrogation of scenes to build selection sets, since it is those solid
 * objects that the user will be interacting with.  (In the case of a selection
 * set built solely from graphical selection #2 and #3 will be the same.
 * However, since the drawn scene will more likely have been specified with one
 * or a small number of higher level objects, the #2 draw list needs to be used
 * to generate a #3 list to support building up the selection list.)
 *
 * When a "who" command is run the user is probably looking for #1, but that is
 * not as clear in the select case - in a tree view, selecting a comb and all
 * of the children of that comb have different implications for editing.  The
 * former, unless the comb is a top level object, implies editing the instance of
 * the comb in its own parent.  The latter implies editing the comb definition,
 * altering the position of its children.  Unlike the former, the latter will
 * impact ALL uses of comb, not just a single instance.  That would imply select
 * should default to #2, but that's most likely not what is desired if the user
 * has selected large numbers of individual solids from #3 - they MAY want that,
 * but they may also be seeking to specify groups of regions or higher level objects
 * to manipulate.  That implies the need to expose some "collapse" operations to
 * the user so they may identify wholly selected parent objects and "promote"
 * to them.
 *
 * We also need an efficient way for both drawing and selection codes to look up
 * and manipulate the state of their corresponding information in the other container.
 * When drawing, the interface may need to illuminate selected objects (which can
 * be selected even if not drawn, and so will need to "appear" selected.)  This requires
 * being able to interrogate the #3 select list from the drawing code to identify if
 * a given full path is selected.  Likewise, when a selection occurs, drawn objects
 * will need to have their illuminate state updated - the #2 select list will need
 * to generate a #3 select list and then update (illuminate or de-illuminate, as the
 * case may be) corresponding solids on the #3 drawn list.  All this needs to also
 * happen quickly, to handle large selections and de-selections on very large geometry
 * hierarchies.  The lists may also be invalidated by geometry editing operations,
 * and so will need to be quickly validated against the .g state or updated in response
 * to .g changes.
 *
 * A complication on the #2 lists is what happens if an erase or deselect
 * command removes part of a previously specified object.  In that case a new
 * #2 list must be generated which captures as much of the original semantic
 * intent of the user specification as possible - "expanding" the relevant
 * entries from the #2 list to their solids, removing the solids from the user
 * specified removal parents, and then collapsing the remaining solids back to
 * a minimal set.
 *
 * Another #2 alteration case is when a higher level specification "subsumes" previous
 * #2 paths into it.  The existing paths must be recognized as subpaths of the
 * specified path, and removed in favor of it.
 *
 * At the moment, we have explicit logic in the drawing code for handling the above
 * cases, using db_full_path top matching.  Syncing between the selection and drawing
 * codes is imperfect - draw does not currently check for selected status when
 * creating scene objects.
 *
 * Design thoughts - what if for both the ged drawn and selected lists we
 * stash unordered sets of path hashes to indicate activity, and then add/remove
 * those hashes based on specified paths and the related solid tree walks?  To
 * generate the #1 set from the #2 we get the parent of each #2, check if all the
 * parent's children are in #4, and if so add it to #4 and process its parent and
 * so on.  If we find a parent without all of its children in #4, remove it from #4
 * if present and report it as a #1.  To go the other way, do a full path tree walk
 * calculating hashes as we go and populating #4.  Any solid we reach that way is
 * part of #3.  If the view containers maintain unordered maps of hashes to scene
 * objects, the selection code can directly identify any active objects.  The
 * highlighting update pass would be one iteration to clear all flags, and then
 * a second over the #3 hashes from the selection to set illum flags on the
 * currently selected objects.  As long as the same hashes are used for both,
 * no further processing would be necessary to ID selection changes.
 *
 * Draw objects also still need to be synced with the .g state in response to
 * update notifications from the .g I/O callbacks, which report directory
 * pointers... we need a way to flag solids based on that info as well, which
 * probably means we still need to keep the db_full_path associated with the
 * scene object.  May want to re-examine the current two-tier storage system
 * and just make all solid objs top level entries.  If we're willing to re-calculate
 * the "drawn" #1 list on the fly, and not worry about #2 for the drawn objects
 * case, that may simplify some things.
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

// TODO - once this settles down, give it a magic number so we can type
// check it after a void cast
struct draw_update_data_t {
    struct db_i *dbip;
    struct db_full_path *fp;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
    struct bg_mesh_lod_context *mesh_c;
    struct resource *res;
};


/* Defined in view.cpp */
GED_EXPORT extern int ged_view_update(struct ged *gedp);

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
