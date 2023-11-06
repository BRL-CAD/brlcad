/*                          D B I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @addtogroup ged_defines
 *
 * Experimental
 *
 * Geometry EDiting Library structures for reflecting the state of
 * the database and views.
 *
 * These are used to provide a fast, explicit expression in memory of the
 * database and view states, to allow applications to quickly display
 * hierarchical information and manipulate view data.
 *
 * We want this to be visible to C++ APIs like libqtcad, so they can reflect
 * the state of the .g hierarchy in their own structures without us or them
 * having to make copies of the data.  Pattern this on how we handle ON_Brep
 */
/** @{ */
/** @file ged/defines.h */

#ifndef GED_DBI_H
#define GED_DBI_H

#include "common.h"
#include "bu/vls.h"

#ifdef __cplusplus
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class GED_EXPORT DbiState;

class GED_EXPORT BSelectState {
    public:
	BSelectState(DbiState *);

	bool select_path(const char *path, bool update);
	bool select_hpath(std::vector<unsigned long long> &hpath);

	bool deselect_path(const char *path, bool update);
	bool deselect_hpath(std::vector<unsigned long long> &hpath);

	void clear();

	bool is_selected(unsigned long long);
	bool is_active(unsigned long long);
	bool is_active_parent(unsigned long long);
	bool is_parent_obj(unsigned long long);
	bool is_immediate_parent_obj(unsigned long long);
	bool is_grand_parent_obj(unsigned long long);

	std::vector<std::string> list_selected_paths();

	void expand();
	void collapse();

	void refresh();
	bool draw_sync();

	unsigned long long state_hash();

	std::unordered_map<unsigned long long, std::vector<unsigned long long>> selected;
	std::unordered_set<unsigned long long> active_paths; // Solid paths to illuminate
	std::unordered_set<unsigned long long> active_parents; // Paths above selection
	// To support highlighting closed paths that have selected primitives
	// below them, we need more information.  This is different than
	// highlighting only the paths related to the specific selected full
	// path - in this situation, the application wants to know about all
	// paths that are above the leaf *object* that is selected, in whatever
	// portion of the database.  Immediate parents are combs whose
	// immediate child is the selected leaf; grand parents are higher level
	// combs above immediate parents
	std::unordered_set<unsigned long long> immediate_parents;
	std::unordered_set<unsigned long long> grand_parents;

	void characterize();

    private:
	DbiState *dbis;

	void add_paths(
		unsigned long long c_hash,
		std::vector<unsigned long long> &path_hashes
		);

	void clear_paths(
		unsigned long long c_hash,
		std::vector<unsigned long long> &path_hashes
		);

	void expand_paths(
		std::vector<std::vector<unsigned long long>> &out_paths,
		unsigned long long c_hash,
		std::vector<unsigned long long> &path_hashes
		);

	void collapse_paths(
		std::vector<std::vector<unsigned long long>> &out_paths
		);

	void clear_paths(std::vector<unsigned long long> &path_hashes, unsigned long long c_hash);
};


class GED_EXPORT BViewState {
    public:
	BViewState(DbiState *);


	// Adds path to the BViewState container, but doesn't trigger a re-draw - that
	// should be done once all paths to be added in a given draw cycle are added.
	// The actual drawing (and mode specifications) are done with redraw and a
	// supplied bv_obj_settings structure.
	void add_path(const char *path);
	void add_hpath(std::vector<unsigned long long> &path_hashes);

	// Erases paths from the view for the given mode.  If mode < 0, all
	// matching paths are erased.  For modes that are un-evaluated, all
	// paths that are subsets of the specified path are removed.  For
	// evaluated modes like 3 (bigE) that generate an evaluated visual
	// specific to that path, only precise path matches are removed
	void erase_path(int mode, int argc, const char **argv);
	void erase_hpath(int mode, unsigned long long c_hash, std::vector<unsigned long long> &path_hashes, bool cache_collapse = true);

