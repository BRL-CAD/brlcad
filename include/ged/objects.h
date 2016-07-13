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



/* defined in inside.c */
GED_EXPORT extern int ged_inside_internal(struct ged *gedp,
					  struct rt_db_internal *ip,
					  int argc,
					  const char *argv[],
					  int arg,
					  char *o_name);


/**
 * Finds the inside primitive per the specified thickness.
 */
GED_EXPORT extern int ged_inside(struct ged *gedp, int argc, const char *argv[]);


/**
 * Creates an arb8 given the following:
 *   1)   3 points of one face
 *   2)   coord x, y or z and 2 coordinates of the 4th point in that face
 *   3)   thickness
 */
GED_EXPORT extern int ged_3ptarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates an arb8 given rotation and fallback angles
 */
GED_EXPORT extern int ged_arb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates an annotation.
 */
GED_EXPORT extern int ged_annotate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Append a pipe point.
 */
GED_EXPORT extern int ged_append_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Allow editing of the matrix, etc., along an arc.
 */
GED_EXPORT extern int ged_arced(struct ged *gedp, int argc, const char *argv[]);

/**
 * Tessellates each operand object, then performs the
 * boolean evaluation, storing result in 'new_obj'
 */
GED_EXPORT extern int ged_bev(struct ged *gedp, int argc, const char *argv[]);


/**
 * Manipulate opaque binary objects.
 * Must specify one of -i (for creating or adjusting objects (input))
 * or -o for extracting objects (output).
 * If the major type is "u" the minor type must be one of:
 *   "f" -> float
 *   "d" -> double
 *   "c" -> char (8 bit)
 *   "s" -> short (16 bit)
 *   "i" -> int (32 bit)
 *   "l" -> long (64 bit)
 *   "C" -> unsigned char (8 bit)
 *   "S" -> unsigned short (16 bit)
 *   "I" -> unsigned int (32 bit)
 *   "L" -> unsigned long (64 bit)
 * For input, source is a file name and dest is an object name.
 * For output source is an object name and dest is a file name.
 * Only uniform array binary objects (major_type=u) are currently supported}}
 */
GED_EXPORT extern int ged_bo(struct ged *gedp, int argc, const char *argv[]);

/**
 * Query or manipulate properties of bot
 */
GED_EXPORT extern int ged_bot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create new_bot by condensing old_bot
 */
GED_EXPORT extern int ged_bot_condense(struct ged *gedp, int argc, const char *argv[]);

/**
 * Uses edge decimation to reduce the number of triangles in the
 * specified BOT while keeping within the specified constraints.
 */
GED_EXPORT extern int ged_bot_decimate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump bots to the specified format.
 */
GED_EXPORT extern int ged_bot_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump displayed bots to the specified format.
 */
GED_EXPORT extern int ged_dbot_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot edge. This splits the triangles that share the edge.
 */
GED_EXPORT extern int ged_bot_edge_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create new_bot by fusing faces in old_bot
 */
GED_EXPORT extern int ged_bot_face_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sort the facelist of BOT solids to optimize ray trace performance
 * for a particular number of triangles per raytrace piece.
 */
