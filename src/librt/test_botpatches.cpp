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
//#include "opennurbs_fit.h"

typedef std::pair<size_t, size_t> Edge;
typedef std::multimap< size_t, Edge> VertToEdge;
typedef std::multimap< Edge, size_t > EdgeToFace;
typedef std::multimap< Edge, size_t > EdgeToPatch;
typedef std::set<Edge> EdgeList;
typedef std::set<EdgeList> CurveList;
typedef std::set<CurveList> LoopList;
typedef std::set<size_t> VertList;
typedef std::set<size_t> FaceList;

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

typedef std::multimap< ThreeDPoint, size_t > FaceCategorization;

class Patch {
      public:
        size_t id;
	ThreeDPoint *category_plane;
	EdgeList edges;
	LoopList loops;
        FaceList faces;
	CurveList outer_loop;
	bool operator< (const Patch &other) const {return id < other.id;}
};


typedef std::set<ThreeDPoint> VectList; 
#if 0
void PatchToVector3d(struct rt_bot_internal *bot, const Patch *patch, on_fit::vector_vec3d &data) {
	FaceList::iterator f_it;
	std::set<size_t> verts;
	std::set<size_t>::iterator v_it;
  int i = 0;
  for (i = 0; i < bot->num_vertices; i++) {
      printf("v(%d): %f %f %f\n", i, V3ARGS(&bot->vertices[3*i]));
  }
	for (f_it = patch->faces.begin(); f_it != patch->faces.end(); f_it++) {
		verts.insert(bot->faces[(*f_it)*3+0]);
		verts.insert(bot->faces[(*f_it)*3+1]);
		verts.insert(bot->faces[(*f_it)*3+2]);
	}
	for (v_it = verts.begin(); v_it != verts.end(); v_it++) {	
		printf("vert %d\n", (int)(*v_it));
		printf("vert(%d): %f %f %f\n", (int)(*v_it), V3ARGS(&bot->vertices[(*v_it)*3]));
	        data.push_back (Eigen::Vector3d (V3ARGS(&bot->vertices[(*v_it)*3])));
	}
}
#endif
void find_edges(struct rt_bot_internal *bot, Patch *patch, FILE* plot, ON_SimpleArray<ON_NurbsCurve> *bedges, EdgeToPatch *edge_to_patch)
{
   size_t pt_A, pt_B, pt_C;
   VertToEdge vert_to_edge;
   EdgeList patch_edges;
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
	  vert_to_edge.insert(std::make_pair((*curredge).first.first, (*curredge).first)); 
	  vert_to_edge.insert(std::make_pair((*curredge).first.second, (*curredge).first)); 
          patch_edges.insert((*curredge).first);
       }
   }

   // We have the edges of the patch - break them down into loops;
   std::set<EdgeList> loops;
   while (!patch_edges.empty()) {
	   std::queue<Edge> edge_queue;
	   EdgeList curr_loop;
	   EdgeList::iterator e_it;
	   edge_queue.push(*patch_edges.begin());
	   while (!edge_queue.empty()) {
		   VertToEdge::iterator v_it;
		   std::pair<VertToEdge::iterator, VertToEdge::iterator> v_range;
		   Edge curr_edge = edge_queue.front();
		   edge_queue.pop();
		   curr_loop.insert(curr_edge);
		   
		   // use verttoedge - pull other two edges from multimap
		   v_range = vert_to_edge.equal_range(curr_edge.first);
		   for (v_it = v_range.first; v_it != v_range.second; v_it++) {
			   if ((*v_it).second != curr_edge) {
				   if (patch_edges.find((*v_it).second) != patch_edges.end()) {
					   edge_queue.push((*v_it).second);
					   patch_edges.erase((*v_it).second);
				   }
			   }
		   }
		   v_range = vert_to_edge.equal_range(curr_edge.second);
		   for (v_it = v_range.first; v_it != v_range.second; v_it++) {
			   if ((*v_it).second != curr_edge) {
				   if (patch_edges.find((*v_it).second) != patch_edges.end()) {
					   edge_queue.push((*v_it).second);
					   patch_edges.erase((*v_it).second);
				   }
			   }
		   }
	   }
	   loops.insert(curr_loop);
   }

   std::cout << "loop count: " << loops.size() << "\n";

   // We have loops - break them into curves.  A curve is shared between two
   // and only two patches.
   while (!loops.empty()) {
         EdgeList curr_loop = *loops.begin();
         loops.erase(loops.begin());
	 CurveList loop_curves;
	 while (!curr_loop.empty()) {
		 size_t other_id; 
		 EdgeList curve_edges;
		 std::queue<Edge> edge_queue;
		 EdgeList::iterator e_it;
		 EdgeToPatch::iterator ep_it;
		 std::pair<EdgeToPatch::iterator, EdgeToPatch::iterator> ep_range;
		 edge_queue.push(*curr_loop.begin());
		 // pull non-this patch ide from loop_edges.begin() - this will be 
		 // the id tested for when pulling other connected edges
		 ep_range = edge_to_patch->equal_range(*curr_loop.begin());
		 for (ep_it = ep_range.first; ep_it != ep_range.second; ep_it++) {
			 if ((*ep_it).second != patch->id) other_id = (*ep_it).second;
		 }
		 curr_loop.erase(curr_loop.begin());
		 while (!edge_queue.empty()) {
			 size_t pt1, pt2;
			 Edge e1, e2;
			 VertToEdge::iterator v_it;
			 std::pair<VertToEdge::iterator, VertToEdge::iterator> v_range;
			 Edge curr_edge = edge_queue.front();
			 edge_queue.pop();
			 curve_edges.insert(curr_edge);
			 pt1 = curr_edge.first;
			 pt2 = curr_edge.second;

			 // use verttoedge - pull other two edges from multimap
			 v_range = vert_to_edge.equal_range(pt1);
			 for (v_it = v_range.first; v_it != v_range.second; v_it++) {
				 if ((*v_it).second != curr_edge) {
					 if (curr_loop.find((*v_it).second) != curr_loop.end()) {
						 e1 = (*v_it).second;
					 }
				 }
			 }
			 v_range = vert_to_edge.equal_range(pt2);
			 for (v_it = v_range.first; v_it != v_range.second; v_it++) {
				 if ((*v_it).second != curr_edge) {
					 if (curr_loop.find((*v_it).second) != curr_loop.end()) {
						 e2 = (*v_it).second;
					 }
				 }
			 }

			 // use edgetopatch - pull non-this patch id from multimap for other two connected edges
			 ep_range = edge_to_patch->equal_range(e1);
			 for (ep_it = ep_range.first; ep_it != ep_range.second; ep_it++) {
				 if ((*ep_it).second != patch->id) {
					 if (other_id == (*ep_it).second) {
						 edge_queue.push(e1);
						 curr_loop.erase(e1);
					 }
				 }
			 }
			 ep_range = edge_to_patch->equal_range(e2);
			 for (ep_it = ep_range.first; ep_it != ep_range.second; ep_it++) {
				 if ((*ep_it).second != patch->id) {
					 if (other_id == (*ep_it).second) {
						 edge_queue.push(e2);
						 curr_loop.erase(e2);
					 }
				 }
			 }
		 }
		 loop_curves.insert(curve_edges);
	 }
	 patch->loops.insert(loop_curves);
   }


   // If we have more than one loop, we need to identify the outer loop.  OpenNURBS handles outer
   // and inner loops separately, so we need to know which loop is our outer surface boundary.
   LoopList patch_loops = patch->loops;
   if (patch_loops.size() > 1) {
	   LoopList::iterator l_it;
	   fastf_t diag_max = 0.0;
	   CurveList outer_loop;
	   for (l_it = patch_loops.begin(); l_it != patch_loops.end(); l_it++) {
		   vect_t min, max, diag;
		   VSETALL(min, MAX_FASTF);
		   VSETALL(max, -MAX_FASTF);
		   CurveList::iterator c_it;
		   for (c_it = (*l_it).begin(); c_it != (*l_it).end(); c_it++) {
			   EdgeList::iterator e_it;
			   for (e_it = (*c_it).begin(); e_it != (*c_it).end(); e_it++) {
				   vect_t ep1, ep2, eproj, eproj2, norm_scale, normal;
				   fastf_t dotP;
				   VMOVE(ep1, &bot->vertices[(*e_it).first]);
				   VMOVE(ep2, &bot->vertices[(*e_it).second]);
				   VSET(normal, patch->category_plane->x, patch->category_plane->y, patch->category_plane->z);
				   dotP = VDOT(ep1, normal);
				   VSCALE(norm_scale, normal, dotP);
				   VSUB2(eproj, ep1, norm_scale);
				   VSCALE(eproj2, eproj, 1.1);
				   VMINMAX(min, max, eproj);

				   dotP = VDOT(ep2, normal);
				   VSCALE(norm_scale, normal, dotP);
				   VSUB2(eproj, ep2, norm_scale);
				   VSCALE(eproj2, eproj, 1.1);
				   VMINMAX(min, max, eproj);
			   }
		   }
		   VSUB2(diag, max, min);

		   if (MAGNITUDE(diag) > diag_max) {
			   diag_max = MAGNITUDE(diag);
			   patch->outer_loop = (*l_it);
		   }
	   }
	   patch->loops.erase(patch->loops.find(patch->outer_loop));
#if 0
	   // Plot loops
	   pl_color(plot, int(256*drand48() + 1.0), int(256*drand48() + 1.0), int(256*drand48() + 1.0));
	   LoopList plot_patch_loops = patch->loops;
	   LoopList::iterator p_l_it;
	   for (p_l_it = plot_patch_loops.begin(); p_l_it != plot_patch_loops.end(); p_l_it++) {
		   CurveList::iterator c_it;
		   for (c_it = (*p_l_it).begin(); c_it != (*p_l_it).end(); c_it++) {
			   EdgeList::iterator e_it;
			   for (e_it = (*c_it).begin(); e_it != (*c_it).end(); e_it++) {
				   pdv_3move(plot, &bot->vertices[(*e_it).first]); 
				   pdv_3cont(plot, &bot->vertices[(*e_it).second]);
			   }
		   }
	   }
	   pl_color(plot, 255, 255, 255);
	   CurveList::iterator c_it;
	   for (c_it = patch->outer_loop.begin(); c_it != patch->outer_loop.end(); c_it++) {
		   EdgeList::iterator e_it;
		   for (e_it = (*c_it).begin(); e_it != (*c_it).end(); e_it++) {
			   pdv_3move(plot, &bot->vertices[(*e_it).first]); 
			   pdv_3cont(plot, &bot->vertices[(*e_it).second]);
		   }
	   }
#endif
   } else {
	   patch->outer_loop = (*patch_loops.begin());
	   patch_loops.erase(patch_loops.begin());
   }

   // Make some edge curves
   if (patch->outer_loop.size() > 1) {
	   CurveList::iterator c_it;
	   for (c_it = patch->outer_loop.begin(); c_it != patch->outer_loop.end(); c_it++) {
		   if ((*c_it).size() > 2) {
		   pl_color(plot, int(256*drand48() + 1.0), int(256*drand48() + 1.0), int(256*drand48() + 1.0));
		   EdgeList::iterator e_it;
		   std::map<size_t, size_t> pnt_cnt;
		   std::multimap<size_t, size_t> vert_map;
		   std::set<size_t>::iterator pnt_it;
		   ON_3dPointArray curve_pnts;
		   for (e_it = (*c_it).begin(); e_it != (*c_it).end(); e_it++) {
			   pnt_cnt[(*e_it).first]++;
			   pnt_cnt[(*e_it).second]++;
			   vert_map.insert(std::make_pair((*e_it).first, (*e_it).second));
			   vert_map.insert(std::make_pair((*e_it).second, (*e_it).first));
		   }
		   int endpt_cnt = 0;
		   std::pair<size_t, size_t> start_and_end;
		   std::map<size_t, size_t>::iterator m_it;
		   for (m_it = pnt_cnt.begin(); m_it != pnt_cnt.end(); m_it++) {
			   if ((*m_it).second == 1 && endpt_cnt == 0) {
				   start_and_end.first = (*m_it).first;
				   endpt_cnt = 1;
			   } else if ((*m_it).second == 1 && endpt_cnt == 1) {
				   start_and_end.second = (*m_it).first;
				   endpt_cnt = 2;
			   }
		   }
                   if (endpt_cnt > 1) {

		   size_t current_pt = start_and_end.first;
		   std::set<size_t> visited;

		   while (current_pt != start_and_end.second) {
			   printf("current_pt: %d\n", (int)current_pt);
			   std::multimap<size_t, size_t>::iterator v_it;
			   std::pair<std::multimap<size_t, size_t>::iterator, std::multimap<size_t, size_t>::iterator> v_range;
			   curve_pnts.Append(ON_3dPoint(&bot->vertices[current_pt]));
			   visited.insert(current_pt);
			   v_range = vert_map.equal_range(current_pt);
			   for (v_it = v_range.first; v_it != v_range.second; v_it++) {
			   //printf("(*v_it).second: %d\n", (int)(*v_it).second);
				if ((*v_it).second != current_pt && visited.find((*v_it).second) == visited.end()) {
				   printf("using %d\n", (int)(*v_it).second);
				   current_pt = (*v_it).second;
				}
			   }
		   }
		   curve_pnts.Append(ON_3dPoint(&bot->vertices[start_and_end.second]));

		   ON_BezierCurve curve_bez((const ON_3dPointArray)curve_pnts);
		   ON_NurbsCurve *curve_nurb = ON_NurbsCurve::New();
		   curve_bez.GetNurbForm(*curve_nurb);
		   curve_nurb->SetDomain(0.0, 1.0);
		   //bedges->Append(*curve_nurb);

		   double pt1[3], pt2[3];
		   int plotres = 1000;
		   ON_Interval dom = curve_nurb->Domain();
		   for (int i = 1; i <= plotres; i++) {
			   ON_3dPoint p = curve_nurb->PointAt(dom.ParameterAt((double) (i - 1)
						   / (double)plotres));
			   VMOVE(pt1, p);
			   p = curve_nurb->PointAt(dom.ParameterAt((double) i / (double)plotres));
			   VMOVE(pt2, p);
			   pdv_3move(plot, pt1); 
			   pdv_3cont(plot, pt2);
		   }
}
}

	   }
   } else {
	// use PCL fitting routine for a full loop fit.
   }


}

