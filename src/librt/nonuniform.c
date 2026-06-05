/*                    N O N U N I F O R M . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"

#include "librt_private.h"


#if defined(DBL_DECIMAL_DIG)
#  define NU_MAT_PRECISION DBL_DECIMAL_DIG
#elif defined(DECIMAL_DIG)
#  define NU_MAT_PRECISION DECIMAL_DIG
#else
#  define NU_MAT_PRECISION 17
#endif


static int
nonuniform_attr_parse(mat_t mat, const char *value)
{
    const char *cp;
    char *endp = NULL;
    int i;

    if (!mat || !value)
	return -1;

    cp = value;
    for (i = 0; i < 16; i++) {
	double d;

	while (isspace((unsigned char)*cp))
	    cp++;
	if (*cp == ',')
	    cp++;
	while (isspace((unsigned char)*cp))
	    cp++;

	errno = 0;
	d = strtod(cp, &endp);
	if (endp == cp || errno == ERANGE || !isfinite(d))
	    return -1;
	mat[i] = (fastf_t)d;
	cp = endp;
    }

    while (isspace((unsigned char)*cp))
	cp++;

    return (*cp == '\0') ? 0 : -1;
}


int
_rt_nonuniform_mat_validate(const mat_t mat)
{
    int i;

    if (!mat)
	return -1;

    for (i = 0; i < 16; i++) {
	if (!isfinite(mat[i]))
	    return -1;
    }

    if (!NEAR_ZERO(mat[12], VDIVIDE_TOL) ||
	!NEAR_ZERO(mat[13], VDIVIDE_TOL) ||
	!NEAR_ZERO(mat[14], VDIVIDE_TOL))
	return -1;

    if (NEAR_ZERO(mat[15], VDIVIDE_TOL))
	return -1;

    if (NEAR_ZERO(bn_mat_determinant(mat), VDIVIDE_TOL))
	return -1;

    return 0;
}


int
_rt_nonuniform_attr_get(mat_t mat, const struct rt_db_internal *ip)
{
    const char *value;

    if (!ip)
	return 0;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_avs.magic != BU_AVS_MAGIC)
	return 0;

    value = bu_avs_get(&ip->idb_avs, RT_MATRIX_NONUNIFORM_ATTR);
    if (!value)
	return 0;

    if (nonuniform_attr_parse(mat, value) < 0)
	return -1;

    return (_rt_nonuniform_mat_validate(mat) < 0) ? -1 : 1;
}


int
_rt_nonuniform_attr_remove(struct rt_db_internal *ip)
{
    if (!ip)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_avs.magic != BU_AVS_MAGIC)
	bu_avs_init_empty(&ip->idb_avs);

    bu_avs_remove(&ip->idb_avs, RT_MATRIX_NONUNIFORM_ATTR);
    return 0;
}


int
_rt_nonuniform_attr_set(struct rt_db_internal *ip, const mat_t mat)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;
    int i;

    if (!ip || !mat)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    if (_rt_nonuniform_mat_validate(mat) < 0)
	return -1;

    if (bn_mat_is_identity(mat))
	return _rt_nonuniform_attr_remove(ip);

    if (ip->idb_avs.magic != BU_AVS_MAGIC)
	bu_avs_init_empty(&ip->idb_avs);

    for (i = 0; i < 16; i++) {
	if (i)
	    bu_vls_putc(&v, ' ');
	bu_vls_printf(&v, "%.*g", NU_MAT_PRECISION, (double)mat[i]);
    }

    bu_avs_add(&ip->idb_avs, RT_MATRIX_NONUNIFORM_ATTR, bu_vls_addr(&v));
    bu_vls_free(&v);
    return 0;
}


int
_rt_nonuniform_attr_compose(struct rt_db_internal *ip, const mat_t mat)
{
    mat_t existing;
    mat_t composed;
    int have_existing;

    if (!ip || !mat)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    if (_rt_nonuniform_mat_validate(mat) < 0)
	return -1;

    have_existing = _rt_nonuniform_attr_get(existing, ip);
    if (have_existing < 0)
	return -1;

    if (have_existing) {
	bn_mat_mul(composed, mat, existing);
    } else {
	MAT_COPY(composed, mat);
    }

    return _rt_nonuniform_attr_set(ip, composed);
}


int
_rt_nonuniform_attr_copy(struct rt_db_internal *op, const struct rt_db_internal *ip)
{
    if (!op || !ip)
	return -1;

    RT_CK_DB_INTERNAL(op);
    RT_CK_DB_INTERNAL(ip);

    if (op == ip)
	return 0;

    if (op->idb_avs.magic == BU_AVS_MAGIC)
	bu_avs_free(&op->idb_avs);

    if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	bu_avs_init(&op->idb_avs, ip->idb_avs.count, "nonuniform attr copy");
	bu_avs_merge(&op->idb_avs, &ip->idb_avs);
    } else {
	bu_avs_init_empty(&op->idb_avs);
    }

    return 0;
}


int
_rt_nonuniform_export4_check(const char *func, const struct rt_db_internal *ip)
{
    mat_t existing;
    int have_existing;
    const char *name = func ? func : "export4";

    have_existing = _rt_nonuniform_attr_get(existing, ip);
    if (have_existing < 0) {
	bu_log("%s: invalid %s attribute cannot be represented in v4 geometry\n", name, RT_MATRIX_NONUNIFORM_ATTR);
	return -1;
    }
    if (have_existing > 0) {
	bu_log("%s: %s cannot be represented in v4 geometry\n", name, RT_MATRIX_NONUNIFORM_ATTR);
	return -1;
    }

    return 0;
}


fastf_t
_rt_nonuniform_volume_scale(const mat_t mat)
{
    fastf_t det;

    if (!mat)
	return 1.0;

    det = mat[0] * (mat[5] * mat[10] - mat[6] * mat[9])
	- mat[1] * (mat[4] * mat[10] - mat[6] * mat[8])
	+ mat[2] * (mat[4] * mat[9] - mat[5] * mat[8]);

    return fabs(det);
}


int
_rt_nonuniform_transform_needed(const struct rt_db_internal *ip, const mat_t mat)
{
    mat_t existing;
    int have_existing;

    if (!ip)
	return 0;

    if (!mat || bn_mat_is_identity(mat))
	return 0;

    have_existing = _rt_nonuniform_attr_get(existing, ip);
    if (have_existing)
	return have_existing;

    return bn_mat_is_non_unif(mat) ? 1 : 0;
}


int
_rt_nonuniform_transform_resolve(mat_t effective, int *remove_attr, const struct rt_db_internal *ip, const mat_t mat)
{
    mat_t existing;
    int have_existing;

    if (!effective || !mat)
	return -1;

    MAT_COPY(effective, mat);
    if (remove_attr)
	*remove_attr = 0;

    if (!ip)
	return bn_mat_is_non_unif(mat) ? 1 : 0;

    have_existing = _rt_nonuniform_attr_get(existing, ip);
    if (have_existing < 0)
	return -1;

    if (!have_existing)
	return bn_mat_is_non_unif(mat) ? 1 : 0;

    if (bn_mat_is_identity(mat))
	return 0;

    bn_mat_mul(effective, mat, existing);
    if (bn_mat_is_non_unif(effective))
	return 1;

    if (remove_attr)
	*remove_attr = 1;

    return 0;
}


void
_rt_nonuniform_transform_nmgregion(struct nmgregion *r, const mat_t mat, const struct bn_tol *tol)
{
    struct bu_ptbl verts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl norms = BU_PTBL_INIT_ZERO;
    mat_t inv, norm_mat;
    int have_norm_mat = 0;
    size_t i;

    if (!r || !mat)
	return;

    NMG_CK_REGION(r);

    nmg_vertex_tabulate(&verts, &r->l.magic, &rt_vlfree);
    for (i = 0; i < BU_PTBL_LEN(&verts); i++) {
	struct vertex *v = (struct vertex *)BU_PTBL_GET(&verts, i);
	point_t p;

	NMG_CK_VERTEX(v);
	if (!v->vg_p)
	    continue;
	NMG_CK_VERTEX_G(v->vg_p);
	MAT4X3PNT(p, mat, v->vg_p->coord);
	VMOVE(v->vg_p->coord, p);
    }
    bu_ptbl_free(&verts);

    if (bn_mat_inverse(inv, mat)) {
	MAT_TRANSPOSE(norm_mat, inv);
	have_norm_mat = 1;
    }

    if (!have_norm_mat)
	return;

    nmg_vertexuse_normal_tabulate(&norms, &r->l.magic, &rt_vlfree);
    for (i = 0; i < BU_PTBL_LEN(&norms); i++) {
	struct vertexuse_a_plane *vua = (struct vertexuse_a_plane *)BU_PTBL_GET(&norms, i);
	vect_t n;

	NMG_CK_VERTEXUSE_A_PLANE(vua);
	MAT3X3VEC(n, norm_mat, vua->N);
	VUNITIZE(n);
	VMOVE(vua->N, n);
    }
    bu_ptbl_free(&norms);

    if (tol)
	nmg_region_a(r, tol);
}


void
_rt_nonuniform_transform_bbox(point_t *omin, point_t *omax, const mat_t mat, const point_t imin, const point_t imax)
{
    int i;

    if (!omin || !omax || !mat || !imin || !imax)
	return;

    VSETALL(*omin, INFINITY);
    VSETALL(*omax, -INFINITY);

    for (i = 0; i < 8; i++) {
	point_t in, out;

	VSET(in,
	     (i & 1) ? imax[X] : imin[X],
	     (i & 2) ? imax[Y] : imin[Y],
	     (i & 4) ? imax[Z] : imin[Z]);
	MAT4X3PNT(out, mat, in);
	VMINMAX(*omin, *omax, out);
    }
}


static void
nonuniform_body_soltab(struct soltab *body_stp, const struct soltab *stp)
{
    *body_stp = *stp;
    VMOVE(body_stp->st_center, stp->st_nu_body_center);
    VMOVE(body_stp->st_min, stp->st_nu_body_min);
    VMOVE(body_stp->st_max, stp->st_nu_body_max);
    body_stp->st_aradius = stp->st_nu_body_aradius;
    body_stp->st_bradius = stp->st_nu_body_bradius;
}


int
_rt_nonuniform_soltab_setup(struct soltab *stp, const mat_t mat, const struct bn_tol *tol)
{
    point_t bmin, bmax;
    point_t center;
    fastf_t radius = 0.0;
    int i;

    if (!stp || !mat)
	return -1;

    RT_CK_SOLTAB(stp);
    if (tol) BN_CK_TOL(tol);

    if (_rt_nonuniform_mat_validate(mat) < 0)
	return -1;

    _rt_nonuniform_soltab_free(stp);

    stp->st_nu_matp = (matp_t)bu_malloc(sizeof(mat_t), "st_nu_matp");
    stp->st_nu_inv_matp = (matp_t)bu_malloc(sizeof(mat_t), "st_nu_inv_matp");
    stp->st_nu_norm_matp = (matp_t)bu_malloc(sizeof(mat_t), "st_nu_norm_matp");

    VMOVE(stp->st_nu_body_center, stp->st_center);
    VMOVE(stp->st_nu_body_min, stp->st_min);
    VMOVE(stp->st_nu_body_max, stp->st_max);
    stp->st_nu_body_aradius = stp->st_aradius;
    stp->st_nu_body_bradius = stp->st_bradius;

    MAT_COPY(stp->st_nu_matp, mat);
    if (!bn_mat_inverse(stp->st_nu_inv_matp, mat)) {
	_rt_nonuniform_soltab_free(stp);
	return -1;
    }
    MAT_TRANSPOSE(stp->st_nu_norm_matp, stp->st_nu_inv_matp);

    _rt_nonuniform_transform_bbox(&bmin, &bmax, mat, stp->st_min, stp->st_max);
    VMOVE(stp->st_min, bmin);
    VMOVE(stp->st_max, bmax);
    VADD2(center, bmin, bmax);
    VSCALE(center, center, 0.5);
    VMOVE(stp->st_center, center);

    for (i = 0; i < 8; i++) {
	point_t c;
	fastf_t d;

	VSET(c,
	     (i & 1) ? bmax[X] : bmin[X],
	     (i & 2) ? bmax[Y] : bmin[Y],
	     (i & 4) ? bmax[Z] : bmin[Z]);
	d = DIST_PNT_PNT(center, c);
	if (d > radius)
	    radius = d;
    }

    if (tol)
	radius += tol->dist;

    stp->st_aradius = stp->st_bradius = radius;
    return 0;
}


void
_rt_nonuniform_soltab_free(struct soltab *stp)
{
    if (!stp)
	return;

    if (stp->st_nu_matp) {
	bu_free((char *)stp->st_nu_matp, "st_nu_matp");
	stp->st_nu_matp = (matp_t)0;
    }
    if (stp->st_nu_inv_matp) {
	bu_free((char *)stp->st_nu_inv_matp, "st_nu_inv_matp");
	stp->st_nu_inv_matp = (matp_t)0;
    }
    if (stp->st_nu_norm_matp) {
	bu_free((char *)stp->st_nu_norm_matp, "st_nu_norm_matp");
	stp->st_nu_norm_matp = (matp_t)0;
    }
}


static int
nonuniform_body_ray_mat(struct xray *body_ray, fastf_t *dir_scale, const mat_t model_to_body, const struct xray *rp);


static int
nonuniform_body_ray(struct xray *body_ray, fastf_t *dir_scale, const struct soltab *stp, const struct xray *rp)
{
    if (!stp)
	return -1;

    return nonuniform_body_ray_mat(body_ray, dir_scale, stp->st_nu_inv_matp, rp);
}


static int
nonuniform_body_ray_mat(struct xray *body_ray, fastf_t *dir_scale, const mat_t model_to_body, const struct xray *rp)
{
    vect_t dir;
    fastf_t mag;

    if (!body_ray || !dir_scale || !model_to_body || !rp)
	return -1;

    *body_ray = *rp;
    MAT4X3PNT(body_ray->r_pt, model_to_body, rp->r_pt);
    MAT4X3VEC(dir, model_to_body, rp->r_dir);
    mag = MAGNITUDE(dir);
    if (NEAR_ZERO(mag, VDIVIDE_TOL))
	return -1;

    VSCALE(body_ray->r_dir, dir, 1.0 / mag);
    body_ray->r_min = rp->r_min * mag;
    body_ray->r_max = rp->r_max * mag;
    *dir_scale = mag;
    return 0;
}


int
_rt_nonuniform_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct soltab body_stp;
    struct xray body_ray;
    fastf_t dir_scale = 1.0;
    int ret;

    if (!stp || !rp)
	return -1;

    RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    if (ap) RT_CK_APPLICATION(ap);

    if (!stp->st_nu_inv_matp)
	return OBJ[stp->st_id].ft_shot(stp, rp, ap, seghead);

    if (nonuniform_body_ray(&body_ray, &dir_scale, stp, rp) < 0)
	return 0;

    nonuniform_body_soltab(&body_stp, stp);
    ret = OBJ[stp->st_id].ft_shot(&body_stp, &body_ray, ap, seghead);
    if (ret > 0) {
	struct seg *segp;

	for (BU_LIST_FOR(segp, seg, &seghead->l)) {
	    segp->seg_stp = stp;
	    segp->seg_in.hit_dist /= dir_scale;
	    segp->seg_out.hit_dist /= dir_scale;
	    segp->seg_in.hit_rayp = rp;
	    segp->seg_out.hit_rayp = rp;
	}
    }

    return ret;
}


int
_rt_nonuniform_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct soltab body_stp;
    struct hit body_hit;
    struct xray body_ray;
    fastf_t dir_scale = 1.0;
    vect_t n;

    if (!hitp || !stp)
	return -1;

    RT_CK_HIT(hitp);
    RT_CK_SOLTAB(stp);
    if (rp) RT_CK_RAY(rp);

    if (!stp->st_nu_inv_matp) {
	OBJ[stp->st_id].ft_norm(hitp, stp, rp);
	return 0;
    }

    if (!rp)
	return -1;

    if (nonuniform_body_ray(&body_ray, &dir_scale, stp, rp) < 0)
	return -1;

    body_hit = *hitp;
    body_hit.hit_dist = hitp->hit_dist * dir_scale;
    body_hit.hit_rayp = &body_ray;
    VJOIN1(body_hit.hit_point, body_ray.r_pt, body_hit.hit_dist, body_ray.r_dir);

    nonuniform_body_soltab(&body_stp, stp);
    OBJ[stp->st_id].ft_norm(&body_hit, &body_stp, &body_ray);

    MAT3X3VEC(n, stp->st_nu_norm_matp, body_hit.hit_normal);
    VUNITIZE(n);
    VMOVE(hitp->hit_normal, n);
    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);

    return 0;
}


int
_rt_nonuniform_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct soltab body_stp;
    struct hit body_hit;
    struct xray body_ray;
    struct xray *model_ray;
    fastf_t dir_scale = 1.0;

    if (!stp || !hitp || !uvp)
	return -1;

    if (!stp->st_nu_inv_matp) {
	OBJ[stp->st_id].ft_uv(ap, stp, hitp, uvp);
	return 0;
    }

    model_ray = hitp->hit_rayp ? hitp->hit_rayp : (ap ? &ap->a_ray : NULL);
    if (!model_ray)
	return -1;

    if (nonuniform_body_ray(&body_ray, &dir_scale, stp, model_ray) < 0)
	return -1;

    body_hit = *hitp;
    body_hit.hit_dist = hitp->hit_dist * dir_scale;
    body_hit.hit_rayp = &body_ray;
    VJOIN1(body_hit.hit_point, body_ray.r_pt, body_hit.hit_dist, body_ray.r_dir);

    nonuniform_body_soltab(&body_stp, stp);
    OBJ[stp->st_id].ft_uv(ap, &body_stp, &body_hit, uvp);
    return 0;
}


static int
nonuniform_unitize(vect_t v)
{
    fastf_t mag;

    if (!isfinite(v[X]) || !isfinite(v[Y]) || !isfinite(v[Z]))
	return -1;

    mag = MAGNITUDE(v);
    if (!isfinite(mag) || NEAR_ZERO(mag, VDIVIDE_TOL))
	return -1;

    VSCALE(v, v, 1.0 / mag);
    return 0;
}


static int
nonuniform_curve_finite(const struct curvature *cvp)
{
    if (!cvp)
	return 0;

    return isfinite(cvp->crv_c1) && isfinite(cvp->crv_c2) &&
	isfinite(cvp->crv_pdir[X]) &&
	isfinite(cvp->crv_pdir[Y]) &&
	isfinite(cvp->crv_pdir[Z]);
}


static int
nonuniform_transform_curve(struct curvature *cvp, const struct curvature *body_cvp, const vect_t body_normal, const mat_t body_to_model, const mat_t model_to_body)
{
    vect_t n, e1, e2;
    vect_t t1, t2, model_n;
    vect_t ev1, ev2;
    fastf_t normal_dot;
    fastf_t alpha;
    fastf_t g11, g12, g22;
    fastf_t l11, l21, l22, rad;
    fastf_t a, c, d;
    fastf_t b1, b2;
    fastf_t h00, h01, h11;
    fastf_t v0, v1;

    if (!cvp || !body_cvp || !body_to_model || !model_to_body)
	return -1;

    if (!nonuniform_curve_finite(body_cvp))
	return -1;

    VMOVE(n, body_normal);
    if (nonuniform_unitize(n) < 0)
	return -1;

    VMOVE(e1, body_cvp->crv_pdir);
    if (nonuniform_unitize(e1) < 0)
	bn_vec_ortho(e1, n);

    normal_dot = VDOT(e1, n);
    VJOIN1(e1, e1, -normal_dot, n);
    if (nonuniform_unitize(e1) < 0)
	bn_vec_ortho(e1, n);

    VCROSS(e2, n, e1);
    if (nonuniform_unitize(e2) < 0)
	return -1;

    MAT4X3VEC(t1, body_to_model, e1);
    MAT4X3VEC(t2, body_to_model, e2);

    VEC3X3MAT(model_n, n, model_to_body);
    alpha = MAGNITUDE(model_n);
    if (!isfinite(alpha) || NEAR_ZERO(alpha, VDIVIDE_TOL))
	return -1;
    VSCALE(model_n, model_n, 1.0 / alpha);

    g11 = VDOT(t1, t1);
    g12 = VDOT(t1, t2);
    g22 = VDOT(t2, t2);
    if (!isfinite(g11) || !isfinite(g12) || !isfinite(g22) ||
	g11 < VDIVIDE_TOL || g22 < VDIVIDE_TOL)
	return -1;

    /*
     * Solve B v = k G v.  With G = L L^T, the symmetric problem is
     * L^-1 B L^-T y = k y, followed by v = L^-T y.
     */
    l11 = sqrt(g11);
    if (!isfinite(l11) || NEAR_ZERO(l11, VDIVIDE_TOL))
	return -1;

    l21 = g12 / l11;
    rad = g22 - l21 * l21;
    if (!isfinite(rad) || rad < VDIVIDE_TOL)
	return -1;
    l22 = sqrt(rad);
    if (!isfinite(l22) || NEAR_ZERO(l22, VDIVIDE_TOL))
	return -1;

    a = 1.0 / l11;
    c = -l21 / (l11 * l22);
    d = 1.0 / l22;
    b1 = body_cvp->crv_c1 / alpha;
    b2 = body_cvp->crv_c2 / alpha;

    h00 = a * a * b1;
    h01 = a * c * b1;
    h11 = c * c * b1 + d * d * b2;

    bn_eigen2x2(&cvp->crv_c1, &cvp->crv_c2, ev1, ev2, h00, h01, h11);

    v1 = ev1[Y] / l22;
    v0 = (ev1[X] - l21 * v1) / l11;
    VCOMB2(cvp->crv_pdir, v0, t1, v1, t2);

    normal_dot = VDOT(cvp->crv_pdir, model_n);
    VJOIN1(cvp->crv_pdir, cvp->crv_pdir, -normal_dot, model_n);
    if (nonuniform_unitize(cvp->crv_pdir) < 0)
	return -1;

    if (!nonuniform_curve_finite(cvp))
	return -1;

    return 0;
}


