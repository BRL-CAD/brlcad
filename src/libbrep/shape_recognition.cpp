#include "common.h"

#include <set>
#include <map>
#include <string>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"


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

struct bu_ptbl *
find_subbreps(const ON_Brep *brep)
{
    struct bu_ptbl *subbreps;
    std::set<std::string> subbrep_keys;
    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    int obj_cnt = 0;

    /* For each face, build the topologically connected subset.  If that
     * subset has not already been seen, add it to the brep's set of
     * subbreps */
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::string key;
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::set<int> fol; /* Faces with outer loops in object loop network */
	std::set<int> fil; /* Faces with only inner loops in object loop network */
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	std::set<int>::iterator s_it;

	// TODO - need to watch for an edge that has two general surfaces associated
	// with it.  That situation is too problematic for us to handle even as a
	// breakdown into subbreps, and if we see it we need to bail.  Ensuring reliable
	// inside/outside testing under those conditions is extremely difficult.
	// Maybe even something to do as an initial test.
	const ON_BrepFace *face = &(brep->m_F[i]);
	faces.insert(i);
	fol.insert(i);
	local_loops.push(face->OuterLoop()->m_loop_index);
	processed_loops.insert(face->OuterLoop()->m_loop_index);
	while(!local_loops.empty()) {
	    const ON_BrepLoop *loop = &(brep->m_L[local_loops.front()]);
	    loops.insert(local_loops.front());
	    local_loops.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		const ON_BrepTrim *trim = &(face->Brep()->m_T[loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(face->Brep()->m_E[trim->m_ei]);
		if (trim->m_ei != -1 && edge->TrimCount() > 1) {
		    edges.insert(trim->m_ei);
		    for (int j = 0; j < edge->TrimCount(); j++) {
			int fio = edge->Trim(j)->FaceIndexOf();
			if (edge->m_ti[j] != ti && fio != -1) {
			    int li = edge->Trim(j)->Loop()->m_loop_index;
			    faces.insert(fio);
			    if (processed_loops.find(li) == processed_loops.end()) {
				local_loops.push(li);
				processed_loops.insert(li);
			    }
			    if (li == brep->m_F[fio].OuterLoop()->m_loop_index) {
				fol.insert(fio);
			    }
			}
		    }
		}
	    }
	}
	for (s_it = faces.begin(); s_it != faces.end(); s_it++) {
	    if (fol.find(*s_it) == fol.end()) {
		fil.insert(*s_it);
	    }
	}
	key = face_set_key(faces);

	/* If we haven't seen this particular subset before, add it */
	if (subbrep_keys.find(key) == subbrep_keys.end()) {
	    subbrep_keys.insert(key);
	    struct subbrep_object_data *new_obj = NULL;
	    BU_GET(new_obj, struct subbrep_object_data);
	    subbrep_object_init(new_obj, brep);
	    new_obj->obj_cnt = &obj_cnt;
	    obj_cnt++;
	    bu_vls_sprintf(new_obj->key, "%s", key.c_str());
	    bu_vls_sprintf(new_obj->name_root, "%d", obj_cnt);
	    set_to_array(&(new_obj->faces), &(new_obj->faces_cnt), &faces);
	    set_to_array(&(new_obj->loops), &(new_obj->loops_cnt), &loops);
	    set_to_array(&(new_obj->edges), &(new_obj->edges_cnt), &edges);
	    set_to_array(&(new_obj->fol), &(new_obj->fol_cnt), &fol);
	    set_to_array(&(new_obj->fil), &(new_obj->fil_cnt), &fil);
	    // Here and only here, we are isolating topological "islands"
	    // Below this level, everything will be connected in some fashion
	    // by the edge network.
	    new_obj->is_island = 1;
	    new_obj->parent = NULL;
	    surface_t hof = highest_order_face(new_obj);
	    if (hof >= SURFACE_GENERAL) {
		new_obj->type = BREP;
		(void)subbrep_make_brep(new_obj);
		bu_log("general surface present: %s\n", bu_vls_addr(new_obj->key));
	    } else {
		int split = 0;
		volume_t vtype = subbrep_shape_recognize(new_obj);
		switch (vtype) {
		    case BREP:
			split = subbrep_split(new_obj);
			if (!split) {
			    (void)subbrep_make_brep(new_obj);
			    bu_log("split unsuccessful: %s\n", bu_vls_addr(new_obj->key));
			} else {
			    // If we did successfully split the brep, do some post-split
			    // clean-up
			    new_obj->type = COMB;
			    if (new_obj->planar_obj) {
				subbrep_planar_close_obj(new_obj);
			    }
			}
			break;
		    case PLANAR_VOLUME:
			subbrep_planar_init(new_obj);
			subbrep_planar_close_obj(new_obj);
			new_obj->local_brep = new_obj->planar_obj->local_brep;
			break;
		    default:
			break;
		}
	    }

	    bu_ptbl_ins(subbreps, (long *)new_obj);
	}
	if (obj_cnt > CSG_BREP_MAX_OBJS) goto bail;
    }

    return subbreps;

bail:
    bu_log("brep converted to more than %d implicits (%d) - not a good CSG candidate\n", CSG_BREP_MAX_OBJS, obj_cnt);
    // Free memory
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	for (unsigned int j = 0; j < BU_PTBL_LEN(obj->children); j++){
	    struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(obj->children,j);
	    subbrep_object_free(cdata);
	}
	subbrep_object_free(obj);
	BU_PUT(obj, struct subbrep_object_data);
    }
    bu_ptbl_free(subbreps);
    bu_ptbl_init(subbreps, 8, "subbrep table");
    return subbreps;
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
 *     with the bounding box of the union object in question.  If yes, add the pointer to subbreps_tree.
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
struct bu_ptbl *
find_top_level_hierarchy(struct bu_ptbl *subbreps)
{
    // Now that we have the subbreps (or their substructures) and their boolean status we need to
    // construct the top level tree.

    std::set<long *> subbrep_set;
    std::set<long *> subbrep_seed_set;
    std::set<long *> unions;
    std::set<long *> subtractions;
    std::set<long *>::iterator sb_it, sb_it2;
    std::map<long *, std::set<long *> > subbrep_subobjs;
    std::map<long *, std::set<long *> > subbrep_ignoreobjs;
    struct bu_ptbl *subbreps_tree;

    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
	subbrep_set.insert(BU_PTBL_GET(subbreps, i));
    }

    /* Separate out top level unions */
    for (sb_it = subbrep_set.begin(); sb_it != subbrep_set.end(); sb_it++) {
	struct subbrep_object_data *obj = (struct subbrep_object_data *)*sb_it;
	//std::cout << bu_vls_addr(obj->key) << " bool: " << obj->params->bool_op << "\n";
	if (obj->fil_cnt == 0) {
	    if (!(obj->params->bool_op == '-')) {
		//std::cout << "Top union found: " << bu_vls_addr(obj->key) << "\n";
		obj->params->bool_op = 'u';
		unions.insert((long *)obj);
		subbrep_set.erase((long *)obj);
	    } else {
		if (obj->params->bool_op == '-') {
		    //std::cout << "zero fills, but a negative shape - " << bu_vls_addr(obj->key) << " added to subtractions\n";
		    subtractions.insert((long *)obj);
		}
	    }
	}
    }

    subbrep_seed_set = unions;

    int iterate = 1;
    while (subbrep_seed_set.size() > 0) {
	//std::cout << "iterate: " << iterate << "\n";
	std::set<long *> subbrep_seed_set_2;
	std::set<long *> subbrep_set_b;
	for (sb_it = subbrep_seed_set.begin(); sb_it != subbrep_seed_set.end(); sb_it++) {
	    std::set<int> tu_fol;
	    struct subbrep_object_data *tu = (struct subbrep_object_data *)*sb_it;
	    array_to_set(&tu_fol, tu->fol, tu->fol_cnt);
	    subbrep_set_b = subbrep_set;
	    for (sb_it2 = subbrep_set.begin(); sb_it2 != subbrep_set.end(); sb_it2++) {
		struct subbrep_object_data *cobj = (struct subbrep_object_data *)*sb_it2;
		for (int i = 0; i < cobj->fil_cnt; i++) {
		    if (tu_fol.find(cobj->fil[i]) != tu_fol.end()) {
			struct subbrep_object_data *up;
			//std::cout << "Object " << bu_vls_addr(cobj->key) << " connected to " << bu_vls_addr(tu->key) << "\n";
			subbrep_seed_set_2.insert(*sb_it2);
			subbrep_set_b.erase(*sb_it2);
			// Determine boolean relationship to parent here, and use parent boolean
			// to decide this object's (cobj) status.  If tu is negative and cobj is unioned
			// relative to tu, then cobj is also negative.

			// This is a make-or-break step of the algorithm - if we cannot determine whether
			// a subbrep is added or subtracted, the whole B-Rep conversion process fails.
			//
			// TODO - In theory a partial conversion might still be achieved by re-inserting the
			// problem subbrep back into the parent B-Rep and proceeding with the rest, but
			// we are not currently set up for that - as of right now, a single failure on
			// this level of logic results in a completely failed csg conversion.

			int bool_test = 0;
			int already_flipped = 0;
			if (cobj->params->bool_op == '\0') {
			    /* First, check the boolean relationship to the parent solid */
			    cobj->parent = tu;
			    bool_test = subbrep_determine_boolean(cobj);
			    //std::cout << "Initial boolean test for " << bu_vls_addr(cobj->key) << ": " << bool_test << "\n";
			    switch (bool_test) {
				case -2:
				    bu_log("Game over - self intersecting shape reported with subbrep %s\n", bu_vls_addr(cobj->key));
				    bu_log("Until breakdown logic for this situation is available, this is a conversion stopper.\n");
				    return NULL;
				case 2:
				    /* Test relative to parent inconclusive - fall back on surface test, if available */
				    if (cobj->negative_shape == -1) bool_test = -1;
				    if (cobj->negative_shape == 1) bool_test = 1;
				    break;
				default:
				    break;
			    }
			} else {
			    //std::cout << "Boolean status of " << bu_vls_addr(cobj->key) << " already determined\n";
			    bool_test = (cobj->params->bool_op == '-') ? -1 : 1;
			    already_flipped = 1;
			}

			switch (bool_test) {
			    case -1:
				cobj->params->bool_op = '-';
				subtractions.insert((long *)cobj);
				subbrep_subobjs[*sb_it].insert((long *)cobj);
				break;
			    case 1:
				cobj->params->bool_op = 'u';
				unions.insert((long *)cobj);
				/* We've got a union - any subtractions that are parents of this
				 * object go in its ignore list */
				up = cobj->parent;
				while (up) {
				    if (up->params->bool_op == '-') subbrep_ignoreobjs[(long *)cobj].insert((long *)up);
				    up = up->parent;
				}
				break;
			    default:
				bu_log("Boolean status of %s could not be determined - conversion failure\n", bu_vls_addr(cobj->key));;
				return NULL;
				break;
			}

			// If the parent ended up subtracted, we need to propagate the change down the
			// children of this subbrep.
			if (bool_test == -1 && !already_flipped) {
			    for (unsigned int j = 0; j < BU_PTBL_LEN(cobj->children); j++){
				struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(cobj->children,j);
				if (cdata && cdata->params) {
				    cdata->params->bool_op = (cdata->params->bool_op == '-') ? 'u' : '-';
				} else {
				    //std::cout << "Child without params??: " << bu_vls_addr(cdata->key) << ", parent: " << bu_vls_addr(cobj->key) << "\n";
				}
			    }

			    /* If we're a B-Rep and a subtraction, the B-Rep will be inside out */
			    if (cobj->type == BREP && cobj->params->bool_op == '-') {
				for (int l = 0; l < cobj->local_brep->m_F.Count(); l++) {
				    ON_BrepFace &face = cobj->local_brep->m_F[l];
				    cobj->local_brep->FlipFace(face);
				}
			    }
			}
			//std::cout << "Boolean status of " << bu_vls_addr(cobj->key) << ": " << cobj->params->bool_op << "\n";
		    }
		}
	    }
	}
	subbrep_seed_set.clear();
	subbrep_seed_set = subbrep_seed_set_2;
	subbrep_set.clear();
	subbrep_set = subbrep_set_b;
	iterate++;
    }

    BU_GET(subbreps_tree, struct bu_ptbl);
    bu_ptbl_init(subbreps_tree, 8, "subbrep tree table");

    for (sb_it = unions.begin(); sb_it != unions.end(); sb_it++) {
	struct subbrep_object_data *pobj = (struct subbrep_object_data *)(*sb_it);
	bu_ptbl_ins(subbreps_tree, (long *)pobj);
	std::set<long *> topological_subtractions = subbrep_subobjs[(long *)pobj];
	std::set<long *> local_subtraction_queue = subtractions;
	for (sb_it2 = topological_subtractions.begin(); sb_it2 != topological_subtractions.end(); sb_it2++) {
	    bu_ptbl_ins(subbreps_tree, *sb_it2);
	    local_subtraction_queue.erase(*sb_it2);
	}
	std::set<long *> ignore_queue = subbrep_ignoreobjs[(long *)pobj];
	for (sb_it2 = ignore_queue.begin(); sb_it2 != ignore_queue.end(); sb_it2++) {
	    local_subtraction_queue.erase(*sb_it2);
	}

	// Now, whatever is left in the local subtraction queue has to be ruled out based on volumetric
	// intersection testing.

	// Construct bounding box for pobj
	if (!pobj->bbox_set) subbrep_bbox(pobj);
	// Iterate over the queue
	for (sb_it2 = local_subtraction_queue.begin(); sb_it2 != local_subtraction_queue.end(); sb_it2++) {
	    struct subbrep_object_data *sobj = (struct subbrep_object_data *)(*sb_it2);
	    //std::cout << "Checking subbrep " << bu_vls_addr(sobj->key) << "\n";
	    // Construct bounding box for sobj
	    if (!sobj->bbox_set) subbrep_bbox(sobj);
	    ON_BoundingBox isect;
	    bool bbi = isect.Intersection(*pobj->bbox, *sobj->bbox);
	    bool bbc = pobj->bbox->Includes(*sobj->bbox);
	    if (bbi || bbc) {
		//std::cout << " Found intersecting subbrep " << bu_vls_addr(sobj->key) << " under " << bu_vls_addr(pobj->key) << "\n";
		bu_ptbl_ins(subbreps_tree, *sb_it2);
	    }
	}
    }
    return subbreps_tree;
}

