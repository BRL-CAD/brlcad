/*                         E D D S P . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/eddsp.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./eddsp.h"

extern const char * get_file_name(struct mged_state *s, char *str);

extern vect_t es_mparam;	/* mouse input param.  Only when es_mvalid set */
extern int es_mvalid;	/* es_mparam valid.  inpara must = 0 */

static void
dsp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_DSP_FNAME:
	    es_edflag = ECMD_DSP_FNAME;
	    break;
	case MENU_DSP_FSIZE:
	    es_edflag = ECMD_DSP_FSIZE;
	    break;
	case MENU_DSP_SCALE_X:
	    es_edflag = ECMD_DSP_SCALE_X;
	    break;
	case MENU_DSP_SCALE_Y:
	    es_edflag = ECMD_DSP_SCALE_Y;
	    break;
	case MENU_DSP_SCALE_ALT:
	    es_edflag = ECMD_DSP_SCALE_ALT;
	    break;
    }
    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item dsp_menu[] = {
    {"DSP MENU", NULL, 0 },
    {"Name", dsp_ed, MENU_DSP_FNAME },
    {"Set X", dsp_ed, MENU_DSP_SCALE_X },
    {"Set Y", dsp_ed, MENU_DSP_SCALE_Y },
    {"Set ALT", dsp_ed, MENU_DSP_SCALE_ALT },
    { "", NULL, 0 }
};

struct menu_item *
mged_dsp_menu_item(const struct bn_tol *UNUSED(tol))
{
    return dsp_menu;
}

static void
dsp_scale(struct mged_state *s, struct rt_dsp_internal *dsp, int idx)
{
    mat_t m, scalemat;

    RT_DSP_CK_MAGIC(dsp);

    MAT_IDN(m);

    if (es_mvalid) {
	bu_log("es_mvalid %g %g %g\n", V3ARGS(es_mparam));
    }

    if (inpara > 0) {
	m[idx] = es_para[0];
	bu_log("Keyboard %g\n", es_para[0]);
    } else if (!ZERO(s->edit_state.es_scale)) {
	m[idx] *= s->edit_state.es_scale;
	bu_log("s->edit_state.es_scale %g\n", s->edit_state.es_scale);
	s->edit_state.es_scale = 0.0;
    }

    bn_mat_xform_about_pnt(scalemat, m, es_keypoint);

    bn_mat_mul(m, dsp->dsp_stom, scalemat);
    MAT_COPY(dsp->dsp_stom, m);

    bn_mat_mul(m, scalemat, dsp->dsp_mtos);
    MAT_COPY(dsp->dsp_mtos, m);

}

void
ecmd_dsp_scale_x(struct mged_state *s)
{
    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSX);
}

void
ecmd_dsp_scale_y(struct mged_state *s)
{
    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSY);
}

void
ecmd_dsp_scale_alt(struct mged_state *s)
{
    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSZ);
}

int
ecmd_dsp_fname(struct mged_state *s)
{
    struct rt_dsp_internal *dsp =
	(struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr;
    const char *fname;
    struct stat stat_buf;
    b_off_t need_size;
    struct bu_vls message = BU_VLS_INIT_ZERO;

    RT_DSP_CK_MAGIC(dsp);

    /* Pop-up the Tk file browser */
    fname = get_file_name(s, bu_vls_addr(&dsp->dsp_name));
    if (! fname) return BRLCAD_OK;

    if (stat(fname, &stat_buf)) {
	bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
	Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	bu_vls_free(&message);
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    }

    need_size = dsp->dsp_xcnt * dsp->dsp_ycnt * 2;
    if (stat_buf.st_size < need_size) {
	bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
	Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
	bu_vls_free(&message);
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    }
    bu_vls_strcpy(&dsp->dsp_name, fname);

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