int
_rt_nonuniform_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp, const mat_t body_to_model, const mat_t model_to_body)
{
    struct curvature body_curve = RT_CURVATURE_INIT_ZERO;
    struct curvature model_curve = RT_CURVATURE_INIT_ZERO;
    struct soltab body_stp;
    struct hit body_hit;
    struct xray body_ray;
    const struct rt_functab *ft;
    fastf_t dir_scale = 1.0;

    if (!cvp || !hitp || !stp || !body_to_model || !model_to_body)
	return -1;

    RT_CK_HIT(hitp);
    RT_CK_SOLTAB(stp);
    *cvp = model_curve;

    if (!hitp->hit_rayp)
	return -1;
    RT_CK_RAY(hitp->hit_rayp);

    ft = &OBJ[stp->st_id];
    if (!ft->ft_norm || !ft->ft_curve)
	return -1;

    if (nonuniform_body_ray_mat(&body_ray, &dir_scale, model_to_body, hitp->hit_rayp) < 0)
	return -1;

    body_hit = *hitp;
    body_hit.hit_dist = hitp->hit_dist * dir_scale;
    body_hit.hit_rayp = &body_ray;
    VJOIN1(body_hit.hit_point, body_ray.r_pt, body_hit.hit_dist, body_ray.r_dir);

    nonuniform_body_soltab(&body_stp, stp);
    ft->ft_norm(&body_hit, &body_stp, &body_ray);
    ft->ft_curve(&body_curve, &body_hit, &body_stp);

    if (nonuniform_transform_curve(&model_curve, &body_curve, body_hit.hit_normal, body_to_model, model_to_body) < 0)
	return -1;

    *cvp = model_curve;
    return 0;
}


