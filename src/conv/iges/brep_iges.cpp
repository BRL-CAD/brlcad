/*                     B R E P _ I G E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/brep_iges.cpp
 *
 * Faithful export of a BRL-CAD rt_brep (OpenNURBS ON_Brep) to IGES,
 * preserving the analytic NURBS geometry rather than tessellating it.
 *
 * A valid closed two-manifold is written with IGES 5.3 topology: one
 * Manifold Solid B-Rep Object (186) references shells (514), faces (510),
 * loops (508), shared edges (504), and shared vertices (502).  Surfaces and
 * curves are written as exact Rational B-Spline entities (128 and 126).
 *
 * Any ON_Surface / ON_Curve is converted to NURBS form on the way out, so
 * planar, analytic, and free-form geometry are all handled uniformly.
 * Open or unrepresentable topology, and the explicit --flatten-brep
 * compatibility mode, use independent Trimmed Surfaces (144) with Curve on
 * a Parametric Surface (142) boundaries.  The modern importer assembles
 * supported 144 faces directly in OpenNURBS.
 */

#include "common.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "brep.h"

#include "vmath.h"
#include "bn/tol.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "bu/log.h"

#include "./iges.h"

extern "C" int flatten_brep;

namespace {

/* Write an ON_Surface as an IGES 128 Rational B-Spline Surface; returns
 * its directory entry number, or 0 on failure. */
int
write_on_surface(const ON_Surface *srf, FILE *fp_dir, FILE *fp_param)
{
    ON_NurbsSurface ns;
    if (!srf->GetNurbForm(ns))
	return 0;

    const int k1 = ns.CVCount(0) - 1;
    const int k2 = ns.CVCount(1) - 1;
    const int m1 = ns.Order(0) - 1;
    const int m2 = ns.Order(1) - 1;
    const int nu = ns.CVCount(0);
    const int nv = ns.CVCount(1);
    const bool rat = ns.IsRational();

    /* IGES wants the full clamped knot vector (KnotCount + 2 values); ON
     * omits the two superfluous end knots, so add them back. */
    std::vector<double> uk(ns.KnotCount(0) + 2);
    uk[0] = ns.SuperfluousKnot(0, 0);
    for (int i = 0; i < ns.KnotCount(0); i++)
	uk[i + 1] = ns.Knot(0, i);
    uk[ns.KnotCount(0) + 1] = ns.SuperfluousKnot(0, 1);

    std::vector<double> vk(ns.KnotCount(1) + 2);
    vk[0] = ns.SuperfluousKnot(1, 0);
    for (int i = 0; i < ns.KnotCount(1); i++)
	vk[i + 1] = ns.Knot(1, i);
    vk[ns.KnotCount(1) + 1] = ns.SuperfluousKnot(1, 1);

    /* IGES order: u index fastest, then v.  Control points are euclidean;
     * weights are listed separately. */
    std::vector<double> wts((size_t)nu * nv);
    std::vector<double> pts((size_t)3 * nu * nv);
    for (int j = 0; j < nv; j++) {
	for (int i = 0; i < nu; i++) {
	    const int idx = j * nu + i;
	    ON_3dPoint p;
	    ns.GetCV(i, j, p);
	    pts[3 * idx + 0] = p.x;
	    pts[3 * idx + 1] = p.y;
	    pts[3 * idx + 2] = p.z;
	    wts[idx] = rat ? ns.Weight(i, j) : 1.0;
	}
    }

    const ON_Interval du = ns.Domain(0);
    const ON_Interval dv = ns.Domain(1);

    return write_nurb_surface_entity(k1, k2, m1, m2, rat ? 1 : 0,
				     ns.IsClosed(0) ? 1 : 0,
				     ns.IsClosed(1) ? 1 : 0,
				     ns.IsPeriodic(0) ? 1 : 0,
				     ns.IsPeriodic(1) ? 1 : 0,
				     &uk[0], &vk[0], &wts[0], &pts[0],
				     du.Min(), du.Max(), dv.Min(), dv.Max(),
				     fp_dir, fp_param);
}


/* Write an ON_Curve as an IGES 126 Rational B-Spline Curve; returns its
 * directory entry number, or 0 on failure. */
int
write_on_curve(const ON_Curve *crv, int planar, int reverse,
	       FILE *fp_dir, FILE *fp_param)
{
    ON_NurbsCurve nc;
    if (!crv->GetNurbForm(nc))
	return 0;
    if (reverse && !nc.Reverse())
	return 0;

    const int k = nc.CVCount() - 1;
    const int m = nc.Order() - 1;
    const int n = nc.CVCount();
    const bool rat = nc.IsRational();

    std::vector<double> knots(nc.KnotCount() + 2);
    knots[0] = nc.SuperfluousKnot(0);
    for (int i = 0; i < nc.KnotCount(); i++)
	knots[i + 1] = nc.Knot(i);
    knots[nc.KnotCount() + 1] = nc.SuperfluousKnot(1);

    std::vector<double> wts(n);
    std::vector<double> pts((size_t)3 * n);
    for (int i = 0; i < n; i++) {
	ON_3dPoint p;
	nc.GetCV(i, p);		/* euclidean; z==0 for 2d parameter curves */
	pts[3 * i + 0] = p.x;
	pts[3 * i + 1] = p.y;
	pts[3 * i + 2] = p.z;
	wts[i] = rat ? nc.Weight(i) : 1.0;
    }

    const ON_Interval dom = nc.Domain();

    return write_nurb_curve_entity(k, m, rat ? 1 : 0, planar,
				   nc.IsClosed() ? 1 : 0,
				   nc.IsPeriodic() ? 1 : 0,
				   &knots[0], &wts[0], &pts[0],
				   dom.Min(), dom.Max(),
				   0.0, 0.0, 1.0, fp_dir, fp_param);
}


/* Write one trimming loop as a Curve on a Parametric Surface (142),
 * building the parameter-space and model-space boundary curves (joining
 * segments with a 102 Composite Curve as needed).  Returns the 142 DE, or
 * 0 on failure. */
int
write_loop(const ON_BrepLoop *loop, int surf_de, FILE *fp_dir, FILE *fp_param)
{
    const int nt = loop->TrimCount();
    if (nt <= 0)
	return 0;

    std::vector<int> bde;	/* parameter-space (2d) curve DEs */
    std::vector<int> cde;	/* model-space (3d) curve DEs */

    for (int ti = 0; ti < nt; ti++) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim)
	    continue;
	if (!trim->TrimCurveOf())
	    continue;
	const int b = write_on_curve(trim, 1, 0, fp_dir, fp_param);
	if (!b)
	    continue;

