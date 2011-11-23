/*                         L I G H T . H
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file light.h
 *
 * @brief
 *  Declarations related to light sources
 *
 */

#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "common.h"

#include "bu.h"
#include "bn.h"
#include "raytrace.h"

/* FIXME: replicated from optical.h */
#ifndef OPTICAL_EXPORT
#  if defined(OPTICAL_DLL_EXPORTS) && defined(OPTICAL_DLL_IMPORTS)
#    error "Only OPTICAL_DLL_EXPORTS or OPTICAL_DLL_IMPORTS can be defined, not both."
#  elif defined(OPTICAL_DLL_EXPORTS)
#    define OPTICAL_EXPORT __declspec(dllexport)
#  elif defined(OPTICAL_DLL_IMPORTS)
#    define OPTICAL_EXPORT __declspec(dllimport)
#  else
#    define OPTICAL_EXPORT
#  endif
#endif

__BEGIN_DECLS

struct light_pt {
    point_t	lp_pt;
    vect_t	lp_norm;
};
#define LPT_MAGIC 0x327649
#define SOME_LIGHT_SAMPLES 128

struct light_specific {
    struct bu_list	l;	/**< @brief doubly linked list */
    /* User-specified fields */
    vect_t	lt_target;	/**< @brief explicit coordinate aim point */
    fastf_t	lt_intensity;	/**< @brief Intensity Lumens (cd*sr): total output */
    fastf_t	lt_angle;	/**< @brief beam dispersion angle (degrees) 0..180 */
    fastf_t	lt_fraction;	/**< @brief fraction of total light */
    int	lt_shadows;	/**< @brief !0 if this light casts shadows, # of rays*/
    int	lt_infinite;	/**< @brief !0 if infinitely distant */
    int	lt_visible;	/**< @brief 0 if implicitly modeled or invisible */
    int	lt_invisible;	/**< @brief 0 if implicitly modeled or invisible */
    int	lt_exaim;	/**< @brief !0 if explicit aim in lt_target */
    fastf_t lt_obscure;	/**< @brief percentage obscuration of light */
    /* Internal fields */
#ifdef RT_MULTISPECTRAL
    struct bn_tabdata *lt_spectrum;	/**< @brief Units?  mw*sr ? */
#else
    vect_t	lt_color;	/**< @brief RGB, as 0..1 */
#endif
    fastf_t	lt_radius;	/**< @brief approximate radius of spherical light */
    fastf_t	lt_cosangle;	/**< @brief cos of lt_angle */
    vect_t	lt_pos;		/**< @brief location in space of light */
    vect_t	lt_vec;		/**< @brief Unit vector from origin to light */
    vect_t	lt_aim;		/**< @brief Unit vector - light beam direction */
    char	*lt_name;	/**< @brief identifying string */
    struct	region *lt_rp;	/**< @brief our region of origin */
    int	lt_pt_count;	/**< @brief count of how many lt_sample_pts have been set */
    struct light_pt *lt_sample_pts; /**< @brief dynamically allocated list of light sample points */
    fastf_t lt_parse_pt[6];
};
#define LIGHT_NULL	((struct light_specific *)0)
#define RT_CK_LIGHT(_p)	BU_CKMAG((_p), LIGHT_MAGIC, "light_specific")

/* defined in sh_light.c */
OPTICAL_EXPORT extern struct light_specific	LightHead;

OPTICAL_EXPORT extern void light_cleanup(void);
OPTICAL_EXPORT extern void light_maker(int num, mat_t v2m);
OPTICAL_EXPORT extern int light_init(struct application *ap);
OPTICAL_EXPORT extern void light_obs(struct application *ap, struct shadework *swp, int have);

__END_DECLS

#endif /* __LIGHT_H__ */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
