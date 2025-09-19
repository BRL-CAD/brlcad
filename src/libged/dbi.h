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
#include "bu/cache.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#ifdef __cplusplus
#include <set>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "ged/defines.h"

class GED_EXPORT DbiPath;
class GED_EXPORT DbiState;
class GED_EXPORT BViewState;

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
	// By default the GObj stores the scene locally, but the caller may
	// specify at targeted object to use instead.
	bool LoadSceneObj(struct bv_scene_obj *o_obj = NULL, BViewState *wvs = NULL, int mode = 0, bool reload = false);

	// Optional scene object(s) associated with this object.
	// Map key is the drawing mode (0 = wireframe, 1 = shaded, etc.)
	// Generally speaking these will be referenced by scene_objs
	// in DbiPath entities.
	//
	// Most commonly, only the default_view will have any objects in
	// its map - but we need to allow for other possibilities.
	std::unordered_map<BViewState *, std::map<size_t, struct bv_scene_obj *>> scene_objs;

	// Record the view used to generate the geometry associated with this
	// path.  Often won't matter, but for LoD sensitive data this is
	// important.  LoadSceneObj should set lod_used to true if the lod_v
	// was involved with geometry setup.
	struct bview *lod_v = NULL;
	bool lod_used = false;

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
};

// There are a variety of settings which can potentially be overridden by
// command line arguments when drawing.  Provide a container to hold such
// arguments, as well as flags (where appropriate) to indicate that a
// non-default value has been set - in other words, the value should override
// one calculated from path data.
class GED_EXPORT DbiPath_Settings {
    public:

	DbiPath_Settings();
	DbiPath_Settings(struct bv_obj_settings *s_obj);
	~DbiPath_Settings();

	// Assignment operator
	DbiPath_Settings& operator=(const DbiPath_Settings& ps);

	// Restore DbiPath_Settings to default values
	void Reset();

	// Read and Write values from/to bv_obj_settings.
	void Read(struct bv_obj_settings *s_obj);
	void Write(struct bv_obj_settings *s_obj);

	// 0 = transparent, 1 = opaque
	fastf_t transparency = 1.0;

	// Drawing color (overrides values from database info)
	struct bu_color color = BU_COLOR_INIT_ZERO;
	// Flag to indicate color holds an override color
	bool override_color = false;

	// Drawing line width for wireframes
	int line_width = 1;

	// arrow tip length
	fastf_t arrow_tip_length = 0.0;

	//  arrow tip width
	fastf_t arrow_tip_width = 0.0;

	// do not use dashed lines for subtraction solids
	int draw_solid_lines_only = 0;

	// do not visualize subtraction solids
	int draw_non_subtract_only = 0;

	// View to use to generate LoD geometry for this path.  The
	// responsibility for setting this lies either at the DbiState level
	// (for shared paths) or at the BViewState level (for paths unique to a
	// view.)  In the latter case, if the GObj lod_v doesn't match the
	// DbiPath_Settings lod_v, the view specific object will need its own
	// geometry.
	//
	// TODO - we'll have to see about the complexity, but another option
	// would be to use a linked view's version of an LoD object if the level
	// matches the calculated value for the current view, and have the view
	// make its own local version if the levels don't match (or, if a local
	// LoD obj does exit, re-validate that a shared obj doesn't match and
	// remove the local version in favor of the shared if the view has changed.
	struct bview *lod_v = NULL;
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

	// Methods for handling bv_scene_obj data
	// Mode being negative one for Read means read the settings from the
	// lowest drawing mode present.  For Write, it means override the
	// settings for all modes present in the DbiPath
	void Read(struct bv_obj_settings *s_obj = NULL, int mode = -1);
	void Write(struct bv_obj_settings *s_obj = NULL, int mode = -1);

	// Set up the bv_scene_obj (or objects, if multiple modes are to be
	// drawn for this path) to get ready for drawing.  Finalize the color,
	// matrix, child object (if needed), etc.
	//
	// If we are baking a path specific to an individual view, pass that
	// view in to allow the baking to account for the independent status
	// of the path object.
	void BakeSceneObjs(BViewState *vs = NULL);

