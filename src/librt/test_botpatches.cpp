#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <iostream>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"

typedef std::multimap< size_t, size_t > FaceCategorization;
typedef std::multimap< size_t, size_t > VertToFace;
typedef std::pair<size_t, size_t> Edge;
typedef std::set<Edge> EdgeList;
typedef std::set<EdgeList> LoopList;
typedef std::set<size_t> VertList;
typedef std::set<size_t> FaceList;

class Patch {
      public:
        int id;
	EdgeList edges;
	LoopList loops;
        FaceList faces;
	bool operator< (const Patch &other) const {return id < other.id;}
};

class ThreeDPoint {
      public:
          fastf_t x, y, z;
          ThreeDPoint (fastf_t, fastf_t, fastf_t);
          bool operator< (const ThreeDPoint &other) const {return (100*x + 10*y + z) < (100*other.x + 10*other.y + other.z);}
};

ThreeDPoint::ThreeDPoint(fastf_t a, fastf_t b, fastf_t c) {
     x = a;
     y = b;
     z = c;
}

typedef std::set<ThreeDPoint> VectList; 

void strip_noedge_matches(struct rt_bot_internal *bot, Patch *patch, FaceList *face_pool, VertToFace *vert_to_face)
{
	size_t vert_A, vert_B, vert_C;
	std::set<size_t>::iterator it;
	std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
	std::pair<VertToFace::iterator, VertToFace::iterator> vertkey;
	VertToFace::iterator vert_it;
	FaceList to_erase;
	size_t curr_patch_cnt = patch->faces.size();
        if (patch->faces.size() > 1) {
	for (it = patch->faces.begin(); it != patch->faces.end(); it++) {
	    if (curr_patch_cnt > 1) {
		VertList a_verts;
		VertList b_verts;
		VertList c_verts;
		int is_ok = 0;
		vert_A = bot->faces[(*it)*3+0]*3;
		vert_B = bot->faces[(*it)*3+1]*3;
		vert_C = bot->faces[(*it)*3+2]*3;
		vertkey = vert_to_face->equal_range(vert_A);
		for( vert_it = vertkey.first; vert_it != vertkey.second; ++vert_it ) {
			size_t face_from_vert = (*vert_it).second;
			if (face_from_vert != (*it)) {
				FaceList::iterator this_it = patch->faces.find(face_from_vert);
				if (this_it != patch->faces.end()) {
					a_verts.insert(bot->faces[face_from_vert*3+0]*3);
					a_verts.insert(bot->faces[face_from_vert*3+1]*3);
					a_verts.insert(bot->faces[face_from_vert*3+2]*3);
				}
			}
		}
		if (a_verts.find(vert_B) != a_verts.end() || a_verts.find(vert_C) != a_verts.end()) {
			is_ok = 1;
		} 
		if (!is_ok) {
			vertkey = vert_to_face->equal_range(vert_B);
			for( vert_it = vertkey.first; vert_it != vertkey.second; ++vert_it ) {
				size_t face_from_vert = (*vert_it).second;
				if (face_from_vert != (*it)) {
					FaceList::iterator this_it = patch->faces.find(face_from_vert);
					if (this_it != patch->faces.end()) {
						b_verts.insert(bot->faces[face_from_vert*3+0]*3);
						b_verts.insert(bot->faces[face_from_vert*3+1]*3);
						b_verts.insert(bot->faces[face_from_vert*3+2]*3);
					}
				}
			}
			if (b_verts.find(vert_A) != b_verts.end() || b_verts.find(vert_C) != b_verts.end()) {
				is_ok = 1;
			}
		} 

		if (!is_ok) {
			vertkey = vert_to_face->equal_range(vert_C);
			for( vert_it = vertkey.first; vert_it != vertkey.second; ++vert_it ) {
				size_t face_from_vert = (*vert_it).second;
				if (face_from_vert != (*it)) {
					FaceList::iterator this_it = patch->faces.find(face_from_vert);
					if (this_it != patch->faces.end()) {
						c_verts.insert(bot->faces[face_from_vert*3+0]*3);
						c_verts.insert(bot->faces[face_from_vert*3+1]*3);
						c_verts.insert(bot->faces[face_from_vert*3+2]*3);
					}
				}
			}
			if (c_verts.find(vert_A) != c_verts.end() || c_verts.find(vert_A) != c_verts.end()) {
				is_ok = 1;
			}
		} 
		if (!is_ok) {
			to_erase.insert((*it));
                        curr_patch_cnt--;
		}
             }
	}
	for (it = to_erase.begin(); it != to_erase.end(); it++) {
	    patch->faces.erase(*it);
	    face_pool->insert(*it);
	}
        }
}


