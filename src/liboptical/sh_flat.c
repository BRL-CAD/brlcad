/*                       S H _ F L A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file sh_flat.c
 *
 * Notes -
 * This is a basic flat shader.  It will display an object with a set color
 * without taking any effects such as curvature, emission, reflection, etc
 * into consideration.  It simply shades an object constantly with either
 * (in order of reverse priority) 1) the default flat color (white),
 * 2) it's set region color, 2) the specified flat shader color (given via
 * the color attribute).
 *
 * Optionally a transparency value may be shown as well.  With transparency
 * turned on, the background objects will blend through depending on the
 * transparency attribute.  Masking may be specified for each RGB color
 * channel (e.g. perhaps only lets red light through).  This is potentially
 * useful to see background objects through foreground objects, as well as
 * in certain signal analyses as a filter.
 *
 * See the flat_parse_tab structure for details on the symantics of other
 * attribute shorthands/alternatives.
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
#include <string.h> /* for memcpy */
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"

extern int rr_render(struct application	*ap, struct partition *pp, struct shadework *swp);

HIDDEN int	flat_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), flat_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	flat_print(register struct region *rp, char *dp), flat_free(char *cp);

/* these are two helper functions to process input color and transparency values */
void normalizedInput_hook(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value);
void singleNormalizedInput_hook(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value);

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct flat_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	point_t color;  /* overriding flat color */
	point_t transparency; /* how transparent the object is per rgb channel*/
};
#define FLAT_MAGIC 0x464c4154   /* magic number is "FLAT" in hex */
#define CK_FLAT_SP(_p) BU_CKMAG(_p, FLAT_MAGIC, "flat_specific")

/* The default values for the variables in the shader specific structure */
const static
struct flat_specific flat_defaults = {
	FLAT_MAGIC,
	{ 1.0, 1.0, 1.0 }, /* full white */
	{ 0.0, 0.0, 0.0 }  /* completely opaque (no transparency)*/
};

#define SHDR_FLAT	((struct flat_specific *)0)
#define SHDR_O(m)	offsetof(struct flat_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct flat_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above.
 *
 * color := is a 0.0 to 1.0 or 0.0 to 255.0 color intensity triplet
 *          representing the RGB color values.
 * rgb := the same as color, just shorter (and implies 0 to 255)
 * bright := sets all three color channels to the same single given intensity.
 *           e.g. a grey value (bright==.2 => color=={.2 .2 .2}==rgb
 * transparency := a color intensity triplet mask to indicate how much of
 *                 background object light are visible through this object.
 * alpha := similar to bright, sets this object's alpha transparency to a
 *          single mask value that gets set for each channel.  e.g 40% opaque
 *          is alpha==.4 which is equiv to transparency=={.4 .4 .4}).
 */
struct bu_structparse flat_parse_tab[] = {
	{ "%f", 3, "color", SHDR_O(color), normalizedInput_hook}, /* for 0->1 color values */
	{ "%f", 3, "rgb", SHDR_O(color), normalizedInput_hook}, /* for 0->255 color values */
	{ "%f", 1, "bright", SHDR_O(color), singleNormalizedInput_hook}, /* for luminosity gray value */
	{ "%f", 3, "transparency", SHDR_O(transparency), normalizedInput_hook}, /* for rgb 0->1 transparency */
	{ "%f", 1, "alpha", SHDR_O(transparency), singleNormalizedInput_hook}, /* for single channel alpha transparency */
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs flat_mfuncs[] = {
	{MF_MAGIC,	"flat",		0,		MFI_HIT,	0, /* !!! try to set to 0 */
	 flat_setup,	flat_render,	flat_print,	flat_free },
	{0,		(char *)0,	0,		0,		0,
	 0,		0,		0,		0 }
};


/* normalizedInput_hook
 *
 * Used as a hooked function for input of values normalized to 1.0.
 *
 * sdp == structure description
 * name == struct member name
 * base == begining of structure
 * value == string containing value
 */
void
normalizedInput_hook( register const struct bu_structparse *sdp, register const char *name, char *base, const char *value ) {

	register double *p = (double *)(base+sdp->sp_offset);
	register int i;
	int ok;

	/* if all the values are in the range [0..1] there's nothing to do */
	for (ok=1, i=0 ; i < sdp->sp_count ; i++, p++) {
		if ( (*p > 1.0) || (*p < 0.0) ) ok = 0;
	}
	if (ok) return;

	/* user specified colors in the range [0..255] (or negative) so we need to
	 * map those into [0..1]
	 */
	p = (double *)(base+sdp->sp_offset);
	for (i=0 ; i < sdp->sp_count ; i++, p++) {
		*p /= 255.0;
	}

	for (ok=1, i=0 ; i < sdp->sp_count ; i++, p++) {
		if ( (*p > 1.0) || (*p < 0.0) ) ok = 0;
	}
	if (ok) bu_log ("User specified values are out of range (0.0 to either 1.0 or 255.0)");
}


/* singleNormalizedInput_hook
 *
 * same as normalizedInput_hook (in fact it calls it) except that only one input
 * is expected from user input.  the hook takes the single input value, and sets
 * it three times.  the value is normalized from 0.0 to 1.0
 */
void
singleNormalizedInput_hook( register const struct bu_structparse *sdp, register const char *name, char *base, const char *value ) {

	register double *p = (double *)(base+sdp->sp_offset);

	normalizedInput_hook(sdp, name, base, value);

	/* copy the first value into the next two locations */
	*(p+1) = *p;
	*(p+2) = *p;
}


/*	F L A T _ S E T U P
 *
 * This routine is called (at prep time) once for each region which uses this
 * shader.  The shader specific flat_specific structure is allocated and
 * default values are set.  Then any user-given values override.
 */
HIDDEN int
flat_setup( register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip) {

	register struct flat_specific	*flat_sp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);

