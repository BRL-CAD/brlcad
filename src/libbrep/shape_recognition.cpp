#include "common.h"

#include <set>
#include <map>
#include <string>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L1_OFFSET 2
#define L2_OFFSET 4

#define WRITE_ISLAND_BREPS 1

// TODO - the topological test by itself is not guaranteed to isolate volumes that
// are uniquely positive or uniquely negative contributions to the overall volume.
// It may be that a candidate subbrep has both positive and
// negative contributions to make to the overall volume.  Hopefully (not sure yet)
// this approach will reduce such situations to planar and general surface volumes,
// but we will need to handle them at those levels.  For example,
//
//                                       *
//                                      * *
//         ---------------------       *   *       ---------------------
//         |                    *     *     *     *                    |
//         |                     * * *       * * *                     |
//         |                                                           |
//         |                                                           |
//         -------------------------------------------------------------
//
// The two faces in the middle contribute both positively and negatively to the
// final surface area, and if this area were extruded would also define both
// positive and negative volume.  Probably convex/concave testing is what
// will be needed to resolve this.

#define TOO_MANY_CSG_OBJS(_obj_cnt, _msgs) \
    if (_obj_cnt > CSG_BREP_MAX_OBJS && _msgs) \
	bu_vls_printf(_msgs, "Error - brep converted to more than %d implicits - not currently a good CSG candidate\n", obj_cnt - 1); \
    if (_obj_cnt > CSG_BREP_MAX_OBJS) \
	goto bail


void
get_face_set_from_loops(struct subbrep_island_data *sb)
{
    const ON_Brep *brep = sb->brep;
    std::set<int> faces;
    std::set<int> fol;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->loops, sb->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	bool is_outer = (face->OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	faces.insert(face->m_face_index);
	if (is_outer) {
	    fol.insert(face->m_face_index);
	} else {
	    fil.insert(face->m_face_index);
	}
    }

    // Pack up results
    set_to_array(&(sb->faces), &(sb->faces_cnt), &faces);
    set_to_array(&(sb->fol), &(sb->fol_cnt), &fol);
    set_to_array(&(sb->fil), &(sb->fil_cnt), &fil);
}

void
get_edge_set_from_loops(struct subbrep_island_data *sb)
{
    const ON_Brep *brep = sb->brep;
    std::set<int> edges;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->loops, sb->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
	    }
	}
    }

    // Pack up results
    set_to_array(&(sb->edges), &(sb->edges_cnt), &edges);
}

/* This is the critical point at which we take a pile of shapes and actually reconstruct a
 * valid boolean tree.  The stages are as follows:
 *
 * 1.  Identify the top level union objects (they will have no fil faces in their structure).
 * 2.  Using the fil faces, build up a queue of subbreps that are topologically connected
 *     by a loop to the top level union objects.
 * 3.  Determine the boolean status of the 2nd layer of objects.
 * 4.  For each sub-object, if that object in turn has fil faces, queue up those objects and goto 3.
 * 5.  Continue iterating until all objects have a boolean operation assigned.
 * 6.  For each unioned object, build a set of all subtraction objects (topologically connected or not.)
 * 7.  add the union pointer to the subbreps_tree, and then add all all topologically connected subtractions
 *     and remove them from the local set.
 * 8.  For the remaining (non-topologically linked) subtractions, get a bounding box and see if it overlaps
 *     with the bounding box of the union object in question.  If yes, we have to stash it for later
 *     evaluation - only solid raytracing will suffice to properly resolve complex cases.
 *     If not, no action is needed.  Once evaluated, remove the subtraction pointer from the set.
 *
 * Initially the test will be axis aligned bounding boxes, but ideally we should use oriented bounding boxes
 * or even tighter tests.  There's a catch here - a positive object overall may be within the bounding box
 * of a subtracting object (like a positive cone at the bottom of a negative cylinder).  First guess at a rule - subtractions
 * may propagate back up the topological tree to unions above the subtraction or in other isolated topological
 * trees, but never to unions below the subtraction
 * object itself.  Which means we'll need an above/below test for union objects relative to a given
 * subtraction object, or an "ignore" list for unions which overrides any bbox tests - that list would include the
 * parent subtraction, that subtraction's parent if it is a subtraction, and so on up to the top level union.  Nested
 * unions and subtractions mean we'll have to go all the way up that particular chain...
 */