	// Return a sorted vector of strings encoding the drawn paths in the
	// view.  If mode == -1 list all paths, otherwise list those specific
	// to the mode.  If list_collapsed is true, return the minimal path set
	// that captures what is drawn - otherwise, return the direct list of
	// scene objects
	std::vector<std::string> list_drawn_paths(int mode, bool list_collapsed);

	// Get a count of the drawn paths
	size_t count_drawn_paths(int mode, bool list_collapsed);

	// Report if a path hash is drawn - 0 == not drawn, 1 == fully drawn, 2 == partially drawn
	int is_hdrawn(int mode, unsigned long long phash);

	// Clear all drawn objects (TODO - should allow mode specification here)
	void clear();

	// A View State refresh regenerates already drawn objects.
	unsigned long long refresh(struct bview *v, int argc, const char **argv);

	// A View State redraw can impact multiple views with a shared state - most of
	// the elements will be the same, but adaptive plotting will be view specific even
	// with otherwise common objects - we must update accordingly.
	unsigned long long redraw(struct bv_obj_settings *vs, std::unordered_set<struct bview *> &views, int no_autoview);

	// Allow callers to calculate the drawing hash of a path
	unsigned long long path_hash(std::vector<unsigned long long> &path, size_t max_len);

	// Debugging methods for printing out current states - the use of hashes
	// means direct inspection of most data isn't informative, so we provide
	// convenience methods that decode it to user-comprehensible info.
	void print_view_state(struct bu_vls *o = NULL);

    private:
	// Sets defining all drawn solid paths (including invalid paths).  The
	// s_keys holds the ordered individual keys of each drawn solid path - it
	// is the latter that allows for the collapse operation to populate
	// drawn_paths.  s_map uses the same key as s_keys to map instances to
	// actual scene objects.  Because objects may be represented by more than
	// one type of scene object (shaded, wireframe, evaluated, etc.) the mapping of
	// key to scene object is not unique - we must also take scene object type
	// into account.
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>> s_map;
	std::unordered_map<unsigned long long, std::vector<unsigned long long>> s_keys;

	// Called when the parent Db context is getting ready to update the data
	// structures - we may need to redraw, so we save any necessary information
	// ahead of the changes.  Although this is a public function of the BViewState,
	// it is practically speaking an implementation detail
	void cache_collapsed();

	DbiState *dbis;

	int check_status(
		std::unordered_set<unsigned long long> *invalid_objects,
		std::unordered_set<unsigned long long> *changed_paths,
		unsigned long long path_hash,
		std::vector<unsigned long long> &cpath,
		bool leaf_expand
		);

	void walk_tree(
		std::unordered_set<struct bv_scene_obj *> &objs,
		unsigned long long chash,
		int curr_mode,
		struct bview *v,
		struct bv_obj_settings *vs,
		matp_t m,
		std::vector<unsigned long long> &path_hashes,
		std::unordered_set<struct bview *> &views,
		unsigned long long *ret
		);

	void gather_paths(
		std::unordered_set<struct bv_scene_obj *> &objs,
		unsigned long long c_hash,
		int curr_mode,
		struct bview *v,
		struct bv_obj_settings *vs,
		matp_t m,
		matp_t lm,
		std::vector<unsigned long long> &path_hashes,
		std::unordered_set<struct bview *> &views,
		unsigned long long *ret
		);

	struct bv_scene_obj * scene_obj(
		std::unordered_set<struct bv_scene_obj *> &objs,
		int curr_mode,
		struct bv_obj_settings *vs,
		matp_t m,
		std::vector<unsigned long long> &path_hashes,
		std::unordered_set<struct bview *> &views,
		struct bview *v
		);

	int leaf_check(unsigned long long chash, std::vector<unsigned long long> &path_hashes);

	// Paths supplied by commands to be incorporated into the drawn state by redraw method
	std::vector<std::vector<unsigned long long>> staged;