void find_edges(struct rt_bot_internal *bot, Patch *patch, FILE* plot)
{
   size_t pt_A, pt_B, pt_C;
   std::set<size_t>::iterator it;
   std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
   for (it = patch->faces.begin(); it != patch->faces.end(); it++) {
       pt_A = bot->faces[(*it)*3+0]*3;
       pt_B = bot->faces[(*it)*3+1]*3; 
       pt_C = bot->faces[(*it)*3+2]*3;
       if (pt_A <= pt_B) {
          edge_face_cnt[std::make_pair(pt_A, pt_B)] += 1;
       } else {
          edge_face_cnt[std::make_pair(pt_B, pt_A)] += 1;
       }
       if (pt_B <= pt_C) {
          edge_face_cnt[std::make_pair(pt_B, pt_C)] += 1;
       } else {
          edge_face_cnt[std::make_pair(pt_C, pt_B)] += 1;
       }
       if (pt_C <= pt_A) {
          edge_face_cnt[std::make_pair(pt_C, pt_A)] += 1;
       } else {
          edge_face_cnt[std::make_pair(pt_A, pt_C)] += 1;
       }
   } 

   for( std::map<std::pair<size_t, size_t>, size_t>::iterator curredge=edge_face_cnt.begin(); curredge!=edge_face_cnt.end(); curredge++) {
       if ((*curredge).second == 1) {
	  //std::cout << "(" << (*curredge).first.first << "," << (*curredge).first.second << ") (";
	  //printf("%f,%f,%f), (%f,%f,%f)", V3ARGS(&bot->vertices[(*curredge).first.first]), V3ARGS(&bot->vertices[(*curredge).first.second]));
	  pdv_3move(plot, &bot->vertices[(*curredge).first.first]); 
	  pdv_3cont(plot, &bot->vertices[(*curredge).first.second]);
          patch->edges.insert((*curredge).first);
       }
   }
   //std::cout << "\n";
}