	// Return the current scene object (or NULL if there isn't one)
	struct bv_scene_obj *SceneObj(int mode = -1, BViewState *vs = NULL);

	// Draw the objects associated with this path in the specified view.
	//
	// Anything in scene_objs gets drawn, so the parent logic should only
	// add scene objects for the modes it wants drawn for this path.
	//
	// All scene objects associated with this path need to be baked before
	// this call is made to be sure their properties are current.
	void Draw(BViewState *vs);

	// Get the leaf directory pointer of indexed element of the path.
	// Default (-1) returns the leaf dp.  Returns NULL if there is no such
	// directory pointer.
	struct directory *GetDp(long ind = -1);

	// Return true if this path is a parent path of p
	bool parent(DbiPath &p);

	// Return true if this path is a child path of p
	bool child(DbiPath &p);

	// Return true this path references the object represented by hash
	bool uses(unsigned long long hash);

	// Use BRL-CAD rules to determine the active color at the
	// path leaf element
	bool color(struct bu_color *c, int mode = -1);

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
	size_t Depth();

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
	std::unordered_map<BViewState *, std::map<size_t, struct bv_scene_obj *>> scene_objs;

	// Allow BViewState access to the parent_path_hashes set
	friend class BViewState;
	// Allow BSelectState access to the parent_path_hashes set
	friend class BSelectState;

    private:
	// Primary vector storing path information.
	//
	// First entry is always a GObj hash, and any subsequent entries
	// are always CombInst hashes.
	std::vector<unsigned long long> elements;

	// Drawing settings.  In principle, a command might specify
	// different settings for different modes (a color override
	// on a wireframe, for example) so we have to be prepared to
	// track that.
	std::map<size_t, DbiPath_Settings> draw_settings;

	// Path state information
	int is_cyclic = 0;  // 0 = not cyclic, 1 = cyclic, 2 = unknown
	bool is_valid = true;
	std::unordered_set<unsigned long long> component_hashes;
	std::unordered_set<unsigned long long> parent_path_hashes;
	unsigned long long path_hash = 0;
	unsigned long long parent_hash = 0;

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
	bool IsActivated(unsigned long long);
	bool IsActiveParent(unsigned long long);
	bool IsActiveAncestor(unsigned long long);

	// If pattern == NULL, return a sorted vector of all selected paths.
	// Otherwise, return paths glob matching the pattern.
	//
	// Does not look for matches in paths below the active set - i.e.
	// if a path a/b is selected and the pattern is a/b/c, it will not
	// report a match even though the a/b/c path is below a/b and is
	// an active subpath of the selection.
	//
	// Primary purpose of this function is to support code wanting to
	// display the set of selected paths.
	std::vector<std::string> FindSelectedPaths(const char *pattern = NULL);

	void Sync();

	unsigned long long state_hash();

	// Explicitly selected paths.  Note that this is NOT an expanded or
	// collapsed set, since (particularly for editing ops) the app needs to
	// know the precise active paths.
	//
	// Selection of multiple levels of a single path is explicitly
	// prevented by Select, since that invites extremely unintuitive
	// editing behaviors.  If a legitimate use case turns up we can
	// reconsider this limitation, but for now it is expressly prohibited
	// and enforced.
	std::unordered_set<unsigned long long> selected;

	// Children of selected paths.  We need to know about these because
	// they are paths drawing codes may need to illuminate - the selected
	// set is necessary for proper highlighting not sufficient.)
	std::unordered_set<unsigned long long> children;

	// To support highlighting closed paths that have selected primitives
	// below them in a tree hierarchy, we need more information.  This is
	// different than highlighting only the paths related to the specific
	// selected full path - in this situation, the application wants to
	// know about all paths that are above the leaf *object* that is
	// selected, in whatever portion of the database.  Immediate parents
	// are combs whose immediate child is the selected leaf; ancestors are
	// higher level combs above immediate parents
	std::unordered_set<unsigned long long> immediate_parents;
	std::unordered_set<unsigned long long> ancestors;

