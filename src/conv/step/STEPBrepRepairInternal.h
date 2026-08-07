/*              S T E P B R E P R E P A I R I N T E R N A L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 * Private interfaces between the schema-neutral STEP BRep repair units.
 * This header is an implementation boundary, not plugin ABI.
 */

#ifndef CONV_STEP_STEPBREPREPAIRINTERNAL_H
#define CONV_STEP_STEPBREPREPAIRINTERNAL_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cfloat>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "brep/cdt.h"
#include "brep/pullback.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"
#include "bu/units.h"

#include "STEPBrepValidation.h"
#include "STEPBudget.h"
#include "STEPConversionStatus.h"
#include "STEPImportInternal.h"
#include "STEPWrapper.h"
#include "step-g/LocalUnits.h"
#include "step-g/OpenNurbsInterfaces.h"

namespace step_brep_detail {

struct PeriodicPullbackCrossing {
    bool detected = false;
    double trim_fraction = 0.0;
    int surface_direction = -1;
};

struct SuppliedOuterBoundCandidate {
    int loop_step_id = 0;
    std::vector<int> nonbridge_edge_step_ids;
};

struct SuppliedFaceOuterBounds {
    int face_index = -1;
    std::vector<SuppliedOuterBoundCandidate> candidates;
};

bool regenerate_trim_polyline(ON_Brep *, ON_BrepTrim &, const ON_Surface *,
    const ON_NurbsCurve &, double, std::string *,
    PeriodicPullbackCrossing * = NULL, const ON_3dPoint * = NULL,
    const ON_3dPoint * = NULL, bool = false, STEPWrapper * = NULL,
    bool = false, ON_Curve ** = NULL);
ON_3dPoint closed_surface_point_at(const ON_Surface *, const ON_3dPoint &);
bool remove_one_proven_shared_dangling_spur(ON_Brep *, STEPWrapper *, int,
    const std::string &);
bool remove_one_noncontractible_repeated_vertex_lobe(ON_Brep *, STEPWrapper *,
    int, const std::string &);
bool remove_one_exact_redundant_zero_area_face(ON_Brep *, STEPWrapper *, int,
    const std::string &);
bool remove_one_exact_distinct_edge_zero_area_face(ON_Brep *, STEPWrapper *,
    int, const std::string &, int * = NULL, int * = NULL, int * = NULL);
bool opposite_trim_curves_coincide(const ON_BrepTrim *, const ON_BrepTrim *,
    double, double = -1.0);
bool remove_adjacent_zero_area_slit(ON_Brep *, int, std::string * = NULL,
    int * = NULL, int * = NULL, double * = NULL, bool * = NULL);
bool periodic_loop_closure(const ON_Brep *, const ON_BrepLoop *,
    const ON_BrepTrim *, const ON_BrepTrim *, double);
bool split_keyhole_loop(ON_Brep *, int, std::string *);
size_t classify_exact_polyline_seams(ON_Brep *);
void refresh_brep_flags_preserving_singular_isos(ON_Brep *, bool,
    STEPWrapper * = NULL, int = 0, const std::string * = NULL);
bool brep_topology_references_are_safe(const ON_Brep *, std::string *);
size_t merge_one_exact_duplicate_step_boundary_edge_pair(ON_Brep *,
    std::string *, bool = false);
size_t merge_one_exact_periodic_boundary_edge_pair(ON_Brep *, std::string *,
    bool = true);
size_t merge_exact_periodic_boundary_edges_for_structural_validation(ON_Brep *,
    std::string *);
bool regenerate_collapsed_periodic_boundary(ON_Brep *, ON_BrepLoop &,
    const ON_Surface *, int, double, STEPWrapper *, int, const std::string &,
    double * = NULL);
bool regenerate_native_seam_periodic_boundary(ON_Brep *, ON_BrepLoop &,
    const ON_Surface *, int, STEPWrapper *, int, const std::string &, double,
    bool = true);
bool infer_missing_planar_triangular_cap(ON_Brep *, STEPWrapper *, int,
    const std::string &, std::string *);
bool split_periodic_boundary_at_native_seam(ON_Brep *, int, int,
    const ON_Surface *, int, double, STEPWrapper *, int, const std::string &,
    bool = true, int * = NULL, bool = false, bool = false, bool = false);
bool split_open_periodic_boundary_crossing(ON_Brep *, ON_BrepFace &, int, int,
    double, STEPWrapper *, int, const std::string &, bool = true,
    const std::set<int> * = NULL, int * = NULL, bool = true, bool = false);
size_t split_supplied_full_period_boundaries(ON_Brep *, STEPWrapper *, int,
    const std::string &);
bool regenerate_split_periodic_boundary_chain(ON_Brep *, ON_BrepLoop &,
    const ON_Surface *, STEPWrapper *, int, const std::string &);

size_t repair_implicit_periodic_face_bands(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::set<int> * = NULL, bool = false);
bool restrict_periodic_face_to_interior_pole_sheet(ON_Brep *, int, int,
    STEPWrapper *, int, const std::string &, bool = true);
size_t finalize_brep_topology(ON_Brep *, bool, STEPWrapper *, int,
    const std::string &, bool * = NULL);

void closed_trim_endpoint_alignments(const ON_BrepTrim &, const ON_BrepEdge &,
    const ON_Surface *, double [2]);
bool closed_trim_endpoint_alignment_is_valid(double);
bool refine_surface_pullback_seeded(const ON_Surface *, const ON_3dPoint &,
    double, ON_3dPoint &, double *, bool = false, double = 0.0);
void normalize_closed_surface_parameter(const ON_Surface *, const ON_3dPoint &,
    double, ON_3dPoint &);
bool has_unwrapped_closed_parameter(const ON_Surface *, const ON_3dPoint &);
ON_3dPoint closed_surface_native_parameter(const ON_Surface *, ON_3dPoint);
bool closed_surface_ev1der(const ON_Surface *, const ON_3dPoint &, ON_3dPoint &,
    ON_3dVector &, ON_3dVector &);
bool regenerate_full_period_boundary_chain(ON_Brep *, int, int, double,
    STEPWrapper *, int, const std::string &, bool = true);
bool align_closed_surface_seam_from_trim_pair(ON_Brep *, const ON_BrepEdge &,
    const ON_BrepLoop &, const ON_Surface *, double, std::string * = NULL);
bool relocate_closed_surface_loop_seam(ON_Brep *, int, int, double,
    STEPWrapper *, int, const std::string &, std::string *,
    double = std::numeric_limits<double>::quiet_NaN(),
    const std::set<int> * = NULL);
bool exact_open_trim_crosses_native_seam(const ON_BrepTrim &,
    const ON_Surface *, int, double, brlcad::PullbackContext &);
bool exact_open_trim_has_spurious_periodic_winding(const ON_BrepTrim &,
    const ON_Surface *, int, double, brlcad::PullbackContext &);

size_t repair_closed_surface_seam_crossings(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::set<int> * = NULL,
    const std::set<int> * = NULL);
void repair_seam_pair_from_exact_edge(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::vector<int> * = NULL,
    std::vector<int> * = NULL, bool = false, const std::set<int> * = NULL);
void repair_paired_seam_boundaries(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::vector<int> *);
void repair_aligned_surface_loop_branches(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::vector<int> &);
void repair_bounded_seam_isos(ON_Brep *, STEPWrapper *, int,
    const std::string &, bool, std::vector<int> * = NULL);
void repair_adjacent_trim_vertices(ON_Brep *, STEPWrapper *, int,
    const std::string &);
double verified_regeneration_tolerance(ON_BrepTrim &, ON_BrepEdge &,
    const ON_Surface *, const ON_NurbsCurve &, double, const ON_Brep *,
    STEPWrapper *, int, const std::string &);
double verified_source_pcurve_tolerance(ON_BrepTrim &, ON_BrepEdge &,
    const ON_Surface *, const ON_NurbsCurve &, double, const ON_Brep *,
    STEPWrapper *, int, const std::string &);
ON_Curve *translated_periodic_trim_for_join(const ON_Surface *,
    const ON_BrepTrim &, const ON_3dPoint &, bool, std::string *,
    double = -1.0,
    double * = NULL);
void repair_invalid_open_pcurves(ON_Brep *, STEPWrapper *, int,
    const std::string &, bool = false, std::set<int> * = NULL,
    const std::set<int> * = NULL);
void repair_missing_singular_trims(ON_Brep *, STEPWrapper *, int,
    const std::string &);
size_t remove_redundant_closed_singular_trims(ON_Brep *, STEPWrapper *, int,
    const std::string &);
bool validate_periodic_trim_translation(const ON_Surface *,
    const ON_BrepTrim &, const ON_Curve &, std::string *, double = -1.0,
    double * = NULL);
size_t repair_ambiguous_singular_periodic_loop_branches(ON_Brep *,
    STEPWrapper *, int, const std::string &);

size_t repair_exact_periodic_loop_branches(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::set<int> * = NULL);
size_t normalize_exact_paired_seam_loops_to_native_domain(ON_Brep *,
    STEPWrapper *, int, const std::string &, const std::set<int> * = NULL);
size_t repair_single_singular_periodic_loop_branches(ON_Brep *, STEPWrapper *,
    int, const std::string &, bool = true, const std::set<int> * = NULL);
size_t repair_multi_singular_periodic_loop_branches(ON_Brep *, STEPWrapper *,
    int, const std::string &, bool = true, const std::set<int> * = NULL);
size_t regenerate_periodic_loop_chains(ON_Brep *, STEPWrapper *, int,
    const std::string &, int = -1, bool = true, bool = false);
void repair_adjacent_trim_endpoints(ON_Brep *, STEPWrapper *, int,
    const std::string &, int = -1, bool = true, bool = false,
    const std::set<int> * = NULL);
size_t repair_paired_seam_loop_endpoints(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::set<int> * = NULL);
void repair_zero_length_boundary_edges(ON_Brep *, STEPWrapper *, int,
    const std::string &);
bool trim_orientation_toggle_preserves_edge_pair(const ON_Brep *,
    const ON_BrepTrim &);
void repair_final_closed_trim_orientations(ON_Brep *, STEPWrapper *, int,
    const std::string &);
void interpret_closed_periodic_face_bounds(ON_Brep *, STEPWrapper *, int,
    const std::string &);
void repair_face_bound_classification(ON_Brep *, STEPWrapper *, int,
    const std::string &, const std::set<int> * = NULL);
std::vector<SuppliedFaceOuterBounds>
supplied_face_outer_bound_ambiguities(const ON_Brep *);
bool retry_unique_face_bound_classification(ON_Brep *, STEPWrapper *, int,
    const std::string &, bool, const std::vector<SuppliedFaceOuterBounds> &);

void repair_closed_trim_orientations(ON_Brep *, STEPWrapper *, int,
    const std::string &, bool = false);
bool retry_topological_keyhole_normalization(ON_Brep *, STEPWrapper *, int,
    const std::string &);

} /* namespace step_brep_detail */

#endif /* CONV_STEP_STEPBREPREPAIRINTERNAL_H */
