/*                        E D H R T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "../edit_private.h"

#define ECMD_HRT_SET_CENTER 43001
#define ECMD_HRT_SET_XDIR   43002
#define ECMD_HRT_SET_YDIR   43003
#define ECMD_HRT_SET_ZDIR   43004
#define ECMD_HRT_SET_D      43005

static int
_hrt_validate(struct rt_edit *s, const struct rt_hrt_internal *h)
{
    fastf_t mag_tol = (s && s->tol) ? s->tol->dist : SMALL_FASTF;
    fastf_t ortho_tol = 1.0e-6;

    if (MAGSQ(h->xdir) <= mag_tol*mag_tol || MAGSQ(h->ydir) <= mag_tol*mag_tol || MAGSQ(h->zdir) <= mag_tol*mag_tol) {
	bu_vls_printf(s->log_str, "HRT axes must be non-zero vectors\n");
	return BRLCAD_ERROR;
    }

    vect_t xu, yu, zu;
    VMOVE(xu, h->xdir);
    VMOVE(yu, h->ydir);
    VMOVE(zu, h->zdir);
    VUNITIZE(xu);
    VUNITIZE(yu);
    VUNITIZE(zu);

    if (!NEAR_ZERO(VDOT(xu, yu), ortho_tol) ||
	!NEAR_ZERO(VDOT(xu, zu), ortho_tol) ||
	!NEAR_ZERO(VDOT(yu, zu), ortho_tol)) {
	bu_vls_printf(s->log_str, "HRT axes must be mutually perpendicular\n");
	return BRLCAD_ERROR;
    }

    if (h->d <= SMALL_FASTF) {
	bu_vls_printf(s->log_str, "HRT cusp distance 'd' must be positive\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

void
rt_edit_hrt_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_HRT_SET_CENTER:
	case ECMD_HRT_SET_XDIR:
	case ECMD_HRT_SET_YDIR:
	case ECMD_HRT_SET_ZDIR:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_HRT_SET_D:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }
}

static void
hrt_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_hrt_set_edit_mode(s, arg);
    rt_edit_process(s);
}

struct rt_edit_menu_item hrt_menu[] = {
    { "HRT MENU", NULL, 0 },
    { "Set Center", hrt_ed, ECMD_HRT_SET_CENTER },
    { "Set X Dir", hrt_ed, ECMD_HRT_SET_XDIR },
    { "Set Y Dir", hrt_ed, ECMD_HRT_SET_YDIR },
    { "Set Z Dir", hrt_ed, ECMD_HRT_SET_ZDIR },
    { "Set Cusp Distance", hrt_ed, ECMD_HRT_SET_D },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_hrt_menu_item(const struct bn_tol *UNUSED(tol))
{
    return hrt_menu;
}

const char *
rt_edit_hrt_keypoint(point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *UNUSED(tol))
{
    static const char *strp = "V";
    struct rt_hrt_internal *h = (struct rt_hrt_internal *)s->es_int.idb_ptr;
    RT_HRT_CK_MAGIC(h);

    MAT4X3PNT(*pt, mat, h->v);
    return strp;
}

int
rt_edit_hrt_edit(struct rt_edit *s)
{
    struct rt_hrt_internal *h = (struct rt_hrt_internal *)s->es_int.idb_ptr;
    RT_HRT_CK_MAGIC(h);

    switch (s->edit_flag) {
	case ECMD_HRT_SET_CENTER:
	    if (s->e_inpara < 3) {
		bu_vls_printf(s->log_str, "set_center requires x y z\n");
		return BRLCAD_ERROR;
	    }
	    VSET(h->v,
		s->e_para[0] * s->local2base,
		s->e_para[1] * s->local2base,
		s->e_para[2] * s->local2base);
	    return _hrt_validate(s, h);
	case ECMD_HRT_SET_XDIR:
	    if (s->e_inpara < 3) {
		bu_vls_printf(s->log_str, "set_xdir requires x y z\n");
		return BRLCAD_ERROR;
	    }
	    VSET(h->xdir,
		s->e_para[0] * s->local2base,
		s->e_para[1] * s->local2base,
		s->e_para[2] * s->local2base);
	    return _hrt_validate(s, h);
	case ECMD_HRT_SET_YDIR:
	    if (s->e_inpara < 3) {
		bu_vls_printf(s->log_str, "set_ydir requires x y z\n");
		return BRLCAD_ERROR;
	    }
	    VSET(h->ydir,
		s->e_para[0] * s->local2base,
		s->e_para[1] * s->local2base,
		s->e_para[2] * s->local2base);
	    return _hrt_validate(s, h);
	case ECMD_HRT_SET_ZDIR:
	    if (s->e_inpara < 3) {
		bu_vls_printf(s->log_str, "set_zdir requires x y z\n");
		return BRLCAD_ERROR;
	    }
	    VSET(h->zdir,
		s->e_para[0] * s->local2base,
		s->e_para[1] * s->local2base,
		s->e_para[2] * s->local2base);
	    return _hrt_validate(s, h);
	case ECMD_HRT_SET_D:
	    if (s->e_inpara < 1) {
		bu_vls_printf(s->log_str, "set_d requires a scalar value\n");
		return BRLCAD_ERROR;
	    }
	    h->d = s->e_para[0] * s->local2base;
	    return _hrt_validate(s, h);
	default:
	    return edit_generic(s);
    }
}

int
rt_edit_hrt_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    return edit_generic_xy(s, mousevec);
}

static const struct rt_edit_param_desc hrt_point_param[] = {
    { "point", "Point", RT_EDIT_PARAM_POINT, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc hrt_vec_param[] = {
    { "vector", "Vector", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc hrt_d_param[] = {
    { "d", "Cusp Distance", RT_EDIT_PARAM_SCALAR, 0,
      SMALL_FASTF, RT_EDIT_PARAM_NO_LIMIT, "length", 0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc hrt_cmds[] = {
    { ECMD_HRT_SET_CENTER, "Set Center",        "geometry", 1, hrt_point_param, 1, 10 },
    { ECMD_HRT_SET_XDIR,   "Set X Direction",   "axes",     1, hrt_vec_param,   1, 20 },
    { ECMD_HRT_SET_YDIR,   "Set Y Direction",   "axes",     1, hrt_vec_param,   1, 30 },
    { ECMD_HRT_SET_ZDIR,   "Set Z Direction",   "axes",     1, hrt_vec_param,   1, 40 },
    { ECMD_HRT_SET_D,      "Set Cusp Distance", "shape",    1, hrt_d_param,     1, 50 }
};

static const struct rt_edit_prim_desc hrt_prim_desc = {
    "hrt", "Heart", 5, hrt_cmds
};

const struct rt_edit_prim_desc *
rt_edit_hrt_edit_desc(void)
{
    return &hrt_prim_desc;
}

int
rt_edit_hrt_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    if (!s || !vals)
	return 0;
    struct rt_hrt_internal *h = (struct rt_hrt_internal *)s->es_int.idb_ptr;
    RT_HRT_CK_MAGIC(h);

    switch (cmd_id) {
	case ECMD_HRT_SET_CENTER:
	    VSCALE(vals, h->v, s->base2local);
	    return 3;
	case ECMD_HRT_SET_XDIR:
	    VSCALE(vals, h->xdir, s->base2local);
	    return 3;
	case ECMD_HRT_SET_YDIR:
	    VSCALE(vals, h->ydir, s->base2local);
	    return 3;
	case ECMD_HRT_SET_ZDIR:
	    VSCALE(vals, h->zdir, s->base2local);
	    return 3;
	case ECMD_HRT_SET_D:
	    vals[0] = h->d * s->base2local;
	    return 1;
	default:
	    return 0;
    }
}
