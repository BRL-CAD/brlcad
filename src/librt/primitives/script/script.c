/*                        S C R I P T . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/script/script.c
 *
 * Provide support for procedural scripts.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/debug.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid SCRIPT, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 SCRIPT is OK
 * !0 Error in description
 *
 */
int
rt_script_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    if (!stp)
        return -1;
    RT_CK_SOLTAB(stp);
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (rtip) RT_CK_RTI(rtip);

    stp->st_specific = (void *)NULL;
    return 0;
}


void
rt_script_print(const struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}


/**
 * Intersect a ray with a script.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_script_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    if (!stp || !rp || !ap || !seghead)
        return 0;

    RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    RT_CK_APPLICATION(ap);

    /* script cannot be ray traced.
     */

    return 0;			/* MISS */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_script_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !rp)
        return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the script.
 */
void
rt_script_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
        return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


void
rt_script_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp)
        return;
}


void
rt_script_free(struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}


int
rt_script_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bv *UNUSED(info))
{
    struct rt_script_internal *script_ip;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    script_ip = (struct rt_script_internal *)ip->idb_ptr;
    RT_SCRIPT_CK_MAGIC(script_ip);

    if (bu_vls_addr(&script_ip->s_type)) {
	bu_log("Script data not found or not specified\n");
    }

    return 0;
}


/**
 * Import a script from the database format to the internal format.
 */
int
rt_script_import4(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *UNUSED(mat), const struct db_i *dbip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (ep) BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    return -1;
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_script_export4(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    if (ep) BU_CK_EXTERNAL(ep);
    if (ip) RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    return -1;
}


/**
 * Import a script from the database format to the internal format.
 */
int
rt_script_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *UNUSED(dbip))
{
    struct rt_script_internal *script_ip;
    unsigned char *ptr;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SCRIPT;
    ip->idb_meth = &OBJ[ID_SCRIPT];
    BU_ALLOC(ip->idb_ptr, struct rt_script_internal);

    script_ip = (struct rt_script_internal *)ip->idb_ptr;
    BU_VLS_INIT(&script_ip->s_type);
    script_ip->script_magic = RT_SCRIPT_INTERNAL_MAGIC;

    ptr = ep->ext_buf;

    bu_vls_init(&script_ip->s_type);
    bu_vls_strncpy(&script_ip->s_type, (char *)ptr,
           ep->ext_nbytes - (ptr - (unsigned char *)ep->ext_buf));

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_script_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *dbip)
{
    struct rt_script_internal *script_ip;
    unsigned char *cp;
    size_t rem;

    rem = ep->ext_nbytes;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_SCRIPT) return -1;
    script_ip = (struct rt_script_internal *)ip->idb_ptr;
    RT_SCRIPT_CK_MAGIC(script_ip);

    BU_CK_EXTERNAL(ep);

    /* tally up size of buffer needed */
    ep->ext_nbytes = SIZEOF_NETWORK_LONG + bu_vls_strlen(&script_ip->s_type) + 1;

    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "script external");

    cp = (unsigned char *)ep->ext_buf;

    *(uint32_t *)cp = htonl(RT_SCRIPT_INTERNAL_MAGIC);
    cp += SIZEOF_NETWORK_LONG;

    bu_strlcpy((char *)cp, bu_vls_addr(&script_ip->s_type), rem);
    cp += bu_vls_strlen(&script_ip->s_type) + 1;
 
    return 0;
}


/**
 * Make human-readable formatted presentation of this primitive.  First
 * line describes type of primitive. Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_script_describe(struct bu_vls *str, const struct rt_db_internal *ip, int UNUSED(verbose), double UNUSED(mm2local))
{
    char buf[256];
    struct rt_script_internal *script_ip =
        (struct rt_script_internal *)ip->idb_ptr;

    RT_SCRIPT_CK_MAGIC(script_ip);
    bu_vls_strcat(str, "Script \n");


    sprintf(buf, "\tScript type: %s\n", bu_vls_addr(&script_ip->s_type));
    bu_vls_strcat(str, buf);
    bu_vls_strcat(str, "\n");


    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_script_ifree(struct rt_db_internal *ip)
{
    struct rt_script_internal *script_ip;

    RT_CK_DB_INTERNAL(ip);

    script_ip = (struct rt_script_internal *)ip->idb_ptr;
    RT_SCRIPT_CK_MAGIC(script_ip);
    script_ip->script_magic = 0;			/* sanity */

    if (BU_VLS_IS_INITIALIZED(&script_ip->s_type))
	bu_vls_free(&script_ip->s_type);

    bu_free((char *)script_ip, "script ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_script_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    BU_CK_VLS(logstr);
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, " script type");

    return BRLCAD_OK;
}


int
rt_script_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_script_internal *script_ip=(struct rt_script_internal *)intern->idb_ptr;

    BU_CK_VLS(logstr);
    RT_SCRIPT_CK_MAGIC(script_ip);

    if (attr == (char *)NULL) {
        bu_vls_strcpy(logstr, "script");
    } else {
        /* unrecognized attribute */
        bu_vls_printf(logstr, "ERROR: Unknown attribute\n");
        return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_script_adjust(struct bu_vls *UNUSED(logstr), struct rt_db_internal *intern, int UNUSED(argc), const char **UNUSED(argv))
{
    struct rt_script_internal *script_ip;

    RT_CK_DB_INTERNAL(intern);
    script_ip = (struct rt_script_internal *)intern->idb_ptr;
    RT_SCRIPT_CK_MAGIC(script_ip);

    /* stub */

    return BRLCAD_OK;
}


int
rt_script_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
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
