#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

/*************************
 *   Utility functions
 *************************/

#if 0
// Make an edge id using the Szudzik pairing function:
long unsigned int edge_id(size_t pt_A, size_t pt_B)
{
    return (pt_A >= pt_B) ? (pt_A * pt_A + pt_A + pt_B) : (pt_B * pt_B + pt_A);
}

// shift one of the vertex index numbers up to form a unique ID.  Offset is
// log10(bot->num_vertices) + 1
long unsigned int edge_id(size_t pt_A, size_t pt_B, int offset)
{
    return (pt_A <= pt_B) ? (pt_A * pow(10,offset) + pt_B) : (pt_B * pow(10,offset) + pt_A);
}

void breakout(long unsigned int id, int offset, size_t *A, size_t *B)
{
    (*B) = (size_t)((long)id % (int)pow(10,offset));
    (*A) = (size_t)(id - (*B))/ (int)(pow(10, offset));
}
#endif

// Make an edge using consistent vertex ordering
static std::pair<size_t, size_t>
mk_edge(size_t pt_A, size_t pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
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

// Given a BoT face, return the three other faces that share an edge with that face
static void
get_connected_faces(struct rt_bot_internal *bot, size_t face_num,  std::map<size_t, std::set<size_t> > *vert_to_face, std::set<size_t> *faces)
{
    std::map<size_t, size_t> face_cnt;
    size_t points[3];
    points[0] = bot->faces[face_num*3+0]*3;
    points[1] = bot->faces[face_num*3+1]*3;
    points[2] = bot->faces[face_num*3+2]*3;
    std::set<size_t>::iterator it;
    for (int i = 0; i < 3; ++i) {
	for (it = (*vert_to_face)[points[i]].begin(); it != (*vert_to_face)[points[i]].end(); it++) {
	    face_cnt[(*it)] += 1;
	}
    }
    std::map<size_t, size_t>::iterator mit;
    for (mit = face_cnt.begin(); mit != face_cnt.end(); ++mit) {
	if ((*mit).second > 1) {
	    faces->insert((*mit).first);
	}
    }
}

/* To avoid drawing duplicate lines, build the final vlist using a set */
static void
plot_patch_borders(std::vector<std::map<std::pair<size_t, size_t>, size_t> > *patch_edge_cnt, struct rt_bot_internal *bot, struct bu_list *vhead)
{
    std::set<std::pair<size_t, size_t> > final_edge_set;
    std::set<std::pair<size_t, size_t> >::iterator fe_it;
    std::map<std::pair<size_t, size_t>, size_t>::iterator e_it;
    for (int i = 0; i < patch_edge_cnt->size(); i++) {
	for (e_it = (*patch_edge_cnt)[i].begin(); e_it != (*patch_edge_cnt)[i].end(); e_it++) {
	    if ((*e_it).second == 1) {
                final_edge_set.insert((*e_it).first);
	    }
	}
    }
    for (fe_it = final_edge_set.begin(); fe_it != final_edge_set.end(); fe_it++) {
	RT_ADD_VLIST(vhead, &(bot->vertices[(*fe_it).first]), BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, &(bot->vertices[(*fe_it).second]), BN_VLIST_LINE_DRAW);
    }
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

    if (bot->num_vertices <= 0 || !bot->vertices || bot->num_faces <= 0 || !bot->faces)
        return 0;

    /* Declare key containers */
    vect_t gvects[6];
    std::vector<double> face_areas;
    std::vector<double> face_normals;
    std::vector<std::vector<size_t> > groups;
    std::vector<std::map<std::pair<size_t, size_t>, size_t> > groups_edge_face_cnt;
    std::vector<std::vector<size_t> > categorization_results;
    std::vector<int> face_to_plane;
    std::vector<std::set<size_t> > patches;
    std::map<size_t, std::set<size_t> > vert_to_face;

    /* Initialize containers */
    face_areas.resize(bot->num_faces);
    face_to_plane.resize(bot->num_faces);
    face_normals.resize(bot->num_faces*3);
    groups.resize(6);
    groups_edge_face_cnt.resize(6);
    categorization_results.resize(6);
    for (int i = 0; i < 6; ++i) {
	categorization_results[i].resize(bot->num_faces); //categories can hold at most all faces in bot
    }
    patches.resize(bot->num_faces*3); // Max number of patches should be the same as the face cnt, but it isn't always
    VSET(gvects[0], -1,0,0);
    VSET(gvects[1], 0,-1,0);
    VSET(gvects[2], 0,0,-1);
    VSET(gvects[3], 1,0,0);
    VSET(gvects[4], 0,1,0);
    VSET(gvects[5], 0,0,1);

    // Calculate face normals dot product with bounding rpp planes
    for (size_t i=0; i < bot->num_faces; ++i) {
	size_t pt_A, pt_B, pt_C;
	size_t result_max;
	double vdot = 0.0;
	double result = 0.0;
	vect_t a, b, norm_dir;
	// Add vert -> edge and edge -> face mappings to global map.
	pt_A = bot->faces[i*3+0]*3;
	pt_B = bot->faces[i*3+1]*3;
	pt_C = bot->faces[i*3+2]*3;
	vert_to_face[pt_A].insert(i);
	vert_to_face[pt_B].insert(i);
	vert_to_face[pt_C].insert(i);
	// Categorize face
	VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
        face_normals[i*3]=norm_dir[0];
        face_normals[i*3+1]=norm_dir[1];
        face_normals[i*3+2]=norm_dir[2];
	for (int j=0; j < 6; j++) {
	    vdot = VDOT(gvects[j],norm_dir);
            categorization_results[j][i] = vdot;
	    if (vdot > result) {
		result_max = j;
		result = vdot;
	    }
	}
	groups[result_max].push_back(i);
	face_to_plane[i]=result_max;
        face_areas[i] = face_area(bot, i);
    }

    // Order the groups by number of bots
    std::vector<int> ordered_groups;
    int group_ids[] = {0, 1, 2, 3, 4, 5};
    std::set<int> group_bin (group_ids, group_ids+6);
    std::set<int>::iterator fg_it;
    while (!group_bin.empty()) {
        int largest_group = 0;
        int max = 0;
	for (fg_it = group_bin.begin(); fg_it != group_bin.end(); fg_it++) {
	    if (groups[(*fg_it)].size() == 0) {
                group_bin.erase((*fg_it));
	    } else {
		if (groups[(*fg_it)].size() > max) {
		    max = groups[(*fg_it)].size();
		    largest_group = (*fg_it);
		}
	    }
	}
	if (max > 0)
	    ordered_groups.push_back(largest_group);
	group_bin.erase(largest_group);
    }

    // All faces must belong to some patch - continue until all faces are processed
    std::vector<int> face_pool (bot->num_faces, 1);
    std::vector<int> face_to_patch (bot->num_faces, -1);
    int patch_cnt = -1;
    for (int i = 0; i < ordered_groups.size(); i++) {
	int largest_face = 0;
	int curr_group = ordered_groups[i];
	while (largest_face != -1) {
	    // Start a new patch
	    vect_t largest_face_normal;
	    largest_face = -1;
            patch_cnt++;
	    // Find largest remaining face in group
	    double face_size_criteria = 0.0;
	    for (int f_it = 0; f_it < groups[curr_group].size(); f_it++) {
		if (face_pool[groups[curr_group][f_it]]) {
		    double fa = face_areas[groups[curr_group][f_it]];
		    if (fa > face_size_criteria) {
			largest_face = groups[curr_group][f_it];
			face_size_criteria = fa;
		    }
		}
	    }
	    if (largest_face != -1) {
		std::queue<int> face_queue;
		face_queue.push(largest_face);

		VSET(largest_face_normal, face_normals[largest_face*3], face_normals[largest_face*3+1], face_normals[largest_face*3+2]);
		face_pool[largest_face] = 0;
		while (!face_queue.empty()) {
		    size_t pt_A, pt_B, pt_C;
		    int face_num = face_queue.front();
		    face_queue.pop();
		    patches[patch_cnt].insert(face_num);
		    face_to_patch[face_num] = patch_cnt;
		    pt_A = bot->faces[face_num*3+0]*3;
		    pt_B = bot->faces[face_num*3+1]*3;
		    pt_C = bot->faces[face_num*3+2]*3;
		    groups_edge_face_cnt[curr_group][mk_edge(pt_A, pt_B)] += 1;
		    groups_edge_face_cnt[curr_group][mk_edge(pt_B, pt_C)] += 1;
		    groups_edge_face_cnt[curr_group][mk_edge(pt_C, pt_A)] += 1;

		    std::set<size_t> connected_faces;
		    std::set<size_t>::iterator cf_it;
		    get_connected_faces(bot, face_num, &(vert_to_face), &connected_faces);
		    for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
			if (face_pool[(*cf_it)]) {
			    double curr_face_area = face_areas[(*cf_it)];
			    if (curr_face_area < (face_size_criteria*10) && curr_face_area > (face_size_criteria*0.1)) {
				vect_t face_normal;
				VSET(face_normal, face_normals[face_num*3], face_normals[face_num*3+1], face_normals[face_num*3+2]);
				if (VDOT(largest_face_normal, face_normal) > 0.85) {
				    face_queue.push((*cf_it));
				    face_pool[(*cf_it)] = 0;
				}
			    }
			}
		    }
		}
	    }
	}
    }
    plot_patch_borders(&(groups_edge_face_cnt), bot, info->vhead);

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