void make_structure(struct rt_bot_internal *bot, VectList *vects, std::set<Patch> *patches, ON_SimpleArray<ON_NurbsCurve> *bedges, FILE* plot) {
        FaceCategorization face_groups;
        FaceCategorization::iterator fg_1, fg_2;
        EdgeToFace edge_to_face;
        EdgeToPatch edge_to_patch;
        int patch_cnt = 1;
	
        VectList::iterator vect_it;
        // Break apart the bot based on angles between faces and bounding rpp planes
        for (size_t i=0; i < bot->num_faces; ++i) {
		size_t pt_A, pt_B, pt_C;
		size_t count = 0;
                const ThreeDPoint *result_max;
		fastf_t vdot = 0.0;
                fastf_t result = 0.0;
		vect_t a, b, dir, norm_dir;
                // Add vert -> edge and edge -> face mappings to global map.
		pt_A = bot->faces[i*3+0]*3;
		pt_B = bot->faces[i*3+1]*3; 
		pt_C = bot->faces[i*3+2]*3;
		if (pt_A <= pt_B) {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_A, pt_B), i));
		} else {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_B, pt_A), i));
		}
		if (pt_B <= pt_C) {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_B, pt_C), i));
		} else {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_C, pt_B), i));
		}
		if (pt_C <= pt_A) {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_C, pt_A), i));
		} else {
                        edge_to_face.insert(std::make_pair(std::make_pair(pt_A, pt_C), i));
		}
                // Categorize face
		VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
		VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
		VCROSS(norm_dir, a, b);
		VUNITIZE(norm_dir);
		for (vect_it = vects->begin(); vect_it != vects->end(); vect_it++) {
			count++;
			VSET(dir, (*vect_it).x, (*vect_it).y, (*vect_it).z);
			VUNITIZE(dir);
			vdot = VDOT(dir, norm_dir);
			//std::cout << "vector(" << count << "): (" << dir[0] << "," << dir[1] << "," << dir[2] << ") . ";
			//std::cout << "(" << norm_dir[0] << "," << norm_dir[1] << "," << norm_dir[2] << ") =  " << vdot << ">?" << result << "\n";
                        if (vdot > result) {
                           result_max = &(*vect_it);
                           result = vdot;
                        }
                
		}
		face_groups.insert(std::make_pair(*result_max, i));
		//std::cout << "Face " << i << ": " << result_max << "," << result << "\n";
        }
        // For each "sub-bot", identify contiguous patches.
        int face_grp = 0;
	for( fg_1 = face_groups.begin(); fg_1 != face_groups.end(); fg_1 = fg_2 ) {
	        face_grp++;
                //std::cout << "Face Group " << face_grp << " faces: \n";
		// Make a list of all faces associated with the current category
		FaceList group_faces, second_tier;
		std::pair<FaceCategorization::iterator, FaceCategorization::iterator> currkey = face_groups.equal_range((*fg_1).first);
		for (fg_2 = currkey.first; fg_2 != currkey.second; fg_2++) {
			size_t curr_face = (*fg_2).second;
			group_faces.insert(curr_face);
			//std::cout << curr_face << " ";
		}
		//std::cout << "\n";

		// All faces must belong to some patch - continue until all faces are processed
		while (!group_faces.empty()) {
			std::pair<EdgeToFace::iterator, EdgeToFace::iterator> edgekey;
			EdgeToFace::iterator edge_it;
			size_t pt_A, pt_B, pt_C;
			Edge e1, e2, e3;
			Patch curr_patch; 
                        curr_patch.id = patch_cnt;
			curr_patch.category_plane = new ThreeDPoint((*fg_1).first.x, (*fg_1).first.y, (*fg_1).first.z);
                        patch_cnt++;
			std::queue<size_t> face_queue;
			FaceList::iterator f_it;
			face_queue.push(*(group_faces.begin()));
			while (!face_queue.empty()) {
				std::set<Edge> face_edges;
				std::set<Edge>::iterator face_edges_it;
				size_t face_num = face_queue.front();
				face_queue.pop();
				curr_patch.faces.insert(face_num);
				group_faces.erase(face_num);
				pt_A = bot->faces[face_num*3+0]*3;
				pt_B = bot->faces[face_num*3+1]*3; 
				pt_C = bot->faces[face_num*3+2]*3;
				if (pt_A <= pt_B) {
					e1.first = pt_A;
					e1.second= pt_B;
				} else {
					e1.first = pt_B;
					e1.second= pt_A;
				}
				face_edges.insert(e1);
				edge_to_patch.insert(std::make_pair(e1, curr_patch.id)); 
				if (pt_B <= pt_C) {
					e2.first = pt_B;
					e2.second= pt_C;
				} else {
					e2.first = pt_C;
					e2.second= pt_B;
				}
				face_edges.insert(e2);
				edge_to_patch.insert(std::make_pair(e2, curr_patch.id)); 
				if (pt_C <= pt_A) {
					e3.first = pt_C;
					e3.second= pt_A;
				} else {
					e3.first = pt_A;
					e3.second= pt_C;
				}
				face_edges.insert(e3);
				edge_to_patch.insert(std::make_pair(e3, curr_patch.id)); 


				int edge_cnt = 0;
				for( face_edges_it = face_edges.begin(); face_edges_it != face_edges.end(); face_edges_it++ ) {
					edge_cnt++;
					edgekey = edge_to_face.equal_range((*face_edges_it));
					//std::cout << "Face(" << face_num << ")(" << edge_cnt << "): ";
					for( edge_it = edgekey.first; edge_it != edgekey.second; edge_it++ ) {
						size_t face_from_edge = (*edge_it).second;
						FaceList::iterator this_it = group_faces.find(face_from_edge);
						//std::cout  << face_from_edge << " ";
						if (this_it != group_faces.end()) {
							//std::cout  << "found(" << face_from_edge << ") ";
							face_queue.push(face_from_edge);
							group_faces.erase(face_from_edge);
						}
					}
					//std::cout  << "\n";
				}

			}
			//std::cout << "Patch: " << curr_patch.id << "\n";
                        find_edges(bot, &curr_patch, plot, bedges, &edge_to_patch);
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
   //struct rt_wdb *wdbp;
   struct bu_vls name;
   //char *bname;
    
   VectList vectors;
   std::set<Patch> patches;
   std::set<Patch>::iterator p_it;
   //ON_Brep *brep = ON_Brep::New();
   FaceList::iterator f_it;

   ON_SimpleArray<ON_NurbsCurve> edges;

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

   make_structure(bot_ip, &vectors, &patches, &edges, plot); 
#if 0
   for (p_it = patches.begin(); p_it != patches.end(); p_it++) {
       std::cout << "Found patch " << (*p_it).id << "\n";
       unsigned order (3);
       on_fit::NurbsDataSurface data;
       PatchToVector3d(bot_ip, &(*p_it), data.interior);
       ON_NurbsSurface nurbs = on_fit::FittingSurface::initNurbsPCABoundingBox (order, &data);
       on_fit::FittingSurface fit (&data, nurbs);
       on_fit::FittingSurface::Parameter params;
       params.interior_smoothness = 0.15;
       params.interior_weight = 1.0;
       params.boundary_smoothness = 0.15;
       params.boundary_weight = 0.0;
       // NURBS refinement
       for (unsigned i = 0; i < 5; i++)
       {
	       fit.refine (0);
	       fit.refine (1);
       }

       // fitting iterations
       for (unsigned i = 0; i < 2; i++)
       {
	       fit.assemble (params);
	       bu_log("Solving %d - iteration %d\n", (*p_it).id, i);
	       fit.solve ();
       }
       brep->NewFace(fit.m_nurbs);
   }

   wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
   bname = (char*)bu_malloc(strlen(argv[2])+6, "char");
   bu_strlcpy(bname, argv[2], strlen(argv[2])+1);
   bu_strlcat(bname, "_brep", strlen(bname)+6);
   if (mk_brep(wdbp, bname, brep) == 0) {
      bu_log("Generated brep object %s\n", bname);
   }
   bu_free(bname, "char");
#endif

   return 0;
}
