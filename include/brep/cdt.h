/*                      C D T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
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
 */
/** @{ */
/** @file brep/cdt.h */
/** @addtogroup brep_util
 *
 * @brief
 * Constrained Delaunay Triangulation of brep solids.
 *
 */

#ifndef BREP_CDT_H
#define BREP_CDT_H

#include "common.h"

#include "bv/vlist.h"
#include "bn/tol.h"
#include "bg/defines.h"
#include "bg/trimesh.h"
#include "brep/defines.h"

__BEGIN_DECLS

/* Container that holds the state of a triangulation */
struct ON_Brep_CDT_State;

/* Conversion-quality tessellation result categories.  These supplement the
 * legacy integer return from ON_Brep_CDT_Tessellate with a stable reason for
 * failure. */
#define BREP_CDT_RESULT_UNATTEMPTED 0
#define BREP_CDT_RESULT_SUCCESS 1
#define BREP_CDT_RESULT_PARTIAL 2
#define BREP_CDT_RESULT_INVALID_BREP -1
#define BREP_CDT_RESULT_INVALID_TOLERANCE -2
#define BREP_CDT_RESULT_INITIALIZATION_FAILED -3
#define BREP_CDT_RESULT_FACE_FAILED -4
#define BREP_CDT_RESULT_MESH_EXPORT_FAILED -5
#define BREP_CDT_RESULT_NON_SOLID -6
#define BREP_CDT_RESULT_INVALID_PSLG -7
#define BREP_CDT_RESULT_DETRIA_FAILED -8
#define BREP_CDT_RESULT_CERTIFICATION_FAILED -9
#define BREP_CDT_RESULT_CHART_FAILED -10
#define BREP_CDT_RESULT_REFINEMENT_LIMIT -11
#define BREP_CDT_RESULT_GEOMETRIC_FAILED -12
#define BREP_CDT_RESULT_REPAIRED 3
#define BREP_CDT_RESULT_REPAIR_FAILED -13

#define BREP_CDT_STAGE_NONE 0
#define BREP_CDT_STAGE_INPUT 1
#define BREP_CDT_STAGE_TOPOLOGY 2
#define BREP_CDT_STAGE_EDGE_INITIALIZATION 3
#define BREP_CDT_STAGE_FACE_TRIANGULATION 4
#define BREP_CDT_STAGE_MESH_ASSEMBLY 5
#define BREP_CDT_STAGE_SOLID_VALIDATION 6
#define BREP_CDT_STAGE_PSLG_VALIDATION 7
#define BREP_CDT_STAGE_DETRIA 8
#define BREP_CDT_STAGE_CHART_CONSTRUCTION 9
#define BREP_CDT_STAGE_ADAPTIVE_REFINEMENT 10
#define BREP_CDT_STAGE_GEOMETRIC_VALIDATION 11
#define BREP_CDT_STAGE_MESH_REPAIR 12

/* A snapshot of the most recent tessellation attempt.  message is always
 * NUL-terminated.  face_index is -1 when no individual face is responsible. */
struct brep_cdt_diagnostic {
    int result;
    int stage;
    int face_index;
    int completed_faces;
    int failed_faces;
    char message[256];
};

