#include "common.h"

#include <map>
#include <queue>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

// Make an edge using consistent vertex ordering
static std::pair<unsigned int, unsigned int>
mk_edge(unsigned int pt_A, unsigned int pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

// Build a set of edge-centric data structures to represent the mesh
unsigned int
edge_mappings(struct rt_bot_internal *bot, unsigned int **edges_ptr, unsigned int **faces_ptr) {

    // Nick Reed's idea - use a C++ map of pairs to ints as a one-time method
    // to generate unique int identifiers for edges.
    std::map<std::pair<unsigned int, unsigned int>, unsigned int> edge_to_int;
    std::pair<std::map<std::pair<unsigned int, unsigned int>, unsigned int>::iterator, bool> ret;
    unsigned int edge_cnt = 0;
    // Each face will have 3 edges
    unsigned int *faces = *edges_ptr;
    // Max possible need for this array is 3 edges for each face, 2 vertex indices per edge
    unsigned int *edges = *faces_ptr;

    for (unsigned int i = 0; i < bot->num_faces; ++i) {
	ret = edge_to_int.insert(std::pair<std::pair<unsigned int, unsigned int>, unsigned int>(mk_edge(bot->faces[i*3+0], bot->faces[i*3+1]), edge_cnt));
	if (ret.second == false) {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 0] = ret.first->second;
	} else {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 0] = edge_cnt;
            edges[2 * edge_cnt + 0] = bot->faces[i*3+0];
            edges[2 * edge_cnt + 1] = bot->faces[i*3+1];
	    edge_cnt++;
	}
	ret = edge_to_int.insert(std::pair<std::pair<unsigned int, unsigned int>, unsigned int>(mk_edge(bot->faces[i*3+1], bot->faces[i*3+2]), edge_cnt));
	if (ret.second == false) {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 1] = ret.first->second;
	} else {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 1] = edge_cnt;
            edges[2 * edge_cnt + 0] = bot->faces[i*3+1];
            edges[2 * edge_cnt + 1] = bot->faces[i*3+2];
	    edge_cnt++;
	}
	ret = edge_to_int.insert(std::pair<std::pair<unsigned int, unsigned int>, unsigned int>(mk_edge(bot->faces[i*3+2], bot->faces[i*3+0]), edge_cnt));
	if (ret.second == false) {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 2] = ret.first->second;
	} else {
	    // rows are faces, columns are the 1st, 2nd and 3rd edges
	    faces[3 * i + 2] = edge_cnt;
            edges[2 * edge_cnt + 0] = bot->faces[i*3+2];
            edges[2 * edge_cnt + 1] = bot->faces[i*3+0];
	    edge_cnt++;
	}
    }

    return edge_cnt;
}


// Calculate area of a face
static double
face_area(struct rt_bot_internal *bot, size_t face_num)
{
    point_t ptA, ptB, ptC;
    double a, b, c, p;
    double area;
    VMOVE(ptA, &bot->vertices[bot->faces[face_num*3+0]*3]);
    VMOVE(ptB, &bot->vertices[bot->faces[face_num*3+1]*3]);
    VMOVE(ptC, &bot->vertices[bot->faces[face_num*3+2]*3]);
    a = DIST_PT_PT(ptA, ptB);
    b = DIST_PT_PT(ptB, ptC);
    c = DIST_PT_PT(ptC, ptA);
    p = (a + b + c)/2;
    area = sqrt(p*(p-a)*(p-b)*(p-c));
    return area;
}

/**********************************************************
 *   Code for identifying semi-flat patches in bots
 *   and creating wireframe edges
 **********************************************************/
