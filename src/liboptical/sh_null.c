/*                       S H _ N U L L . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file sh_null.c
 *
 * Notes -
 * This is the null, aka invisible, shader.  It is potentially useful as a performance
 * metric as well as to hide objects from a scene rendering.  It simply does nothing.
 *
 * This is the most basic shader.
 *
 * Author -
 * Christopher Sean Morrison
 *
 * Source -
 * The U.S. Army Research Laboratory
 * Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"

extern int rr_render(struct application *ap, struct partition *pp, struct shadework *swp);

HIDDEN int	null_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), null_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	null_print(register struct region *rp, char *dp), null_free(char *cp);

/* The "mfuncs" table describes what the user interface may call this shader.
 * The null shader may be referred to as null or invisible.  Note that the
 * four shader functions *must* be defined, even if they do nothing.
 */
struct mfuncs null_mfuncs[] = {
	{MF_MAGIC,	"null",		0,		MFI_HIT,	0,
	null_setup,	null_render,	null_print,	null_free },

	{MF_MAGIC,	"invisible",		0,		MFI_HIT,	0,
	null_setup,	null_render,	null_print,	null_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	N U L L _ S E T U P
 *
 *	This routine is called (at prep time) once for each region which uses this
 *  shader.  Any shader-specific initialization should be done here.  It should
 *  return 1 on success and -1 on failure.  Alternatively, this routine should
 *  return 0 to delete this region's shader information after setup (i.e. it's
 *  not needed for whatever reason to it won't be rendered).
 *
 *  The null shader has nothing to do during setup since it doesn't actually
 *  have anything to do during render0.  It's setup returns 0 since there's no
 *  need to keep any region info.  This means that null_render will not even
 *  get called.
 */
HIDDEN int
null_setup( register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip ) {

	/* no point to check the arguments since we do nothing with them.  we leave the error
	 * checking to elsewhere when used.
	 */

	/* no point in keeping this region's data around */
	return 0;
}


/*
 *	N U L L _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.  This is, of course, not necessary when setup returns 0.
 *
 *  The null shader actually does "something", though it is not called.
 *  It has to at least pass the ray through so that it can actually
 *  raytrace what is visible behind the invisible object.  Otherwise,
 *  an empty black void would be rendered.  this is not really important
 *  though, since it shouldn't normally be called.
 */
HIDDEN int
null_render( struct application *ap, struct partition *pp, struct shadework *swp, char *dp ) {

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	/* shadework structures do not have magic numbers or other means to test
	 * their validity
	 */

	bu_log("Who called null_render explicitly?");

	/* here is what actually makes the object invisible/null instead of being a
	 * black void (if render ever is called).
	 */
	(void)rr_render(ap, pp, swp);

	return(1);
}


/*
 *	N U L L _ P R I N T
 *
 * This routine is called if setup fails (which it never should).
 */
HIDDEN void
null_print( register struct region *rp, char *dp ) {
	bu_log("%S uses the null shader\n", rp->reg_name);
}


/*
 *	N U L L _ F R E E
 *
 *  This routine is called after all rendering has completed.  The intent is
 *  normally to release any specific structures that were allocated during
 *  setup or rendering.
 *
 *  The null shader allocates nothing.  Therefore it releases nothing.
 */
HIDDEN void
null_free( char *cp ) {
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