/**
 * Explicit controls for the post-tessellation mesh-repair tier.
 *
 * Repair is never attempted by ON_Brep_CDT_Tessellate.  Hole and component
 * operations must also be enabled and bounded explicitly in @p mesh.  A zero
 * max_surface_deviation uses the tessellation's maximum chord tolerance.  A
 * zero max_deviation_samples uses the library's bounded default.  The area
 * change limit applies to the complete repaired mesh; zero disables that
 * additional aggregate limit.  By default deviation samples must project
 * inside a trimmed source face.  allow_untrimmed_surface_match permits an
 * explicitly reported fallback to the source face's underlying analytic
 * surface when defective trim topology prevents that classification.  The
 * fast-face fallback supplies display-CDT triangles for failed rigorous faces
 * before mesh repair; its point, byte, and time limits bound that extra work.
 * use_full_fast_fallback instead supplies one coherent whole-B-Rep display
 * mesh.  It is intended as a lower-fidelity alternative when mixing rigorous
 * and display face meshes leaves irreparable boundary topology.
 * use_full_fast_fallback_if_needed preserves the rigorous-first attempt and
 * retries with that whole display mesh only if the mixed mesh cannot be
 * certified.  try_invalid_brep permits the repair entry point to make one
 * rigorous attempt after broad OpenNURBS validity failure, while retaining
 * the mesher's closed-manifold and paired-edge topology prerequisites.
 * Edge-initialization failures caused by disagreeing paired p-curves may make
 * one repair-only retry when their shared midpoint stays within the same
 * maximum surface-deviation bound of both faces and the native edge curve.
 * With the automatic zero poisson_scale, use_poisson_reconstruction first
 * gives conservative mesh repair one chance to close the whole display mesh
 * without replacing its triangles.  Only if that fails does Screened Poisson
 * replace the display mesh with an implicit-surface reconstruction.  An
 * internal triangle cap bounds that opportunistic pass; use_full_fast_fallback
 * without Poisson remains the explicit route for larger local repairs.  An
 * explicit nonzero scale requests Poisson directly.  This is the most
 * approximate tier and is therefore separately opt-in.  Its depth
 * is restricted to the bounded range accepted by this API.  A zero
 * poisson_scale tries the upstream 1.1 domain scale followed by a 1.2 retry
 * if the first reconstructed mesh cannot be certified.  If either scale
 * exceeds the reference-area bound or omits input coverage, the same bounded
 * scale pair is retried with one area-weighted sample budget.  If the default
 * Neumann reconstruction remains open, the bounded scale and sampling
 * attempts are repeated with a Dirichlet boundary
 * and stronger non-exact screening.  This closure-biased fallback is reported
 * explicitly and remains subject to every solid, area, deviation, and input
 * coverage gate.  A value from 1.0 through 2.0 requests one fixed scale
 * without either automatic retry.  Disconnected B-Rep face components are
 * reconstructed independently so a large component cannot erase a smaller
 * one; max_poisson_components bounds that work.
 * relaxed_fidelity_factor is a separately opt-in final acceptance tier for a
 * mesh which already satisfies every solid and Manifold requirement.  A
 * value from 1 through 4 multiplies the strict surface-deviation and enabled
 * area-change limits without changing the generated mesh.  Acceptance is
 * tagged explicitly in the report and provenance callback; zero disables it.
 */
/** No approximate geometry was needed. */
#define BREP_CDT_REPAIR_APPROX_NONE 0
/** A failed face used display triangulation with rigorous edge samples. */
#define BREP_CDT_REPAIR_APPROX_CONSTRAINED_FACE 1
/** Bounded local mesh repair added or reconstructed a tagged neighborhood. */
#define BREP_CDT_REPAIR_APPROX_LOCAL_MESH 2
/** The complete B-Rep used display triangulation. */
#define BREP_CDT_REPAIR_APPROX_FULL_FAST 3
/** The complete display mesh was replaced by an implicit reconstruction. */
#define BREP_CDT_REPAIR_APPROX_POISSON 4
/** A Manifold mesh used the explicitly relaxed final fidelity bound. */
#define BREP_CDT_REPAIR_APPROX_RELAXED_FIDELITY 5

/**
 * Report the B-Rep topology whose interpretation required approximation.
 * The callback is invoked only for an accepted repaired solid.  face_indices
 * and edge_indices remain valid only for the duration of the call.  Callers
 * producing a BoT may use these lists and approximation_tier to attach
 * provenance attributes to the converted object.
 */
typedef void (*brep_cdt_repair_provenance_t)(int approximation_tier,
	const int *face_indices, size_t face_count, const int *edge_indices,
	size_t edge_count, void *data);

struct brep_cdt_repair_settings {
    struct bg_trimesh_repair_settings mesh;
    fastf_t max_surface_deviation;
    size_t max_deviation_samples;
    fastf_t max_area_change_percent;
    int allow_untrimmed_surface_match;
    int use_fast_face_fallback;
    int use_full_fast_fallback;
    int use_poisson_reconstruction;
    int poisson_depth;
    size_t max_poisson_components;
    size_t max_fast_points;
    size_t max_fast_result_bytes;
    long max_fast_time_ms;
    fastf_t poisson_scale;
    int use_full_fast_fallback_if_needed;
    int try_invalid_brep;
    brep_cdt_repair_provenance_t provenance;
    void *provenance_data;
    fastf_t relaxed_fidelity_factor;
};

