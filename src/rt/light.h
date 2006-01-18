/*                         L I G H T . H
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file light.h
 *
 *  Declarations related to light sources
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *
 *  @(#)$Header$ (BRL)
 */
struct light_pt {
	point_t	lp_pt;
	vect_t	lp_norm;
};
#define LPT_MAGIC 0x327649
#define MAX_LIGHT_SAMPLES 128

struct light_specific {
	struct bu_list	l;	/* doubly linked list */
	/* User-specified fields */
	vect_t	lt_target;	/* explicit coordinate aim point */
	fastf_t	lt_intensity;	/* Intensity Lumens (cd*sr): total output */
	fastf_t	lt_angle;	/* beam dispersion angle (degrees) 0..180 */
	fastf_t	lt_fraction;	/* fraction of total light */
	int	lt_shadows;	/* !0 if this light casts shadows, # of rays*/
	int	lt_infinite;	/* !0 if infinitely distant */
	int	lt_visible;	/* 0 if implicitly modeled or invisible */
	int	lt_invisible;	/* 0 if implicitly modeled or invisible */
	int	lt_exaim;	/* !0 if explicit aim in lt_target */
	fastf_t lt_obscure;	/* percentage obscuration of light */
	/* Internal fields */
#ifdef RT_MULTISPECTRAL
	struct bn_tabdata *lt_spectrum;	/* Units?  mw*sr ? */
#else
	vect_t	lt_color;	/* RGB, as 0..1 */
#endif
	fastf_t	lt_radius;	/* approximate radius of spherical light */
	fastf_t	lt_cosangle;	/* cos of lt_angle */
	vect_t	lt_pos;		/* location in space of light */
	vect_t	lt_vec;		/* Unit vector from origin to light */
	vect_t	lt_aim;		/* Unit vector - light beam direction */
	char	*lt_name;	/* identifying string */
	struct	region *lt_rp;	/* our region of origin */
	int	lt_pt_count;
	struct light_pt lt_sample_pts[MAX_LIGHT_SAMPLES];
	fastf_t lt_parse_pt[6];
};
#define LIGHT_NULL	((struct light_specific *)0)
#define LIGHT_MAGIC	0xdbddbdb7
#define RT_CK_LIGHT(_p)	BU_CKMAG((_p), LIGHT_MAGIC, "light_specific")

extern struct light_specific	LightHead;

extern void light_cleanup(void);
extern void light_maker(int num, mat_t v2m);
extern int light_init(struct application *ap);

BU_EXTERN(void light_obs, (struct application *ap, struct shadework *swp, int have) );



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
