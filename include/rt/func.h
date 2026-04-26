/*                        R T F U N C . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2026 United States Government as represented by
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
/** @addtogroup rt_obj
 *
 * @brief Primitive manipulation functions from former functab
 * callback table.
 *
 * As this is a relatively new set of interfaces, consider these
 * functions preliminary (i.e. DEPRECATED) and subject to change until
 * this message goes away.
 *
 */
#ifndef RT_FUNC_H
#define RT_FUNC_H

#include "common.h"

#include "bu/list.h"
#include "bu/parse.h"
#include "bu/vls.h"
#include "bg/plane.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/functab.h"
#include "rt/hit.h"
#include "rt/piece.h"
#include "rt/resource.h"
#include "rt/seg.h"
#include "rt/soltab.h"
#include "rt/tol.h"
#include "rt/db_internal.h"
#include "rt/db_instance.h"
#include "rt/rt_instance.h"
#include "rt/xray.h"
#include "pc.h"

/** @{ */
/** @file rt/func.h */

__BEGIN_DECLS

/**
 * prep an object for ray tracing
 */
RT_EXPORT extern int rt_obj_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);

/**
 * shoot a ray at an object that has been prepped for ray tracing
 */
RT_EXPORT extern int rt_obj_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead);

/**
 * TBD.
 */
RT_EXPORT extern int rt_obj_piece_shot(struct rt_piecestate *psp, struct rt_piecelist *plp, double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead);

/**
 * TBD.
 */
RT_EXPORT extern int rt_obj_piece_hitsegs(struct rt_piecestate *psp, struct seg *seghead, struct application *ap);

/**
 * print an objects parameters in debug/diagnostic form
 */
RT_EXPORT extern int rt_obj_print(const struct soltab *stp);

/**
 * calculate a normal on an object that has been hit via rt_shot()
 */
RT_EXPORT extern int rt_obj_norm(struct hit *hitp, struct soltab *stp, struct xray *rp);

/**
 * calculate object uv parameterization for a given hit point
 */
RT_EXPORT extern int rt_obj_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp);

/**
 * calculate object curvature for a given hit point
 */
RT_EXPORT extern int rt_obj_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp);

/**
 * DEPRECATED: Unimplemented.
 */
DEPRECATED RT_EXPORT extern int rt_obj_class(void);

/**
 * release the memory used by a solid
 */
RT_EXPORT extern int rt_obj_free(struct soltab *stp);

/**
 * obtain a vlist wireframe representation of an object for plotting purposes
 */
RT_EXPORT extern int rt_obj_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

/**
 * shoot an array of rays at a set of homogeneous objects.
 */
RT_EXPORT extern int rt_obj_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap);

/**
 * tessellate an object (into NMG form)
 */
RT_EXPORT extern int rt_obj_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

/**
 * tessellate an object (into NURBS NMG form)
 */
RT_EXPORT extern int rt_obj_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol);

/**
 * v4/v5 object import from disk
 */
RT_EXPORT extern int rt_obj_import(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip);

/**
 * v4/v5 object export to disk
 */
RT_EXPORT extern int rt_obj_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);

/**
 * free the internal representation of an object
 */
RT_EXPORT extern int rt_obj_ifree(struct rt_db_internal *ip);

/**
 * db object 'get' support, obtain a tcl list representation
 */
RT_EXPORT extern int rt_obj_get(struct bu_vls *logstr, const struct rt_db_internal *ip, const char *attr);

/**
 * db object 'adjust' support, modify a tcl list representation
 */
RT_EXPORT extern int rt_obj_adjust(struct bu_vls *logstr, struct rt_db_internal *ip, int argc, const char **argv);

/**
 * describe an object in text form (used by the 'l' command)
 */
RT_EXPORT extern int rt_obj_describe(struct bu_vls *logstr, const struct rt_db_internal *ip, int verbose, double mm2local);

/**
 * create a 'default' object
 */
RT_EXPORT extern int rt_obj_make(const struct rt_functab *ftp, struct rt_db_internal *ip);

/**
 * apply a matrix transformation to an object (translation, rotation, scale)
 */
RT_EXPORT extern int rt_obj_xform(struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int release, struct db_i *dbip);

/**
 * obtain parameters for an object in libpc form
 */