	int c = 0;
	const ON_BrepEdge *edge = trim->Edge();
	if (edge) {
	    if (edge->EdgeCurveOf())
		c = write_on_curve(edge, 0, trim->m_bRev3d ? 1 : 0,
		    fp_dir, fp_param);
	}
	if (!c)
	    c = b;		/* singular trim: iges-g uses only the 2d curve */

	bde.push_back(b);
	cde.push_back(c);
    }

    if (bde.empty())
	return 0;

    const int bcurve = (bde.size() > 1)
	? write_composite_curve_entity(&bde[0], (int)bde.size(), fp_dir, fp_param)
	: bde[0];
    const int ccurve = (cde.size() > 1)
	? write_composite_curve_entity(&cde[0], (int)cde.size(), fp_dir, fp_param)
	: cde[0];

    return write_curve_on_surface_entity(surf_de, bcurve, ccurve, fp_dir, fp_param);
}

struct BrepTopologyPlan {
    int outer_component = -1;
    std::vector<std::vector<int> > components;
};


std::vector<std::vector<int> >
face_components(const ON_Brep &brep)
{
    std::vector<std::set<int> > adjacency(brep.m_F.Count());
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	std::vector<int> faces;
	for (int trim_index = 0; trim_index < edge.m_ti.Count(); ++trim_index) {
	    const int index = edge.m_ti[trim_index];
	    if (index < 0 || index >= brep.m_T.Count())
		continue;
	    const int face_index = brep.m_T[index].FaceIndexOf();
	    if (face_index >= 0 && face_index < brep.m_F.Count() &&
		    std::find(faces.begin(), faces.end(), face_index) ==
		    faces.end())
		faces.push_back(face_index);
	}
	for (size_t first = 0; first < faces.size(); ++first)
	    for (size_t second = first + 1; second < faces.size(); ++second) {
		adjacency[faces[first]].insert(faces[second]);
		adjacency[faces[second]].insert(faces[first]);
	    }
    }

    std::vector<bool> visited(brep.m_F.Count(), false);
    std::vector<std::vector<int> > components;
    for (int seed = 0; seed < brep.m_F.Count(); ++seed) {
	if (visited[seed])
	    continue;
	components.push_back(std::vector<int>());
	std::queue<int> pending;
	pending.push(seed);
	visited[seed] = true;
	while (!pending.empty()) {
	    const int face = pending.front();
	    pending.pop();
	    components.back().push_back(face);
	    for (std::set<int>::const_iterator neighbor =
		    adjacency[face].begin(); neighbor != adjacency[face].end();
		    ++neighbor) {
		if (visited[*neighbor])
		    continue;
		visited[*neighbor] = true;
		pending.push(*neighbor);
	    }
	}
    }
    return components;
}


