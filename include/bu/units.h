/*                         U N I T S . H
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

/** @file units.h
 *
 */
#ifndef BU_UNITS_H
#define BU_UNITS_H

#include "common.h"

#include "bu/defines.h"
#include "bu/parse.h"

/** @addtogroup units */
/** @{ */


/** @file libbu/units.c
 *
 * Module of libbu to handle units conversion between strings and mm.
 *
 */

/**
 * Given a string representation of a unit of distance (e.g., "feet"),
 * return the multiplier which will convert that unit into the default
 * unit for the dimension (millimeters for length, mm^3 for volume,
 * and grams for mass.)
 *
 * Returns 0.0 on error and >0.0 on success
 */
BU_EXPORT extern double bu_units_conversion(const char *str);

/**
 * Given a conversion factor to mm, search the table to find what unit
 * this represents.
 *
 * To accommodate floating point fuzz, a "near miss" is allowed.  The
 * algorithm depends on the table being sorted small-to-large.
 *
 * Returns -
 * char* units string
 * NULL	No known unit matches this conversion factor.
 */
BU_EXPORT extern const char *bu_units_string(const double mm);
BU_EXPORT extern struct bu_vls *bu_units_strings_vls(void);

/**
 * Given a conversion factor to mm, search the table to find the
 * closest matching unit.
 *
 * Returns -
 * char* units string
 * NULL	Invalid conversion factor (non-positive)
 */
BU_EXPORT extern const char *bu_nearest_units_string(const double mm);

/**
 * Given a string of the form "25cm" or "5.2ft" returns the
 * corresponding distance in mm.
 *
 * Returns -
 * -1 on error
 * >0 on success
 */
BU_EXPORT extern double bu_mm_value(const char *s);

/**
 * Used primarily as a hooked function for bu_structparse tables to
 * allow input of floating point values in other units.
 */
BU_EXPORT extern void bu_mm_cvt(const struct bu_structparse *sdp,
				const char *name,
				void *base,
				const char *value);



/** @} */

#endif  /* BU_UNITS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