void
find_hierarchy(struct bu_vls *UNUSED(msgs), struct subbrep_tree_node *node, struct bu_ptbl *islands)
{
    int depth = 0;
    if (!node) return;

    std::map<int, long*> fol_to_i;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	for (int j = 0; j < id->fol_cnt; j++) {
	    fol_to_i.insert(std::make_pair(id->fol[j], (long *)id));
	}
    }

    /* Separate out top level unions */
    if (node->parent == NULL) {
	int top_cnt = 0;
	for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	    struct subbrep_island_data *obj = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	    if (obj->fil_cnt == 0) {
		std::cout << "Top union found: " << bu_vls_addr(obj->key) << "\n";
		top_cnt++;
		node->island = obj;
	    }
	}
	if (top_cnt != 1) {
	    bu_log("Error - incorrect toplevel union count: %d\n", top_cnt);
	    return;
	}
    } else {
	struct subbrep_tree_node *n = node->parent;
	while (n) {
	    depth++;
	    n = n->parent;
	}
    }


    std::set<long *> subislands;
    std::set<long *>::iterator s_it;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	if (BU_STR_EQUAL(bu_vls_addr(id->key), bu_vls_addr(node->island->key))) continue;
	bu_log("%*schecking island %s\n", depth, " ", bu_vls_addr(id->key));
	// For this node's inner loops, find the corresponding islands.
	for (int j = 0; j < id->fil_cnt; j++) {
	    struct subbrep_island_data *idparent = (struct subbrep_island_data *)fol_to_i.find(id->fil[j])->second;
	    if (BU_STR_EQUAL(bu_vls_addr(node->island->key), bu_vls_addr(idparent->key))) {
		subislands.insert((long *)id);
	    }
	}
    }
    for (s_it = subislands.begin(); s_it != subislands.end(); s_it++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)(*s_it);
	struct subbrep_tree_node *n = NULL;
	BU_GET(n, struct subbrep_tree_node);
	subbrep_tree_init(n);
	n->island = id;
	n->parent = node;
	bu_ptbl_ins(node->children, (long *)n);

	if (id->nucleus->params->bool_op == '-') {
	    bu_log("%*sadding subtraction %s\n", depth, " ", bu_vls_addr(id->key));
	    bu_ptbl_ins_unique(node->island->subtractions, (long *)id);
	    // A subtraction must also be checked against positive parent islands.
	    struct subbrep_tree_node *pn = node->parent;
	    while (pn) {
		if (pn->island->nucleus->params->bool_op == 'u') {
		    bu_log("check bboxes\n");
		    ON_BoundingBox isect;
		    bool bbi = isect.Intersection(*pn->island->bbox, *id->bbox);
		    // If we have overlap, we have a possible subtraction operation
		    // between this island and the parent.  It's not guaranteed that
		    // there is an interaction, but to make sure will require ray testing
		    // so for speed we accept that there may be extra subtractions in
		    // the tree.
		    if (bbi) {
			bu_log("found one: %s!\n", bu_vls_addr(id->key));
			bu_ptbl_ins_unique(pn->island->subtractions, (long *)id);
		    }
		}
		pn = pn->parent;
	    }
	}
    }

    for (unsigned int i = 0; i < BU_PTBL_LEN(node->children); i++) {
	struct subbrep_tree_node *n = (struct subbrep_tree_node *)BU_PTBL_GET(node->children, i);
	(void)find_hierarchy(NULL, n, islands);
    }
}


int cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cylinder corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCylinder(&corig);
    delete csorig;

    ON_Cylinder ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCylinder(&ccand);
    delete cscand;

    // Make sure the surfaces are on the same cylinder
    if (corig.circle.Center().DistanceTo(ccand.circle.Center()) > BREP_CYLINDRICAL_TOL) return 0;

    // Make sure the radii are the same
    if (!NEAR_ZERO(corig.circle.Radius() - ccand.circle.Radius(), VUNITIZE_TOL)) return 0;

    return 1;
}


int
shoal_filter_loop(int control_loop, int candidate_loop, struct subbrep_island_data *data)
{
    const ON_Brep *brep = data->brep;
    surface_t *face_surface_types = (surface_t *)data->face_surface_types;
    const ON_BrepFace *forig = brep->m_L[control_loop].Face();
    const ON_BrepFace *fcand = brep->m_L[candidate_loop].Face();
    surface_t otype  = face_surface_types[forig->m_face_index];
    surface_t ctype  = face_surface_types[fcand->m_face_index];

    switch (otype) {
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    if (ctype != SURFACE_CYLINDRICAL_SECTION && ctype != SURFACE_CYLINDER) return 0;
	    if (!cyl_validate_face(forig, fcand)) return 0;
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    if (ctype != SURFACE_SPHERICAL_SECTION && ctype != SURFACE_SPHERE) return 0;
	    break;
	default:
	    if (otype != ctype) return 0;
	    break;
    }

    return 1;
}

int
shoal_build(int **s_loops, int loop_index, struct subbrep_island_data *data)
{
    int s_loops_cnt = 0;
    const ON_Brep *brep = data->brep;
    std::set<int> processed_loops;
    std::set<int> shoal_loops;
    std::queue<int> todo;

    shoal_loops.insert(loop_index);
    todo.push(loop_index);

    while(!todo.empty()) {
	int lc = todo.front();
	const ON_BrepLoop *loop = &(brep->m_L[lc]);
	processed_loops.insert(lc);
	todo.pop();
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge) {
		for (int j = 0; j < edge->m_ti.Count(); j++) {
		    const ON_BrepTrim *t = &(brep->m_T[edge->m_ti[j]]);
		    int li = t->Loop()->m_loop_index;
		    if (processed_loops.find(li) == processed_loops.end()) {
			if (shoal_filter_loop(lc, li, data)) {
			    shoal_loops.insert(li);
			    todo.push(li);
			} else {
			    processed_loops.insert(li);
			}
		    }
		}
	    }
	}
    }

    set_to_array(s_loops, &(s_loops_cnt), &shoal_loops);
    return s_loops_cnt;
}



/* In order to represent complex shapes, it is necessary to identify
 * subsets of subbreps that can be represented as primitives.  This
 * function will identify such subsets, if possible.  If a subbrep
 * can be successfully decomposed into primitive candidates, the
 * type of the subbrep is set to COMB and the children bu_ptbl is
 * populated with subbrep_island_data sets. */
