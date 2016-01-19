/*                           G E D . H
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
/** @addtogroup libged
 *
 * Functions provided by the LIBGED geometry editing library.  These
 * routines are a procedural basis for the geometric editing
 * capabilities available in BRL-CAD.  The library is tightly coupled
 * to the LIBRT library for geometric representation and analysis.
 *
 */
/** @{ */
/** @file ged.h */

#ifndef GED_H
#define GED_H

#include "common.h"

#include "dm/bview.h"
#include "raytrace.h"
#include "rt/solid.h"
#include "ged/defines.h"
#include "ged/database.h"
#include "ged/objects.h"
#include "ged/framebuffer.h"
#include "ged/view.h"


__BEGIN_DECLS








/* defined in clip.c */
GED_EXPORT extern int ged_clip(fastf_t *xp1,
			       fastf_t *yp1,
			       fastf_t *xp2,
			       fastf_t *yp2);
GED_EXPORT extern int ged_vclip(vect_t a,
				vect_t b,
				fastf_t *min,
				fastf_t *max);

/* defined in copy.c */
GED_EXPORT extern int ged_dbcopy(struct ged *from_gedp,
				 struct ged *to_gedp,
				 const char *from,
				 const char *to,
				 int fflag);


/* defined in inside.c */
GED_EXPORT extern int ged_inside_internal(struct ged *gedp,
					  struct rt_db_internal *ip,
					  int argc,
					  const char *argv[],
					  int arg,
					  char *o_name);


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
 * Returns lots of information about the specified object(s)
 */
GED_EXPORT extern int ged_analyze(struct ged *gedp, int argc, const char *argv[]);


/**
 * Report the size of the bounding box (rpp) around the specified object
 */
GED_EXPORT extern int ged_bb(struct ged *gedp, int argc, const char *argv[]);


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
 * Make color entry.
 */
GED_EXPORT extern int ged_color(struct ged *gedp, int argc, const char *argv[]);

/**
 * create, update, remove, and list geometric and dimensional constraints.
 */
GED_EXPORT extern int ged_constraint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy cylinder and position at end of original cylinder
 */