void
set_filter_obj(const ON_Surface *surface, struct filter_obj *obj)
{
    if (!obj) return;
    filter_obj_init(obj);
    obj->stype = GetSurfaceType(surface, obj);
    // If we've got a planar face, we can stop now - planar faces
    // are determinative of volume type only when *all* faces are planar,
    // and that case is handled elsewhere - anything is "allowed".
    if (obj->stype == SURFACE_PLANE) obj->type = BREP;

    // If we've got a general surface, anything is allowed
    if (obj->stype == SURFACE_GENERAL) obj->type = BREP;

    if (obj->stype == SURFACE_CYLINDRICAL_SECTION || obj->stype == SURFACE_CYLINDER) obj->type = CYLINDER;

    if (obj->stype == SURFACE_CONE) obj->type = CONE;

    if (obj->stype == SURFACE_SPHERICAL_SECTION || obj->stype == SURFACE_SPHERE) obj->type = SPHERE;

    if (obj->stype == SURFACE_TORUS) obj->type = TORUS;
}

int
apply_filter_obj(const ON_Surface *surf, int UNUSED(loop_index), struct filter_obj *obj)
{
    int ret = 1;
    struct filter_obj *local_obj;
    BU_GET(local_obj, struct filter_obj);
    int surface_type = (int)GetSurfaceType(surf, local_obj);

    std::set<int> allowed;

    if (obj->type == CYLINDER) {
	allowed.insert(SURFACE_CYLINDRICAL_SECTION);
	allowed.insert(SURFACE_CYLINDER);
	allowed.insert(SURFACE_PLANE);
    }

    if (obj->type == CONE) {
	allowed.insert(SURFACE_CONE);
	allowed.insert(SURFACE_PLANE);
    }

    if (obj->type == SPHERE) {
	allowed.insert(SURFACE_SPHERICAL_SECTION);
	allowed.insert(SURFACE_SPHERE);
	allowed.insert(SURFACE_PLANE);
    }

    if (obj->type == TORUS) {
	allowed.insert(SURFACE_TORUS);
	allowed.insert(SURFACE_PLANE);
    }


    // If the face's surface type is not part of the allowed set for
    // this object type, we're done
    if (allowed.find(surface_type) == allowed.end()) {
	ret = 0;
	goto filter_done;
    }
    if (obj->type == CYLINDER) {
	if (surface_type == SURFACE_PLANE) {
	    int is_parallel = obj->cylinder->Axis().IsParallelTo(local_obj->plane->Normal(), BREP_PLANAR_TOL);
	    if (is_parallel == 0) {
	       ret = 0;
	       goto filter_done;
	    }
	}
	if (surface_type == SURFACE_CYLINDER || surface_type == SURFACE_CYLINDRICAL_SECTION ) {
	    // Make sure the surfaces are on the same cylinder
	    if (obj->cylinder->circle.Center().DistanceTo(local_obj->cylinder->circle.Center()) > BREP_CYLINDRICAL_TOL) {
	       ret = 0;
	       goto filter_done;
	    }
	}
    }
    if (obj->type == CONE) {
    }
    if (obj->type == SPHERE) {
    }
    if (obj->type == TORUS) {
    }

filter_done:
    filter_obj_free(local_obj);
    BU_PUT(local_obj, struct filter_obj);
    return ret;
}

