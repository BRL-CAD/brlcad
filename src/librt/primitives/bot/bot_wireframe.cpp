#include "common.h"

#include <queue>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


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
    double *face_areas= (double *)bu_calloc(bot->num_faces, sizeof(double), "areas");
    double *face_normals= (double *)bu_calloc(bot->num_faces * 3, sizeof(double), "normals");
    /* Stash all the initial categorization test results - can be re-used later */
//    unsigned int *categorization_results = (unsigned int *)bu_calloc(sizeof(unsigned int) * bot->num_faces * 6, "categorization results");
    /* Initial group assignments */
    unsigned int *groups = (unsigned int *)bu_calloc(bot->num_faces, sizeof(unsigned int), "groups");
    /* Initially, patch breakdown is made in a faces -> patch_id map */
    unsigned int *patches= (unsigned int *)bu_calloc(bot->num_faces, sizeof(unsigned int), "patch_id_numbers");
    /* Don't know yet how big this needs to be */
    unsigned int *vert_to_face = NULL;

    /* Sanity check vertex indicies in face definitions */
    for (unsigned int i = 0; i < bot->num_faces; ++i) {
        if ((unsigned int)bot->faces[i*3+0] > (unsigned int)(bot->num_vertices)) bu_log("bot->faces[%d*3+0]: %d, bot->num_vertices: %d", i, bot->faces[i*3+0], bot->num_vertices);
        if ((unsigned int)bot->faces[i*3+1] > (unsigned int)(bot->num_vertices)) bu_log("bot->faces[%d*3+1]: %d, bot->num_vertices: %d", i, bot->faces[i*3+1], bot->num_vertices);
        if ((unsigned int)bot->faces[i*3+2] > (unsigned int)(bot->num_vertices)) bu_log("bot->faces[%d*3+2]: %d, bot->num_vertices: %d", i, bot->faces[i*3+2], bot->num_vertices);
    } 
    /* Pre-compute face areas once */
    for (unsigned int i = 0; i < bot->num_faces; i++) {
	face_areas[i] = face_area(bot, i);
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
    /* Determine the maximun number of faces associated with any one vertex */
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
    for (unsigned int i = 0; i < bot->num_faces; i++) {
	for (unsigned int j = 0; j < 3; j++) {
            unsigned int k = 0;
	    unsigned int vert = bot->faces[i*3+j];
            /* rows are vertex indexes, columns hold the faces */
	    while (vert_to_face[max_face_cnt * vert + k] && k < max_face_cnt) k++;
            /* Need to increment face index by one so we can reference 
	     * the first face and still use the true/false test that 0 allows */
	    vert_to_face[max_face_cnt * vert + k] = i + 1;
            //bu_log("vert_to_face(%d,%d)[%d] = %d\n", vert, k, max_face_cnt * vert + k, i + 1); 
	}
    }
    
    /* Order the groups by number of bots */
    unsigned int group_cnt[6] = {0};
    unsigned int ordered_groups[6] = {0};
    for (unsigned int i = 0; i < bot->num_faces; i++) {
        group_cnt[groups[i]]++;
    }
    for (unsigned int i = 0; i < 6; i++) {
        unsigned int more = 5;
	for (unsigned int j = 0; j < 6; j++) {
            if (group_cnt[j] > group_cnt[i]) more--;
	}
        ordered_groups[more] = i;
    }



    // All faces must belong to some patch - continue until all faces are processed
    unsigned int patch_cnt = 0;
    for (unsigned int i = 0; i < 6; i++) {
	int largest_face = 0;
	while (largest_face != -1) {
	    // Start a new patch
	    vect_t largest_face_normal;
	    largest_face = -1;
	    patch_cnt++;
	    // Find largest remaining face in group
	    double face_size_criteria = 0.0;
	    for (unsigned int j = 0; j < bot->num_faces; j++) {
		if (ordered_groups[i] == groups[j] && !patches[j] && (face_areas[j] > face_size_criteria)) {
		    largest_face = j;
		    face_size_criteria = face_areas[j];
		}
	    }
	    if (largest_face != -1) {
		std::queue<unsigned int> face_queue;
		face_queue.push(largest_face);
		patches[largest_face] = patch_cnt;
		VSET(largest_face_normal, face_normals[largest_face*3], face_normals[largest_face*3+1], face_normals[largest_face*3+2]);
		while (!face_queue.empty()) {
		    unsigned int face_num = face_queue.front();
		    face_queue.pop();
		    for (unsigned int k = 0; k < 3; k++) {
			unsigned int vert_id = bot->faces[face_num*3+k]; 
			for (unsigned int l = 0; l < vert_face_cnt[vert_id]; l++) {
			    unsigned int new_face = vert_to_face[max_face_cnt * vert_id  + l] - 1;
			    if (groups[new_face] == ordered_groups[i] && !patches[new_face]) {
				if (face_areas[new_face] > face_areas[largest_face] * 0.1) {
				    vect_t new_face_normal;
				    VSET(new_face_normal, face_normals[new_face*3], face_normals[new_face*3+1], face_normals[new_face*3+2]);
				    if (VDOT(largest_face_normal, new_face_normal) > 0.65) {
					face_queue.push(new_face);
					patches[new_face] = patch_cnt;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
    bu_log("patch_cnt: %d\n", patch_cnt);
    bu_free(face_areas, "face_areas");
    bu_free(face_normals, "face_normals");
    bu_free(groups, "groups");
    bu_free(vert_to_face, "vert_to_face");

    unsigned int *patch_vert_cnt = (unsigned int *)bu_malloc(sizeof(unsigned int) * bot->num_vertices, "patch_vert_cnt");
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
            if (patch_vert_cnt[j] && patch_vert_cnt[j] != vert_face_cnt[j]) vert_edge_status[j]++;
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