bool
build_topology_plan(const ON_Brep &brep, BrepTopologyPlan &plan,
		    std::string &diagnostic)
{
    ON_TextLog validation_log;
    if (!brep.IsValid(&validation_log)) {
	diagnostic = "OpenNURBS BRep is invalid";
	return false;
    }
    bool oriented = false;
    bool has_boundary = true;
    if (!brep.IsSolid() || !brep.IsManifold(&oriented, &has_boundary) ||
	    !oriented || has_boundary) {
	diagnostic = "BRep is not a closed, oriented manifold";
	return false;
    }
    plan.components = face_components(brep);
    if (plan.components.empty()) {
	diagnostic = "BRep has no connected face component";
	return false;
    }
    if (plan.components.size() == 1) {
	plan.outer_component = 0;
	return true;
    }

    std::vector<std::unique_ptr<ON_Brep> > component_breps;
    component_breps.reserve(plan.components.size());
    for (size_t component = 0; component < plan.components.size(); ++component) {
	ON_Brep *copy = brep.DuplicateFaces(
	    static_cast<int>(plan.components[component].size()),
	    plan.components[component].data(), false);
	if (!copy || !copy->IsValid() || !copy->IsSolid()) {
	    delete copy;
	    diagnostic = "a connected BRep component is not a closed shell";
	    return false;
	}
	component_breps.push_back(std::unique_ptr<ON_Brep>(copy));
    }

    std::vector<int> outer_candidates;
    for (size_t candidate = 0; candidate < component_breps.size(); ++candidate) {
	bool contains_all = true;
	const ON_BoundingBox candidate_bounds =
	    component_breps[candidate]->BoundingBox();
	for (size_t other = 0; other < component_breps.size() && contains_all;
		++other) {
	    if (other == candidate)
		continue;
	    if (!component_breps[other]->m_V.Count() ||
		    !candidate_bounds.Includes(
			component_breps[other]->BoundingBox(), true) ||
		    !ON_BrepPointInside(component_breps[other]->m_V[0].Point(),
			component_breps[candidate].get()))
		contains_all = false;
	}
	if (contains_all)
	    outer_candidates.push_back(static_cast<int>(candidate));
    }
    if (outer_candidates.size() != 1) {
	diagnostic = "disconnected shells do not form one provable solid with voids";
	return false;
    }
    plan.outer_component = outer_candidates.front();
    return true;
}


bool
nurb_compatible(const ON_Curve *curve)
{
    ON_NurbsCurve nurb;
    return curve && curve->GetNurbForm(nurb) && nurb.IsValid();
}


bool
nurb_compatible(const ON_Surface *surface)
{
    ON_NurbsSurface nurb;
    return surface && surface->GetNurbForm(nurb) && nurb.IsValid();
}