	// View objects may also be selected
	bool SelectObj(const char *name = NULL);
	bool SelectObj(struct bv_scene_obj *s = NULL);
	bool DeSelectObj(const char *name = NULL);
	bool DeSelectObj(struct bv_scene_obj *s = NULL);
	std::unordered_set<struct bv_scene_obj *> selected_viewobjs;
	std::vector<std::string> FindSelectedObjs(const char *pattern = NULL);

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
	BViewState(DbiState *db = NULL, struct bview *ev = NULL);
	~BViewState();

	// Calculate internal hashes of the current state of the view, display
	// manager and scene object data.  This will sometimes be called by
	// libged routines, but should also be called by application codes
	// when they make view changes to the bview or change dmp settings.
	//
	// (Since those are separate C data structures and can be changed
	// in ways other than BViewState member functions, we cannot always
	// know locally when a hash re-calc is in order.)
	void Hash();

	// Determine based on hashes whether the view data has changed since
	// the last Render() call.
	bool Dirty();

	// Report if there are any active objects (DbiPath or vanilla scene
	// objects) active in the view.  This will include any paths or
	// objects active due to views connected to this one by Link().
	bool Empty();

	// Trigger an autoview on the bview using currently defined
	// objects.
	void Autoview(fastf_t factor = 0.0, bool dbipaths = true, bool view_only = true);

	// Provide a C-style array of all bv_scene_obj entities active
	// as either DbiPath leaves or explicit view objects.  Optional
	// values may be used to disable DbiPath or view-only objs - default
	// is to return everything.
	//
	// Linked view's objects will also be included.
	//
	// bu_ptbl is internally managed by BViewState.
	const struct bu_ptbl * FindSceneObjsPtbl(const char *pattern = NULL, bool dbipaths = true, bool view_only = true);
	// As above, but return a std::vector
	std::vector<struct bv_scene_obj *> FindSceneObjs(const char *pattern = NULL, bool dbipaths = true, bool view_only = true);

	// Allow a view state to specify a "linked" view state where it will
	// source data in addition to its own.
	//
	// Paths and Objects added to this view after linking will not be
	// visible in the ls target view - the link is one way.
	//
	// The goal is to avoid duplicate work managing multiple draw/erase
	// lists unless we actually have independent views that must do that
	// work, but allow for the possibility of completely independent view
	// contents.
	//
	// Conceptually each BViewState can be thought of as maintaining a
	// notion of a scene definition, in addition to the bview camera info.
	// By default each is separate, but when linked the client view will
	// leverage the target linked view's info rather than having to repeat
	// work for itself (typically additional views will link to the default
	// view in most usage scenarios, such as quad view.)
	//
	// By default Link and Unlink will establish links for DbiPath
	// objects - to establish a link for view_objs supply the additional
	// "true" parameter.  So to completely link the current view to the
	// default view, both for DbiPaths and view-only scene objects, the
	// calls would look like:
	//
	// v->Link(dbis->default_view);        // DbiPaths
	// v->Link(dbis->default_view, true);  // View objects
	//
	// Similarly, to fully decouple the views:
	//
	// v->UnLink(dbis->default_view);        // DbiPaths
	// v->UnLink(dbis->default_view, true);  // View objects
	//
	// Note that objects being incorporated into this view via Link from
	// another view will be drawn using the original view's settings to
	// determine LoD geometries, NOT the current view.
	//
	// If the Link target is the default view, the default view's logic
	// will try to take into account all active views and pick the highest
	// level of detail needed for any individual view.  This may mean some
	// views will draw some objects at a higher LoD than strictly needed
	// for that individual view, but avoids data duplication overall.
	//
	// If the Link target is a view other than the default view, the LoD
	// will be based on whatever the other view is using and NOT be
	// adjusted for the current view.  In that situation, the rendering in
	// this view may be coarser than the LoD settings call for.
	//
	// If the application needs to always have LoD governed by the current
	// view regardless of other view states, it should avoid linking views
	// and populate the paths independently for each view.  This will mean
	// data duplication and consume more memory, but is the only way to
	// ensure the scene objects in multiple views are displayed at the LoD
	// dictated by the local view settings.
	bool Link(BViewState *ls = NULL, bool view_objs = false);
	bool UnLink(BViewState *ls = NULL, bool view_objs = false);

	// Return the name of this view
	std::string Name();

