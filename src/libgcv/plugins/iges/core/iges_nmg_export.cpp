/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_writer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>

#include "brep.h"
#include "bu/malloc.h"
#include "nmg.h"
#include "rt/geom.h"

namespace brlcad {
namespace iges {
namespace {

class ShellGroups {
public:
    ShellGroups(struct nmgregion &region, const struct bn_tol &tolerance)
    {
	count_ = nmg_find_outer_and_void_shells(&region, &groups_, &rt_vlfree, &tolerance);
    }
    ~ShellGroups()
    {
	for (size_t index = 0; index < count_; ++index) {
	    bu_ptbl_free(groups_[index]);
	    bu_free(groups_[index], "IGES shell group");
	}
	bu_free(groups_, "IGES shell groups");
    }
    ShellGroups(const ShellGroups &) = delete;
    ShellGroups &operator=(const ShellGroups &) = delete;
    size_t size() const { return count_; }
    std::vector<struct shell *> shells(size_t index) const
    {
	std::vector<struct shell *> result;
	for (size_t item = 0; item < BU_PTBL_LEN(groups_[index]); ++item)
	    result.push_back(reinterpret_cast<struct shell *>(BU_PTBL_GET(groups_[index], item)));
	return result;
    }

private:
    size_t count_ = 0;
    struct bu_ptbl **groups_ = nullptr;
};

class ShellWriter {
public:
    ShellWriter(Writer &writer, const std::vector<struct shell *> &shells) :
	writer_(writer), shells_(shells) {}