bool
preflight_topology(const ON_Brep &brep, BrepTopologyPlan &plan,
		   std::string &diagnostic)
{
    if (!build_topology_plan(brep, plan, diagnostic))
	return false;
    if (!brep.m_V.Count() || !brep.m_E.Count()) {
	diagnostic = "BRep has no vertices or edges";
	return false;
    }
    for (int vertex_index = 0; vertex_index < brep.m_V.Count(); ++vertex_index)
	if (!brep.m_V[vertex_index].Point().IsValid()) {
	    diagnostic = "BRep contains an invalid vertex";
	    return false;
	}
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	if (edge.m_vi[0] < 0 || edge.m_vi[0] >= brep.m_V.Count() ||
		edge.m_vi[1] < 0 || edge.m_vi[1] >= brep.m_V.Count() ||
		edge.m_ti.Count() != 2 || !nurb_compatible(&edge)) {
	    diagnostic = "BRep contains an edge that cannot be represented exactly";
	    return false;
	}
    }
    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep.m_F[face_index];
	if (face.m_si < 0 || face.m_si >= brep.m_S.Count() ||
		!face.OuterLoop() || !nurb_compatible(face.SurfaceOf())) {
	    diagnostic = "BRep contains a face without an exact surface or outer loop";
	    return false;
	}
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop || loop->TrimCount() < 1) {
		diagnostic = "BRep contains an empty trimming loop";
		return false;
	    }
	    for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
		const ON_BrepTrim *trim = loop->Trim(trim_index);
		if (!trim || !nurb_compatible(trim)) {
		    diagnostic = "BRep contains a trim without an exact parameter curve";
		    return false;
		}
		if (trim->m_ei >= 0) {
		    if (trim->m_ei >= brep.m_E.Count()) {
			diagnostic = "BRep trim has an invalid edge reference";
			return false;
		    }
		} else if (trim->m_type != ON_BrepTrim::singular ||
			!trim->Vertex(0) || trim->Vertex(0)->m_vertex_index < 0 ||
			trim->Vertex(0)->m_vertex_index >= brep.m_V.Count()) {
		    diagnostic = "edge-free BRep trim is not a valid pole";
		    return false;
		}
	    }
	}
    }
    return true;
}