	if (rdebug&RDEBUG_SHADE)
		bu_log("flat_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( flat_sp, flat_specific );
	*dpp = (char *)flat_sp;

	/* color priority:
	 *
	 * priority goes first to the flat shader's color parameter
	 * second priority is to the material color value
	 * third  priority is to the flat shader's default color (white)
	 */

	/* initialize the default values for the shader */
	memcpy(flat_sp, &flat_defaults, sizeof(struct flat_specific) );

	/* load the material color if it was set */
	if (rp->reg_mater.ma_color_valid) {
		VMOVE(flat_sp->color, rp->reg_mater.ma_color);
	}

	/* parse the user's arguments for this use of the shader. */
	if (bu_struct_parse( matparm, flat_parse_tab, (char *)flat_sp ) < 0 )
		return(-1);

	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", flat_parse_tab, (char *)flat_sp );
	}

	return(1);
}


/*
 *	F L A T _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 *
 *  The flat shader is probably the second most simple shader (second to
 *  the null/invisible shader -- it does nothing).  It shades an object
 *  a constant color.  The only complexity comes into play when a
 *  transparency value is set.  Then we get the color value behind the
 *  one we are shading and blend accordingly with the flat color.
 */
int
flat_render( struct application *ap, struct partition *pp, struct shadework *swp, char *dp ) {

	register struct flat_specific *flat_sp = (struct flat_specific *)dp;
	LOCAL const point_t unit = {1.0, 1.0, 1.0};
	point_t intensity;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_FLAT_SP(flat_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "flat_render Parameters:", flat_parse_tab, (char *)flat_sp );

	/* do the actual flat color shading for the flat object. if the object is
	 * not transparent, just put the color.  if the object is transparent, do
	 * a little more work determining the background pixel, and then blend with
	 * the flat foreground object.
	 */
	if (VNEAR_ZERO(flat_sp->transparency, SMALL_FASTF)) {

		/* just put the flat value */
		VMOVE(swp->sw_color, flat_sp->color);
	} else {

		/* this gets the background pixel value, if the transparency is not 0 */
		swp->sw_transmit=1.0; /*!!! try to remove */
		VMOVE(swp->sw_basecolor, flat_sp->transparency);
		(void)rr_render( ap, pp, swp );

		/* now blend with the foreground object being shaded */
		VSUB2(intensity, unit, flat_sp->transparency);  /* inverse transparency is how much we want */
		VELMUL(intensity, intensity, flat_sp->color); /* ??? is there a way to merge this mul->add step? */
		VADD2(swp->sw_color, swp->sw_color, intensity);
	}

	return(1);
}


/*
 *	F L A T _ P R I N T
 */
HIDDEN void
flat_print( register struct region *rp, char *dp ) {
	bu_struct_print( rp->reg_name, flat_parse_tab, (char *)dp );
}


/*
 *	F L A T _ F R E E
 */
HIDDEN void
flat_free( char *cp ) {
	bu_free( cp, "flat_specific" );
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