RT_EXPORT extern int rt_obj_params(struct pc_pc_set *ps, const struct rt_db_internal *ip);

/**
 * mirror an object about a plane
 */
RT_EXPORT extern int rt_obj_mirror(struct rt_db_internal *ip, const plane_t *plane);

/**
 * if `stp` is prepped, serialize; otherwise, deserialize from `external`
 */
RT_EXPORT extern int rt_obj_prep_serialize(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version);

/**
 * Stopping-criteria parameters for rt_crofton_shoot() and rt_crofton_sample().
 *
 * All fields default to zero.  Behaviour when all three are zero (or the
 * pointer is NULL) is identical to the historical default: fire 2 000 rays
 * and stop when two successive 1 %-threshold iterations agree.
 *
 * When one or more non-zero fields are provided, sampling continues until
 * the FIRST criterion that is met:
 *
 *   n_rays > 0        Stop once this many rays have been fired in total.
 *
 *   stability_mm > 0  Stop once the estimate is "stable" to within the
 *                     given linear dimension.  Stability is measured as the
 *                     change in equivalent-sphere radius between successive
 *                     iterations: r_sa = sqrt(SA / (4*pi)) for surface area
 *                     and r_v = cbrt(3*V / (4*pi)) for volume.  Sampling
 *                     stops when both |Δr_sa| and |Δr_v| (for whichever
 *                     outputs are requested) are < stability_mm.
 *
 *   time_ms > 0       Stop once this many wall-clock milliseconds have
 *                     elapsed, returning the best estimate accumulated so far.
 *
 * When multiple fields are non-zero the first criterion to fire wins,
 * giving callers fine control over the accuracy / speed trade-off.
 */
struct rt_crofton_params {
    size_t n_rays;       /**< max total rays; 0 = no limit via this criterion */
    double stability_mm; /**< equivalent-radius stability target (mm); 0 = disabled */
    double time_ms;      /**< wall-clock time budget (ms); 0 = disabled */
};


