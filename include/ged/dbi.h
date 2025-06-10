/*                          D B I . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
#include "vmath.h"
#include "bu/vls.h"

#ifdef __cplusplus
#include <set>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class GED_EXPORT DbiPath;
class GED_EXPORT DbiState;

// In-memory representation of a Comb Tree instance from a .g database
//
// The intent of this structure is to capture and represent each unique
// instance of an object inside of one comb.  So, for example, if we have a
// comb A with the following tree:
//
// A
// |- B
// |- C
// |- [M1] C
// |- [M2] C
// |- [M1] C
//
// the correct setup of the instances from the tree of A would be:
//
// A/B
// A/C@0
// A/C@1
// A/C@2
// A/C@3
// A/C@4
//
// Note in particular that even though there are two instances of C in
// A with the exact same matrix, they are present in the tree definition
// as distinct entities and are therefore separate comb instances.
//
// These are not full fledged geometry database objects (GObj objects) since
// the information they store is different, but (like GObj objects) they
// are intended to reflect the on-disk .g geometry state and should be
// updated, created and destroyed to reflect changes in the .g file.
//
// Individual CombInst containers will be primarily associated with a GObj
// class, but will also be registered in a top level DbiState container so
// their hashes can be quickly decoded when acting as as members of paths.
//
// No CombInst should survive if its parent GObj is deleted or edited in
// a way to invalidate a particular CombInst, since that would mean the
// in-memory data is no longer correctly reflecting the geometry state.
class GED_EXPORT CombInst {

    public:
	CombInst(DbiState *dbis, const char *p_name, const char *o_name, unsigned long long icnt, int i_op, matp_t i_mat);
	~CombInst();

	// Return the BRL-CAD op type of this instance
	// (DB_OP_UNION, DB_OP_SUBTRACT, DB_OP_INTERSECT)
	db_op_t bool_op();

	// Calculate Bounding Box of the instance (i.e. oname's bounding box
	// positioned using the m matrix of this instance.)
	void bbox(point_t *min, point_t *max);

	// Object name and instance name strings
	std::string cname; // Name of parent comb
	std::string oname; // Name of instanced object
	std::string iname; // Name of instanced object with id-based suffix making it unique in the comb tree.  Empty if not needed.
	unsigned long long id = 0; // In cases where iname is not unique in the comb, this is incremented to provide uniqueness

	// Hash of parent comb name cname.  Useful for looking up GObj
	// associated with the parent database comb.
	unsigned long long chash;

	// Hash of instance object name oname.  Useful for looking up GObj
	// associated with the database obj named by the instance, if one is
	// present. (A comb may name a non-existent object, so the instanced
	// object string to valid database object mapping is NOT guaranteed.)
	unsigned long long ohash;

	// Hash of Unique identifier string for this instance.  Used as the key
	// for the GObj c map that stores CombInst containers as well as the
	// global CombInst identifier in DbiState.
	unsigned long long ihash;

	// Boolean op of instance
	// (OP_UNION, OP_SUBTRACT or OP_INTERSECT)
	int boolean_op;

	// Instance matrix (default is MAT_ZERO)
	mat_t m;
	bool non_default_matrix = false;

	// Parent database state info.  We store this so that a CombInst can (in principle)
	// unpack anything it needs to - for example, to get the GObj of the parent:
	// GObj *parent_obj = d->gobjs[chash].
	DbiState *d;

	// Related DbiPath instances
	//
	// Any path listed in this set references this CombInst.  If this
	// CombInst is deleted, all paths in this set are invalid and should be
	// returned to the dbi_paths queue.
	//
	// One of the reasons for using path hashes rather than pointers to
	// reference DbiPath instances is to allow flexibility when it comes to
	// these sorts of operations - if a DbiPath is removed and then
	// recreated by a subsequent action before (say) the selection state is
	// validated, there is no need to update its contents since the new
	// registered DbiPath for a recreated path is just as good as the old
	// version - it will use the same hash, even though it's DbiPath memory
	// pointer is different.
	std::unordered_set<unsigned long long> rpaths;
};

// In-memory representation of a BRL-CAD Geometry database object.
//
// Combs and Solids are represented by GObj classes.  Combs will define an
// array of CombInst classes to represent their tree, and the lifetime of
// the CombInst classes is dependent on the state of the GObj.
class GED_EXPORT GObj {

    public:
	GObj(DbiState *d_s, struct directory *dp_i);
	~GObj();

	void bbox(point_t *min, point_t *max);

	std::string name;
	unsigned long long hash = 0;

	// Standard attributes that can impact drawing which are normally only
	// accessible by loading in the attributes of the object.  We stash
	// them here to have the information available without having to
	// interrogate the on-disk data.
	int c_inherit = 0; // color inheritance flag
	int region_id = -1; // region_id
	int region_flag = 0; // region

	struct bu_color color; // color
	bool color_set = false;

	// If the object is a comb, this vector is used to record the tree
	// info.  Otherwise it is not used. The vector preserves comb ordering
	// for listing.
	// Note: to match MGED's 'l' printing you need to use a
	// reverse_iterator
	std::vector<CombInst *> cv;

	// Parent database state info
	DbiState *d = NULL;

	// librt directory pointer
	struct directory *dp = NULL;

	// Instructs the GObj to (re)populate its scene_obj in the specified
	// mode.  If reload == false (the default) it will accept a pre-existing
	// scene object - most of the time these will be load-once-view-many.
	bool LoadSceneObj(struct bview *v = NULL, int mode = 0, bool reload = false);

	// Optional scene object(s) associated with this object.
	// Map key is the drawing mode (0 = wireframe, 1 = shaded, etc.)
	// Generally speaking these will be referenced by scene_objs
	// in DbiPath entities.
	std::map<size_t, struct bv_scene_obj *> scene_objs;

	// Allow DbiPaths access to the rpaths container
	friend class DbiPath;
    private:

	// A Comb bounding box depends on the definition of everything
	// below it in the tree, and consequently the validity of a stored
	// local version cannot be trusted unless the calling application knows
	// no potentially problematic editing operations have been made since
	// the last bbox calculation.  However, if a comb is reused frequently
	// avoiding the need to recalculated the local instance bbox is a
	// potential optimization.  USE WITH CARE.
	//
	// Any edit operation needs to invalidate not only the immediate GObj
	// and CombInst cname bboxes but also any Comb object bboxes that
	// reference those bojects - and then also invalidate the boxes of GObj
	// combs that reference THOSE combs and so on.  TODO - warrants a
	// DbiState method whose job is invalidating cached bboxes after an
	// edit.
	vect_t bb_min;
	vect_t bb_max;
	bool bb_valid = false;

	// Initialize the cv array of CombInst types based on the comb tree
	void GenCombInstances();

	// Related DbiPath instances
	//
	// Any path listed in this set uses this GObj either as a root or a
	// parent in a CombInst - if this GObj is deleted, all paths in this
	// set are invalid and should be returned to the dbi_paths queue.
	//
	// One of the reasons for using path hashes rather than pointers to
	// reference DbiPath instances is to allow flexibility when it comes to
	// these sorts of operations - if a DbiPath is removed and then
	// recreated by a subsequent action before (say) the selection state is
	// validated, there is no need to update its contents since the new
	// registered DbiPath for a recreated path is just as good as the old
	// version - it will use the same hash, even though it's DbiPath memory
	// pointer is different.
	std::unordered_set<unsigned long long> rpaths;
};

// TODO - every DbiPath potentially has a bv_scene_obj associated with it,
// which would in turn reference a child obj to get its actual geometry - the
// role of the bv_scene_obj in the path would be to set the matrix, color and
// boolean line drawing style dictated by the path - it would set the
// s_inherit_settings flag in its own object, just pulling the geometry from
// the child.
//
// The child obj of the DbiPath scene obj would be either an evaluated
// visualization scene obj (bigE, for example) or (in the cases where the leaf
// oname object is a solid) the GObj scene obj.  (The potentially interesting
// case of de-duping xpushed meshes could then be handled at the GObj level,
// where that GObj could define its own parent/child (without an inherit flag)
// that injected a matrix positioning a PCA scene object for that specific GObj
// without needing any other changes to the drawing pass.)
class GED_EXPORT DbiPath {
    public:
	DbiPath(DbiState *dbis = NULL, const char *path = NULL);
	~DbiPath();
	DbiPath(const DbiPath &p);

	// Initialization - allows us to re-use an existing container
	void Init(const char *path = NULL);

	// Prepares the container for reuse
	void Reset();

	// Equality operator - return true if p is equal to this path
	bool operator==(const DbiPath& p) const
	{
	    return (path_hash == p.path_hash);
	}

	// Assignment operator
	DbiPath& operator=(const DbiPath& p);

	// Return true if this path is a parent path of p
	bool parent(DbiPath &p);

	// Return true if this path is a child path of p
	bool child(DbiPath &p);

	// Return true this path references the object represented by hash
	bool uses(unsigned long long hash);

	// Use BRL-CAD rules to determine the active color at the
	// path leaf element
	bool color(struct bu_color *c);

	// Accumulate the matrices along the path
	bool matrix(matp_t m);

	// Determine if there is a subtraction operation along the path
	bool is_subtraction();

	// Determine if there is a intersection operation along the path
	bool is_intersection();

	// Report if the path is currently valid in the DbiState (NOTE: does
	// not necessarily imply that DbiState matches .g database state - that
	// is not the responsibility of DbiPath.)
	bool valid();

	// Report if the path is currently cyclic.  If full_check is true every
	// element in the path is checked - by default, only the leaf element
	// is verified against all of its parents.  (The latter is faster for
	// deep paths, and frequently that check alone is enough.)
	bool cyclic(bool full_check = false);

	// Report how deep the leaf element is in the hierarchy.  A top level
	// single object path, for example would be depth 0, and a/b/c would be
	// depth 2.
	size_t depth();

	// Produce a string representing the path.  pmax may be set to the
	// maximum number of path elements to include, and verbose is specified
	// to annotate the presence of additional information
	std::string str(size_t pmax = 0, int verbose = 0);

	// Calculate the bounding box of the leaf element, positioned according
	// to the instance matrices of any parent elements in the path.
	bool bbox(point_t *bmin, point_t *bmax);

	// Calculate a hash of the path elements
	unsigned long long hash(size_t max_len = 0);

	// Appends to end of path if path isn't already cyclic
	//
	// If this path was registered with dbi_paths (that does not happen by
	// default but user code can do it) then this operation will
	// de-register it since the hash key of the path no longer matches the
	// original hash used to register it.  The path is not automatically
	// re-registered as the new path it represents, since that may not
	// reflect user intent (if the code is in the process of pushing
	// multiple paths to form a longer path, for example.)  If
	// re-registration is desired once temporary operations are complete
	// that is up to the caller.
	unsigned long long push(unsigned long long element);

	// Removes the last element from the path.  By default recalculates
	// cyclic and valid properties.
	//
	// If check is false, assumes validity and resets is_cyclic and
	// is_valid without testing.  (The later is for performance sensitive
	// cases where inputs are well controlled.)
	//
	// If this path was registered with dbi_paths (that does not happen by
	// default but user code can do it) then this operation will
	// de-register it since the hash key of the path no longer matches the
	// original hash used to register it.  It is not automatically
	// re-registered as the new path, since that may not reflect user
	// intent - if re-registration is desired that is up to the caller.
	void pop(bool check = true);

	// Get the CombInst on the path at ind.  If ind is 0 (default), return
	// the Leaf comb instance - by definition the root element of a path is
	// always a GObj, so there can be no CombInst there.
	//
	// Returns NULL if path is empty or only contains a single GObj, or if ind
	// is greater than the depth of the path.
	CombInst *GetCombInst(size_t ind = 0);

	// Get the GObj on the path at ind.  If ind is 0 (default), return the
	// Root GObj (the first element in the path), else return the GObj of
	// the CombInst instance object at ind. (I.e. if the comb instance path
	// is A/B, GetGObj will return the GObj instance associated with B.)
	//
	// If the path is empty, ind is greater than the depth of the path,
	// or the CombInst oname does not map to a GObj instance the return
	// is NULL.
	GObj *GetGObj(size_t ind = 0);

	// Optional scene object(s) associated with this path.
	// Map key is the drawing mode (0 = wireframe, 1 = shaded, etc.)
	std::map<size_t, struct bv_scene_obj *> scene_objs;

	// Allow BViewState access to the parent_path_hashes set
	friend class BViewState;
    private:
	// Primary vector storing path information.
	//
	// First entry is always a GObj hash, and any subsequent entries
	// are always CombInst hashes.
	std::vector<unsigned long long> elements;

	// Path state information
	int is_cyclic = 0;  // 0 = not cyclic, 1 = cyclic, 2 = unknown
	bool is_valid = true;
	std::unordered_set<unsigned long long> component_hashes;
	std::unordered_set<unsigned long long> parent_path_hashes;
	unsigned long long path_hash;

	// Database state with which this path is associated
	DbiState *d = NULL;

	// Utility routines for processing string path specifiers
	void split_path(std::vector<std::string> &objs, const char *str);
	std::string name_deescape(std::string &name);
};


class GED_EXPORT BSelectState {
    public:
	BSelectState(DbiState *);

	// Add a path to the selection set
	bool Select(const char *path = NULL, bool update = false);
	bool Select(unsigned long long phash = 0, bool update = false);

	// Remove a path from the selection set
	bool DeSelect(const char *path = NULL, bool update = false);
	bool DeSelect(unsigned long long phash = 0, bool update = false);

	// Clear all paths from the selection set
	void Clear();

	bool IsSelected(unsigned long long);
	bool IsActive(unsigned long long);
	bool IsActiveParent(unsigned long long);
	bool IsParentObj(unsigned long long hash = 0, int level = 0); // 0 = any, 1 = Immediate, 2 = Grandparent

	std::vector<std::string> list_selected_paths();

	void ExpandPaths();
	void CollapsePaths();

	void Refresh();
	bool draw_sync();

	unsigned long long state_hash();

	// Explicitly selected paths.  Note that this is NOT an expanded or collapsed
	// set, since (particularly for editing ops) the app may need to know the precise
	// active paths.  On the other hand, we may want the set of expanded hashes to
	// use for checking when drawing.  We may also want not just the set of leaf expansions
	// but ALL intermediate hashes, since evaluated mode drawing will not go to leaf
	// solids but should still be illuminated if active in a selection.
	std::unordered_set<unsigned long long> selected;

	// Solid paths to illuminate
	// TODO - may not need this if we can just ask each drawn leaf path if
	// any of its parents are selected?  With a huge number of individually
	// selected paths that would be a problem, since each one would have to
	// be checked, but we might able to mitigate that with a
	// selected_collapsed set
	std::unordered_set<unsigned long long> active_paths; // Solid paths to illuminate 

	// To support highlighting closed paths that have selected primitives
	// below them, we need more information.  This is different than
	// highlighting only the paths related to the specific selected full
	// path - in this situation, the application wants to know about all
	// paths that are above the leaf *object* that is selected, in whatever
	// portion of the database.  Immediate parents are combs whose
	// immediate child is the selected leaf; ancestors are higher level
	// combs above immediate parents
	std::unordered_set<unsigned long long> immediate_parents;
	std::unordered_set<unsigned long long> ancestors;

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

// TODO - may want a reusable memory pool of DbiPath instances to save on mallocing
class GED_EXPORT BViewState {
    public:
	BViewState(DbiState *);

	// Adds path to the BViewState container drawn set, but doesn't trigger
	// a re-draw - that should be done once all paths to be added in a
	// given draw cycle are queued up.  The actual drawing (and setting
	// specifications) are done with Redraw and a supplied bv_obj_settings
	// structure.  By default path is assigned a wireframe drawing mode,
	// but other modes may be specified.
	void AddPath(const char *path = NULL, int mode = 0);
	void AddPath(DbiPath *p = NULL, int mode = 0);
	void AddPath(unsigned long long phash = 0, int mode = 0);

	// Erases paths from the view for the given mode.  If mode < 0, all
	// matching paths are erased.  For modes that are un-evaluated, all
	// paths that are subsets of the specified path are removed.  For
	// evaluated modes like 3 (bigE) that generate an evaluated visual
	// specific to that path, only precise path matches are removed.
	//
	// Like AddPath, this stages paths for subsequent processing but does
	// not actually trigger a redraw.
	void RemovePath(const char *p, int mode = -1);
	void RemovePath(DbiPath *p, int mode = -1);
	void RemovePath(unsigned long long phash = 0, int mode = -1);

	// Return a sorted vector of paths encoding the drawn paths in the
	// view.  If mode == -1 list all paths, otherwise list those specific
	// to the mode.  If collapsed is true, return the minimal path set
	// that captures what is drawn - otherwise, return the direct list of
	// scene objects
	std::vector<unsigned long long> DrawnPaths(int mode, bool collapsed);

	// Get a count of the drawn paths.  If collapsed is true the minimal
	// path set capturing what is drawn is counted, otherwise all scene
	// objects are counted.  mode == -1 means count all modes, otherwise
	// count only the specified mode.
	size_t DrawnPathCount(bool collapsed = true, int mode = -1);

	// Report if a DbiPath is drawn:
	//
	// 0 == not drawn, 1 == fully drawn, 2 == partially drawn
	//
	// If mode == -1, the return will reflect the collective state
	// of all modes - i.e. if part of a comb is drawn as wireframe,
	// and part as solid, and in combination all paths are visualized,
	// IsDrawn will report fully drawn
	int IsDrawn(DbiPath *p = NULL, int mode = -1);
	int IsDrawn(unsigned long long hash = 0, int mode = -1);

	// Clear all drawn objects
	void Clear(int mode = -1);

	// A refresh regenerates already drawn objects.  If paths is NULL,
	// force a regeneration of all active path geometry.  (Generally you'll
	// want to use redraw rather than refresh for performance reasons.)
	void Refresh(std::vector<unsigned long long> *paths = NULL);

	// A Rredraw can impact multiple views with a shared state - most of
	// the elements will be the same, but adaptive plotting will be view
	// specific even with otherwise common objects - we must update
	// accordingly.
	//
	// TODO - with the new setup, changed_hashes just get generated inside
	// Redraw itself - need to investigate.  Need the set of DbiPath
	// hashes corresponding to all paths and all parents of paths involved
	// with any sort of change.
	unsigned long long Redraw(
	       	std::unordered_set<struct bview *> *views = NULL,
		struct bv_obj_settings *vs = NULL,
	       	int no_autoview = 1,
		std::unordered_set<unsigned long long> *changed_hashes = NULL);

	// Debugging methods for printing out current states - the use of hashes
	// means direct inspection of most data isn't informative, so we provide
	// convenience methods that decode it to user-comprehensible info.
	std::string print_view_state();

	friend class BSelectState;
    private:
	// Sets holding all drawn paths.
	//
	// drawn_paths holds the paths specified by the callers.  If parent or
	// child paths are added to drawn_paths via add or remove ops, existing
	// paths will be consolidated or split to reflect the changed state.
	// With the exception of split paths due to a partial erasure, drawn_paths
	// contents will be determined by user calls. Generally drawn_paths will be
	// the basis used for things like the who command.  A consolidate()
	// method can be run to collapse explicitly specified tree entries into
	// higher level paths in drawn_paths if the app wants to do so, but it will
	// not happen by default.
	//
	// drawn_paths stores paths for all drawing modes - details of what modes
	// are active for a specific path are stored in the expanded paths.
	std::unordered_map<size_t, std::unordered_set<unsigned long long>> drawn_paths;

	// Set of all leaf node drawn paths.  As with drawn_paths, this set covers
	// all modes.  For unevaluated drawing modes like wireframe this will
	// hold the set of leaf nodes to solids, but for more exotic modes like
	// bigE evaluated wireframe drawing higher level paths are also considered
	// "leaf" nodes.  Those require special handling when it comes to
	// collapsing this set for specific drawing modes.
	std::unordered_set<unsigned long long> s_expanded;

	// Sorted lists of fully drawn paths organized by mode.  Suitable
	// for generating ordered lists of path strings
	std::unordered_map<int, std::vector<unsigned long long>> mode_collapsed;

	// Sorted and de-duped list of fully drawn paths independent of mode.
	// Suitable for generating ordered lists of path strings
	std::vector<std::vector<unsigned long long>> all_collapsed;

	// Set of hashes of all drawn paths and subpaths.  This allows calling codes to
	// spot check any path to see if it is active, without having to interrogate
	// other data structures or walk down the tree.  A key of -1 for the map
	// selects the set of paths drawn in AT LEAST one mode.
	std::unordered_map<int, std::unordered_set<unsigned long long>> fully_drawn_paths;

	// Set of partially drawn paths, constructed during the collapse operation.
	// This holds the paths that should return 2 for IsDrawn.  A key of -1 for the map
	// selects the set of paths drawn in AT LEAST one mode.
	std::unordered_map<int, std::unordered_set<unsigned long long>> partially_drawn_paths;

	DbiState *dbis;

};

#define GED_DBISTATE_DB_CHANGE   0x01
#define GED_DBISTATE_VIEW_CHANGE 0x02

struct ged_draw_cache;

class GED_EXPORT DbiState {
    public:
	DbiState(struct ged *);
	~DbiState();

	// Database Instance associated with this container
	struct ged *gedp = NULL;

	// Update the DbiState to reflect current .g state
	unsigned long long update();

	/*******************************************************************
         *                Database utility routines
	 *******************************************************************/
	// Checks if the hash indexes either a GObj or a CombInst
	// entity in DbiState
	bool valid_hash(unsigned long long phash);

	// Returns either the gobj name or the parent/instance string for a
	// comb instance.  If neither can be found returns "Unknown hash: #".
	std::string hash_str(unsigned long long phash);

	// Retrieval methods for GObjs and CombInsts.
	GObj *GetGObj(struct directory *dp);
	GObj *GetGObj(unsigned long long);
	CombInst *GetCombInst(unsigned long long);

	// Get a DbiPath instance.  Will either reuse an existing DbiPath
	// or (if necessary) create a new one.
	DbiPath *GetDbiPath();

	// Clear a DbiPath and store it for later reuse.
	void PutDbiPath(DbiPath *p);

	// Return a set of GObj hashes for top level objects.  By default
	// objects with cyclic paths are not returned - to add them to the
	// set, set show_cyclic to true
	std::vector<unsigned long long> tops(bool show_cyclic = false);

	// Given a set of paths, create a set of paths representing the
	// expansion of the trees of those input paths to their leaves.
	//
	// By default the expanded paths in the return vector will be created and
	// registered in dbi_paths if they didn't already exist, to ensure that
	// (at least at the time of return) ExpandPaths results all correspond
	// to a DbiPath pointer.
	std::vector<unsigned long long> ExpandPaths(std::vector<unsigned long long> &paths, bool create_paths = true);

	// Given a set of paths, create a set of paths consisting of the
	// shallowest path descriptions that fully capture the geometry present
	// in the original set.
	//
	// By default the collapsed paths in the return vector will be created and
	// registered in dbi_paths if they didn't already exist, to ensure that
	// (at the time of return) CollapsePaths results all correspond to a
	// DbiPath pointer.
	std::vector<unsigned long long> CollapsePaths(std::vector<unsigned long long> &paths, bool create_paths = true);

	// Build up a set of hashes of ALL paths below the specified path, not
	// just the leaf paths.  The targeted output set is not cleared, so
	// calling PathsBelow on multiple paths in succession will gradually
	// build up a multi-path set.  Unlike ExpandPaths and CollapsePaths
	// PathsBelow does NOT (by default) ensure a DbiPath exists for every
	// path added to the output set.
	//
	// The primary use case for this method is to allow drawing code to
	// determine if its path is part of an active selection set - for
	// that purpose a DbiPath does not need to be guaranteed present, so
	// there isn't any need to pay the overhead.  We do allow the caller
	// to request they be created and registered as an option.
	void AddPathsBelow(std::unordered_set<unsigned long long> *s, unsigned long long phash, bool create_paths = false);
	// Like AddPathsBelow, except it removes the hashes from the set rather
	// than adding them and has no option to create paths.
	void RemovePathsBelow(std::unordered_set<unsigned long long> *s, unsigned long long phash);

	/*******************************************************************
         *                      View States
	 *******************************************************************/
	// The shared view is common to multiple views, so we always update it.
	// For other associated views (if any), we track their drawn states
	// separately, but they too need to update in response to database
	// changes (as well as draw/erase commands).
	//
	// If v is NULL, return the shared view.
	BViewState *GetBViewState(struct bview *v);

	/*******************************************************************
         *                    Selection States
	 *******************************************************************/
	// We have a "default" selection state that is always available,
	// and applications may define other named selection states.
	//
	// If sname is NULL, return the default selection state.  RemoveSelection
	// being passed NULL will not delete d_selection, but will reset it to
	// the default state.
	BSelectState * GetSelectionSet(const char *sname = NULL);
	BSelectState * AddSelectionSet(const char *sname = NULL);
	void RemoveSelectionSet(const char *sname);
	std::vector<std::string> ListSelectionSets();

	/*******************************************************************
         *                     Database Paths
	 *******************************************************************/
	// Decode hashes into DbiPath instances, when we have them.  (If a path
	// hash doesn't have an entry here, the hash is not usable in most
	// DbiState and related logic.)  There is no requirement that a path
	// have exactly one DbiPath instance, but dbi_paths can hold only one
	// so if application code creates more the management of those other
	// instances is their responsibility.
	//
	// If a DbiPath is registered here (it does not happen by default,
	// but user code can do it) altering or deleting the DbiPath instance
	// will automatically de-register that DbiPath from dbi_paths.
	//
	// Path alterations to an unregistered DbiPath that happens to match
	// a hash in dbi_paths will have no effect on dbi_paths.
	std::unordered_map<unsigned long long, DbiPath *> dbi_paths;

	// Container holding all pre-allocated DbiPath instances available
	// for reuse.
	std::queue<DbiPath *> dbiq;

    public:
	// Allow the implementation containers to access the core DbiState maps
	friend class GObj;
	friend class CombInst;
	friend class DbiPath;

    private:
	// These maps are the ".g ground truth" of the database objects
	std::unordered_map<struct directory *, GObj *> dp2g;
	std::unordered_map<unsigned long long, GObj *> gobjs;
	std::unordered_map<unsigned long long, CombInst *> combinsts;

	// Private methods and resources relating to database objects
	std::vector<GObj *> get_gobjs(std::vector<unsigned long long> &path);
	std::vector<CombInst *> get_combinsts(std::vector<unsigned long long> &path);
	bool need_update_nref = true;
	void clear_cache(struct directory *dp);
	struct resource *res = NULL;
	struct ged_draw_cache *dcache = NULL;
	void collect_paths(std::unordered_set<unsigned long long> *opaths, DbiPath &p, bool create_paths);
	void clear_paths(std::unordered_set<unsigned long long> *opaths, DbiPath &p);
	void expand_path(std::vector<unsigned long long> *opaths, DbiPath &p, bool create_paths);
	void gather_cyclic(
		std::unordered_set<unsigned long long> &cyclic,
		unsigned long long c_hash, DbiPath &p
		);
	unsigned long long update_dp(struct directory *dp);

	// Debugging methods for printing out current states - the use of hashes
	// means direct inspection of most data isn't informative, so we provide
	// convenience methods that decode it to user-comprehensible info.
	void print_dbi_state(struct bu_vls *o = NULL, bool report_view_states = false);

	// Data related to Views
	BViewState *shared_vs = NULL;
	std::unordered_map<struct bview *, BViewState *> view_states;

	// Data related to Selections
	BSelectState *d_selection;
	std::unordered_map<std::string, BSelectState *> selected_sets;

	// Data to be used by callbacks
	std::unordered_set<struct directory *> added;
	std::unordered_set<struct directory *> changed;
	std::unordered_set<struct directory *> removed;
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