#define BREP_CDT_REPAIR_SETTINGS_INIT {BG_TRIMESH_REPAIR_SETTINGS_INIT, 0.0, 4096, 1.0, 0, 1, 0, 0, 8, 64, 1048576, 134217728, 5000, 0.0, 0, 0, NULL, NULL, 0.0}

/** Provenance and quality measurements for a repair attempt. */
struct brep_cdt_repair_report {
    struct bg_trimesh_repair_report mesh;
    struct brep_cdt_diagnostic source_diagnostic;
    int source_failed_faces;
    int changed_faces;
    size_t deviation_samples;
    size_t deviation_projection_failures;
    size_t untrimmed_surface_samples;
    size_t input_mesh_surface_samples;
    int fast_fallback_attempted_faces;
    int fast_fallback_used_faces;
    int fast_fallback_failed_faces;
    int fast_fallback_triangles;
    int full_fast_fallback_used;
    int poisson_reconstruction_attempted;
    int poisson_reconstruction_applied;
    int poisson_input_points;
    int poisson_components;
    int poisson_output_points;
    int poisson_output_faces;
    int poisson_attempts;
    int poisson_area_sampling_applied;
    int poisson_boundary_fallback_applied;
    fastf_t poisson_scale;
    fastf_t max_surface_deviation;
    fastf_t rms_surface_deviation;
    fastf_t allowed_surface_deviation;
    fastf_t reference_area;
    fastf_t reference_area_change_percent;
    fastf_t area_change_percent;
    size_t coverage_samples;
    size_t coverage_failures;
    fastf_t max_coverage_deviation;
    fastf_t rms_coverage_deviation;
    int relaxed_tessellation_attempted;
    int relaxed_tessellation_completed_faces;
    int bounded_edge_retry_attempted;
    int bounded_edge_retry_completed_faces;
    int bounded_edge_approximation_edges;
    int bounded_edge_approximation_faces;
    fastf_t max_bounded_edge_deviation;
    size_t fast_fallback_constrained_edges;
    size_t fast_fallback_constrained_samples;
    int added_patch_components;
    int largest_added_patch_faces;
    fastf_t largest_added_patch_area;
    int rigorous_first_attempted;
    int rigorous_first_result;
    int rigorous_first_fast_faces;
    size_t rigorous_first_constrained_edges;
    size_t rigorous_first_constrained_samples;
    fastf_t rigorous_first_reference_area;
    fastf_t rigorous_first_output_area;
    fastf_t rigorous_first_area_change_percent;
    int approximation_tier;
    int approximation_faces;
    int approximation_edges;
    /** Triangles retained byte-for-byte from the rigorous input mesh. */
    int retained_rigorous_triangles;
    /** Triangles replaced only inside accepted, tagged local neighborhoods. */
    int missing_rigorous_triangles;
    /** Certified triangles retained by a bounded hanging-edge subdivision. */
    int subdivided_rigorous_triangles;
    /** Accepted local neighborhoods replacing certified triangles. */
    int replaced_rigorous_components;
    int largest_replaced_rigorous_triangles;
    size_t largest_replaced_boundary_edges;
    fastf_t replaced_rigorous_area;
    fastf_t largest_replaced_rigorous_area;
    /** Failed adaptive faces retained as approximate repair input. */
    int best_effort_faces;
    int best_effort_triangles;
    int best_effort_folded_triangles;
    /** Chord samples projected to their source face within four tolerances. */
    size_t best_effort_reference_samples;
    /** Chord samples with no source projection inside that diagnostic bound. */
    size_t best_effort_reference_failures;
    fastf_t max_best_effort_surface_deviation;
    /** Sparse local periodic strips joining rigorous neighboring boundaries. */
    int boundary_strip_faces;
    int boundary_strip_triangles;
    size_t boundary_strip_constrained_edges;
    size_t boundary_strip_constrained_samples;
    /** Topological disks spanning complete authoritative shared boundaries. */
    int topological_disk_faces;
    int topological_disk_triangles;
    size_t topological_disk_constrained_edges;
    size_t topological_disk_constrained_samples;
    /** Final acceptance used the separately enabled relaxed fidelity tier. */
    int relaxed_fidelity_applied;
    fastf_t relaxed_fidelity_factor;
    fastf_t relaxed_surface_deviation_limit;
    fastf_t relaxed_area_change_percent_limit;
};

