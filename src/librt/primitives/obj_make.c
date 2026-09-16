/*                    O B J _ M A K E . C
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

#include "common.h"

#include "bu/ptbl.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"


/**
 * Geometry-alias map: a handful of make/in type words are pure geometry
 * aliases of another primitive ("sph" is an ELL, "rcc" is a TGC, "arb6" is
 * an ARB8, ...).  These have no functab of their own (or, for sph/rec, one
 * that shares methods but has no ft_make), so we map them onto their base
 * and pass the original word through to ft_make as the variant selector.
 */
static const struct {
    const char *variant;
    const char *base;
} obj_make_amap[] = {
    {"ell1", "ell"}, {"sph",  "ell"},
    {"rcc",  "tgc"}, {"rec",  "tgc"}, {"tec", "tgc"}, {"trc", "tgc"},
    {"rpp",  "arb8"}, {"arb7", "arb8"}, {"arb6", "arb8"}, {"arb5", "arb8"}, {"arb4", "arb8"},
    {"grp",  "grip"},
    {NULL, NULL}
};


/**
 * Resolve a user type word to the functab label that owns its ft_make
 *
 * TODO: This mapping is intentionally NOT taught to rt_get_functab_by_label
 * (atleast yet, needs to be verified): doing so could break put/form/import
 */
static const char *
obj_make_base_label(const char *label)
{
    for (int i = 0; obj_make_amap[i].variant; i++) {
	if (BU_STR_EQUAL(label, obj_make_amap[i].variant))
	    return obj_make_amap[i].base;
    }

    return label;
}


/**
 * Types that own a real ft_make but are not makeable
 * TODO: some of these can/should be makeable, but as a first pass of refactor these were not supported
 */
static int
obj_make_denied(const char *label)
{
    static const char * const denylist[] = {
	"binunif", "comb", "constrnt", "dsp", "ebm", "material",
	"nurb", "revolve", "script", "spline", "submodel", "vol",
	NULL
    };

    for (int i = 0; denylist[i]; i++) {
	if (BU_STR_EQUAL(label, denylist[i]))
	    return 1;
    }
    return 0;
}


int
rt_obj_make(const char *label, const point_t origin, double scale, struct rt_db_internal *ip)
{
    const char* base;
    const struct rt_functab* ftp;

    if (!label || !ip)
	return BRLCAD_ERROR;

    RT_CK_DB_INTERNAL(ip);

    if (obj_make_denied(label))
	return BRLCAD_ERROR;

    /* resolve any geometry alias to its base, then look up the functab */
    base = obj_make_base_label(label);
    ftp = rt_get_functab_by_label(base);
    if (!ftp)
	return BRLCAD_ERROR;

    if (!ftp->ft_make)
	return BRLCAD_ERROR;

    return ftp->ft_make(ftp, ip, label, origin, scale);
}


static int
obj_make_is_makeable(const char *label)
{
    const struct rt_functab *ftp;

    if (obj_make_denied(label))
	return 0;

    ftp = rt_get_functab_by_label(obj_make_base_label(label));
    return (ftp && ftp->ft_make) ? 1 : 0;
}


static int
obj_make_label_cmp(const void *p1, const void *p2, void *UNUSED(ctx))
{
    const char *a = *(const char * const *)p1;
    const char *b = *(const char * const *)p2;
    return bu_strcmp(a, b);
}


void
rt_obj_make_labels(struct bu_vls *labels, const char *sep)
{
    struct bu_ptbl tbl = BU_PTBL_INIT_ZERO;
    const char *join = (sep) ? sep : " ";

    if (!labels)
	return;

    /* every functab entry that owns an ft_make and isn't denied */
    for (int id = 1; id <= ID_MAXIMUM; id++) {
	if (OBJ[id].ft_make && !obj_make_denied(OBJ[id].ft_label))
	    bu_ptbl_ins(&tbl, (long *)OBJ[id].ft_label);
    }

    /* plus the geometry-alias words whose base is makeable */
    for (int i = 0; obj_make_amap[i].variant; i++) {
	if (obj_make_is_makeable(obj_make_amap[i].variant))
	    bu_ptbl_ins(&tbl, (long *)obj_make_amap[i].variant);
    }

    /* alphabetical for a clean, scannable list */
    bu_sort((void *)BU_PTBL_BASEADDR(&tbl), BU_PTBL_LEN(&tbl),
	    sizeof(long *), obj_make_label_cmp, NULL);

    for (size_t i = 0; i < BU_PTBL_LEN(&tbl); i++) {
	const char *w = (const char *)BU_PTBL_GET(&tbl, i);
	bu_vls_printf(labels, "%s%s", (i) ? join : "", w);
    }

    bu_ptbl_free(&tbl);
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