int
subbrep_split(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    bu_log("splitting %s\n", bu_vls_addr(data->key));
    int csg_fail = 0;
    std::set<int> loops;
    std::set<int> active;
    std::set<int> fully_planar;
    std::set<int> partly_planar;
    std::set<int>::iterator l_it, e_it;

    array_to_set(&loops, data->loops, data->loops_cnt);
    array_to_set(&active, data->loops, data->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(data->brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	surface_t surface_type = ((surface_t *)data->face_surface_types)[face->m_face_index];
	// Characterize the planar faces. TODO - see if we actually need this or not...
	if (surface_type != SURFACE_PLANE) {
	    // non-planar face - check to see whether it's already part of a shoal.  If not,
	    // create a new one - otherwise, skip.
	    if (active.find(loop->m_loop_index) != active.end()) {
		std::set<int> shoal_loops;
		struct subbrep_shoal_data *sh;
		BU_GET(sh, struct subbrep_shoal_data);
		subbrep_shoal_init(sh, data);
		(*(data->obj_cnt))++;
		sh->params->id = (*(data->obj_cnt));
		sh->i = data;
		sh->loops_cnt = shoal_build(&(sh->loops), loop->m_loop_index, data);
		for (int i = 0; i < sh->loops_cnt; i++) {
		    active.erase(sh->loops[i]);
		}
		int local_fail = 0;
		switch (surface_type) {
		    case SURFACE_CYLINDRICAL_SECTION:
		    case SURFACE_CYLINDER:
			if (!cylinder_csg(msgs, sh, BREP_CYLINDRICAL_TOL)) local_fail++;
			break;
		    case SURFACE_CONE:
			local_fail++;
			break;
		    case SURFACE_SPHERICAL_SECTION:
		    case SURFACE_SPHERE:
			local_fail++;
			break;
		    case SURFACE_TORUS:
			local_fail++;
			break;
		    default:
			break;
		}
		if (!local_fail) {
		    bu_ptbl_ins(data->children, (long *)sh);
		} else {
		    csg_fail++;
		}
	    }
	}
    }

    if (csg_fail > 0) {
	/* Clear children - we found something we can't handle, so there will be no
	 * CSG conversion of this subbrep. Cleanup is handled one level up. */
	return 0;
    }
    // We had a successful conversion - now generate a nucleus.  Need to
    // characterize the nucleus as positive or negative volume.  Can also
    // be degenerate if all planar faces are coplanar or if there are no
    // planar faces at all (perfect cylinder, for example) so will need
    // rules for those situations...
    if (!island_nucleus(msgs, data)) return 0;

    // If we've got a single nucleus shape and no children, set the island
    // type to be that of the nucleus type.  Otherwise, it's a comb.
    if (BU_PTBL_LEN(data->children) == 0) {
	data->type = data->nucleus->params->type;
    } else {
	data->type = COMB;
    }

    // Note is is possible to have degenerate *edges*, not just vertices -
    // if a shoal shares part of an outer loop (possible when connected
    // only by a vertex - see example) then the edges in the parent loop
    // that are part of that shoal are degenerate for the purposes of the
    // "parent" shape.  May have to limit our inputs to
    // non-self-intersecting loops for now...
    return 1;
}

struct bu_ptbl *
find_subbreps(struct bu_vls *msgs, const ON_Brep *brep)
{
    /* Number of successful conversion operations */
    int successes = 0;
    /* Overall object counter */
    int obj_cnt = 0;
    /* Return node */
    struct subbrep_tree_node *root = NULL;
    /* Container to hold island data structures */
    struct bu_ptbl *subbreps;
    std::set<struct subbrep_island_data *> islands;
    std::set<struct subbrep_island_data *>::iterator is_it;
    struct bu_ptbl *final_brep_array;

    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    // Characterize all face surface types once
    std::map<int, surface_t> fstypes;
    surface_t *face_surface_types = (surface_t *)bu_calloc(brep->m_F.Count(), sizeof(surface_t), "surface type array");
    for (int i = 0; i < brep->m_F.Count(); i++) {
	face_surface_types[i] = GetSurfaceType(brep->m_F[i].SurfaceOf());
    }

    // Use the loops to identify subbreps
    std::set<int> brep_loops;
    /* Build a set of loops.  All loops will be assigned to a subbrep */
    for (int i = 0; i < brep->m_L.Count(); i++) {
	brep_loops.insert(i);
    }

    while (!brep_loops.empty()) {
	std::set<int> loops;
	std::queue<int> todo;

	/* For each iteration, we have a new island */
	struct subbrep_island_data *sb = NULL;
	BU_GET(sb, struct subbrep_island_data);
	subbrep_island_init(sb, brep);

	// Seed the queue with the first loop in the set
	std::set<int>::iterator top = brep_loops.begin();
	todo.push(*top);

	// Process the queue
	while(!todo.empty()) {
	    const ON_BrepLoop *loop = &(brep->m_L[todo.front()]);
	    loops.insert(todo.front());
	    brep_loops.erase(todo.front());
	    todo.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		    for (int j = 0; j < edge->TrimCount(); j++) {
			int li = edge->Trim(j)->Loop()->m_loop_index;
			if (loops.find(li) == loops.end()) {
			    todo.push(li);
			    loops.insert(li);
			    brep_loops.erase(li);
			}
		    }
		}
	    }
	}

	// If our object count is growing beyond reason, bail
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);

	// Pass the counter pointer through in case sub-objects need
	// to generate IDs as well.
	sb->obj_cnt = &obj_cnt;

	// Pass along shared data
	sb->brep = brep;
	sb->face_surface_types = (void *)face_surface_types;

	// Assign loop set to island
	set_to_array(&(sb->loops), &(sb->loops_cnt), &loops);

	// Assemble the set of faces
	get_face_set_from_loops(sb);

	// Assemble the set of edges
	get_edge_set_from_loops(sb);

	// Get key based on face indicies
	std::set<int> faces;
	array_to_set(&faces, sb->faces, sb->faces_cnt);
	std::string key = set_key(faces);
	bu_vls_sprintf(sb->key, "%s", key.c_str());

	// Check to see if we have a general surface that precludes conversion
	surface_t hof = subbrep_highest_order_face(sb);
	if (hof >= SURFACE_GENERAL) {
	    if (msgs) bu_vls_printf(msgs, "Note - general surface present in island %s, representing as B-Rep\n", bu_vls_addr(sb->key));
	    sb->type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	    bu_ptbl_ins(subbreps, (long *)sb);
	    continue;
	}

	int split = subbrep_split(msgs, sb);
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);
	if (!split) {
	    if (msgs) bu_vls_printf(msgs, "Note - split of %s unsuccessful, making brep\n", bu_vls_addr(sb->key));
	    sb->type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	} else {
	    successes++;
#if WRITE_ISLAND_BREPS
	    (void)subbrep_make_brep(msgs, sb);
#endif
	}


	bu_ptbl_ins(subbreps, (long *)sb);
    }

    /* If we didn't do anything to simplify the shape, we're stuck with the original */
    if (!successes) {
	if (msgs) bu_vls_printf(msgs, "%*sNote - no successful simplifications\n", L1_OFFSET, " ");
	goto bail;
    }

    /* Build the bounding boxes for all islands once */
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	struct subbrep_island_data *island = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	subbrep_bbox(island);
    }

    bu_log("find us some hierarchy...\n");
    BU_GET(root, struct subbrep_tree_node);
    subbrep_tree_init(root);
    find_hierarchy(NULL, root, subbreps);
    if (!root->island) {
	bu_log("no hierarchy found ??...\n");
	return NULL;
    }

    // Assign island IDs, populate set
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	struct subbrep_island_data *si = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	si->id = (si != root->island) ? si->id = i + 1 : 0;
	islands.insert(si);
    }

    // Build the final subbrep array so the root node is the first entry in the array
    // TODO: Probably a more efficient way to do all this, if it even matters...
    BU_GET(final_brep_array, struct bu_ptbl);
    bu_ptbl_init(final_brep_array, 8, "init array");
    // Start with the root
    bu_ptbl_ins(final_brep_array, (long *)root->island);
    islands.erase(root->island);
    for (is_it = islands.begin(); is_it != islands.end(); is_it++) {
	bu_ptbl_ins(final_brep_array, (long *)*is_it);
	islands.erase(is_it);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);

    return final_brep_array;

bail:
    // Free memory
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_island_data *obj = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	subbrep_island_free(obj);
	BU_PUT(obj, struct subbrep_island_data);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);
    return NULL;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