#define BREP_CDT_REPAIR_REPORT_INIT {BG_TRIMESH_REPAIR_REPORT_INIT, {BREP_CDT_RESULT_UNATTEMPTED, BREP_CDT_STAGE_NONE, -1, 0, 0, {0}}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0}

/* Create and initialize a CDT state with default tolerances.  bv
 * must be a pointer to an ON_Brep object. */
extern BREP_EXPORT struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv, const char *objname);

/* Destroy a CDT state */
extern BREP_EXPORT void
ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s);

extern BREP_EXPORT const char *
ON_Brep_CDT_ObjName(struct ON_Brep_CDT_State *s);

/* Set/get the CDT tolerances. */
extern BREP_EXPORT void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct bg_tess_tol *t);
extern BREP_EXPORT void
ON_Brep_CDT_Tol_Get(struct bg_tess_tol *t, const struct ON_Brep_CDT_State *s);

/* Return the ON_Brep associated with state s. */
extern BREP_EXPORT void *
ON_Brep_CDT_Brep(struct ON_Brep_CDT_State *s);

/* Given a state, produce a triangulation.  Returns 0 if a solid, valid
 * triangulation was produced, 1 if a triangulation was produced but it
 * isn't solid, and -1 if no triangulation could be produced. If faces is
 * non-null, the triangulation will only attempt to triangulate the
 * specified face(s) and the return code will be the number of successfully
 * triangulated faces.  If the CDT tolerances have been updated since the
 * last Tessellate call, the old tessellation information will be replaced. */
extern BREP_EXPORT int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s, int face_cnt, int *faces);

/**
 * Attempt an explicitly bounded mesh repair after tessellation left a
 * partial or non-solid mesh.  The original failed-face diagnostics remain
 * available as repair provenance.  A repaired result is accepted only when
 * it is a closed, oriented manifold with valid vertex links, nondegenerate
 * triangles, bounded area change, and sampled deviation from the source
 * B-Rep within the requested tolerance.  Nonadjacent triangle intersections
 * are rejected unless explicitly permitted by the mesh settings.  A
 * certified triangle neighborhood may be reconstructed only when its area
 * and boundary stay within the configured hole limits; every affected B-Rep
 * face and edge is then reported as approximation provenance.  A failed
 * single-loop face may also be spanned as a topological disk when every trim
 * has complete authoritative shared-edge samples.  This preserves its exact
 * boundary while explicitly reporting the interior as a local approximation.
 *
 * Returns 1 if the state already contains a certified solid, 0 if repair
 * produced a certified approximation, and -1 if repair was not possible or
 * did not satisfy all postconditions.
 */
extern BREP_EXPORT int
ON_Brep_CDT_Repair(struct ON_Brep_CDT_State *s,
	const struct brep_cdt_repair_settings *settings,
	struct brep_cdt_repair_report *report);

/* Given a state, report the status of its triangulation. -3 indicates a
 * failed attempt to tessellate, -2 indicates a non-solid tessellation is
 * present after an attempt to tessellate all faces, -1 is a state which
 * has had no tessellation attempt made, 0 indicates a solid, valid full
 * brep tessellation is present, and >0 indicates that number of faces has
 * been tessellated but not the full brep. */
extern BREP_EXPORT int
ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s);

/* Retrieve structured information about the most recent tessellation attempt.
 * Returns 0 on success and -1 for invalid arguments. */
extern BREP_EXPORT int
ON_Brep_CDT_Diagnostic(struct brep_cdt_diagnostic *diagnostic,
	const struct ON_Brep_CDT_State *s);

/* Return the number of faces which failed in the most recent tessellation.
 * If faces is non-NULL, copy at most capacity stable face indices. */
extern BREP_EXPORT int
ON_Brep_CDT_Failed_Faces(int *faces, int capacity,
	const struct ON_Brep_CDT_State *s);

