/*                          S H A D E W O R K . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup liboptical */
/** @{ */
/** @file shadework.h
 *
 */

#ifndef OPTICAL_SHADEWORK_H
#define OPTICAL_SHADEWORK_H

#include "common.h"
#include "bn/tabdata.h"
#include "rt/hit.h"
#include "rt/seg.h"

/* for light_specific, cyclic dependency */
/* # include "optical/light.h" */

#define SW_NLIGHTS	16		/* Max # of light sources */

struct shadework {
    fastf_t                sw_transmit;                /**< @brief 0.0 -> 1.0 */
    fastf_t                sw_reflect;                 /**< @brief 0.0 -> 1.0 */
    fastf_t                sw_extinction;              /**< @brief extinction coeff, mm^-1 */
    fastf_t                sw_refrac_index;            /**< @brief refactive index */
    fastf_t                sw_temperature;             /**< @brief color temperature */
    fastf_t                sw_color[3];                /**< @brief shaded color */
    fastf_t                sw_basecolor[3];            /**< @brief base color */
    struct hit             sw_hit;                     /**< @brief ray hit (dist, point, normal) */
    struct uvcoord         sw_uv;
    fastf_t                sw_intensity[3*SW_NLIGHTS]; /**< @brief light intensities */
    fastf_t                sw_tolight[3*SW_NLIGHTS];   /**< @brief light directions */
    struct light_specific *sw_visible[SW_NLIGHTS];     /**< @brief visibility flags/ptrs */
    fastf_t                sw_lightfract[SW_NLIGHTS];  /**< @brief % light visible */
    int                    sw_xmitonly;                /**< @brief flag: need sw_transmit only */
    int                    sw_inputs;                  /**< @brief fields from mf_inputs actually filled */
    int                    sw_frame;                   /**< @brief # of current frame */
    struct seg *           sw_segs;                    /**< @brief segs which made partition */
};

__BEGIN_DECLS

extern void pr_shadework(const char *str, const struct shadework *swp);

__END_DECLS

#endif /* OPTICAL_SHADEWORK_H */

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