	// Adds path to the BViewState container drawn set, but doesn't trigger
	// a re-draw - that should be done once all paths to be added in a
	// given draw cycle are queued up.  The actual drawing (and setting
	// specifications) are done with Redraw and a supplied bv_obj_settings
	// structure.  By default path is assigned a wireframe drawing mode,
	// but other modes may be specified.
	//
	// Our mode argument is deliberately unsigned here, because we do not
	// want to allow adding all modes at once (the logical interpretation
	// of a -1 argument for mode.
	void AddPath(const char *path = NULL, unsigned int mode = 0, struct bv_obj_settings *vs = NULL);
	void AddPath(DbiPath *p = NULL, unsigned int mode = 0, struct bv_obj_settings *vs = NULL);
	void AddPath(unsigned long long phash = 0, unsigned int mode = 0, struct bv_obj_settings *vs = NULL);

	// Erases paths from the view for the given mode.  If mode < 0, all
	// matching paths are erased.  For modes that are un-evaluated, all
	// paths that are subsets of the specified path are removed.  For
	// evaluated modes like 3 (bigE) that generate an evaluated visual
	// specific to that path, only precise path matches are removed.
	//
	// Like AddPath, this stages paths for subsequent processing but does
	// not actually trigger a redraw.
	void RemovePath(const char *p, int mode = -1, bool rebuild = true);
	void RemovePath(DbiPath *p, int mode = -1, bool rebuild = true);
	void RemovePath(unsigned long long phash = 0, int mode = -1, bool rebuild = true);

	// An application may add and remove arbitrary scene objects to a
	// scene, not just geometry specified DbiPaths.  These objects may be
	// either part of the common 3D scene shown by all views, or specific
	// to this particular view.  A scene object cannot be added if there
	// is already an object with the name s->name present in the scene.
	// Correspondingly, RemoveObj wil return failure if it cannot find
	// the object to remove.  AddObj and RemoveObj will not operate on
	// DbiPath based scene objects, which are handled separately.
	bool AddObj(struct bv_scene_obj *s);
	bool RemoveObj(struct bv_scene_obj *s, bool free_obj = true);
	bool RemoveObjs(const char *pattern, bool free_objs = true);

	// Return a sorted vector of path strings corresponding to the drawn
	// paths in the view.  If mode == -1 list all paths, otherwise list
	// those specific to the mode.
	std::vector<std::string> DrawnPaths(int mode = -1);

	// Get a count of the drawn paths.  If explicitly is true the count
	// reflects the explicitly drawn paths, otherwise all leaf scene
	// objects are counted.  mode == -1 means count all modes, otherwise
	// count only the specified mode.
	size_t DrawnPathCount(bool explicitly = true, int mode = -1);

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

	// Erase specified objects from the scene.  By default these are
	// path specifiers, but if process_scene_objs = true non-geometry view
	// objects will be checked instead.
	void Erase(int mode = -1, bool process_scene_objs = false, int argc = 0, const char **av = NULL);

	// A render does not update drawn object geometry - it just triggers a
	// redraw of the scene.  If there is any chance the database has
	// changed, a dbis->Sync() should be called (which will trigger a
	// Redraw) to make sure the scene objects are current before
	// Render is called again.
	void Render();

	// A redraw validates, updates (and - if necessary - rebuilds)
	// drawn DbiPaths and their corresponding scene objects.  If a draw
	// command has added new paths to the scene, this is the command
	// that will make sure the associated scene object geometry is
	// correct.
	//
	// If autoview is true, and the internal empty flag has been set
	// previously by Empty(), Redraw() will assume the view is being
	// populated for the first time and will attempt to initialize it
	// based on the currently available geometry.
	//
	// Note that a Redraw will not automatically trigger Render calls
	// for the views - whether to Render immediately or wait for
	// further processing is up to the calling code.
	void Redraw(bool autoview = true);


	// TODO - add update callback function that parent code can use
	// to take action after a ReDraw, Add*, Remove*, etc.  Generally
	// that will probably constitute calling Render(), since after any
	// modding action the Render may be out of date.
	// bu_clbk_t update_clbk;

	// Debugging methods for printing out current states - the use of hashes
	// means direct inspection of most data isn't informative, so we provide
	// convenience methods that decode it to user-comprehensible info.
	std::string print_view_state();