/* Copy the diagnostic recorded for a failed face in the most recent
 * tessellation.  Returns zero on success and -1 if no failure was recorded
 * for face_index. */
extern BREP_EXPORT int
ON_Brep_CDT_Face_Diagnostic(struct brep_cdt_diagnostic *diagnostic,
	int face_index, const struct ON_Brep_CDT_State *s);

/* Construct a vlist plot from the tessellation.  Modes are:
 *
 * 0 - shaded 3D triangles
 * 1 - 3D triangle wireframe
 * 2 - 2D triangle wireframe (from parametric space)
 *
 * Returns 0 if vlist was successfully generated, else -1
 */
extern BREP_EXPORT int
ON_Brep_CDT_VList(
    struct bv_vlblock *vbp,
    struct bu_list *vlfree,
    struct bu_color *c,
    int mode,
    struct ON_Brep_CDT_State *s);

/* Given two or more triangulation states, refine them to clear any face
 * overlaps introduced by the triangulation.  If any of the states are
 * un-tessellated, first perform the tessellation indicated by the state
 * settings and then proceed to resolve after all states have an initial
 * tessellation.  Returns 0 if no changes were needed, the number of
 * updated CDT states if changes were made, and -1 if one or more
 * unresolvable overlaps were encountered.  Individual CDT states may
 * subsequently be queried for other information about their specific
 * states with other function calls - this function returns only the
 * overall result. */
extern BREP_EXPORT int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt, double lthreshold, int timeout);

#if 0
/* Report the number of other tessellation states which manifest unresolvable
 * overlaps with state s.  If the ovlps argument is non-null, populate with
 * the problematic states.  If no resolve step was performed on s, return -1 */
extern BREP_EXPORT int
ON_Brep_CDT_UnResolvable_Ovlps(std::vector<struct ON_Brep_CDT_State *> *ovlps, struct ON_Brep_CDT_State *s);
#endif

/* Retrieve the face, vertex and normal information from a tessellation state
 * in the form of integer and fastf_t arrays. */
/* TODO - need to allow optional specification of specific faces here -
 * have already hit one scenario where I want triangle information from
 * specific faces. */
extern BREP_EXPORT int
ON_Brep_CDT_Mesh(
    int **faces, int *fcnt,
    fastf_t **vertices, int *vcnt,
    int **face_normals, int *fn_cnt,
    fastf_t **normals, int *ncnt,
    struct ON_Brep_CDT_State *s,
    int exp_face_cnt, int *exp_faces
    );

#ifdef __cplusplus
/* Original (fast but not watertight) routine used for plotting */
extern BREP_EXPORT int
brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const ON_Brep *brep, struct bu_list *p_vhead,
	struct bv_vlblock *vbp, struct bu_list *vlfree,
      	int index, int plottype, int num_points);

/* Routine to capture the triangles from the fast CDT process
 * for caching */