    int write(const std::string &name)
    {
	for (auto *shell : shells_) {
	    struct faceuse *face;
	    for (BU_LIST_FOR(face, faceuse, &shell->fu_hd)) {
		if (face->orientation != OT_SAME)
		    continue;
		if (!face->f_p->g.magic_p || *face->f_p->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
		    throw std::domain_error("NMG face has no planar surface geometry");
		struct loopuse *loop;
		for (BU_LIST_FOR(loop, loopuse, &face->lu_hd)) {
		    if (BU_LIST_FIRST_MAGIC(&loop->down_hd) != NMG_EDGEUSE_MAGIC)
			throw std::domain_error("NMG face has a non-edge boundary loop");
		    struct edgeuse *edge;
		    for (BU_LIST_FOR(edge, edgeuse, &loop->down_hd)) {
			if (edge->g.magic_p && *edge->g.magic_p != NMG_EDGE_G_LSEG_MAGIC)
			    throw std::domain_error("NMG edge has non-linear geometry");
			register_vertex(edge->vu_p->v_p);
			register_vertex(edge->eumate_p->vu_p->v_p);
			if (!edge_indices_.count(edge->e_p)) {
			    edge_indices_[edge->e_p] = edges_.size();
			    edges_.push_back(edge);
			}
		    }
		}
	    }
	}
	if (edges_.empty())
	    return 0;
	vertex_list_ = writer_.vertex_list(vertices_.data(), vertices_.size() / 3);
	std::vector<BrepEdge> records;
	for (const auto *edge : edges_) {
	    const auto *start = edge->vu_p->v_p;
	    const auto *end = edge->eumate_p->vu_p->v_p;
	    int curve_id;
	    if (writer_.options().nurbs_facets) {
		ON_LineCurve line(ON_3dPoint(start->vg_p->coord), ON_3dPoint(end->vg_p->coord));
		curve_id = export_curve(writer_, line);
	    } else {
		ParameterWriter parameters(110);
		parameters.point(start->vg_p->coord).point(end->vg_p->coord);
		curve_id = writer_.entity(EntitySpec(110), parameters);
	    }
	    records.push_back({curve_id, vertex_indices_.at(start), vertex_indices_.at(end)});
	}
	edge_list_ = writer_.edge_list(vertex_list_, records.data(), records.size());
	std::vector<int> shell_ids;
	std::vector<int> trimmed_faces;
	for (auto *shell : shells_) {
	    std::vector<int> faces;
	    struct faceuse *face;
	    for (BU_LIST_FOR(face, faceuse, &shell->fu_hd)) {
		if (face->orientation != OT_SAME)
		    continue;
		const int id = write_face(*face);
		if (!id)
		    return 0;
		faces.push_back(id);
	    }
	    if (writer_.options().mode == ExportOptions::Mode::Trimmed)
		trimmed_faces.insert(trimmed_faces.end(), faces.begin(), faces.end());
	    else {
		const std::vector<int> orientations(faces.size(), 1);
		shell_ids.push_back(writer_.shell(faces.data(), orientations.data(), faces.size()));
	    }
	}
	if (writer_.options().mode == ExportOptions::Mode::Trimmed)
	    return writer_.group(name, trimmed_faces);
	if (shell_ids.empty() || std::find(shell_ids.begin(), shell_ids.end(), 0) != shell_ids.end())
	    return 0;
	const std::vector<int> void_orientations(shell_ids.size() - 1, 1);
	return writer_.solid(name.c_str(), shell_ids.front(), 1,
	    shell_ids.data() + 1, void_orientations.data(), void_orientations.size());
    }

private:
    void register_vertex(const struct vertex *vertex)
    {
	if (vertex_indices_.count(vertex))
	    return;
	if (!vertex->vg_p)
	    throw std::domain_error("NMG vertex has no coordinates");
	vertex_indices_[vertex] = vertices_.size() / 3;
	vertices_.insert(vertices_.end(), vertex->vg_p->coord, vertex->vg_p->coord + 3);
    }

    int write_face(struct faceuse &face)
    {
	plane_t coefficients;
	const double orientation = (face.orientation != OT_SAME) != (face.f_p->flip != 0) ? -1.0 : 1.0;
	HSCALE(coefficients, face.f_p->g.plane_p->N, orientation);
	const ON_3dVector normal(coefficients);
	if (!normal.IsValid() || normal.LengthSquared() <= ON_ZERO_TOLERANCE)
	    throw std::domain_error("NMG face has an invalid normal");
	const ON_3dPoint origin = ON_origin + coefficients[3] / normal.LengthSquared() * normal;
	const ON_Plane plane(origin, normal);
	std::vector<struct loopuse *> loops;
	struct loopuse *loop;
	for (BU_LIST_FOR(loop, loopuse, &face.lu_hd))
	    loops.push_back(loop);
	std::stable_partition(loops.begin(), loops.end(), [](const auto *item) { return item->orientation == OT_SAME; });
	if (loops.empty() || loops.front()->orientation != OT_SAME)
	    throw std::domain_error("NMG face has no outer loop");

	const bool trimmed = writer_.options().mode == ExportOptions::Mode::Trimmed;
	int surface_id;
	if (writer_.options().nurbs_facets || trimmed) {
	    ON_BoundingBox bounds;
	    for (const auto *source : loops) {
		struct edgeuse *edge;
		for (BU_LIST_FOR(edge, edgeuse, &source->down_hd)) {
		    double u, v;
		    if (!plane.ClosestPointTo(ON_3dPoint(edge->vu_p->v_p->vg_p->coord), &u, &v))
			return 0;
		    bounds.Set(ON_3dPoint(u, v, 0.0), bounds.IsValid());
		}
	    }
	    ON_PlaneSurface surface(plane);
	    surface.SetExtents(0, ON_Interval(bounds.m_min.x, bounds.m_max.x), true);
	    surface.SetExtents(1, ON_Interval(bounds.m_min.y, bounds.m_max.y), true);
	    surface_id = export_surface(writer_, surface);
	} else {
	    ParameterWriter point(116);
	    point.point(origin).integer(0);
	    const int point_id = writer_.entity(EntitySpec(116), point);
	    ParameterWriter direction(123);
	    direction.point(plane.zaxis);
	    const int direction_id = writer_.entity(EntitySpec(123), direction);
	    ParameterWriter parameters(190);
	    parameters.integer(point_id).integer(direction_id);
	    surface_id = writer_.entity(EntitySpec(190), parameters);
	}
	if (!surface_id)
	    return 0;
	std::vector<int> loop_ids;
	for (const auto *source : loops) {
	    std::vector<BrepLoopUse> uses;
	    std::vector<int> model_curves, parameter_curves;
	    struct edgeuse *edge;
	    for (BU_LIST_FOR(edge, edgeuse, &source->down_hd)) {
		const size_t edge_index = edge_indices_.at(edge->e_p);
		const bool same_direction = edges_[edge_index]->vu_p->v_p == edge->vu_p->v_p;
		uses.push_back({BrepLoopUse::Edge, edge_index, same_direction ? 1 : 0, 0, 0});
		if (!trimmed && !writer_.options().nurbs_facets)
		    continue;
		const ON_3dPoint start(edge->vu_p->v_p->vg_p->coord);
		const ON_3dPoint end(edge->eumate_p->vu_p->v_p->vg_p->coord);
		ON_2dPoint uv_start, uv_end;
		if (!plane.ClosestPointTo(start, &uv_start.x, &uv_start.y) ||
		    !plane.ClosestPointTo(end, &uv_end.x, &uv_end.y))
		    return 0;
		const int parameter_curve = export_curve(writer_, ON_LineCurve(uv_start, uv_end), true);
		if (!parameter_curve)
		    return 0;
		if (trimmed) {
		    model_curves.push_back(export_curve(writer_, ON_LineCurve(start, end)));
		    parameter_curves.push_back(parameter_curve);
		} else {
		    // A NURBS plane needs explicit UV trims; consumers need not
		    // infer its parameterization from the model-space edges.
		    uses.back().parameter_curve_de = parameter_curve;
		}
	    }
	    if (trimmed) {
		const int model_id = writer_.composite_curve(model_curves.data(), model_curves.size());
		const int parameter_id = writer_.composite_curve(parameter_curves.data(), parameter_curves.size());
		loop_ids.push_back(writer_.curve_on_surface(surface_id, parameter_id, model_id));
	    } else {
		loop_ids.push_back(writer_.loop(vertex_list_, edge_list_, uses.data(), uses.size()));
	    }
	}
	return trimmed ? writer_.trimmed_surface(surface_id, loop_ids.front(),
	    loop_ids.data() + 1, loop_ids.size() - 1) :
	    writer_.face(surface_id, loop_ids.data(), loop_ids.size(), 1);
    }

    Writer &writer_;
    const std::vector<struct shell *> &shells_;
    std::vector<double> vertices_;
    std::map<const struct vertex *, size_t> vertex_indices_;
    std::vector<const struct edgeuse *> edges_;
    std::map<const struct edge *, size_t> edge_indices_;
    int vertex_list_ = 0;
    int edge_list_ = 0;
};
} // namespace

ExportedEntity
export_nmg_region(Writer &writer, struct nmgregion &region, const std::string &name)
{
    ShellGroups groups(region, writer.options().tolerance);
    if (!groups.size()) {
	writer.omission(name + " has no outer shell, cannot write a manifold solid");
	return {};
    }
    std::vector<int> solids;
    for (size_t group = 0; group < groups.size(); ++group) {
	const auto shells = groups.shells(group);
	ShellWriter shell_writer(writer, shells);
	const int id = shell_writer.write(groups.size() == 1 ? name : "");
	if (!id)
	    return {};
	solids.push_back(id);
    }
    return {solids.size() == 1 ? solids.front() : writer.assembly(name, solids), true};
}

ExportedEntity
export_nmg(Writer &writer, struct rt_db_internal &internal, const std::string &name)
{
    std::unique_ptr<struct model, decltype(&nmg_km)> owned_model(nullptr, nmg_km);
    struct model *model = nullptr;
    if (internal.idb_type == ID_NMG) {
	model = static_cast<struct model *>(internal.idb_ptr);
    } else {
	if (internal.idb_type <= ID_NULL || internal.idb_type > ID_MAXIMUM ||
	    !OBJ[internal.idb_type].ft_tessellate)
	    return {};
	if (internal.idb_type == ID_BOT &&
	    static_cast<const struct rt_bot_internal *>(internal.idb_ptr)->mode != RT_BOT_SOLID) {
	    writer.omission(name + " is a plate mode primitive and cannot be converted to IGES format");
	    return {};
	}
	owned_model.reset(nmg_mm());
	model = owned_model.get();
	struct nmgregion *region = nullptr;
	if (OBJ[internal.idb_type].ft_tessellate(&region, model, &internal,
	    &writer.options().tessellation, &writer.options().tolerance))
	    return {};
    }
    if (!model)
	return {};
    std::vector<struct nmgregion *> regions;
    struct nmgregion *region;
    for (BU_LIST_FOR(region, nmgregion, &model->r_hd))
	regions.push_back(region);
    std::vector<int> solids;
    for (auto *source : regions) {
	const auto solid = export_nmg_region(writer, *source, regions.size() == 1 ? name : "");
	if (!solid)
	    return {};
	solids.push_back(solid.directory);
    }
    if (solids.empty())
	return {};
    return {solids.size() == 1 ? solids.front() : writer.assembly(name, solids), true};
}
} // namespace iges
} // namespace brlcad

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
