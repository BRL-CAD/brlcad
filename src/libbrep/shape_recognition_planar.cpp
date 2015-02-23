#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"


int
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	if (GetSurfaceType(data->brep->m_F[data->faces[i]].SurfaceOf(), NULL) != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;
    return 1;
}



void
subbrep_planar_init(struct subbrep_object_data *sdata)
{
    struct subbrep_object_data *data = sdata->parent;

    if (!data) return;
    if (data->planar_obj) return;
    BU_GET(data->planar_obj, struct subbrep_object_data);
    subbrep_object_init(data->planar_obj, data->brep);
    bu_vls_sprintf(data->planar_obj->key, "%s_planar", bu_vls_addr(data->key));


    data->planar_obj->local_brep = ON_Brep::New();
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> loop_map;
    std::map<int, int> c3_map;
    std::map<int, int> c2_map;
    std::map<int, int> trim_map;
    std::set<int> faces;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int> isolated_trims;  // collect 2D trims whose parent loops aren't fully included here
    array_to_set(&faces, data->faces, data->faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);
    array_to_set(&loops, data->loops, data->loops_cnt);

    for (int i = 0; i < data->edges_cnt; i++) {
        int c3i;
        const ON_BrepEdge *old_edge = &(data->brep->m_E[data->edges[i]]);
        //std::cout << "old edge: " << old_edge->Vertex(0)->m_vertex_index << "," << old_edge->Vertex(1)->m_vertex_index << "\n";

        // Get the 3D curves from the edges
        if (c3_map.find(old_edge->EdgeCurveIndexOf()) == c3_map.end()) {
            ON_Curve *nc = old_edge->EdgeCurveOf()->Duplicate();
	    // Don't continue unless the edge is linear
            ON_Curve *tc = old_edge->EdgeCurveOf()->Duplicate();
	    if (tc->IsLinear()) {
		c3i = data->planar_obj->local_brep->AddEdgeCurve(nc);
		c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
	    } else {
		continue;
	    }
        } else {
            c3i = c3_map[old_edge->EdgeCurveIndexOf()];
        }



        // Get the vertices from the edges
        int v0i, v1i;
        if (vertex_map.find(old_edge->Vertex(0)->m_vertex_index) == vertex_map.end()) {
            ON_BrepVertex& newv0 = data->planar_obj->local_brep->NewVertex(old_edge->Vertex(0)->Point(), old_edge->Vertex(0)->m_tolerance);
            v0i = newv0.m_vertex_index;
            vertex_map[old_edge->Vertex(0)->m_vertex_index] = v0i;
        } else {
            v0i = vertex_map[old_edge->Vertex(0)->m_vertex_index];
        }
        if (vertex_map.find(old_edge->Vertex(1)->m_vertex_index) == vertex_map.end()) {
            ON_BrepVertex& newv1 = data->planar_obj->local_brep->NewVertex(old_edge->Vertex(1)->Point(), old_edge->Vertex(1)->m_tolerance);
            v1i = newv1.m_vertex_index;
            vertex_map[old_edge->Vertex(1)->m_vertex_index] = v1i;
        } else {
            v1i = vertex_map[old_edge->Vertex(1)->m_vertex_index];
        }
        ON_BrepEdge& new_edge = data->planar_obj->local_brep->NewEdge(data->planar_obj->local_brep->m_V[v0i], data->planar_obj->local_brep->m_V[v1i], c3i, NULL ,0);
        edge_map[old_edge->m_edge_index] = new_edge.m_edge_index;
        //std::cout << "new edge: " << v0i << "," << v1i << "\n";

        // Get the 2D curves from the trims
        for (int j = 0; j < old_edge->TrimCount(); j++) {
            ON_BrepTrim *old_trim = old_edge->Trim(j);
            if (faces.find(old_trim->Face()->m_face_index) != faces.end()) {
                if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
                    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
                    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
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
                    ON_Surface *ts = old_trim->Face()->SurfaceOf()->Duplicate();
		    if (ts->IsPlanar(NULL, BREP_PLANAR_TOL)) {
			int nsid = data->planar_obj->local_brep->AddSurface(ns);
			surface_map[old_trim->Face()->SurfaceIndexOf()] = nsid;
			ON_BrepFace &new_face = data->planar_obj->local_brep->NewFace(nsid);
			face_map[old_trim->Face()->m_face_index] = new_face.m_face_index;
			if (fil.find(old_trim->Face()->m_face_index) != fil.end()) {
			    data->planar_obj->local_brep->FlipFace(new_face);
			}
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
                        ON_BrepLoop &nl = data->planar_obj->local_brep->NewLoop(ON_BrepLoop::outer, data->planar_obj->local_brep->m_F[face_map[old_loop->m_fi]]);
                        loop_map[old_loop->m_loop_index] = nl.m_loop_index;
                    }
                }
            }
        }
    }

    // Now, create new trims using the old loop definitions and the maps
    std::map<int, int>::iterator loop_it;
    for (loop_it = loop_map.begin(); loop_it != loop_map.end(); loop_it++) {
        const ON_BrepLoop *old_loop = &(data->brep->m_L[(*loop_it).first]);
        ON_BrepLoop &new_loop = data->planar_obj->local_brep->m_L[(*loop_it).second];
        for (int j = 0; j < old_loop->TrimCount(); j++) {
            const ON_BrepTrim *old_trim = old_loop->Trim(j);
            ON_BrepEdge *o_edge = old_trim->Edge();
            if (o_edge) {
                ON_BrepEdge &n_edge = data->planar_obj->local_brep->m_E[edge_map[o_edge->m_edge_index]];
                ON_BrepTrim &nt = data->planar_obj->local_brep->NewTrim(n_edge, old_trim->m_bRev3d, new_loop, c2_map[old_trim->TrimCurveIndexOf()]);
                nt.m_tolerance[0] = old_trim->m_tolerance[0];
                nt.m_tolerance[1] = old_trim->m_tolerance[1];
                nt.m_iso = old_trim->m_iso;
            } else {
                /* If we didn't have an edge originally, we need to add the 2d curve here */
                if (c2_map.find(old_trim->TrimCurveIndexOf()) == c2_map.end()) {
                    ON_Curve *nc = old_trim->TrimCurveOf()->Duplicate();
                    int c2i = data->planar_obj->local_brep->AddTrimCurve(nc);
                    c2_map[old_trim->TrimCurveIndexOf()] = c2i;
                }
                if (vertex_map.find(old_trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
                    ON_BrepVertex& newvs = data->planar_obj->local_brep->NewVertex(old_trim->Vertex(0)->Point(), old_trim->Vertex(0)->m_tolerance);
		    vertex_map[old_trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;
                    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(newvs, new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
                    nt.m_tolerance[0] = old_trim->m_tolerance[0];
                    nt.m_tolerance[1] = old_trim->m_tolerance[1];
                } else {
                    ON_BrepTrim &nt = data->planar_obj->local_brep->NewSingularTrim(data->planar_obj->local_brep->m_V[vertex_map[old_trim->Vertex(0)->m_vertex_index]], new_loop, old_trim->m_iso, c2_map[old_trim->TrimCurveIndexOf()]);
                    nt.m_tolerance[0] = old_trim->m_tolerance[0];
                    nt.m_tolerance[1] = old_trim->m_tolerance[1];
                }
            }
        }
    }

    // Need to preserve the vertex map for this, since we're not done building up the brep
    map_to_array(&(data->planar_obj->planar_obj_vert_map), &(data->planar_obj->planar_obj_vert_cnt), &vertex_map);
}

bool
end_of_trims_match(ON_Brep *brep, ON_BrepLoop *loop, int lti)
{
    bool valid = true;
    int ci0, ci1, next_lti;
    ON_3dPoint P0, P1;
    const ON_Curve *pC0, *pC1;
    const ON_Surface *surf = loop->Face()->SurfaceOf();
    double urange = surf->Domain(0)[1] - surf->Domain(0)[0];
    double vrange = surf->Domain(1)[1] - surf->Domain(1)[0];
    // end-of-trims matching test from opennurbs_brep.cpp
    const ON_BrepTrim& trim0 = brep->m_T[loop->m_ti[lti]];
    next_lti = (lti+1)%loop->TrimCount();
    const ON_BrepTrim& trim1 = brep->m_T[loop->m_ti[next_lti]];
    ON_Interval trim0_domain = trim0.Domain();
    ON_Interval trim1_domain = trim1.Domain();
    ci0 = trim0.m_c2i;
    ci1 = trim1.m_c2i;
    pC0 = brep->m_C2[ci0];
    pC1 = brep->m_C2[ci1];
    P0 = pC0->PointAt( trim0_domain[1] ); // end of this 2d trim
    P1 = pC1->PointAt( trim1_domain[0] ); // start of next 2d trim
    if ( !(P0-P1).IsTiny() )
    {
	// 16 September 2003 Dale Lear - RR 11319
	//    Added relative tol check so cases with huge
	//    coordinate values that agreed to 10 places
	//    didn't get flagged as bad.
	//double xtol = (fabs(P0.x) + fabs(P1.x))*1.0e-10;
	//double ytol = (fabs(P0.y) + fabs(P1.y))*1.0e-10;
	//
	// Oct 12 2009 Rather than using the above check, BRL-CAD uses
	// relative uv size
	double xtol = (urange) * trim0.m_tolerance[0];
	double ytol = (vrange) * trim0.m_tolerance[1];
	if ( xtol < ON_ZERO_TOLERANCE )
	    xtol = ON_ZERO_TOLERANCE;
	if ( ytol < ON_ZERO_TOLERANCE )
	    ytol = ON_ZERO_TOLERANCE;
	double dx = fabs(P0.x-P1.x);
	double dy = fabs(P0.y-P1.y);
	if ( dx > xtol || dy > ytol ) valid = false;
    }
    return valid;
}

bool
rebuild_loop(ON_Brep *brep, ON_BrepLoop *old_loop)
{
    ON_BoundingBox loop_pbox, cbox;
    const ON_BrepTrim& seed_trim = brep->m_T[old_loop->m_ti[0]];
    ON_BrepEdge *previous_edge = seed_trim.Edge();
    ON_BrepEdge *cedge = seed_trim.Edge();
    ON_BrepVertex *v_second = seed_trim.Vertex(1);
    ON_BrepVertex *v_last = seed_trim.Vertex(0);
    ON_BrepFace *face = old_loop->Face();
    ON_Surface *ts = old_loop->Face()->SurfaceOf()->Duplicate();
    ON_Plane face_plane;
    ON_Xform proj_to_plane;
    int mating_edge_found = 1;
    if (!ts->IsPlanar(&face_plane, BREP_PLANAR_TOL)) return false;

    brep->DeleteLoop(brep->m_L[old_loop->m_loop_index], false);
    ON_BrepLoop& loop = brep->NewLoop(ON_BrepLoop::outer, brep->m_F[face->m_face_index]);

    std::cout << "seed edge: " << cedge->m_edge_index << "\n";

    std::set<int> used_edges;

    used_edges.insert(cedge->m_edge_index);

    while (v_second != v_last && mating_edge_found == 1) {
	ON_3dPoint p = v_second->Point();
	mating_edge_found = 0;
	for (int i = 0; i < brep->m_E.Count(); i++) {
	    cedge = &(brep->m_E[i]);
	    if (used_edges.find(cedge->m_edge_index) == used_edges.end()) {
		ON_Curve *ec = cedge->EdgeCurveOf()->Duplicate();
		if (ec->IsLinear()) {
		    ON_BrepVertex *v_start = cedge->Vertex(0);
		    ON_BrepVertex *v_end = cedge->Vertex(1);
		    ON_3dPoint p1 = v_start->Point();
		    ON_3dPoint p2 = v_end->Point();
		    if (p1.DistanceTo(p) < 0.01 || p2.DistanceTo(p) < 0.01) {
			double d1 = fabs(face_plane.DistanceTo(v_start->Point()));
			double d2 = fabs(face_plane.DistanceTo(v_end->Point()));
			//std::cout << "candidate edge " << cedge->m_edge_index << ": " << d1 << "," << d2 << "\n";
			// if edge is on plane, continue
			if (d1 < 0.01 && d2 < 0.01) {
			    bool bRev3d = false;
			    std::cout << "found mating edge: " << cedge->m_edge_index << "\n";
			    mating_edge_found = 1;
			    used_edges.insert(cedge->m_edge_index);
			    const ON_Curve *c3;
			    if (p1.DistanceTo(p) < 0.01) {
				c3 = new ON_LineCurve(p1, p2);
			    } else {
				bRev3d = true;
				c3 = new ON_LineCurve(p2, p1);
			    }
			    ON_NurbsCurve *c2 = new ON_NurbsCurve();
			    c3->GetNurbForm(*c2);
			    c2->Transform(proj_to_plane);
			    c2->GetBoundingBox(cbox);
			    c2->ChangeDimension(2);
			    c2->MakePiecewiseBezier(2);
			    int c2i = brep->AddTrimCurve(c2);
			    ON_BrepTrim &trim = brep->NewTrim(brep->m_E[cedge->m_edge_index], bRev3d, loop, c2i);
			    trim.m_type = ON_BrepTrim::mated;
			    trim.m_tolerance[0] = 0.0;
			    trim.m_tolerance[1] = 0.0;
			    loop_pbox.Union(cbox);

			    // Once the new curve is made (if necessary) and assigned to
			    // the loop, update the vert appropriately (may be start or
			    // end depending)
			    if (p1.DistanceTo(p) < 0.01) {
				v_second = v_end;
			    } else {
				v_second = v_start;
			    }
			    delete ec;
			    break;
			}
		    }
		}
		delete ec;
	    }
	}
	previous_edge = cedge;
	if (!mating_edge_found) {
	    std::cout << "no mating edge found\n";
	    return false;
	}
    }

    ON_PlaneSurface *fs = new ON_PlaneSurface(face_plane);

    fs->SetDomain(0, loop.m_pbox.m_min.x, loop.m_pbox.m_max.x );
    fs->SetDomain(1, loop.m_pbox.m_min.y, loop.m_pbox.m_max.y );
    fs->SetExtents(0,fs->Domain(0));
    fs->SetExtents(1,fs->Domain(1));
    const int si = brep->AddSurface(fs);

    face->m_si = si;

    brep->SetTrimIsoFlags(brep->m_F[face->m_face_index]);
    brep->SetTrimTypeFlags(true);

    if (brep->LoopDirection(brep->m_L[loop.m_loop_index]) != 1) brep->FlipLoop(brep->m_L[loop.m_loop_index]);



    return true;

}

void
subbrep_planar_close_obj(struct subbrep_object_data *data)
{
    struct subbrep_object_data *pdata = data->planar_obj;
    for (int i = 0; i < pdata->local_brep->m_F.Count(); i++) {
	ON_BrepFace &face = pdata->local_brep->m_F[i];
	for (int j = 0; j < face.LoopCount(); j++) {
	    ON_BrepLoop *loop = face.Loop(j);
	    bool valid = true;
	    for (int lti = 0; lti < loop->m_ti.Count(); lti++) {
		if (!end_of_trims_match(pdata->local_brep, loop, lti)) {
		    valid = rebuild_loop(pdata->local_brep, loop);
		}
	    }
	    if (!valid) {
		std::cout << "invalid loop " << loop->m_loop_index << " in planar object " << bu_vls_addr(data->key) << " face " << i << "\n";
	    }
	}

	// TODO - if we pass the above test, make sure all edges have two faces to identify any missing
	// faces in the arb

    }
}

int subbrep_add_planar_face(struct subbrep_object_data *data, ON_Plane *pcyl,
       	ON_SimpleArray<const ON_BrepVertex *> *vert_loop)
{
    // We use the planar_obj's local_brep to store new faces.  The planar local
    // brep contains the relevant linear and planar components from its parent
    // - our job here is to add the new surface, identify missing edges to
    // create, find existing edges to re-use, and call NewFace with the
    // results.  At the end we should have just the faces needed
    // to define the planar volume of interest.
    struct subbrep_object_data *pdata = data->planar_obj;
    std::vector<int> edges;
    ON_SimpleArray<ON_Curve *> curves_2d;
    std::map<int, int> vert_map;
    array_to_map(&vert_map, pdata->planar_obj_vert_map, pdata->planar_obj_vert_cnt);

    ON_3dPoint p1 = pdata->local_brep->m_V[vert_map[((*vert_loop)[0])->m_vertex_index]].Point();
    ON_3dPoint p2 = pdata->local_brep->m_V[vert_map[((*vert_loop)[1])->m_vertex_index]].Point();
    ON_3dPoint p3 = pdata->local_brep->m_V[vert_map[((*vert_loop)[2])->m_vertex_index]].Point();
    ON_Plane loop_plane(p1, p2, p3);
    ON_BoundingBox loop_pbox, cbox;

      // get 2d trim curves
    ON_Xform proj_to_plane;
    proj_to_plane[0][0] = loop_plane.xaxis.x;
    proj_to_plane[0][1] = loop_plane.xaxis.y;
    proj_to_plane[0][2] = loop_plane.xaxis.z;
    proj_to_plane[0][3] = -(loop_plane.xaxis*loop_plane.origin);
    proj_to_plane[1][0] = loop_plane.yaxis.x;
    proj_to_plane[1][1] = loop_plane.yaxis.y;
    proj_to_plane[1][2] = loop_plane.yaxis.z;
    proj_to_plane[1][3] = -(loop_plane.yaxis*loop_plane.origin);
    proj_to_plane[2][0] = loop_plane.zaxis.x;
    proj_to_plane[2][1] = loop_plane.zaxis.y;
    proj_to_plane[2][2] = loop_plane.zaxis.z;
    proj_to_plane[2][3] = -(loop_plane.zaxis*loop_plane.origin);
    proj_to_plane[3][0] = 0.0;
    proj_to_plane[3][1] = 0.0;
    proj_to_plane[3][2] = 0.0;
    proj_to_plane[3][3] = 1.0;

    ON_PlaneSurface *s = new ON_PlaneSurface(loop_plane);
    const int si = pdata->local_brep->AddSurface(s);

    double flip = ON_DotProduct(loop_plane.Normal(), pcyl->Normal());

    for (int i = 0; i < vert_loop->Count(); i++) {
	int vind1, vind2;
	const ON_BrepVertex *v1, *v2;
	v1 = (*vert_loop)[i];
	vind1 = vert_map[v1->m_vertex_index];
	if (i < vert_loop->Count() - 1) {
	    v2 = (*vert_loop)[i+1];
	} else {
	    v2 = (*vert_loop)[0];
	}
	vind2 = vert_map[v2->m_vertex_index];
	ON_BrepVertex &new_v1 = pdata->local_brep->m_V[vind1];
	ON_BrepVertex &new_v2 = pdata->local_brep->m_V[vind2];

	// Because we may have already created a needed edge only in the new
	// Brep with a previous face, we have to check all the edges in the new
	// structure for a vertex match.
	ON_BrepEdge *new_edgep;
	int edge_found = 0;
	for (int j = 0; j < pdata->local_brep->m_E.Count(); j++) {
	    int ev1 = pdata->local_brep->m_E[j].Vertex(0)->m_vertex_index;
	    int ev2 = pdata->local_brep->m_E[j].Vertex(1)->m_vertex_index;
	    if ((ev1 == vind1) && (ev2 == vind2)) {
		edges.push_back(pdata->local_brep->m_E[j].m_edge_index);
		edge_found = 1;

		// Get 2D curve from this edge's 3D curve
		const ON_Curve *c3 = pdata->local_brep->m_E[j].EdgeCurveOf();
		ON_NurbsCurve *c2 = new ON_NurbsCurve();
		c3->GetNurbForm(*c2);
		c2->Transform(proj_to_plane);
		c2->GetBoundingBox(cbox);
		c2->ChangeDimension(2);
		c2->MakePiecewiseBezier(2);
		curves_2d.Append(c2);
		loop_pbox.Union(cbox);
		break;
	    }
	}
	if (!edge_found) {
	    int c3i = pdata->local_brep->AddEdgeCurve(new ON_LineCurve(new_v1.Point(), new_v2.Point()));
	    // Get 2D curve from this edge's 3D curve
	    const ON_Curve *c3 = pdata->local_brep->m_C3[c3i];
	    ON_NurbsCurve *c2 = new ON_NurbsCurve();
	    c3->GetNurbForm(*c2);
	    c2->Transform(proj_to_plane);
	    c2->GetBoundingBox(cbox);
	    c2->ChangeDimension(2);
	    c2->MakePiecewiseBezier(2);
	    curves_2d.Append(c2);
	    loop_pbox.Union(cbox);

	    ON_BrepEdge &new_edge = pdata->local_brep->NewEdge(pdata->local_brep->m_V[vind1], pdata->local_brep->m_V[vind2], c3i, NULL ,0);
	    edges.push_back(new_edge.m_edge_index);
	}
    }

    ON_BrepFace& face = pdata->local_brep->NewFace( si );
    ON_BrepLoop& loop = pdata->local_brep->NewLoop(ON_BrepLoop::outer, face);
    loop.m_pbox = loop_pbox;
    for (int i = 0; i < vert_loop->Count(); i++) {
	ON_NurbsCurve *c2 = (ON_NurbsCurve *)curves_2d[i];
	int c2i = pdata->local_brep->AddTrimCurve(c2);
	ON_BrepEdge &edge = pdata->local_brep->m_E[edges.at(i)];
	ON_BrepTrim &trim = pdata->local_brep->NewTrim(edge, false, loop, c2i);
	trim.m_type = ON_BrepTrim::mated;
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;
    }

    // set face domain
    s->SetDomain(0, loop.m_pbox.m_min.x, loop.m_pbox.m_max.x );
    s->SetDomain(1, loop.m_pbox.m_min.y, loop.m_pbox.m_max.y );
    s->SetExtents(0,s->Domain(0));
    s->SetExtents(1,s->Domain(1));

    // need to update trim m_iso flags because we changed surface shape
    pdata->local_brep->SetTrimIsoFlags(face);
    pdata->local_brep->SetTrimTypeFlags(true);
    if (flip < 0) pdata->local_brep->FlipFace(face);
}




// TODO - need a way to detect if a set of planar faces would form a
// self-intersecting polyhedron.  Self-intersecting polyhedrons are the ones
// with the potential to make both positive and negative contributions to a
// solid. One possible idea:
//
// For all edges in polyhedron test whether each edge intersects any face in
// the polyhedron to which it does not belong.  The test can be simple - for
// each vertex, construct a vector from the vertex to some point on the
// candidate face (center point is probably a good start) and see if the dot
// products of those two vectors with the face normal vector agree in sign.
// For a given edge, if all dot products agree in pair sets, then the edge does
// not intersect any face in the polyhedron.  If no edges intersect, the
// polyhedron is not self intersecting.  If some edges do intersect (probably
// at least three...) then those edges identify sub-shapes that need to be
// constructed.
//
// Note that this is also a concern for non-planar surfaces that have been
// reduced to planar surfaces as part of the process - probably should
// incorporate a bounding box test to determine if such surfaces can be
// part of sub-object definitions (say, a cone subtracted from a subtracting
// cylinder to make a positive cone volume inside the cylinder) or if the
// cone has to be a top level unioned positive object (if the cone's apex
// point is outside the cylinder's subtracting area, for example, the tip
// of the code will not be properly added as a positive volume if it is just
// a subtraction from the cylinder.


// Returns 1 if point set forms a convex polyhedron, 0 if the point set
// forms a degenerate chull, and -1 if the point set is concave
int
convex_point_set(struct subbrep_object_data *data, std::set<int> *verts)
{
    // Use chull3d to find the set of vertices that are on the convex
    // hull.  If all of them are, the point set defines a convex polyhedron.
    // If the points are coplanar, return 0 - not a volume
    return 0;
}

/* These functions will return 2 if successful, 1 if unsuccessful but point set
 * is convex, 0 if unsuccessful and vertex set's chull is degenerate (i.e. the
 * planar component of this CSG shape contributes no positive volume),  and -1
 * if unsuccessful and point set is neither degenerate nor convex */
int
point_set_is_arb4(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);
    if (is_convex == 1) {
	// TODO - deduce and set up proper arb4 point ordering
	return 2;
    }
    return 0;
}
int
point_set_is_arb5(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb5 test
    return 0;
}
int
point_set_is_arb6(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb6 test
    return 0;
}
int
point_set_is_arb7(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb7 test
    return 0;
}
int
point_set_is_arb8(struct subbrep_object_data *data, std::set<int> *verts)
{
    int is_convex = convex_point_set(data, verts);

    if (!is_convex == 1) {
	return is_convex;
    }
    // TODO - arb8 test
    return 0;
}

/* If we're going with an arbn, we need to add one plane for each face.  To
 * make sure the normal is in the correct direction, find the center point of
 * the verts and the center point of the face verts to construct a vector which
 * can be used in a dot product test with the face normal.*/
int
point_set_is_arbn(struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts, int do_test)
{
    int is_convex;
    if (!do_test) {
	is_convex = 1;
    } else {
	is_convex = convex_point_set(data, verts);
    }
    if (!is_convex == 1) return is_convex;

    // TODO - arbn assembly
    return 2;
}

// In the worst case, make a brep for later conversion into an nmg.
// The other possibility here is an arbn csg tree, but that needs
// more thought...
int
subbrep_make_planar_brep(struct subbrep_object_data *data)
{
    // TODO - check for self intersections in the candidate shape, and handle
    // if any are discovered.
    return 0;
}

int
planar_switch(int ret, struct subbrep_object_data *data, std::set<int> *faces, std::set<int> *verts)
{
    switch (ret) {
	case -1:
	    return subbrep_make_planar_brep(data);
	    break;
	case 0:
	    return 0;
	    break;
	case 1:
	    return point_set_is_arbn(data, faces, verts, 0);
	    break;
	case 2:
	    return 1;
	    break;
    }
    return 0;
}


int
subbrep_make_planar(struct subbrep_object_data *data)
{
    // First step is to count vertices, using the edges
    std::set<int> subbrep_verts;
    std::set<int> faces;
    for (int i = 0; i < data->edges_cnt; i++) {
	const ON_BrepEdge *edge = &(data->brep->m_E[i]);
	subbrep_verts.insert(edge->Vertex(0)->m_vertex_index);
	subbrep_verts.insert(edge->Vertex(1)->m_vertex_index);
    }
    array_to_set(&faces, data->faces, data->faces_cnt);

    int vert_cnt = subbrep_verts.size();
    int ret = 0;
    switch (vert_cnt) {
	case 0:
	    std::cout << "no verts???\n";
	    return 0;
	    break;
	case 1:
	    std::cout << "one vertex - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 2:
	    std::cout << "two vertices - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 3:
	    std::cout << "three vertices - not a candidate for a planar volume\n";
	    return 0;
	    break;
	case 4:
	    if (point_set_is_arb4(data, &subbrep_verts) != 2) {
		return 0;
	    }
	    return 1;
	    break;
	case 5:
	    ret = point_set_is_arb5(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 6:
	    ret = point_set_is_arb6(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 7:
	    ret = point_set_is_arb7(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	case 8:
	    ret = point_set_is_arb8(data, &subbrep_verts);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;
	default:
	    ret = point_set_is_arbn(data, &faces, &subbrep_verts, 1);
	    return planar_switch(ret, data, &faces, &subbrep_verts);
	    break;

    }
    return 0;
}


// TODO - need to check for self-intersecting planar objects - any
// such object needs to be further deconstructed, since it has
// components that may be making both positive and negative contributions


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
