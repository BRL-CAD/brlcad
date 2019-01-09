/*                    P N T S _ U T I L. C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file libged/pnts_util.c
 *
 * utility functionality for simple Point Set (pnts) primitive operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/color.h"
#include "rt/geom.h"
#include "./ged_private.h"
#include "./pnts_util.h"

#define PNT_V_IN(_pt, _key, _v)               \
    switch (_key) {                           \
        case 'x':                             \
            ((struct _pt *)point)->v[X] = _v; \
            break;                            \
        case 'y':                             \
            ((struct _pt *)point)->v[Y] = _v; \
            break;                            \
        case 'z':                             \
            ((struct _pt *)point)->v[Z] = _v; \
            break;                            \
    }

#define PNT_C_IN(_pt, _key, _v)                       \
    switch (_key) {                                   \
        case 'r':                                     \
            ((struct _pt *)point)->c.buc_rgb[0] = _v; \
            break;                                    \
        case 'g':                                     \
            ((struct _pt *)point)->c.buc_rgb[1] = _v; \
            break;                                    \
        case 'b':                                     \
            ((struct _pt *)point)->c.buc_rgb[2] = _v; \
            break;                                    \
    }

#define PNT_S_IN(_pt, _key, _v)           \
    switch (_key) {                       \
	case 's':                         \
	   ((struct _pt *)point)->s = _v; \
	break;                            \
    }

#define PNT_N_IN(_pt, _key, _v)              \
    switch (_key) {                          \
	case 'i':                            \
	   ((struct _pt *)point)->n[X] = _v; \
	break;                               \
	case 'j':                            \
	   ((struct _pt *)point)->n[Y] = _v; \
	break;                               \
	case 'k':                            \
	   ((struct _pt *)point)->n[Z] = _v; \
	break;                               \
    }


const char *
_ged_pnt_default_fmt_str(rt_pnt_type type) {
    static const char *pntfmt = "xyz";
    static const char *colfmt = "xyzrgb";
    static const char *scafmt = "xyzs";
    static const char *nrmfmt = "xyzijk";
    static const char *colscafmt = "xyzsrgb";
    static const char *colnrmfmt = "xyzijkrgb";
    static const char *scanrmfmt = "xyzijks";
    static const char *colscanrmfmt = "xyzijksrgb";

    switch (type) {
	case RT_PNT_TYPE_PNT:
	    return pntfmt;
	    break;
	case RT_PNT_TYPE_COL:
	    return colfmt;
	    break;
	case RT_PNT_TYPE_SCA:
	    return scafmt;
	    break;
	case RT_PNT_TYPE_NRM:
	    return nrmfmt;
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    return colscafmt;
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    return colnrmfmt;
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    return scanrmfmt;
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    return colscanrmfmt;
	    break;
	default:
	    return NULL;
	    break;
    }
}

void
_ged_pnt_v_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_PNT:
	    PNT_V_IN(pnt, key, val);
	    break;
	case RT_PNT_TYPE_COL:
	    PNT_V_IN(pnt_color, key, val);
	    break;
	case RT_PNT_TYPE_SCA:
	    PNT_V_IN(pnt_scale, key, val);
	    break;
	case RT_PNT_TYPE_NRM:
	    PNT_V_IN(pnt_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_V_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_V_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_V_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_V_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
_ged_pnt_c_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_COL:
	    PNT_C_IN(pnt_color, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_C_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_C_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_C_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
_ged_pnt_s_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_SCA:
	    PNT_S_IN(pnt_scale, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    PNT_S_IN(pnt_color_scale, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_S_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_S_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}

void
_ged_pnt_n_set(void *point, rt_pnt_type type, char key, fastf_t val) {
    switch (type) {
	case RT_PNT_TYPE_NRM:
	    PNT_N_IN(pnt_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    PNT_N_IN(pnt_color_normal, key, val);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    PNT_N_IN(pnt_scale_normal, key, val);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    PNT_N_IN(pnt_color_scale_normal, key, val);
	    break;
	default:
	    break;
    }
}


rt_pnt_type
_ged_pnts_fmt_type(const char *fc)
{
    int has_pnt = 0;
    int has_nrm = 0;
    int has_s = 0;
    int has_c = 0;

    if (!fc || !strlen(fc)) return RT_PNT_UNKNOWN;
    if (strchr(fc, 'x') && strchr(fc, 'y') && strchr(fc, 'z')) has_pnt = 1;
    if (strchr(fc, 'i') && strchr(fc, 'j') && strchr(fc, 'k')) has_nrm = 1;
    if (strchr(fc, 's')) has_s = 1;
    if (strchr(fc, 'r') && strchr(fc, 'g') && strchr(fc, 'b')) has_c = 1;

    if (has_pnt && has_nrm && has_s && has_c) return RT_PNT_TYPE_COL_SCA_NRM;
    if (has_pnt && has_nrm && has_s) return RT_PNT_TYPE_SCA_NRM;
    if (has_pnt && has_nrm && has_c) return RT_PNT_TYPE_COL_NRM;
    if (has_pnt && has_nrm) return RT_PNT_TYPE_NRM;
    if (has_pnt && has_s && has_c) return RT_PNT_TYPE_COL_SCA;
    if (has_pnt && has_s) return RT_PNT_TYPE_SCA;
    if (has_pnt && has_c) return RT_PNT_TYPE_SCA;
    if (has_pnt) return RT_PNT_TYPE_PNT;

    return RT_PNT_UNKNOWN;
}

rt_pnt_type
_ged_pnts_fmt_guess(int numcnt) {
    switch (numcnt) {
	case 3:
	    return RT_PNT_TYPE_PNT;
	    break;
	case 4:
	    return RT_PNT_TYPE_SCA;
	    break;
	case 6:
	    bu_log("Warning - found 6 numbers, which is ambiguous - assuming RT_PNT_TYPE_NRM.  To read in as RT_PNT_TYPE_COL, specify the format string \"xyzrgb\"\n");
	    return RT_PNT_TYPE_NRM;
	    break;
	case 7:
	    bu_log("Warning - found 7 numbers, which is ambiguous - assuming RT_PNT_TYPE_COL_SCA.  To read in as RT_PNT_TYPE_SCA_NRM, specify the format string \"xyzijks\"\n");
	    return RT_PNT_TYPE_COL_SCA;
	    break;
	case 9:
	    return RT_PNT_TYPE_COL_NRM;
	    break;
	case 10:
	    return RT_PNT_TYPE_COL_SCA_NRM;
	    break;
	default:
	    return RT_PNT_UNKNOWN;
	    break;
    }
}

int
_ged_pnts_fmt_match(rt_pnt_type t, int numcnt)
{
    if (t == RT_PNT_UNKNOWN || numcnt < 3 || numcnt > 10) return 0;

    if (numcnt == 3 && t == RT_PNT_TYPE_PNT) return 1;
    if (numcnt == 4 && t == RT_PNT_TYPE_SCA) return 1;
    if (numcnt == 6 && t == RT_PNT_TYPE_NRM) return 1;
    if (numcnt == 6 && t == RT_PNT_TYPE_COL) return 1;
    if (numcnt == 7 && t == RT_PNT_TYPE_COL_SCA) return 1;
    if (numcnt == 7 && t == RT_PNT_TYPE_SCA_NRM) return 1;
    if (numcnt == 9 && t == RT_PNT_TYPE_COL_NRM) return 1;
    if (numcnt == 10 && t == RT_PNT_TYPE_COL_SCA_NRM) return 1;

    return 0;
}

void
_ged_pnts_init_head_pnt(struct rt_pnts_internal *pnts)
{
    if (!pnts) return;
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_INIT(&(((struct pnt *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_INIT(&(((struct pnt_color *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_INIT(&(((struct pnt_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_INIT(&(((struct pnt_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_INIT(&(((struct pnt_color_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_scale_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_scale_normal *)pnts->point)->l));
	    break;
	default:
	    break;
    }
}


void *
_ged_pnts_new_pnt(rt_pnt_type t)
{
    void *point = NULL;
    if (t == RT_PNT_UNKNOWN) return NULL;
    switch (t) {
	case RT_PNT_TYPE_PNT:
	    BU_ALLOC(point, struct pnt);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_ALLOC(point, struct pnt_color);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_ALLOC(point, struct pnt_scale);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_ALLOC(point, struct pnt_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_ALLOC(point, struct pnt_color_scale);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_ALLOC(point, struct pnt_color_normal);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_ALLOC(point, struct pnt_scale_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_ALLOC(point, struct pnt_color_scale_normal);
	    break;
	default:
	    break;
    }
    return point;
}

void
_ged_pnts_add(struct rt_pnts_internal *pnts, void *point)
{
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_PUSH(&(((struct pnt *)pnts->point)->l), &((struct pnt *)point)->l);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_PUSH(&(((struct pnt_color *)pnts->point)->l), &((struct pnt_color *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_PUSH(&(((struct pnt_scale *)pnts->point)->l), &((struct pnt_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_PUSH(&(((struct pnt_normal *)pnts->point)->l), &((struct pnt_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_PUSH(&(((struct pnt_color_scale *)pnts->point)->l), &((struct pnt_color_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_PUSH(&(((struct pnt_color_normal *)pnts->point)->l), &((struct pnt_color_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_PUSH(&(((struct pnt_scale_normal *)pnts->point)->l), &((struct pnt_scale_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_PUSH(&(((struct pnt_color_scale_normal *)pnts->point)->l), &((struct pnt_color_scale_normal *)point)->l);
	    break;
	default:
	    break;
    }
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
