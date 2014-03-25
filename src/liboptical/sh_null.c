/*                       S H _ N U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file liboptical/sh_null.c
 *
 * Notes -
 * This is the null, a/k/a invisible, shader.  It is potentially useful as a performance
 * metric as well as to hide objects from a scene rendering.  It simply does nothing.
 *
 * This is the most basic shader.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


HIDDEN int sh_null_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int sh_null_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void sh_null_print(register struct region *rp, genptr_t dp);
HIDDEN void sh_null_free(genptr_t cp);

/* The "mfuncs" table describes what the user interface may call this shader.
 * The null shader may be referred to as null or invisible.  Note that the
 * four shader functions *must* be defined, even if they do nothing.
 */
struct mfuncs null_mfuncs[] = {
    {MF_MAGIC,	"null",		0,		MFI_HIT,	0, sh_null_setup,	sh_null_render,	sh_null_print,	sh_null_free },
    {MF_MAGIC,	"invisible",	0,		MFI_HIT,	0, sh_null_setup,	sh_null_render,	sh_null_print,	sh_null_free },
    {0,		(char *)0,	0,		0,		0, 0,		0,		0,		0 }
};


/*
 * This routine is called (at prep time) once for each region which uses this
 * shader.  Any shader-specific initialization should be done here.  It should
 * return 1 on success and -1 on failure.  Alternatively, this routine should
 * return 0 to delete this region's shader information after setup (i.e. it's
 * not needed for whatever reason to it won't be rendered).
 *
 * The null shader has nothing to do during setup since it doesn't actually
 * have anything to do during render0.  Its setup returns 0 since there's no
 * need to keep any region info.  This means that sh_null_render will not even
 * get called.
 */
HIDDEN int
sh_null_setup(register struct region *UNUSED(rp), struct bu_vls *UNUSED(matparm), genptr_t *UNUSED(dpp), const struct mfuncs *UNUSED(mfp), struct rt_i *UNUSED(rtip))
{
    /* no point to check the arguments since we do nothing with them.  we leave the error
     * checking to elsewhere when used.
     */

    /* no point in keeping this region's data around */
    return 0;
}


/*
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.  This is, of course, not necessary when setup returns 0.
 *
 * The null shader actually does "something", though it is not called.
 * It has to at least pass the ray through so that it can actually
 * raytrace what is visible behind the invisible object.  Otherwise,
 * an empty black void would be rendered.  this is not really important
 * though, since it shouldn't normally be called.
 */
HIDDEN int
sh_null_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t UNUSED(dp))
{
    /* check the validity of the arguments we got */

    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);

    /* shadework structures do not have magic numbers or other means to test
     * their validity
     */

    bu_log("Who called sh_null_render explicitly?");

    /* here is what actually makes the object invisible/null instead of being a
     * black void (if render ever is called).
     */
    (void)rr_render(ap, pp, swp);

    return 1;
}


/*
 * This routine is called if setup fails (which it never should).
 */
HIDDEN void
sh_null_print(register struct region *rp, genptr_t UNUSED(dp))
{
    bu_log("%s uses the null shader\n", rp->reg_name);
}


/*
 * This routine is called after all rendering has completed.  The intent is
 * normally to release any specific structures that were allocated during
 * setup or rendering.
 *
 * The null shader allocates nothing.  Therefore it releases nothing.
 */
HIDDEN void
sh_null_free(genptr_t UNUSED(cp))
{
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