extern BREP_EXPORT int
brep_cdt_fast(int **faces, int *face_cnt, vect_t **pnt_norms, point_t **pnts, int *pntcnt,
	const ON_Brep *brep, int index, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

/* Resource controls and diagnostics for display-quality tessellation.  A
 * zero option value selects the library default.  max_time_ms is checked
 * between faces; the per-face samplers also have fixed progress and recursion
 * guards to prevent non-terminating refinement.  face_status, when non-NULL,
 * is called exactly once per requested face during serial result assembly.
 * face_output, when non-NULL, reports the contiguous output ranges assigned
 * to each completed face with drawable geometry.  The optional trim-sample
 * callbacks replace sampling for trims where trim_sample_count returns at
 * least two points.  Samples must be ordered in trim direction and supply
 * the trim parameter, face UV coordinate, and exact 3D boundary coordinate.
 * trim_sample_source may associate an opaque identity with each supplied
 * sample.  point_source reports that identity for output points derived from
 * supplied samples, or NULL for generated points.  Its point_index is in the
 * final, concatenated output point array.  The identity is never dereferenced
 * by libbrep and need remain valid only until brep_cdt_fast_ex returns.
 * Callbacks may be invoked concurrently when max_workers exceeds one. */
struct brep_cdt_fast_options {
    size_t max_workers;
    size_t max_result_bytes;
    size_t max_points;
    long max_time_ms;
    int allow_partial;
    void (*face_status)(int face_index, int status, void *data);
    void *face_status_data;
    void (*face_output)(int face_index, size_t first_face,
	size_t face_count, size_t first_point, size_t point_count, void *data);
    void *face_output_data;
    size_t (*trim_sample_count)(int face_index, int trim_index, void *data);
    int (*trim_sample)(int face_index, int trim_index, size_t sample_index,
	fastf_t *trim_parameter, point2d_t uv, point_t point, void *data);
    void *trim_sample_data;
    const void *(*trim_sample_source)(int face_index, int trim_index,
	size_t sample_index, void *data);
    void (*point_source)(int face_index, size_t point_index,
	const void *source, void *data);
    void *point_source_data;
};

#define BREP_CDT_FAST_FACE_COMPLETED 0
#define BREP_CDT_FAST_FACE_FAILED 1
#define BREP_CDT_FAST_FACE_SKIPPED_DEGENERATE 2
#define BREP_CDT_FAST_FACE_NOT_PROCESSED 3

/* completed_faces includes faces proven to have no drawable area;
 * skipped_degenerate_faces reports that subset explicitly. */
struct brep_cdt_fast_report {
    int requested_faces;
    int completed_faces;
    int failed_faces;
    size_t result_bytes;
    int hit_time_limit;
    int hit_memory_limit;
    int hit_point_limit;
    /* Completed faces proven to have exactly zero parametric area. */
    int skipped_degenerate_faces;
};

#define BREP_CDT_FAST_OK 0
#define BREP_CDT_FAST_PARTIAL 1
#define BREP_CDT_FAST_ERROR -1
#define BREP_CDT_FAST_LIMIT -2

extern BREP_EXPORT void
brep_cdt_fast_options_default(struct brep_cdt_fast_options *options);

extern BREP_EXPORT int
brep_cdt_fast_ex(int **faces, int *face_cnt, vect_t **pnt_norms,
	point_t **pnts, int *pntcnt, const ON_Brep *brep, int index,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct brep_cdt_fast_options *options,
	struct brep_cdt_fast_report *report);
#endif

/* PImpl exposure of some mesh operations for use in tests - not to be considered public API */
struct cdt_bmesh_impl;
struct cdt_bmesh {
    struct cdt_bmesh_impl *i;
};
extern BREP_EXPORT int cdt_bmesh_create(struct cdt_bmesh **m);
extern BREP_EXPORT void cdt_bmesh_destroy(struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_deserialize(const char *fname, struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_bmesh_repair(struct cdt_bmesh *m);
extern BREP_EXPORT int cdt_test_boundary_start(void);
extern BREP_EXPORT int cdt_test_boundary_steiner_filter(void);
extern BREP_EXPORT int cdt_test_spurious_components(void);
extern BREP_EXPORT int cdt_test_local_defects(void);
extern BREP_EXPORT int cdt_test_edge_singular_pair(void);
extern BREP_EXPORT int cdt_test_linear_edge_spacing(void);
extern BREP_EXPORT int cdt_test_bounded_edge_midpoint(void);
extern BREP_EXPORT int cdt_test_assembled_mesh_validation(void);
extern BREP_EXPORT int cdt_test_assembled_shared_chords(void);
extern BREP_EXPORT int cdt_test_repair_edge_tube(void);
extern BREP_EXPORT int cdt_test_repair_triangle_split(void);
extern BREP_EXPORT int cdt_test_repair_patch_limits(void);
extern BREP_EXPORT int cdt_test_repair_duplicate_quarantine(void);
extern BREP_EXPORT int cdt_test_repair_periodic_strip(void);
extern BREP_EXPORT int cdt_test_repair_rigorous_boundary(void);
extern BREP_EXPORT int cdt_test_repair_patch_boundary(void);
extern BREP_EXPORT int cdt_test_subtolerance_edge_collapse(void);
extern BREP_EXPORT int cdt_test_subtolerance_ring(void);
extern BREP_EXPORT int cdt_test_developable_clean(void);

__END_DECLS

/** @} */

#endif  /* BREP_CDT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