void
_rt_nonuniform_transform_vlist(struct bu_list *vhead, const mat_t mat)
{
    struct bv_vlist *vp;

    if (!vhead || !mat)
	return;

    for (BU_LIST_FOR(vp, bv_vlist, vhead)) {
	size_t i;

	BV_CK_VLIST(vp);
	for (i = 0; i < vp->nused; i++) {
	    switch (vp->cmd[i]) {
		case BV_VLIST_LINE_MOVE:
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_POLY_END:
		case BV_VLIST_TRI_MOVE:
		case BV_VLIST_TRI_DRAW:
		case BV_VLIST_TRI_END:
		case BV_VLIST_POINT_DRAW:
		    {
			point_t p;
			MAT4X3PNT(p, mat, vp->pt[i]);
			VMOVE(vp->pt[i], p);
		    }
		    break;
		case BV_VLIST_POLY_VERTNORM:
		case BV_VLIST_TRI_VERTNORM:
		    {
			vect_t n;
			mat_t inv, norm_mat;

			if (bn_mat_inverse(inv, mat)) {
			    MAT_TRANSPOSE(norm_mat, inv);
			    MAT3X3VEC(n, norm_mat, vp->pt[i]);
			    VUNITIZE(n);
			    VMOVE(vp->pt[i], n);
			}
		    }
		    break;
		default:
		    break;
	    }
	}
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
