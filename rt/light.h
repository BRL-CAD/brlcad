/*
 *			L I G H T . H
 *
 *  Declarations related to light sources
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 *
 *  @(#)$Header$ (BRL)
 */
struct light_specific {
	struct bu_list	l;	/* doubly linked list */
	/* User-specified fields */
	vect_t	lt_dir;		/* explicit coordinate aim */
	fastf_t	lt_intensity;	/* Intensity Lumens (cd*sr): total output */
	fastf_t	lt_angle;	/* beam dispersion angle (degrees) 0..180 */
	fastf_t	lt_fraction;	/* fraction of total light */
	int	lt_shadows;	/* !0 if this light casts shadows, # of rays*/
	int	lt_infinite;	/* !0 if infinitely distant */
	int	lt_visible;	/* 0 if implicitly modeled or invisible */
	int	lt_exaim;	/* !0 if explicit aim in lt_dir */
	fastf_t lt_obscure;	/* percentage obscuration of light */
	/* Internal fields */
#if RT_MULTISPECTRAL
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
};
#define LIGHT_NULL	((struct light_specific *)0)
#define LIGHT_MAGIC	0xdbddbdb7
#define RT_CK_LIGHT(_p)	BU_CKMAG((_p), LIGHT_MAGIC, "light_specific")

extern struct light_specific	LightHead;

RT_EXTERN(void light_visibility, (struct application *ap,
				  struct shadework *swp, int have) );

RT_EXTERN(void light_obs, (struct application *ap,
				  struct shadework *swp, int have) );