/**
 * Run the Cauchy-Crofton ray-sampling estimator on an already-prepared
 * raytrace instance.  The caller owns @p rtip and must call rt_free_rti
 * after this function returns.
 *
 * @param rtip          Prepared raytrace instance (rt_prep_parallel must
 *                      have been called first).
 * @param params        Stopping criteria.  NULL or all-zero → 2 000-ray default.
 * @param out_surf_area Receives the estimated surface area (mm^2).
 * @param out_volume    Receives the estimated volume (mm^3).
 * @return  The total number of ray-surface crossings accumulated during
 *          sampling (>= 0) on success; -1 on bad arguments.  A return
 *          value of 0 means no geometry was intersected by the sampler.
 *
 * @section crofton_near_tol Near-tolerance sliver geometry: CSG vs BoT divergence
 *
 * The Cauchy-Crofton formula is mathematically exact for any well-defined
 * solid, but its numerical result depends critically on how the underlying
 * raytracer reports intersections.  Two representations of what is intended
 * to be the same geometry can yield radically different -- and both
 * internally consistent -- surface-area estimates when the geometry contains
 * a sub-tolerance sliver.
 *
 * @subsection crofton_sliver_anatomy Anatomy of the sliver
 *
 * Consider a window-frame region modelled as a base box minus a slightly
 * smaller cutout box (the r.wind6 pattern in havoc.g).  If the subtractor's
 * face protrudes @e less than BN_TOL_DIST (0.0005 mm) past the base face,
 * a thin sliver of near-zero thickness is present in the raw CSG description.
 * The sliver has two large faces (the Z-faces of the cutout interior, each
 * ~41 600 mm² for a 160 × 260 mm cutout) and a negligible edge band.
 *
 * @subsection crofton_csg_behavior CSG raytracer behavior (boolweave filtering)
 *
 * The BRL-CAD CSG Boolean evaluator (boolweave) discards any solid segment
 * whose thickness is below BN_TOL_DIST.  For a ray fired perpendicular to the
 * sliver faces the two-segment chord through the sliver is 0.000340 mm thick
 * -- shorter than BN_TOL_DIST -- so boolweave merges the entry and exit
 * events and reports a MISS through that region.
 *
 * For oblique rays, however, the apparent thickness grows with the secant of
 * the angle from normal: chord = d / cos(θ).  Once θ exceeds approximately
 * 47° (for a 0.000340 mm gap and a 0.0005 mm tolerance) the sliver segment
 * survives boolweave filtering and contributes two crossing events.  Since
 * the Crofton bounding sphere uniformly samples all directions, roughly 68%
 * of solid angles subtend the sliver at angles steep enough to be counted.
 * The result is that the CSG SA estimate converges to a value that is ~74%
 * higher than the ideal clean-frame SA -- not because sampling is insufficient,
 * but because the CSG raytracer is correctly reflecting its own view of the
 * geometry: the sliver is visible from the majority of directions but hidden
 * from near-perpendicular directions.  Denser sampling does not reduce this
 * bias; experiments with up to 2 000 000 rays confirm that the CSG estimate
 * is stable at ~89 900 mm² (vs an ideal of 51 520 mm²) within the first
 * 500 000 rays and does not drift further regardless of sample count.
 *
 * @subsection crofton_bot_behavior BoT raytracer behavior (per-triangle hits)
 *
 * A triangle mesh (BoT) has no boolweave layer.  Every ray that intersects a
 * triangle face produces a hit event regardless of how thin the resulting
 * segment is.  A BoT tessellated from the raw CSG without any sliver
 * correction therefore exposes both large sliver faces to the full Crofton
 * hemisphere, converging to ~130 400 mm² -- approximately the ideal frame SA
 * plus both full sliver faces (51 520 + 2 × 41 600 ≈ 134 720 mm²).
 *
 * @subsection crofton_perturb_behavior Perturbed BoT (correct result)
 *
 * The facetize command's variant-planning / perturb pass enlarges the
 * subtractor just enough to eliminate the sub-tolerance sliver before Manifold
 * performs its Boolean evaluation.  The resulting BoT has no phantom interior
 * faces and its Crofton SA converges to ~51 750 mm² -- within 0.5% of the
 * analytic ideal -- typically in fewer than 64 000 rays (<0.1 s).
 *
 * @subsection crofton_summary Summary table (measured, 200 × 300 × 8 mm frame,
 *             gap = 0.000340 mm < BN_TOL_DIST = 0.0005 mm)
 *
 * | Representation         | Converged SA (mm²) | vs ideal | Stable by   |
 * |------------------------|--------------------|----------|-------------|
 * | CSG (raw, unperturbed) |  ~89 900           | +74.5%   | ~500k rays  |
 * | BoT (perturbed)        |  ~51 750           |  +0.5%   | ~64k rays   |
 * | BoT (no perturb)       | ~130 400           | +153%    | ~32k rays   |
 *
 * @subsection crofton_implication Design implication
 *
 * The Crofton estimator is accurate and well-converged in all three cases;
 * the divergence is caused by the geometry, not by sampling noise.  When
 * comparing a CSG Crofton SA against a BoT Crofton SA as a facetize quality
 * check, geometry with sub-tolerance slivers will always produce a mismatch
 * no matter how many rays are fired.  For such geometry the volume estimate
 * (which is insensitive to sliver SA: sliver volume is ~14 mm³ out of
 * 147 200 mm³, or 0.01%) is a far more reliable cross-check metric.
 */
RT_EXPORT extern int rt_crofton_shoot(struct rt_i *rtip, const struct rt_crofton_params *params, double *out_surf_area, double *out_volume);


/**
 * Cauchy-Crofton surface-area and/or volume estimator for a primitive.
 *
 * Creates a temporary in-memory raytrace of @p ip and fires random chord
 * rays until the stopping criteria in @p params are satisfied (or all
 * criteria are zero / params is NULL, in which case the 2 000-ray default
 * is used).  Stores the estimated surface area in @p *area (if non-NULL)
 * and the estimated volume in @p *vol (if non-NULL).
 *
 * This is the primary high-level entry point.  Primitives that require
 * controlled accuracy (e.g. TGC TEC case, triaxial ELL, EHY r1≠r2, HYP,
 * superell, ETO/TOR spindle) should call this function directly with
 * appropriate params.  Callers that already have a prepared rt_i should
 * call rt_crofton_shoot() instead.
 */
RT_EXPORT extern void rt_crofton_sample(fastf_t *area, fastf_t *vol,
					const struct rt_db_internal *ip,
					const struct rt_crofton_params *params);

__END_DECLS

#endif  /* RT_FUNC_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
