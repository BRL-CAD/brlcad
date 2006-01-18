/*                          S H A D E W O R K . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @file shadework.h
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */
#ifndef SHADEWORK_H
#define SHADEWORK_H

/* for light_specific */
/* #include "light.h" */

#define SW_NLIGHTS	16		/* Max # of light sources */

/*
 *			S H A D E W O R K
 */
struct shadework {
/* XXX At least the first three of these need to be spectral curves */
	fastf_t		sw_transmit;	/* 0.0 -> 1.0 */
	fastf_t		sw_reflect;	/* 0.0 -> 1.0 */
	fastf_t		sw_extinction;	/* extinction coeff, mm^-1 */
	fastf_t		sw_refrac_index;
	fastf_t		sw_temperature;
#if RT_MULTISPECTRAL
	struct bn_tabdata *msw_color;
	struct bn_tabdata *msw_basecolor;
#else
	fastf_t		sw_color[3];	/* shaded color */
	fastf_t		sw_basecolor[3]; /* base color */
#endif
	struct hit	sw_hit;		/* ray hit (dist,point,normal) */
	struct uvcoord	sw_uv;
#if RT_MULTISPECTRAL
	struct bn_tabdata *msw_intensity[SW_NLIGHTS];
#else
	fastf_t		sw_intensity[3*SW_NLIGHTS]; /* light intensities */
#endif
	fastf_t		sw_tolight[3*SW_NLIGHTS];   /* light directions */
	struct light_specific	*sw_visible[SW_NLIGHTS]; /* visibility flags/ptrs */
	fastf_t		sw_lightfract[SW_NLIGHTS];/* % light visible */
	int		sw_xmitonly;	/* flag: need sw_transmit only */
					/* sw_xmitonly=1, compute transmission only */
					/* sw_xmitonly=2, want parameters only, not even transmission */
	int		sw_inputs;	/* fields from mf_inputs actually filled */
	int		sw_frame;	/* # of current frame */
	fastf_t		sw_frametime;	/* frame time delta off 1st frame */
	fastf_t		sw_pixeltime;	/* pixel time delta off 1st pixel of 1st frame */
	struct seg	*sw_segs;	/* segs which made partition */
/*
 * The following is experimental.  DO NOT USE
 */
#define	SW_SET_TRANSMIT		0x01
#define SW_SET_REFLECT		0x02
#define SW_SET_REFRAC_INDEX	0x04
#define SW_SET_EXTINCTION	0x10
#define SW_SET_AMBIENT		0x20
#define SW_SET_EMISSION		0x40
	int		sw_phong_set_vector;
	fastf_t		sw_phong_transmit;
	fastf_t		sw_phong_reflect;
	fastf_t		sw_phong_ri;
	fastf_t		sw_phong_extinction;
	fastf_t		sw_phong_ambient;
	fastf_t		sw_phong_emission;
/*
 * End of experimental
 */
};

BU_EXTERN(void		pr_shadework, (const char *str, const struct shadework *swp));
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