extern "C" int
rt_bot_adaptive_plot(struct rt_db_internal *ip, const struct rt_view_info *info)
{
    struct rt_bot_internal *bot;
    RT_CK_DB_INTERNAL(ip);
    BU_CK_LIST_HEAD(info->vhead);
    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    vect_t gvects[6];
    VSET(gvects[0], -1,0,0);
    VSET(gvects[1], 0,-1,0);
    VSET(gvects[2], 0,0,-1);
    VSET(gvects[3], 1,0,0);
    VSET(gvects[4], 0,1,0);
    VSET(gvects[5], 0,0,1);

    if (bot->num_vertices <= 0 || !bot->vertices || bot->num_faces <= 0 || !bot->faces)
        return 0;

    /* Declare key containers */
    double *face_normals= (double *)bu_calloc(bot->num_faces * 3, sizeof(double), "normals");
    /* Stash all the initial categorization test results - can be re-used later */
//    unsigned int *categorization_results = (unsigned int *)bu_calloc(sizeof(unsigned int) * bot->num_faces * 6, "categorization results");
    /* Initial group assignments */
    unsigned int *groups = (unsigned int *)bu_calloc(bot->num_faces, sizeof(unsigned int), "groups");
    /* Initially, patch breakdown is made in a faces -> patch_id map */
    unsigned int *patches= (unsigned int *)bu_calloc(bot->num_faces, sizeof(unsigned int), "patch_id_numbers");
    /* Don't know yet how big these needs to be */
    unsigned int *vert_to_face = NULL;

    /* Sanity check vertex indices in face definitions */
    for (unsigned int i = 0; i < bot->num_faces; ++i) {
        if ((unsigned int)bot->faces[i*3+0] > (unsigned int)(bot->num_vertices)) return 0;
        if ((unsigned int)bot->faces[i*3+1] > (unsigned int)(bot->num_vertices)) return 0;
        if ((unsigned int)bot->faces[i*3+2] > (unsigned int)(bot->num_vertices)) return 0;
    }

    /* Pre-compute face normals once */
    for (unsigned int i = 0; i < bot->num_faces; i++) {
	vect_t a, b, norm_dir;
	VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
	face_normals[i*3]=norm_dir[0];
	face_normals[i*3+1]=norm_dir[1];
	face_normals[i*3+2]=norm_dir[2];
    }

    /* Calculate categorization results and assign groups */
    for (unsigned int i = 0; i < bot->num_faces; i++) {
	int result_max = 0;
	double result = 0.0;
	double vdot = 0.0;
	vect_t norm_dir;
	VSET(norm_dir, face_normals[i*3], face_normals[i*3+1], face_normals[i*3+2]);
	for (unsigned int j=0; j < 6; j++) {
	    vdot = VDOT(gvects[j],norm_dir);
	    //categorization_results[i*j] = vdot;
	    if (vdot > result) {
		result_max = j;
		result = vdot;
	    }
	}
        groups[i] = result_max;
    }
    bu_free(face_normals, "face_normals");

    /* Determine the maximum number of faces associated with any one vertex */
    unsigned int *vert_face_cnt = (unsigned int *)bu_calloc(bot->num_vertices, sizeof(unsigned int), "vert face cnt");
    for (unsigned int i = 0; i < bot->num_faces; i++) {
        vert_face_cnt[bot->faces[i*3+0]]++;
        vert_face_cnt[bot->faces[i*3+1]]++;
        vert_face_cnt[bot->faces[i*3+2]]++;
    }
    unsigned int max_face_cnt = 0;
    for (unsigned int i = 0; i < bot->num_vertices; i++) {
	if (vert_face_cnt[i] > max_face_cnt)
	    max_face_cnt = vert_face_cnt[i];
    }
    /* Now, allocate a 2D array that can hold the full vert->face map
     * and populate it */
    vert_to_face = (unsigned int *)bu_calloc(bot->num_vertices * max_face_cnt, sizeof(unsigned int), "map");
    unsigned int *vert_sizes = (unsigned int *)bu_calloc(bot->num_vertices, sizeof(unsigned int), "map");
    for (unsigned int i = 0; i < bot->num_faces; i++) {
	for (unsigned int j = 0; j < 3; j++) {
	    unsigned int vert = bot->faces[i*3+j];
            unsigned int k = vert_sizes[vert];
            /* rows are vertex indexes, columns hold the faces */
            /* Need to increment face index by one so we can reference
	     * the first face and still use the true/false test that 0 allows */
	    vert_to_face[max_face_cnt * vert + k] = i + 1;
            vert_sizes[vert]++;
            //bu_log("vert_to_face(%d,%d)[%d] = %d\n", vert, k, max_face_cnt * vert + k, i + 1);
	}
    }


    /* Order the groups by number of bots */
    /* Note - needed only when group boarders are not enforced in patch building */
//    unsigned int group_cnt[6] = {0};
    unsigned int ordered_groups[6] = {0,1,2,3,4,5};
/*    for (unsigned int i = 0; i < bot->num_faces; i++) {
        group_cnt[groups[i]]++;
    }
    for (unsigned int i = 0; i < 6; i++) {
        unsigned int more = 0;
	for (unsigned int j = 0; j < 6; j++) {
            if (group_cnt[j] > group_cnt[i]) more++;
	}
        ordered_groups[more] = i;
    }
*/


    // All faces must belong to some patch - continue until all faces are processed
    unsigned int patch_cnt = 0;
    for (unsigned int i = 0; i < 6; i++) {
	int unused = 0;
	while (unused != -1) {
            unsigned int face_stp = 0;
	    // Start a new patch
	    unused = -1;
	    patch_cnt++;
	    // Find remaining face in group
	    while (unused == -1 && face_stp < bot->num_faces) {
		if (ordered_groups[i] == groups[face_stp] && !patches[face_stp]) {
                   unused = face_stp;
		}
                face_stp++;
            }
	    if (unused != -1) {
		std::queue<unsigned int> face_queue;
		face_queue.push(unused);
		patches[unused] = patch_cnt;
		while (!face_queue.empty()) {
		    unsigned int face_num = face_queue.front();
		    face_queue.pop();
		    for (unsigned int k = 0; k < 3; k++) {
			unsigned int vert_id = bot->faces[face_num*3+k];
			for (unsigned int l = 0; l < vert_face_cnt[vert_id]; l++) {
			    unsigned int new_face = vert_to_face[max_face_cnt * vert_id  + l] - 1;
			    if (groups[new_face] == ordered_groups[i] && !patches[new_face]) {
				face_queue.push(new_face);
				patches[new_face] = patch_cnt;
			    }
			}
		    }
		}
	    }
	}
    }
    bu_free(groups, "groups");
    bu_free(vert_to_face, "vert_to_face");

    unsigned int *patch_vert_cnt = vert_sizes;
    memset(patch_vert_cnt, 0, bot->num_vertices * sizeof(unsigned int));
    unsigned int *vert_edge_status = (unsigned int *)bu_calloc(bot->num_vertices, sizeof(unsigned int), "vert status");
    for (unsigned int i = 0; i < patch_cnt; i++) {
        memset(patch_vert_cnt, 0, bot->num_vertices * sizeof(unsigned int));
	for (unsigned int j = 0; j < bot->num_faces; j++) {
	    if (patches[j] == i+1) {
		patch_vert_cnt[bot->faces[j*3+0]]++;
		patch_vert_cnt[bot->faces[j*3+1]]++;
		patch_vert_cnt[bot->faces[j*3+2]]++;
	    }
	}
	for (unsigned int j = 0; j < bot->num_vertices; j++) {
	    if (patch_vert_cnt[j] && patch_vert_cnt[j] != vert_face_cnt[j]) {
		vert_edge_status[j]++;
	    }
	}
    }
    bu_free(patches, "patches");
    bu_free(patch_vert_cnt, "patch_vert_cnt");
    bu_free(vert_face_cnt, "vert_face_cnt");

    for (unsigned int j = 0; j < bot->num_faces; j++) {
	if (vert_edge_status[bot->faces[j*3+0]] &&  vert_edge_status[bot->faces[j*3+1]]) {
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+0]*3]), BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+1]*3]), BN_VLIST_LINE_DRAW);
	}
        if (vert_edge_status[bot->faces[j*3+1]] &&  vert_edge_status[bot->faces[j*3+2]]) {
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+1]*3]), BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+2]*3]), BN_VLIST_LINE_DRAW);
	}
	if (vert_edge_status[bot->faces[j*3+2]] &&  vert_edge_status[bot->faces[j*3+0]]) {
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+2]*3]), BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(info->vhead, &(bot->vertices[bot->faces[j*3+0]*3]), BN_VLIST_LINE_DRAW);
	}
    }

    bu_free(vert_edge_status, "vert status");

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