	// The collapsed drawn paths from the previous db state, organized
	// by drawn mode
	void depth_group_collapse(
		std::vector<std::vector<unsigned long long>> &collapsed,
		std::unordered_set<unsigned long long> &d_paths,
		std::unordered_set<unsigned long long> &p_d_paths,
	       	std::map<size_t, std::unordered_set<unsigned long long>> &depth_groups
		);
	std::unordered_map<int, std::vector<std::vector<unsigned long long>>> mode_collapsed;
	std::vector<std::vector<unsigned long long>> all_collapsed;

	// Set of hashes of all drawn paths and subpaths, constructed during the collapse
	// operation from the set of drawn solid paths.  This allows calling codes to
	// spot check any path to see if it is active, without having to interrogate
	// other data structures or walk down the tree.
	std::unordered_map<int, std::unordered_set<unsigned long long>> drawn_paths;
	std::unordered_set<unsigned long long> all_drawn_paths;

	// Set of partially drawn paths, constructed during the collapse operation.
	// This holds the paths that should return 2 for is_hdrawn
	std::unordered_map<int, std::unordered_set<unsigned long long>> partially_drawn_paths;
	std::unordered_set<unsigned long long> all_partially_drawn_paths;

	friend class BSelectState;
};

#define GED_DBISTATE_DB_CHANGE   0x01
#define GED_DBISTATE_VIEW_CHANGE 0x02

struct ged_draw_cache;

class GED_EXPORT DbiState {
    public:
	DbiState(struct ged *);
	~DbiState();

	unsigned long long update();

	std::vector<unsigned long long> tops(bool show_cyclic);

	bool path_color(struct bu_color *c, std::vector<unsigned long long> &elements);

	bool path_is_subtraction(std::vector<unsigned long long> &elements);
	db_op_t bool_op(unsigned long long, unsigned long long);

	bool get_matrix(matp_t m, unsigned long long p_key, unsigned long long i_key);
	bool get_path_matrix(matp_t m, std::vector<unsigned long long> &elements);

	bool get_bbox(point_t *bbmin, point_t *bbmax, matp_t curr_mat, unsigned long long hash);
	bool get_path_bbox(point_t *bbmin, point_t *bbmax, std::vector<unsigned long long> &elements);

	bool valid_hash(unsigned long long phash);
	bool valid_hash_path(std::vector<unsigned long long> &phashes);
	bool print_hash(struct bu_vls *opath, unsigned long long phash);
	void print_path(struct bu_vls *opath, std::vector<unsigned long long> &path, size_t pmax = 0, int verbsose = 0);

	const char *pathstr(std::vector<unsigned long long> &path, size_t pmax = 0);
	const char *hashstr(unsigned long long);

	std::vector<unsigned long long> digest_path(const char *path);

	unsigned long long path_hash(std::vector<unsigned long long> &path, size_t max_len);

	void clear_cache(struct directory *dp);

	BViewState *get_view_state(struct bview *);

	std::vector<BSelectState *> get_selected_states(const char *sname);
	BSelectState * find_selected_state(const char *sname);

	void put_selected_state(const char *sname);
	std::vector<std::string> list_selection_sets();

	// These maps are the ".g ground truth" of the comb structures - the set
	// associated with each hash contains all the child hashes from the comb
	// definition in the database for quick lookup, and the vector preserves
	// the comb ordering for listing.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> p_c;
	// Note: to match MGED's 'l' printing you need to use a reverse_iterator
	std::unordered_map<unsigned long long, std::vector<unsigned long long>> p_v;

	// Translate individual object hashes to their directory names.  This map must
	// be updated any time a database object changes to remain valid.
	struct directory *get_hdp(unsigned long long);
	std::unordered_map<unsigned long long, struct directory *> d_map;

