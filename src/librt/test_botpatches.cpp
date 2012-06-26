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

typedef std::multimap< size_t, size_t > FaceList;
typedef FaceList::iterator FaceListIter;
typedef std::list< std::pair<size_t, size_t> > EdgeList;
typedef std::set<std::set<size_t> > PatchSet;

struct vector_list {
   struct bu_list l;
   vect_t v;
};

void find_patches(struct rt_bot_internal *bot, struct vector_list *vects, FaceList *faces) 
{
	struct vector_list *vect;
	for (size_t i=0; i < bot->num_faces; ++i) {
		size_t count = 0;
		size_t result_max = 0;
		fastf_t tmp = 0.0;
		fastf_t result = 0.0;
		vect_t a, b, vtmp, norm_dir;
		VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
		VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
		VCROSS(norm_dir, a, b);
		VUNITIZE(norm_dir);
		for (BU_LIST_FOR(vect, vector_list, &vects->l)) {
			VSET(vtmp, vect->v[0], vect->v[1], vect->v[2]);
			VUNITIZE(vtmp);
			tmp = VDOT(vtmp, norm_dir);
			if (tmp > result) {
				result_max = count;
				result = tmp;
			}
			count++;
		}
		faces->insert(std::make_pair(result_max, i));
	}
}

void split_patches(struct rt_bot_internal *bot, FaceList *faces, PatchSet *patches) 
{
   FaceListIter m_it, s_it; 
   for( m_it = faces->begin(); m_it != faces->end(); m_it = s_it ) {
         std::set<size_t> all_faces;
         std::pair<FaceListIter, FaceListIter> currkey = faces->equal_range((*m_it).first);
         for (s_it = currkey.first; s_it != currkey.second; s_it++) {
            size_t curr_face = (*s_it).second;
            all_faces.insert(curr_face);
         }
	 while (!all_faces.empty()) {
		 size_t a, b, c;
		 std::queue<size_t> face_queue;
		 std::set<size_t> patch_faces;
		 std::set<size_t>::iterator f_it;

		 face_queue.push(*all_faces.begin());
		 patch_faces.insert(*all_faces.begin());
		 all_faces.erase(all_faces.begin());
		 while (!face_queue.empty()) {
			 size_t face_num = face_queue.front();
			 face_queue.pop();
			 a = bot->faces[face_num*3+0];
			 b = bot->faces[face_num*3+1];
			 c = bot->faces[face_num*3+2];
			 for (f_it = all_faces.begin(); f_it != all_faces.end(); f_it++) {
				 size_t curr_a, curr_b, curr_c;
				 curr_a = bot->faces[*f_it*3+0];
				 curr_b = bot->faces[*f_it*3+1];
				 curr_c = bot->faces[*f_it*3+2];
				 if ((a == curr_a && b == curr_b) ||
						 (a == curr_b && b == curr_a) ||
						 (a == curr_b && b == curr_c) ||
						 (a == curr_c && b == curr_b) ||
						 (a == curr_a && b == curr_c) ||
						 (a == curr_c && b == curr_a) ||
						 (a == curr_a && c == curr_b) ||
						 (a == curr_b && c == curr_a) ||
						 (a == curr_b && c == curr_c) ||
						 (a == curr_c && c == curr_b) ||
						 (a == curr_a && c == curr_c) ||
						 (a == curr_c && c == curr_a) ||
						 (b == curr_a && c == curr_b) ||
						 (b == curr_b && c == curr_a) ||
						 (b == curr_b && c == curr_c) ||
						 (b == curr_c && c == curr_b) ||
						 (b == curr_a && c == curr_c) ||
						 (b == curr_c && c == curr_a)) {
					 /* Found a shared edge of a neighboring triangle */
					 patch_faces.insert(*f_it);
                                         face_queue.push(*f_it);
					 all_faces.erase(f_it);
				 }
			 }
		 }
		 patches->insert(patch_faces);
	 }
   }
}

void find_edges(struct rt_bot_internal *bot, FILE *plot, EdgeList *edges)
{
   size_t i;
   size_t pt_A, pt_B, pt_C;
   std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
   for (i = 0; i < bot->num_faces; ++i) {
       pt_A = bot->faces[i*3+0]*3;
       pt_B = bot->faces[i*3+1]*3; 
       pt_C = bot->faces[i*3+2]*3;
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
	  //std::cout << "(" << (*curredge).first.first << "," << (*curredge).first.second << ")" << ": ";
	  pdv_3move(plot, &bot->vertices[(*curredge).first.first]); 
	  pdv_3cont(plot, &bot->vertices[(*curredge).first.second]);
          edges->push_back((*curredge).first);
       }
   }
}