void
add_loops_from_face(int f_ind, struct subbrep_object_data *data, std::set<int> *loops, std::queue<int> *local_loops, std::set<int> *processed_loops)
{
    for (int j = 0; j < data->brep->m_F[f_ind].m_li.Count(); j++) {
	int loop_in_set = 0;
	int loop_ind = data->brep->m_F[f_ind].m_li[j];
	int k = 0;
	while (k < data->loops_cnt) {
	    if (data->loops[k] == loop_ind) loop_in_set = 1;
	    k++;
	}
	if (loop_in_set) loops->insert(loop_ind);
	if (loop_in_set && processed_loops->find(loop_ind) == processed_loops->end()) {
	    local_loops->push(loop_ind);
	}
    }
}


/* In order to represent complex shapes, it is necessary to identify
 * subsets of subbreps that can be represented as primitives.  This
 * function will identify such subsets, if possible.  If a subbrep
 * can be successfully decomposed into primitive candidates, the
 * type of the subbrep is set to COMB and the children bu_ptbl is
 * populated with subbrep_object_data sets. */
int
subbrep_split(struct subbrep_object_data *data)
{
    //if (BU_STR_EQUAL(bu_vls_addr(data->key), "325_326_441_527_528")) {
    //	std::cout << "looking at 325_326_441_527_528\n";
    //}
    int csg_fail = 0;
    std::set<int> processed_faces;
    std::set<std::string> subbrep_keys;
    /* For each face, identify the candidate solid type.  If that
     * subset has not already been seen, add it to the brep's set of
     * subbreps */
    for (int i = 0; i < data->faces_cnt; i++) {
	std::string key;
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	std::set<int>::iterator s_it;
	struct filter_obj *filters;
	BU_GET(filters, struct filter_obj);
	std::set<int> locally_processed_faces;

	set_filter_obj(data->brep->m_F[data->faces[i]].SurfaceOf(), filters);
	if (filters->type == BREP) {
	    filter_obj_free(filters);
	    BU_PUT(filters, struct filter_obj);
	    continue;
	}
	if (filters->stype == SURFACE_PLANE) {
	    filter_obj_free(filters);
	    BU_PUT(filters, struct filter_obj);
	    continue;
	}
	faces.insert(data->faces[i]);
	//std::cout << "working: " << data->faces[i] << "\n";
	locally_processed_faces.insert(data->faces[i]);
	add_loops_from_face(data->faces[i], data, &loops, &local_loops, &processed_loops);

	while(!local_loops.empty()) {
	    int curr_loop = local_loops.front();
	    local_loops.pop();
	    if (processed_loops.find(curr_loop) == processed_loops.end()) {
		loops.insert(curr_loop);
		processed_loops.insert(curr_loop);
		for (int ti = 0; ti < data->brep->m_L[curr_loop].m_ti.Count(); ti++) {
		    const ON_BrepTrim *trim = &(data->brep->m_T[data->brep->m_L[curr_loop].m_ti[ti]]);
		    const ON_BrepEdge *edge = &(data->brep->m_E[trim->m_ei]);
		    if (trim->m_ei != -1 && edge->TrimCount() > 1) {
			edges.insert(trim->m_ei);
			for (int j = 0; j < edge->TrimCount(); j++) {
			    int fio = edge->Trim(j)->FaceIndexOf();
			    if (fio != -1 && locally_processed_faces.find(fio) == locally_processed_faces.end()) {
				surface_t stype = GetSurfaceType(data->brep->m_F[fio].SurfaceOf(), NULL);
				// If fio meets the criteria for the candidate shape, add it.  Otherwise,
				// it's not part of this shape candidate
				if (apply_filter_obj(data->brep->m_F[fio].SurfaceOf(), curr_loop, filters)) {
				    // TODO - more testing needs to be done here...  get_allowed_surface_types
				    // returns the volume_t, use it to do some testing to evaluate
				    // things like normals and shared axis
				    //std::cout << "accept: " << fio << "\n";
				    faces.insert(fio);
				    locally_processed_faces.insert(fio);
				    // The planar faces will always share edges with the non-planar
				    // faces, which drive the primary shape.  Also, planar faces are
				    // more likely to have other edges that relate to other shapes.
				    if (stype != SURFACE_PLANE)
					add_loops_from_face(fio, data, &loops, &local_loops, &processed_loops);
				}
			    }
			}
		    }
		}
	    }
	}
	key = face_set_key(faces);

	/* If we haven't seen this particular subset before, add it */
	if (subbrep_keys.find(key) == subbrep_keys.end()) {
	    subbrep_keys.insert(key);
	    struct subbrep_object_data *new_obj;
	    BU_GET(new_obj, struct subbrep_object_data);
	    subbrep_object_init(new_obj, data->brep);
	    bu_vls_sprintf(new_obj->key, "%s", key.c_str());
	    set_to_array(&(new_obj->faces), &(new_obj->faces_cnt), &faces);
	    set_to_array(&(new_obj->loops), &(new_obj->loops_cnt), &loops);
	    set_to_array(&(new_obj->edges), &(new_obj->edges_cnt), &edges);
	    new_obj->fol_cnt = 0;
	    new_obj->fil_cnt = 0;
	    new_obj->parent = data;

	    new_obj->type = filters->type;
	    switch (new_obj->type) {
		case CYLINDER:
		    if (!cylinder_csg(new_obj, BREP_CYLINDRICAL_TOL)) {
			bu_log("cylinder csg failure: %s\n", key.c_str());
			csg_fail++;
		    }
		    break;
		case CONE:
		    if (!cone_csg(new_obj, BREP_CONIC_TOL)) {
			bu_log("cone csg failure: %s\n", key.c_str());
			csg_fail++;
		    }
		    break;
		case SPHERE:
		    if (!sphere_csg(new_obj, BREP_SPHERICAL_TOL)) {
			bu_log("sphere csg failure: %s\n", key.c_str());
			csg_fail++;
		    }
		    break;
		case ELLIPSOID:
		    bu_log("TODO: process partial ellipsoid\n");
		    /* TODO - Until we properly handle these shapes, this is a failure case */
		    csg_fail++;
		    break;
		case TORUS:
		    if (!torus_csg(new_obj, BREP_TOROIDAL_TOL)) {
			bu_log("torus csg failure: %s\n", key.c_str());
			csg_fail++;
		    }
		    break;
		default:
		    /* Unknown object type - can't convert to csg */
		    csg_fail++;
		    break;
	    }

	    bu_ptbl_ins(data->children, (long *)new_obj);
	}
	filter_obj_free(filters);
	BU_PUT(filters, struct filter_obj);
    }
    if (subbrep_keys.size() == 0 || csg_fail > 0) {
	goto csg_abort;
    }
    data->type = COMB;
    return 1;
csg_abort:
    /* Clear children - we found something we can't handle, so there will be no
     * CSG conversion of this subbrep */
    data->type = BREP;
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->children); i++) {
	struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(data->children,i);
	subbrep_object_free(cdata);
    }
    return 0;
}