	friend class BSelectState;
	friend class DbiPath;
	friend class DbiState;
	friend class GObj;

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
	std::unordered_map<size_t, std::unordered_set<unsigned long long>> s_expanded;

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

	// Set of user provided scene objects specific to this view
	std::unordered_set<struct bv_scene_obj *> scene_objs;

	DbiState *dbis;

	// Linked state (if any) from which to draw DbiPaths
	BViewState *l_dbipath = NULL;
	// Linked state (if any) from which to draw View Objects
	BViewState *l_viewobj = NULL;

	struct bview *v;
	bool local_v;

	// We start with an empty view
	bool view_empty = true;

	// Container to hold scene objects when
	// exposing via FindSceneObjsPtbl
	struct bu_ptbl eobjs = BU_PTBL_INIT_ZERO;

	// State hashes used for Dirty comparison
	unsigned long long view_hash = 0;
	unsigned long long dmp_hash = 0;
	unsigned long long objs_hash = 0;
	unsigned long long old_view_hash = 0;
	unsigned long long old_dmp_hash = 0;
	unsigned long long old_objs_hash = 0;
};

#define GED_DBISTATE_DB_CHANGE   0x01
#define GED_DBISTATE_VIEW_CHANGE 0x02

class GED_EXPORT DbiState {
    public:
	DbiState(struct ged *);
	~DbiState();

	// Database Instance associated with this container
	struct ged *gedp = NULL;

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
	// or (if necessary) create a new one.  For arguments with a path
	// or hash, will return the registered version if one is already
	// present or create a registered version if one is not.  If a
	// supplied hash or path are invalid NULL is returned.
	DbiPath *GetDbiPath();
	DbiPath *GetDbiPath(const char *path);
	DbiPath *GetDbiPath(unsigned long long hash);

	// Clear a DbiPath and store it for later reuse.
	void PutDbiPath(DbiPath *p);

	// Once add and remove operations are complete for all views in a
	// particular processing cycle, the DbiPath containers need to be
	// finalized for drawing purposes.  This process is referred to as
	// "baking" the DbiPaths.  It includes things like setting color and
	// matrix information on the DbiPath's associated scene object.  Once
	// paths are baked (assuming no database state changes, which require a
	// Sync() call) the DbiPaths are ready to Render().
	void BakePaths();

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

	// A Sync updates the internal DbiState.  A Sync should be performed
	// after any database changes.  The update process can be made more
	// efficient by user-supplied information in the added, changed and
	// removed callback sets - if none of those are pre-populated, the only
	// option is to do a full rebuild of the DbiState from the .g file.
	// By default of all of the callback sets are empty Sync will assume
	// there is no work to do - to make it do the full repopulation
	// force can be set to true.
	//
	// A fully executed Sync will also call Redraw on any views in the
	// DbiState, since there is a good chance that database changes have
	// invalidated some aspect of the drawn views.
	void Sync(bool force = false);

	// Data from librt callbacks
	std::unordered_set<struct directory *> added;
	std::unordered_set<struct directory *> changed;
	std::unordered_set<struct directory *> removed;

	/*******************************************************************
         *                      View States
	 *******************************************************************/
	// If v is NULL, a new bview will be created associated with and managed
	// by the BViewState.  If v != NULL, and no pre-existing state is found,
	// return a new state - otherwise return the found pre-existing state.
	BViewState *AddBViewState(struct bview *v = NULL);

	// If v == NULL, return the default_view state.  Otherwise, either
	// return the BViewState associated with v or NULL (if no such state
	// exists.)
	BViewState *GetBViewState(struct bview *v = NULL);

	// Look up a view by matching a pattern.  Does not create a view if one
	// is not found - just returns empty vector.
	std::vector<BViewState *> FindBViewState(const char *pattern = NULL);

	// Record in all active views whether the view is empty.  This is used
	// at the beginning of a draw cycle to allow the views to know whether
	// or not they need to trigger an autoview alignment.  By default they
	// will if they need to, although the caller may disable this when they
	// call Redraw().
	void FlagEmpty();