void find_edges2(struct rt_bot_internal *bot, const std::set<size_t> *faces, FILE *plot, EdgeList *edges)
{
   size_t pt_A, pt_B, pt_C;
   std::set<size_t>::iterator it;
   std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
   for (it = faces->begin(); it != faces->end(); it++) {
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
	  //std::cout << "(" << (*curredge).first.first << "," << (*curredge).first.second << ")" << ": ";
	  pdv_3move(plot, &bot->vertices[(*curredge).first.first]); 
	  pdv_3cont(plot, &bot->vertices[(*curredge).first.second]);
          edges->push_back((*curredge).first);
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
//   struct rt_bot_list *headRblp = NULL;
 //  struct rt_bot_list *split_bots = NULL;
  // struct rt_db_internal bot_intern;
//   struct rt_bot_list *rblp;
 //  struct rt_bot_list *splitlp;
   struct vector_list *vects;
   struct vector_list *vect;
   struct bu_vls name;
   static FILE* plot = NULL;
   point_t min, max;
   FaceList faces;
   PatchSet patches;

   int i = 0;
  // int j = 0;

   BU_GET(vects, struct vector_list);
   BU_LIST_INIT(&(vects->l));
   VSET(vects->v, 0, 0, 1);
   BU_GET(vect, struct vector_list);
   VSET(vect->v, -1,0,0);
   BU_LIST_PUSH(&(vects->l), &(vect->l));
   BU_GET(vect, struct vector_list);
   VSET(vect->v, 0,-1,0);
   BU_LIST_PUSH(&(vects->l), &(vect->l));
   BU_GET(vect, struct vector_list);
   VSET(vect->v, 0,0,-1);
   BU_LIST_PUSH(&(vects->l), &(vect->l));
   BU_GET(vect, struct vector_list);
   VSET(vect->v, 1,0,0);
   BU_LIST_PUSH(&(vects->l), &(vect->l));
   BU_GET(vect, struct vector_list);
   VSET(vect->v, 0,1,0);
   BU_LIST_PUSH(&(vects->l), &(vect->l));
   BU_GET(vect, struct vector_list);
   VSET(vect->v, 0,0,1);
   BU_LIST_PUSH(&(vects->l), &(vect->l)); 

   bu_vls_init(&name);

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

   find_patches(bot_ip, vects, &faces);
   split_patches(bot_ip, &faces, &patches);
 
   plot = fopen("bots.pl", "w");
   VSET(min, -2048, -2048, -2048);
   VSET(max, 2048, 2048, 2048);
   pdv_3space(plot, min, max);

   std::set<std::set<size_t> >::iterator patch_it;         
   for( patch_it = patches.begin(); patch_it != patches.end(); patch_it++ ) {
	std::set<size_t>::iterator face_it;
	for( face_it = (*patch_it).begin(); face_it != (*patch_it).end(); face_it++ ) {
	  pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+0]*3]); 
	  pdv_3cont(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+1]*3]); 
	  pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+1]*3]); 
	  pdv_3cont(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+2]*3]); 
	  pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+2]*3]); 
	  pdv_3cont(plot, &bot_ip->vertices[bot_ip->faces[(*face_it)*3+0]*3]); 
        }
   }

   plot = fopen("out.pl", "w");
   VSET(min, -2048, -2048, -2048);
   VSET(max, 2048, 2048, 2048);
   pdv_3space(plot, min, max);

//   headRblp = rt_bot_patches(bot_ip);

//   int bot_cnt = 0;
   i = 0;

   //std::set<std::set<size_t> >::iterator patch_it;         
   for( patch_it = patches.begin(); patch_it != patches.end(); patch_it++ ) {
        EdgeList edges;
        find_edges2(bot_ip, &(*patch_it), plot, &edges);
	for( std::list<std::pair<size_t, size_t> >::iterator curredge=edges.begin(); curredge!=edges.end(); curredge++) {
		std::cout << "(" << (*curredge).first << "," << (*curredge).second << ")" << "\n";
	}

   }

#if 0
   for (BU_LIST_FOR(rblp, rt_bot_list, &headRblp->l)) {
	   j = 0;
	   split_bots = rt_bot_split(rblp->bot);
	   for (BU_LIST_FOR(splitlp, rt_bot_list, &split_bots->l)) {
		   EdgeList edges;
		   //std::cout << "BoT " << i << "," << j << "\n";
		   find_edges(splitlp->bot, plot, &edges);
		   for( std::list<std::pair<size_t, size_t> >::iterator curredge=edges.begin(); curredge!=edges.end(); curredge++) {
			   //std::cout << "(" << (*curredge).first << "," << (*curredge).second << ")" << "\n";
                   }
                   bot_cnt++;
		   bu_vls_sprintf(&name, "auto-%d_%d.bot", i, j);
		   RT_DB_INTERNAL_INIT(&bot_intern);
		   bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		   bot_intern.idb_type = ID_BOT;
		   bot_intern.idb_meth = &rt_functab[ID_BOT];
		   bot_intern.idb_ptr = (genptr_t)splitlp->bot;
		   dp = db_diradd(dbip, bu_vls_addr(&name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&bot_intern.idb_type);
		   if (dp == RT_DIR_NULL) {
			   printf("auuugh - dp says diradd failed!\n");
		   } else {
			   if (rt_db_put_internal(dp, dbip, &bot_intern, &rt_uniresource) < 0) {
				   printf("auuugh - put_internal failed!\n");
			   }
		   }
                   j++;
	   }
	   i++;
   }
std::cout << "BoT Count: " << bot_cnt << "\n";
   rt_bot_list_free(headRblp, 0);
   rt_db_free_internal(&intern);
   bu_vls_free(&name); 
#endif
   return 0;
}