/* Preserve OpenNURBS topology as one IGES Manifold Solid B-Rep Object. */
int
write_topological_brep(const ON_Brep &brep, const char *name,
		       const BrepTopologyPlan &plan,
		       FILE *fp_dir, FILE *fp_param)
{
    std::vector<double> vertices(static_cast<size_t>(brep.m_V.Count()) * 3);
    for (int index = 0; index < brep.m_V.Count(); ++index) {
	const ON_3dPoint point = brep.m_V[index].Point();
	vertices[3 * index] = point.x;
	vertices[3 * index + 1] = point.y;
	vertices[3 * index + 2] = point.z;
    }
    const int vertex_list_de = write_brep_vertex_list_entity(vertices.data(),
	vertices.size() / 3, fp_dir, fp_param);
    if (!vertex_list_de)
	return 0;

    std::vector<iges_brep_edge> edges(brep.m_E.Count());
    for (int index = 0; index < brep.m_E.Count(); ++index) {
	const ON_BrepEdge &edge = brep.m_E[index];
	edges[index].curve_de = write_on_curve(&edge, 0, 0, fp_dir, fp_param);
	edges[index].start_vertex = static_cast<size_t>(edge.m_vi[0]);
	edges[index].end_vertex = static_cast<size_t>(edge.m_vi[1]);
	if (!edges[index].curve_de)
	    return 0;
    }
    const int edge_list_de = write_brep_edge_list_entity(vertex_list_de,
	edges.data(), edges.size(), fp_dir, fp_param);
    if (!edge_list_de)
	return 0;

    std::vector<int> surface_des(brep.m_S.Count(), 0);
    std::vector<int> face_des(brep.m_F.Count(), 0);
    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep.m_F[face_index];
	int &surface_de = surface_des[face.m_si];
	if (!surface_de)
	    surface_de = write_on_surface(face.SurfaceOf(), fp_dir, fp_param);
	if (!surface_de)
	    return 0;

	std::vector<const ON_BrepLoop *> ordered_loops;
	ordered_loops.push_back(face.OuterLoop());
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (loop != face.OuterLoop())
		ordered_loops.push_back(loop);
	}
	std::vector<int> loop_des;
	loop_des.reserve(ordered_loops.size());
	for (std::vector<const ON_BrepLoop *>::const_iterator loop =
		ordered_loops.begin(); loop != ordered_loops.end(); ++loop) {
	    std::vector<iges_brep_loop_use> uses((*loop)->TrimCount());
	    for (int trim_index = 0; trim_index < (*loop)->TrimCount(); ++trim_index) {
		const ON_BrepTrim *trim = (*loop)->Trim(trim_index);
		iges_brep_loop_use &use = uses[trim_index];
		use.parameter_curve_de = write_on_curve(trim, 1, 0,
		    fp_dir, fp_param);
		use.isoparametric = trim->m_iso != ON_Surface::not_iso;
		if (trim->m_ei >= 0) {
		    use.kind = IGES_BREP_EDGE_USE;
		    use.index = static_cast<size_t>(trim->m_ei);
		    use.orientation = trim->m_bRev3d ? 0 : 1;
		} else {
		    use.kind = IGES_BREP_VERTEX_USE;
		    use.index = static_cast<size_t>(
			trim->Vertex(0)->m_vertex_index);
		    use.orientation = 1;
		}
		if (!use.parameter_curve_de)
		    return 0;
	    }
	    const int loop_de = write_brep_loop_entity(vertex_list_de,
		edge_list_de, uses.data(), uses.size(), fp_dir, fp_param);
	    if (!loop_de)
		return 0;
	    loop_des.push_back(loop_de);
	}
	face_des[face_index] = write_brep_face_entity(surface_de,
	    loop_des.data(), loop_des.size(), 1, fp_dir, fp_param);
	if (!face_des[face_index])
	    return 0;
    }

    std::vector<int> shell_des(plan.components.size(), 0);
    std::vector<int> shell_orientations(plan.components.size(), 0);
    for (size_t component = 0; component < plan.components.size(); ++component) {
	const bool shell_same = static_cast<int>(component) == plan.outer_component;
	std::vector<int> component_faces;
	std::vector<int> face_orientations;
	component_faces.reserve(plan.components[component].size());
	face_orientations.reserve(plan.components[component].size());
	for (std::vector<int>::const_iterator face_index =
		plan.components[component].begin();
		face_index != plan.components[component].end(); ++face_index) {
	    component_faces.push_back(face_des[*face_index]);
	    face_orientations.push_back(
		brep.m_F[*face_index].m_bRev != shell_same ? 1 : 0);
	}
	shell_des[component] = write_brep_shell_entity(component_faces.data(),
	    face_orientations.data(), component_faces.size(), fp_dir, fp_param);
	shell_orientations[component] = shell_same ? 1 : 0;
	if (!shell_des[component])
	    return 0;
    }

    std::vector<int> void_des;
    std::vector<int> void_orientations;
    for (size_t component = 0; component < shell_des.size(); ++component) {
	if (static_cast<int>(component) == plan.outer_component)
	    continue;
	void_des.push_back(shell_des[component]);
	void_orientations.push_back(shell_orientations[component]);
    }
    return write_brep_solid_entity(name,
	!iges_name_is_independent(name), shell_des[plan.outer_component],
	shell_orientations[plan.outer_component],
	void_des.empty() ? NULL : void_des.data(),
	void_orientations.empty() ? NULL : void_orientations.data(),
	void_des.size(), fp_dir, fp_param);
}


/* Write each face as an independent Trimmed Surface (144).  This fallback
 * preserves exact geometry for valid open BReps and other non-solid models. */