	// Call Redraw() for v and all views linked to v.  Generally used after
	// a command line command.  By default (i.e. no specific view argument)
	// will trigger a Redraw on the default view and all views linked to
	// the default view.
	//
	// This method is defined in DbiState because actions in one view may
	// frequently have implications for other views linked to that view -
	// and a database state change can also impact multiple views.  This
	// method is intended to save client codes the trouble of bookkeeping
	// when it comes to making sure the various views get updated.
	void Redraw(BViewState *v = NULL, bool autoview = true);

	// Call Render() for v and all views linked to v.  Generally used after
	// a command line command.  By default (i.e. no specific view argument)
	// will trigger a Render on the default view and all views linked to
	// the default view.
	void Render(BViewState *v = NULL);

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
	void RemoveSelectionSet(BSelectState *s = NULL);
	BSelectState * AddSelectionSet(const char *sname = NULL);
	void RemoveSelectionSets(const char *pattern = NULL);
	std::vector<std::string> FindSelectionSets(const char *pattern = NULL);

	/*******************************************************************
         *                       Scene Data
	 *******************************************************************/
	// The combination of the dbi_paths and scene_objs sets constitute
	// the primary 3D "scene" description for a GED application.  The
	// former handle scene objects specific to each geometry instance
	// being shown, and the latter handle arbitrary additional elements
	// added to the scene (common examples would be rtcheck overlap
	// lines and nirt raytracing lines, but any valid scene object
	// will be rendered so applications may wish to add their own
	// data as well.)

	// The dbi_paths set is the primary means of decode hashes into DbiPath
	// instances.  (If a path hash doesn't have an entry here, the hash is
	// not usable in the majority of DbiState and related logic - hashs
	// without DbiPaths are only useful for jobs like checking whether
	// something is selected.)  There is no requirement that a path have
	// exactly one DbiPath instance, but dbi_paths can hold only one per
	// hash so if application code creates additional copies the management
	// of those other instances is their responsibility.
	//
	// If a DbiPath is registered here (it does not happen by *default* on
	// any DbiPath creation or change, but various code paths will do it)
	// altering or deleting the DbiPath instance will automatically
	// de-register that DbiPath from dbi_paths.
	//
	// Path alterations to an unregistered DbiPath that happens to match
	// a hash in dbi_paths will have no effect on dbi_paths.
	std::unordered_map<unsigned long long, DbiPath *> dbi_paths;

	// Container holding all pre-allocated DbiPath instances available
	// for reuse.
	std::queue<DbiPath *> dbiq;

	// One of the ".g ground truth" maps to the database objects.  This
	// one (unlike gobjs and combinsts) is public so it can be updated
	// by garbage_collect without necessitating blowing away the entire
	// DbiState and rebuilding everything.
	std::unordered_map<struct directory *, GObj *> dp2g;

    public:
	// Allow the implementation containers to access the core DbiState maps
	friend class GObj;
	friend class CombInst;
	friend class DbiPath;
	friend class BViewState;

    private:
	// These maps are the ".g ground truth" of the database objects
	std::unordered_map<unsigned long long, GObj *> gobjs;
	std::unordered_map<unsigned long long, CombInst *> combinsts;

	// Private methods and resources relating to database objects
	std::vector<GObj *> get_gobjs(std::vector<unsigned long long> &path);
	std::vector<CombInst *> get_combinsts(std::vector<unsigned long long> &path);
	void clear_cache(struct directory *dp);
	struct resource *res = NULL;
	struct bu_cache *dcache = NULL;
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
	BViewState *default_view;
	std::unordered_map<struct bview *, BViewState *> view_states;

	// Data related to Selections
	BSelectState *d_selection = NULL;
	std::unordered_map<std::string, BSelectState *> selected_sets;
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

/* DbiState is basically a C++ API, but there are a few cases where we
 * may want to interface with some of its capabilities in C.  Define
 * some functions for that purpose. */

GED_EXPORT extern const struct bu_ptbl *
ged_find_scene_objs(struct ged *gedp, struct bview *v, const char *pattern, int dbipaths, int view_only);

GED_EXPORT struct bv_scene_obj *
ged_vlblock_scene_obj(struct ged *gedp, struct bview *v, const char *name, struct bv_vlblock *vbp);


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