GED_EXPORT extern int ged_bot_face_sort(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot face into three parts (i.e. by adding a point to the center)
 */
GED_EXPORT extern int ged_bot_face_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Flip/reverse the specified bot's orientation.
 */
GED_EXPORT extern int ged_bot_flip(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fuse bot
 */
GED_EXPORT extern int ged_bot_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create bot_dest by merging the bot sources.
 */
GED_EXPORT extern int ged_bot_merge(struct ged *gedp, int argc, const char *argv[]);

/**
 * Calculate vertex normals for the BOT primitive
 */
GED_EXPORT extern int ged_bot_smooth(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot
 */
GED_EXPORT extern int ged_bot_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sync the specified bot's orientation (i.e. make sure all neighboring triangles have same orientation).
 */
GED_EXPORT extern int ged_bot_sync(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fuse bot vertices
 */
GED_EXPORT extern int ged_bot_vertex_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * BREP utility command
 */
GED_EXPORT extern int ged_brep(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create constraint object
 */
GED_EXPORT extern int ged_cc(struct ged *gedp, int argc, const char *argv[]);

/**
 * Performs a deep copy of object.
 */
GED_EXPORT extern int ged_clone(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make coil shapes.
 */
GED_EXPORT extern int ged_coil(struct ged *gedp, int argc, const char *argv[]);

/**
 * create, update, remove, and list geometric and dimensional constraints.
 */
GED_EXPORT extern int ged_constraint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy cylinder and position at end of original cylinder
 */
GED_EXPORT extern int ged_cpi(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delete the specified pipe point.
 */
GED_EXPORT extern int ged_delete_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Arb specific edits.
 */
GED_EXPORT extern int ged_edarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit combination materials.
 *
 * Command relies on rmater, editit, and wmater commands.
 */
GED_EXPORT extern int ged_edmater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Evaluate objects via NMG tessellation
 */
GED_EXPORT extern int ged_ev(struct ged *gedp, int argc, const char *argv[]);

/**
 * Facetize the specified objects
 */
GED_EXPORT extern int ged_facetize(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the arb edge nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_arb_edge_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the bot edge nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_bot_edge_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the bot point nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_botpt_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Add a metaball point.
 */
GED_EXPORT extern int ged_add_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delete a metaball point.
 */
GED_EXPORT extern int ged_delete_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the metaball point nearest the specified point in model coordinates.
 */
GED_EXPORT extern int ged_find_metaballpt_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move a metaball point.
 */
GED_EXPORT extern int ged_move_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the pipe point nearest the specified point in model coordinates.
 */
GED_EXPORT extern int ged_find_pipept_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Given an NMG solid, break it up into several NMG solids, each
 * containing a single shell with a single sub-element.
 */
GED_EXPORT extern int ged_fracture(struct ged *gedp, int argc, const char *argv[]);
/**
 * Get a bot's edges
 */
GED_EXPORT extern int ged_get_bot_edges(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the object's type
 */
GED_EXPORT extern int ged_get_type(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a primitive via keyboard.
 */
GED_EXPORT extern int ged_in(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes a bot object out of the specified section.
 */
GED_EXPORT extern int ged_importFg4Section(struct ged *gedp, int argc, const char *argv[]);

/**
  * Joint command ported to the libged library.
  */
GED_EXPORT extern int ged_joint(struct ged *gedp, int argc, const char *argv[]);

/**
  * New joint command.
  */
GED_EXPORT extern int ged_joint2(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill/delete the specified objects from the database
 */
GED_EXPORT extern int ged_kill(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill/delete the specified objects from the database, removing all references
 */
GED_EXPORT extern int ged_killall(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill all references to the specified object(s).
 */
GED_EXPORT extern int ged_killrefs(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill all paths belonging to objects
 */
GED_EXPORT extern int ged_killtree(struct ged *gedp, int argc, const char *argv[]);

/**
 * List object's tree as a tcl list of {operator object} pairs
 */
GED_EXPORT extern int ged_lt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make a new primitive.
 */
GED_EXPORT extern int ged_make(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates a point-cloud (pnts) given the following:
 *   1)   object name
 *   2)   path and filename to point data file
 *   3)   point data file format (xyzrgbsijk?)
 *   4)   point data file units or conversion factor to mm
 *   5)   default diameter of each point
 */
GED_EXPORT extern int ged_make_pnts(struct ged *gedp, int argc, const char *argv[]);

/**
 * Mirror the primitive or combination along the specified axis.
 */
GED_EXPORT extern int ged_mirror(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move an arb's edge through point
 */
GED_EXPORT extern int ged_move_arb_edge(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move/rename a database object
 */
GED_EXPORT extern int ged_move(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move/rename all occurrences object
 */
GED_EXPORT extern int ged_move_all(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move an arb's face through point
 */
GED_EXPORT extern int ged_move_arb_face(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified bot point. This can be relative or absolute.
 */
GED_EXPORT extern int ged_move_botpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified bot points. This movement is always relative.
 */
GED_EXPORT extern int ged_move_botpts(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified pipe point.
 */
GED_EXPORT extern int ged_move_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * NMG command with subcommands for altering NMG datastructure.
 */
GED_EXPORT extern int ged_nmg(struct ged *gedp, int argc, const char *argv[]);

/**
 * Decimate NMG primitive via edge collapse
 */
GED_EXPORT extern int ged_nmg_collapse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Attempt to fix an NMG primitive's normals.
 */
GED_EXPORT extern int ged_nmg_fix_normals(struct ged *gedp, int argc, const char *argv[]);

/**
 * Simplify the NMG primitive, if possible
 */
GED_EXPORT extern int ged_nmg_simplify(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get object center.
 */
GED_EXPORT extern int ged_ocenter(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate obj about the keypoint by
 */
GED_EXPORT extern int ged_orotate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale obj about the keypoint by sf.
 */
GED_EXPORT extern int ged_oscale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Translate obj by dx dy dz.
 */
GED_EXPORT extern int ged_otranslate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prefix the specified objects with the specified prefix
 */
GED_EXPORT extern int ged_prefix(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepend a pipe point.
 */
GED_EXPORT extern int ged_prepend_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate obj's attributes by rvec.
 */
GED_EXPORT extern int ged_protate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale obj's attributes by sf.
 */
GED_EXPORT extern int ged_pscale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set an obj's attribute to the specified value.
 */
GED_EXPORT extern int ged_pset(struct ged *gedp, int argc, const char *argv[]);

/**
 * Translate obj's attributes by tvec.
 */
GED_EXPORT extern int ged_ptranslate(struct ged *gedp, int argc, const char *argv[]);

/**
 *Pull objects' path transformations from primitives
 */
GED_EXPORT extern int ged_pull(struct ged *gedp, int argc, const char *argv[]);

/**
 * Push objects' path transformations to primitives
 */
GED_EXPORT extern int ged_push(struct ged *gedp, int argc, const char *argv[]);

/**
 * Replace the matrix on an arc
 */
GED_EXPORT extern int ged_putmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a region
 */
GED_EXPORT extern int ged_region(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes and arb given a point, 2 coord of 3 pts, rot, fb and thickness.
 */
GED_EXPORT extern int ged_rfarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate an arb's face through point
 */
GED_EXPORT extern int ged_rotate_arb_face(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the point.
 */
GED_EXPORT extern int ged_rot_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the transparency of the specified object
 *
 * @todo - belongs in view?
 */
GED_EXPORT extern int ged_set_transparency(struct ged *gedp, int argc, const char *argv[]);

/**
 * Breaks the NMG model into separate shells
 */
GED_EXPORT extern int ged_shells(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a group using a sphere
 */
GED_EXPORT extern int ged_sphgroup(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the specified region's id.
 */
GED_EXPORT extern int ged_whatid(struct ged *gedp, int argc, const char *argv[]);

/**
 * Push object path transformations to solids, creating primitives if necessary
 */
GED_EXPORT extern int ged_xpush(struct ged *gedp, int argc, const char *argv[]);

/**
 * Voxelize the specified objects
 */
GED_EXPORT extern int ged_voxelize(struct ged *gedp, int argc, const char *argv[]);


/** defined in get_obj_bounds.c
 *
 * @todo - belongs in view?
 *
 */
GED_EXPORT extern int ged_get_obj_bounds(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       int use_air,
			       point_t rpp_min,
			       point_t rpp_max);


/**
 * Decompose nmg_solid into maximally connected shells
 */
GED_EXPORT extern int ged_decompose(struct ged *gedp, int argc, const char *argv[]);


/**
 * returns form for objects of type "type"
 */
GED_EXPORT extern int ged_form(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a human
 */
GED_EXPORT extern int ged_human(struct ged *gedp, int argc, const char *argv[]);

/**
 * Simpler, command-line version of 'mater' command.
 */
GED_EXPORT extern int ged_shader(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a tire
 */
GED_EXPORT extern int ged_tire(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a track
 */
GED_EXPORT extern int ged_track(struct ged *gedp, int argc, const char *argv[]);

/**
 *
 *
 * Usage:
 *     tracker [-fh] [# links] [increment] [spline.iges] [link...]
 */
GED_EXPORT extern int ged_tracker(struct ged *gedp, int argc, const char *argv[]);

/* defined in track.c */
GED_EXPORT extern int ged_track2(struct bu_vls *log_str, struct rt_wdb *wdbp, const char *argv[]);

/* defined in wdb_importFg4Section.c */
GED_EXPORT int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);



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