	// For invalid comb entry strings, we can't point to a directory pointer.  This
	// map must also be updated after every db change - if a directory pointer hash
	// maps to an entry in this map it needs to be removed, and newly invalid entries
	// need to be added.
	std::unordered_map<unsigned long long, std::string> invalid_entry_map;

	// This is a map of non-uniquely named child instances (i.e. instances that must be
	// numbered) to the .g database name associated with those instances.  Allows for
	// one unique entry in p_c rather than requiring per-instance duplication
	std::unordered_map<unsigned long long, unsigned long long> i_map;
	std::unordered_map<unsigned long long, std::string> i_str;

	// Matrices above comb instances are critical to geometry placement.  For non-identity
	// matrices, we store them locally so they may be accessed without having to unpack
	// the comb from disk.
	std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>> matrices;

	// Similar to matrices, store non-union bool ops for instances
	std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>> i_bool;


	// Bounding boxes for each solid.  To calculate the bbox for a comb, the
	// children are walked combining the bboxes.  The idea is to be able to
	// retrieve solid bboxes and calculate comb bboxes without having to touch
	// the disk beyond the initial per-solid calculations, which may be done
	// once per load and/or dimensional change.
	std::unordered_map<unsigned long long, std::vector<fastf_t>> bboxes;


	// We also have a number of standard attributes that can impact drawing,
	// which are normally only accessible by loading in the attributes of
	// the object.  We stash them in maps to have the information available
	// without having to interrogate the disk
	std::unordered_map<unsigned long long, int> c_inherit; // color inheritance flag
	std::unordered_map<unsigned long long, unsigned int> rgb; // color RGB value  (r + (g << 8) + (b << 16))
	std::unordered_map<unsigned long long, int> region_id; // region_id


	// Data to be used by callbacks
	std::unordered_set<struct directory *> added;
	std::unordered_set<struct directory *> changed;
	std::unordered_set<unsigned long long> changed_hashes;
	std::unordered_set<unsigned long long> removed;
	std::unordered_map<unsigned long long, std::string> old_names;

	// The shared view is common to multiple views, so we always update it.
	// For other associated views (if any), we track their drawn states
	// separately, but they too need to update in response to database
	// changes (as well as draw/erase commands).
	BViewState *shared_vs = NULL;
	std::unordered_map<struct bview *, BViewState *> view_states;

	// We have a "default" selection state that is always available,
	// and applications may define other named selection states.
	BSelectState *default_selected;
	std::unordered_map<std::string, BSelectState *> selected_sets;

	// Database Instance associated with this container
	struct ged *gedp = NULL;
	struct db_i *dbip = NULL;

	bool need_update_nref = true;

	// Debugging methods for printing out current states - the use of hashes
	// means direct inspection of most data isn't informative, so we provide
	// convenience methods that decode it to user-comprehensible info.
	void print_dbi_state(struct bu_vls *o = NULL, bool report_view_states = false);

    private:
	void gather_cyclic(
		std::unordered_set<unsigned long long> &cyclic,
		unsigned long long c_hash,
		std::vector<unsigned long long> &path_hashes
		);
	void print_leaves(
		std::set<std::string> &leaves,
		unsigned long long c_hash,
		std::vector<unsigned long long> &path_hashes
		);

	void populate_maps(struct directory *dp, unsigned long long phash, int reset);
	unsigned long long update_dp(struct directory *dp, int reset);
	unsigned int color_int(struct bu_color *);
	int int_color(struct bu_color *c, unsigned int);
	struct resource *res = NULL;
	struct ged_draw_cache *dcache = NULL;
	struct bu_vls hash_string = BU_VLS_INIT_ZERO;
	struct bu_vls path_string = BU_VLS_INIT_ZERO;
};


#else

/* Placeholders to allow for compilation when we're included in a C file */
typedef struct _dbi_state {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} DbiState;
typedef struct _bview_state {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} BViewState;
typedef struct _bselect_state {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} BSelectState;

#endif

#endif /* GED_DBI_H */

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