int
write_trimmed_brep(ON_Brep *brep, FILE *fp_dir, FILE *fp_param)
{
    if (!brep)
	return 0;

    int last_de = 0;
    for (int fi = 0; fi < brep->m_F.Count(); fi++) {
	const ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *srf = face.SurfaceOf();
	if (!srf)
	    continue;

	const int surf_de = write_on_surface(srf, fp_dir, fp_param);
	if (!surf_de) {
	    bu_log("brep_to_iges: face %d surface not representable, skipped\n", fi);
	    continue;
	}

	int outer_de = 0;
	std::vector<int> inner_des;
	for (int li = 0; li < face.LoopCount(); li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    if (!loop)
		continue;
	    const int cos_de = write_loop(loop, surf_de, fp_dir, fp_param);
	    if (!cos_de)
		continue;
	    if (loop->m_type == ON_BrepLoop::inner)
		inner_des.push_back(cos_de);
	    else
		outer_de = cos_de;
	}

	if (!outer_de) {
	    bu_log("brep_to_iges: face %d has no usable outer loop, skipped\n", fi);
	    continue;
	}

	last_de = write_trimmed_surface_entity(surf_de, outer_de,
					       inner_des.empty() ? NULL : &inner_des[0],
					       (int)inner_des.size(),
					       fp_dir, fp_param);
    }

    return last_de;
}


int
write_on_brep(ON_Brep *brep, const char *name,
	      FILE *fp_dir, FILE *fp_param)
{
    if (!brep)
	return 0;
    if (flatten_brep)
	return write_trimmed_brep(brep, fp_dir, fp_param);
    BrepTopologyPlan plan;
    std::string diagnostic;
    if (preflight_topology(*brep, plan, diagnostic))
	return write_topological_brep(*brep, name, plan, fp_dir, fp_param);
    bu_log("brep_to_iges: %s; exporting independent trimmed surfaces\n",
	diagnostic.c_str());
    return write_trimmed_brep(brep, fp_dir, fp_param);
}


} /* anonymous namespace */


/* g-iges globals / helpers (C linkage). */
extern "C" int solid_is_brep;
extern "C" int nmg_to_iges(struct rt_db_internal *, char *, FILE *, FILE *, struct bu_list *);


extern "C" int
brep_to_iges(struct rt_db_internal *ip, char *name,
	     FILE *fp_dir, FILE *fp_param, struct bu_list *vlfree)
{
    (void)vlfree;

    RT_CK_DB_INTERNAL(ip);
    struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
    if (!bi || bi->magic != RT_BREP_INTERNAL_MAGIC || !bi->brep) {
	bu_log("brep_to_iges: %s is not a valid brep\n", name ? name : "(null)");
	return 0;
    }

    ON::Begin();
    solid_is_brep = 1;

    const int de = write_on_brep(bi->brep, name, fp_dir, fp_param);
    if (!de) {
	bu_log("brep_to_iges: no faces written for %s\n", name ? name : "(null)");
	return 0;
    }
    return de;
}


/* Export a non-brep primitive by first converting it to an ON_Brep via its
 * ft_brep callback, so curved analytic primitives (eto, rpc, rhc, epa, ehy,
 * hyp, superell, revolve, particle, ...) are written as faithful NURBS
 * surfaces rather than being dropped or faceted.  Falls back to the faceted
 * NMG exporter when no brep callback exists or the brep is unusable. */
extern "C" int
primitive_brep_to_iges(struct rt_db_internal *ip, char *name,
		       FILE *fp_dir, FILE *fp_param, struct bu_list *vlfree)
{
    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type > 0 && ip->idb_type <= ID_MAXIMUM && OBJ[ip->idb_type].ft_brep) {
	struct bn_tol tol;
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.0005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1.0e-6;
	tol.para = 1.0 - tol.perp;

	ON::Begin();

	ON_Brep *brep = NULL;
	OBJ[ip->idb_type].ft_brep(&brep, ip, &tol);
	if (brep) {
	    if (brep->IsValid()) {
		solid_is_brep = 1;
		const int de = write_on_brep(brep, name, fp_dir, fp_param);
		delete brep;
		if (de)
		    return de;
	    } else {
		bu_log("primitive_brep_to_iges: %s brep invalid, tessellating instead\n",
		       name ? name : "(null)");
		delete brep;
	    }
	}
    }

    /* fall back to a faceted (tessellated) export */
    return nmg_to_iges(ip, name, fp_dir, fp_param, vlfree);
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
