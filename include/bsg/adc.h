/*                        A D C . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup bsg_adc
 *
 * Angle/Distance Cursor functions.
 * Canonical home; bv/adc.h is a backward-compatibility bridge.
 */
/** @{ */
/** @file bsg/adc.h */

#ifndef BSG_ADC_H
#define BSG_ADC_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

BSG_EXPORT void adc_model_to_adc_view(struct bsg_adc_state *adcs, mat_t model2view, fastf_t amax);
BSG_EXPORT void adc_grid_to_adc_view(struct bsg_adc_state *adcs, mat_t model2view, fastf_t amax);
BSG_EXPORT void adc_view_to_adc_grid(struct bsg_adc_state *adcs, mat_t model2view);
BSG_EXPORT void adc_reset(struct bsg_adc_state *adcs, mat_t view2model, mat_t model2view);

__END_DECLS

#endif  /* BSG_ADC_H */

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