GED_EXPORT extern int ged_cpi(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get libbu's debug bit vector
 */
GED_EXPORT extern int ged_debugbu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump of the database's directory
 */
GED_EXPORT extern int ged_debugdir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's debug bit vector
 */
GED_EXPORT extern int ged_debuglib(struct ged *gedp, int argc, const char *argv[]);

/**
 * Provides user-level access to LIBBU's bu_prmem()
 */
GED_EXPORT extern int ged_debugmem(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_memprint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's NMG debug bit vector
 */
GED_EXPORT extern int ged_debugnmg(struct ged *gedp, int argc, const char *argv[]);

/**
 * Decompose nmg_solid into maximally connected shells
 */
GED_EXPORT extern int ged_decompose(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delay the specified amount of time
 */
GED_EXPORT extern int ged_delay(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delete the specified pipe point.
 */
GED_EXPORT extern int ged_delete_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Echo the specified arguments.
 */
GED_EXPORT extern int ged_echo(struct ged *gedp, int argc, const char *argv[]);

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
 * Globs expression against database objects
 */
GED_EXPORT extern int ged_expand(struct ged *gedp, int argc, const char *argv[]);

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
 * returns form for objects of type "type"
 */
GED_EXPORT extern int ged_form(struct ged *gedp, int argc, const char *argv[]);

/**
 * Given an NMG solid, break it up into several NMG solids, each
 * containing a single shell with a single sub-element.
 */
GED_EXPORT extern int ged_fracture(struct ged *gedp, int argc, const char *argv[]);

/**
 * Calculate a geometry diff
 */
GED_EXPORT extern int ged_gdiff(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get a bot's edges
 */
GED_EXPORT extern int ged_get_bot_edges(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the object's type
 */
GED_EXPORT extern int ged_get_type(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_gqa(struct ged *gedp, int argc, const char *argv[]);

/**
 * Query or manipulate properties of a graph.
 */
GED_EXPORT extern int ged_graph(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a human
 */
GED_EXPORT extern int ged_human(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a primitive via keyboard.
 */
GED_EXPORT extern int ged_in(struct ged *gedp, int argc, const char *argv[]);

/**
 * Finds the inside primitive per the specified thickness.
 */
GED_EXPORT extern int ged_inside(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes a bot object out of the specified section.
 */
GED_EXPORT extern int ged_importFg4Section(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set region ident codes.
 */
GED_EXPORT extern int ged_item(struct ged *gedp, int argc, const char *argv[]);

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
 * Used to control logging.
 */
GED_EXPORT extern int ged_log(struct ged *gedp, int argc, const char *argv[]);

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
 * Trace a single ray from the current view.
 */
GED_EXPORT extern int ged_nirt(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_vnirt(struct ged *gedp, int argc, const char *argv[]);

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
 * Open a database
 */
GED_EXPORT extern int ged_reopen(struct ged *gedp, int argc, const char *argv[]);

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
 * Print color table
 */
GED_EXPORT extern int ged_prcolor(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prefix the specified objects with the specified prefix
 */
GED_EXPORT extern int ged_prefix(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepend a pipe point.
 */
GED_EXPORT extern int ged_prepend_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Preview a new style RT animation script.
 */
GED_EXPORT extern int ged_preview(struct ged *gedp, int argc, const char *argv[]);

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
 * Get/set query_ray attributes
 */
GED_EXPORT extern int ged_qray(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern void ged_init_qray(struct ged_drawable *gdp);
GED_EXPORT extern void ged_free_qray(struct ged_drawable *gdp);

/**
 * Read region ident codes from filename.
 */
GED_EXPORT extern int ged_rcodes(struct ged *gedp, int argc, const char *argv[]);

/**
 * Change the default region ident codes: item air los mat
 */
GED_EXPORT extern int ged_regdef(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a region
 */
GED_EXPORT extern int ged_region(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes and arb given a point, 2 coord of 3 pts, rot, fb and thickness.
 */
GED_EXPORT extern int ged_rfarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the rotation matrix.
 */
GED_EXPORT extern int ged_rmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Read material properties from a file.
 */
GED_EXPORT extern int ged_rmater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate an arb's face through point
 */
GED_EXPORT extern int ged_rotate_arb_face(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the point.
 */
GED_EXPORT extern int ged_rot_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Invoke any raytracing application with the current view.
 */
GED_EXPORT extern int ged_rrt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the raytracing application.
 */
GED_EXPORT extern int ged_rt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Abort the current raytracing processes.
 */
GED_EXPORT extern int ged_rtabort(struct ged *gedp, int argc, const char *argv[]);

/**
 * Check for overlaps in the current view.
 */
GED_EXPORT extern int ged_rtcheck(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the rtwizard application.
 */
GED_EXPORT extern int ged_rtwizard(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save keyframe in file (experimental)
 */
GED_EXPORT extern int ged_savekey(struct ged *gedp, int argc, const char *argv[]);

/**
 * Interface to search functionality (i.e. Unix find for geometry)
 */
GED_EXPORT extern int ged_search(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the output handler script
 */
GED_EXPORT extern int ged_set_output_script(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the unix plot output mode
 */
GED_EXPORT extern int ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the transparency of the specified object
 */
GED_EXPORT extern int ged_set_transparency(struct ged *gedp, int argc, const char *argv[]);

/**
 * Simpler, command-line version of 'mater' command.
 */
GED_EXPORT extern int ged_shader(struct ged *gedp, int argc, const char *argv[]);

/**
 * Breaks the NMG model into separate shells
 */
GED_EXPORT extern int ged_shells(struct ged *gedp, int argc, const char *argv[]);

/**
 * Show the matrix transformations along path
 */
GED_EXPORT extern int ged_showmats(struct ged *gedp, int argc, const char *argv[]);

/**
 * Performs simulations.
 */
GED_EXPORT extern int ged_simulate(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_solids_on_ray(struct ged *gedp, int argc, const char *argv[]);

/**
 * Slew the view
 */
GED_EXPORT extern int ged_slew(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a group using a sphere
 */
GED_EXPORT extern int ged_sphgroup(struct ged *gedp, int argc, const char *argv[]);

/**
 * Count/list primitives/regions/groups
 */
GED_EXPORT extern int ged_summary(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sync up the in-memory database with the on-disk database.
 */
GED_EXPORT extern int ged_sync(struct ged *gedp, int argc, const char *argv[]);

/**
 * The ged_tables() function serves idents, regions and solids.
 *
 * Make ascii summary of region idents.
 *
 */
GED_EXPORT extern int ged_tables(struct ged *gedp, int argc, const char *argv[]);

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

/**
 * Return the specified region's id.
 */
GED_EXPORT extern int ged_whatid(struct ged *gedp, int argc, const char *argv[]);

/**
 * The ged_which() function serves both whichair and whichid.
 *
 * Find the regions with the specified air codes.  Find the regions
 * with the specified region ids.
 */
GED_EXPORT extern int ged_which(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return all combinations with the specified shaders.
 */
GED_EXPORT extern int ged_which_shader(struct ged *gedp, int argc, const char *argv[]);

/**
 * Push object path transformations to solids, creating primitives if necessary
 */
GED_EXPORT extern int ged_xpush(struct ged *gedp, int argc, const char *argv[]);

/**
 * Voxelize the specified objects
 */
GED_EXPORT extern int ged_voxelize(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern bview_polygon *ged_clip_polygon(ClipType op, bview_polygon *subj, bview_polygon *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern bview_polygon *ged_clip_polygons(ClipType op, bview_polygons *subj, bview_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern int ged_export_polygon(struct ged *gedp, bview_data_polygon_state *gdpsp, size_t polygon_i, const char *sname);
GED_EXPORT extern bview_polygon *ged_import_polygon(struct ged *gedp, const char *sname);
GED_EXPORT extern fastf_t ged_find_polygon_area(bview_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size);
GED_EXPORT extern int ged_polygons_overlap(struct ged *gedp, bview_polygon *polyA, bview_polygon *polyB);
GED_EXPORT extern void ged_polygon_fill_segments(struct ged *gedp, bview_polygon *poly, vect2d_t vfilldir, fastf_t vfilldelta);



/* defined in trace.c */

#define _GED_MAX_LEVELS 12
struct _ged_trace_data {
    struct ged *gtd_gedp;
    struct directory *gtd_path[_GED_MAX_LEVELS];
    struct directory *gtd_obj[_GED_MAX_LEVELS];
    mat_t gtd_xform;
    int gtd_objpos;
    int gtd_prflag;
    int gtd_flag;
};


GED_EXPORT extern void ged_trace(struct directory *dp,
		       int pathpos,
		       const mat_t old_xlate,
		       struct _ged_trace_data *gtdp,
		       int verbose);


/* defined in get_obj_bounds.c */
GED_EXPORT extern int ged_get_obj_bounds(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       int use_air,
			       point_t rpp_min,
			       point_t rpp_max);


/* defined in track.c */
GED_EXPORT extern int ged_track2(struct bu_vls *log_str, struct rt_wdb *wdbp, const char *argv[]);

/* defined in wdb_importFg4Section.c */
GED_EXPORT int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);


/***************************************
 * Conceptual Documentation for LIBGED *
 ***************************************
 *
 * Below are developer notes for a data structure layout that this
 * library is being migrated towards.  This is not necessarily the
 * current status of the library, but rather a high-level concept for
 * how the data might be organized down the road for the core data
 * structures available for application and extension management.
 *
 * struct ged {
 *   dbip
 *   views * >-----.
 *   result()      |
 * }               |
 *                 |
 * struct view { <-'
 *   geometry * >------.
 *   update()          |
 * }                   |
 *                     |
 * struct geometry { <-'
 *   display lists
 *   directory *
 *   update()
 * }
 *
 */

__END_DECLS

#endif /* GED_H */

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
