/*                        A D C . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @addtogroup ged_adc
 *
 * Geometry EDiting Library Angle Distance Cursor Functions.
 *
 */
/** @{ */
/** @file ged/view/adc.h */

#ifndef GED_VIEW_ADC_H
#define GED_VIEW_ADC_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

/* defined in adc.c */
GED_EXPORT extern void ged_calc_adc_pos(struct bv *gvp);
GED_EXPORT extern void ged_calc_adc_a1(struct bv *gvp);
GED_EXPORT extern void ged_calc_adc_a2(struct bv *gvp);
GED_EXPORT extern void ged_calc_adc_dst(struct bv *gvp);
/**
 * Angle distance cursor.
 */
GED_EXPORT extern int ged_adc(struct ged *gedp, int argc, const char *argv[]);

__END_DECLS

#endif /* GED_VIEW_ADC_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