int
subbrep_make_brep(struct subbrep_object_data *data)
{
    if (data->local_brep) return 0;
    data->local_brep = ON_Brep::New();
    // For each edge in data, find the corresponding loop in data and construct
    // a new face in the brep with the surface from the original face and the
    // loop in data as the new outer loop.  Trim down the surface for the new
    // role.  Add the corresponding 3D edges and sync things up.

    // Each edge will map to two faces in the original Brep.  We only want the
    // subset of data that is part of this particular subobject - to do that,
    // we need to map elements from their indices in the old Brep to their
    // locations in the new.
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> loop_map;
    std::map<int, int> c3_map;
    std::map<int, int> c2_map;
    std::map<int, int> trim_map;
    //std::map<int, int> subloop_map;  // When not all of the trims from an old loop are used, make new loops here so we have somewhere to stash the trims.  They'll be useful if we want/need to construct faces closing the new subbreps.

    std::set<int> faces;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int> isolated_trims;  // collect 2D trims whose parent loops aren't fully included here
    array_to_set(&faces, data->faces, data->faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);
    array_to_set(&loops, data->loops, data->loops_cnt);

    // Each edge has a trim array, and the trims will tell us which loops
    // are to be included and which faces the trims belong to.  There will
    // be some trims that belong to a face that is not included in the
    // subbrep face list, and those are rejected - those rejections indicate
    // a new face needs to be created to close the Brep.  The edges will drive
    // the population of new_brep initially - for each element pulled in by
    // the edge, it is either added or (if it's already in the map) references
    // are updated.

    for (int i = 0; i < data->edges_cnt; i++) {
	int c3i;
	const ON_BrepEdge *old_edge = &(data->brep->m_E[data->edges[i]]);
	//std::cout << "old edge: " << old_edge->Vertex(0)->m_vertex_index << "," << old_edge->Vertex(1)->m_vertex_index << "\n";

	// Get the 3D curves from the edges
	if (c3_map.find(old_edge->EdgeCurveIndexOf()) == c3_map.end()) {
	    ON_Curve *nc = old_edge->EdgeCurveOf()->Duplicate();
	    c3i = data->local_brep->AddEdgeCurve(nc);
	    c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
	} else {
	    c3i = c3_map[old_edge->EdgeCurveIndexOf()];
	}

	// Get the vertices from the edges
	int v0i, v1i;
	if (vertex_map.find(old_edge->Vertex(0)->m_vertex_index) == vertex_map.end()) {
	    ON_BrepVertex& newv0 = data->local_brep->NewVertex(old_edge->Vertex(0)->Point(), old_edge->Vertex(0)->m_tolerance);
	    v0i = newv0.m_vertex_index;
	    vertex_map[old_edge->Vertex(0)->m_vertex_index] = v0i;
	} else {
	    v0i = vertex_map[old_edge->Vertex(0)->m_vertex_index];
	}
	if (vertex_map.find(old_edge->Vertex(1)->m_vertex_index) == vertex_map.end()) {
	    ON_BrepVertex& newv1 = data->local_brep->NewVertex(old_edge->Vertex(1)->Point(), old_edge->Vertex(1)->m_tolerance);
	    v1i = newv1.m_vertex_index;
	    vertex_map[old_edge->Vertex(1)->m_vertex_index] = v1i;
	} else {
	    v1i = vertex_map[old_edge->Vertex(1)->m_vertex_index];
	}
	ON_BrepEdge& new_edge = data->local_brep->NewEdge(data->local_brep->m_V[v0i], data->local_brep->m_V[v1i], c3i, NULL ,0);
	edge_map[old_edge->m_edge_index] = new_edge.m_edge_index;
	//std::cout << "new edge: " << v0i << "," << v1i << "\n";

	// Get the 2D curves from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
		if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
		    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
		    int c2i = data->local_brep->AddTrimCurve(nc);
		    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
		    //std::cout << "c2i: " << c2i << "\n";
		}
	    }
	}

	// Get the faces and surfaces from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    if (face_map.find(old_trim->Face()->m_face_index) == face_map.end()) {
		if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
		    ON_Surface *ns = old_trim->Face()->SurfaceOf()->Duplicate();
		    int nsid = data->local_brep->AddSurface(ns);
		    surface_map[old_trim->Face()->SurfaceIndexOf()] = nsid;
		    ON_BrepFace &new_face = data->local_brep->NewFace(nsid);
		    face_map[old_trim->Face()->m_face_index] = new_face.m_face_index;
		    //std::cout << "old_face: " << old_trim->Face()->m_face_index << "\n";
		    //std::cout << "new_face: " << new_face.m_face_index << "\n";
		    if (fil.find(old_trim->Face()->m_face_index) != fil.end()) {
			data->local_brep->FlipFace(new_face);
		    }
		    if (old_trim->Face()->m_bRev) {
			data->local_brep->FlipFace(new_face);
		    }
		}
	    }
	}

	// Get the loops from the trims
	for (int j = 0; j < old_edge->TrimCount(); j++) {
	    ON_BrepTrim *old_trim = old_edge->Trim(j);
	    ON_BrepLoop *old_loop = old_trim->Loop();
	    if (face_map.find(old_trim->Face()->m_face_index) != face_map.end()) {
		if (loops.find(old_loop->m_loop_index) != loops.end()) {
		    if (loop_map.find(old_loop->m_loop_index) == loop_map.end()) {
			// After the initial breakout, all loops in any given subbrep are outer loops,
			// whatever they were in the original brep.
			ON_BrepLoop &nl = data->local_brep->NewLoop(ON_BrepLoop::outer, data->local_brep->m_F[face_map[old_loop->m_fi]]);
			loop_map[old_loop->m_loop_index] = nl.m_loop_index;
			//std::cout << "adding loop: " << old_loop->m_loop_index << "\n";
		    }
		} //else {
		    //std::cout << "have isolated trim whose parent loop isn't fully included\n";
		   /* if (subloop_map.find(old_loop->m_loop_index) == subloop_map.end()) {
			ON_BrepLoop &nl = data->local_brep->NewLoop(ON_BrepLoop::outer, data->local_brep->m_F[face_map[old_loop->m_fi]]);
			subloop_map[old_loop->m_loop_index] = nl.m_loop_index;
			isolated_trims.insert(old_trim->m_trim_index);
		    }
		}*/
	    }
	}
    }

    // Now, create new trims using the old loop definitions and the maps
    std::map<int, int>::iterator loop_it;
    for (loop_it = loop_map.begin(); loop_it != loop_map.end(); loop_it++) {
	const ON_BrepLoop *old_loop = &(data->brep->m_L[(*loop_it).first]);
	ON_BrepLoop &new_loop = data->local_brep->m_L[(*loop_it).second];
	for (int j = 0; j < old_loop->TrimCount(); j++) {
	    const ON_BrepTrim *old_trim = old_loop->Trim(j);
	    //std::cout << "loop[" << (*loop_it).first << "," << (*loop_it).second << "]: trim " << old_trim->m_trim_index << "\n";
	    ON_BrepEdge *o_edge = old_trim->Edge();
	    if (o_edge) {
		ON_BrepEdge &n_edge = data->local_brep->m_E[edge_map[o_edge->m_edge_index]];
		//std::cout << "edge(" << o_edge->m_edge_index << "," << n_edge.m_edge_index << ")\n";
		ON_BrepTrim &nt = data->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2_map[old_trim->TrimCurveIndexOf()]);
		nt.m_tolerance[0] = old_trim->m_tolerance[0];
		nt.m_tolerance[1] = old_trim->m_tolerance[1];

		nt.m_iso = old_trim->m_iso;
	    } else {
		/* If we didn't have an edge originally, we need to add the 2d curve here */
		if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
		    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
		    int c2i = data->local_brep->AddTrimCurve(nc);
		    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
		    //std::cout << "2D only c2i: " << c2i << "\n";
		}
		if (vertex_map.find(old_trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvs = data->local_brep->NewVertex(old_trim->Vertex(0)->Point(), old_trim->Vertex(0)->m_tolerance);
		    vertex_map[old_trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;

		    ON_BrepTrim &nt = data->local_brep->NewSingularTrim(newvs, new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		} else {
		    ON_BrepTrim &nt = data->local_brep->NewSingularTrim(data->local_brep->m_V[vertex_map[old_trim->Vertex(0)->m_vertex_index]], new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		    nt.m_tolerance[0] = old_trim->m_tolerance[0];
		    nt.m_tolerance[1] = old_trim->m_tolerance[1];
		}
	    }
	}
    }
#if 0
    std::set<int>::iterator trims_it;
    for (trims_it = isolated_trims.begin(); trims_it != isolated_trims.end(); trims_it++) {
	const ON_BrepTrim *old_trim = &(data->brep->m_T[*trims_it]);
	ON_BrepLoop &new_loop = data->local_brep->m_L[subloop_map[old_trim->Loop()->m_loop_index]];
	ON_BrepEdge *o_edge = old_trim->Edge();
	if (o_edge) {
	    ON_BrepEdge &n_edge = data->local_brep->m_E[edge_map[o_edge->m_edge_index]];
	    //std::cout << "edge(" << o_edge->m_edge_index << "," << n_edge.m_edge_index << ")\n";
	    ON_BrepTrim &nt = data->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2_map[old_trim->TrimCurveIndexOf()]);
	    nt.m_tolerance[0] = old_trim->m_tolerance[0];
	    nt.m_tolerance[1] = old_trim->m_tolerance[1];

	    nt.m_iso = old_trim->m_iso;
	} else {
	    /* If we didn't have an edge originally, we need to add the 2d curve here */
	    if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
		ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
		int c2i = data->local_brep->AddTrimCurve(nc);
		c2_map[old_trim->TrimCurveIndexOf()] = c2i;
		//std::cout << "2D only c2i: " << c2i << "\n";
	    }
	    if (vertex_map.find(old_trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		ON_BrepVertex& newvs = data->local_brep->NewVertex(old_trim->Vertex(0)->Point(), old_trim->Vertex(0)->m_tolerance);
		ON_BrepTrim &nt = data->local_brep->NewSingularTrim(newvs, new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		nt.m_tolerance[0] = old_trim->m_tolerance[0];
		nt.m_tolerance[1] = old_trim->m_tolerance[1];
	    } else {
		ON_BrepTrim &nt = data->local_brep->NewSingularTrim(data->local_brep->m_V[vertex_map[old_trim->Vertex(0)->m_vertex_index]], new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
		nt.m_tolerance[0] = old_trim->m_tolerance[0];
		nt.m_tolerance[1] = old_trim->m_tolerance[1];
	    }
	}
    }
#endif
    // Make sure all the loop directions are correct
    for (int l = 0; l < data->local_brep->m_L.Count(); l++) {
	if (data->local_brep->LoopDirection(data->local_brep->m_L[l]) != 1) {
	    data->local_brep->FlipLoop(data->local_brep->m_L[l]);
	}
    }

    data->local_brep->ShrinkSurfaces();
    data->local_brep->CullUnusedSurfaces();

    //std::cout << "new brep done: " << bu_vls_addr(data->key) << "\n";

    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