void make_structure(struct rt_bot_internal *bot, VectList *vects, std::set<Patch> *patches, FILE* plot) {
        FaceCategorization face_groups;
        FaceCategorization::iterator fg_1, fg_2;
        VertToFace vert_to_face;
        int patch_cnt = 1;
	
        VectList::iterator vect_it;
        // Break apart the bot based on angles between faces and bounding rpp planes
        for (size_t i=0; i < bot->num_faces; ++i) {
		size_t count = 0;
                size_t result_max = 0;
		fastf_t vdot = 0.0;
                size_t result = 0.0;
		vect_t a, b, dir, norm_dir;
                // Add vertex -> face mappings to global map.
		vert_to_face.insert(std::make_pair(bot->faces[i*3+0]*3, i));
		vert_to_face.insert(std::make_pair(bot->faces[i*3+1]*3, i));
		vert_to_face.insert(std::make_pair(bot->faces[i*3+2]*3, i));
                // Categorize face
		VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
		VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
		VCROSS(norm_dir, a, b);
		VUNITIZE(norm_dir);
		for (vect_it = vects->begin(); vect_it != vects->end(); vect_it++) {
			count++;
		//	std::cout << "vector: (" << (*vect_it).x << "," << (*vect_it).y << "," << (*vect_it).z << ")\n";
			VSET(dir, (*vect_it).x, (*vect_it).y, (*vect_it).z);
			VUNITIZE(dir);
			vdot = VDOT(dir, norm_dir);
                        if (vdot > result) {
                           result_max = count;
                           result = vdot;
                        }
                
		}
		face_groups.insert(std::make_pair(result_max, i));
        }
        // For each "sub-bot", identify contiguous patches.
	for( fg_1 = face_groups.begin(); fg_1 != face_groups.end(); fg_1 = fg_2 ) {
		// Make a list of all faces associated with the current category
		FaceList group_faces, second_tier;
		std::pair<FaceCategorization::iterator, FaceCategorization::iterator> currkey = face_groups.equal_range((*fg_1).first);
		for (fg_2 = currkey.first; fg_2 != currkey.second; fg_2++) {
			size_t curr_face = (*fg_2).second;
			group_faces.insert(curr_face);
		}
		// All faces must belong to some patch - continue until all faces are processed
		while (!group_faces.empty()) {
			std::pair<VertToFace::iterator, VertToFace::iterator> vertkey;
			VertToFace::iterator vert_it;
			//size_t a, b, c;
                        int i;
			Patch curr_patch; 
                        curr_patch.id = patch_cnt;
                        patch_cnt++;
			std::queue<size_t> face_queue;
			FaceList::iterator f_it;
			face_queue.push(*(group_faces.begin()));
			while (!face_queue.empty()) {
				size_t face_num = face_queue.front();
				face_queue.pop();
				curr_patch.faces.insert(face_num);
				group_faces.erase(face_num);
				for (i = 0; i < 3; i++) {
					vertkey = vert_to_face.equal_range(bot->faces[face_num*3+i]*3);
					for( vert_it = vertkey.first; vert_it != vertkey.second; ++vert_it ) {
						size_t face_from_vert = (*vert_it).second;
						FaceList::iterator this_it = group_faces.find(face_from_vert);
						if (this_it != group_faces.end()) {
							face_queue.push(face_from_vert);
							group_faces.erase(face_from_vert);
						}
					}
				}
			}
                        strip_noedge_matches(bot, &curr_patch, &second_tier, &vert_to_face);
			//std::cout << "Patch: " << curr_patch.id << "\n";
                        find_edges(bot, &curr_patch, plot);
			patches->insert(curr_patch);
                        if (group_faces.empty() && !second_tier.empty()) {
			   group_faces.swap(second_tier);
			}
		}
	}
}


int
main (int argc, char *argv[])
{
   struct db_i *dbip;
   struct directory *dp;
   struct rt_db_internal intern;
   struct rt_bot_internal *bot_ip = NULL;
   struct bu_vls name;
   VectList vectors;
   std::set<Patch> patches;
   std::set<Patch>::iterator p_it;

   vectors.insert(*(new ThreeDPoint(-1,0,0)));
   vectors.insert(*(new ThreeDPoint(0,-1,0)));
   vectors.insert(*(new ThreeDPoint(0,0,-1)));
   vectors.insert(*(new ThreeDPoint(1,0,0)));
   vectors.insert(*(new ThreeDPoint(0,1,0)));
   vectors.insert(*(new ThreeDPoint(0,0,1)));

   bu_vls_init(&name);

   static FILE* plot = NULL;
   point_t min, max;
   plot = fopen("edges.pl", "w");
   VSET(min, -2048, -2048, -2048);
   VSET(max, 2048, 2048, 2048);
   pdv_3space(plot, min, max);

   if (argc != 3) {
      bu_exit(1, "Usage: %s file.g object", argv[0]);
   }

   dbip = db_open(argv[1], "r+w");
   if (dbip == DBI_NULL) { 
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
   }

   if (db_dirbuild(dbip) < 0)
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
  
   dp = db_lookup(dbip, argv[2], LOOKUP_QUIET); 
   if (dp == RT_DIR_NULL) { 
      bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
   } else {
      bu_log("Got %s\n", dp->d_namep);
   }

   RT_DB_INTERNAL_INIT(&intern)
   if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
      bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
   }

   if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
      bu_exit(1, "ERROR: object %s does not appear to be of type BoT\n", argv[2]);
   } else {
      bot_ip = (struct rt_bot_internal *)intern.idb_ptr;   
   }
   RT_BOT_CK_MAGIC(bot_ip);

   make_structure(bot_ip, &vectors, &patches, plot); 

   for (p_it = patches.begin(); p_it != patches.end(); p_it++) {
       //std::cout << "Found patch " << (*p_it).id << "\n";
   }

   return 0;
}
