/*                         U N I T S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef BU_UNITS_H
#define BU_UNITS_H

#include "common.h"

#include "bu/defines.h"
#include "bu/parse.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_units
 * @brief
 * Module of libbu to handle units conversion between strings and mm.
 */
/** @{ */
/** @file bu/units.h */

/**
 * Convert the provided string into a units conversion factor.
 *
 * Given a string for a unit of length (e.g., "feet", "yd"), volume
 * (e.g., "cm^3", "cu yards"), or mass (e.g., "kg", "grain", or "oz")
 * return the multiplier (aka conversion factor) that converts the
 * unit into the default (millimeters for length, mm^3 for volume, and
 * grams for mass.) Values may be optionally specified with the unit
 * (e.g., "5ft") to get the conversion factor for a particular
 * quantity.
 *
 * Returns 0.0 on error and >0.0 on success
 */
BU_EXPORT extern double bu_units_conversion(const char *str);


/**
 * Given a conversion factor to mm, search the table to find what unit
 * this represents.
 *
 * To accommodate floating point fuzz, a "near miss" is allowed.
 *
 * Returns -
 * char* units string
 * NULL	No known unit matches this conversion factor.
 */
BU_EXPORT extern const char *bu_units_string(const double mm);


/** undocumented */
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
				const char *value,
				void *data);


/* Values for bu_humanize_number's flags parameter. */
#define	BU_HN_DECIMAL		0x01
#define	BU_HN_NOSPACE		0x02
#define	BU_HN_B			0x04
#define	BU_HN_DIVISOR_1000	0x08
#define	BU_HN_IEC_PREFIXES	0x10

/* Values for bu_humanize_number's scale parameter. */
#define	BU_HN_GETSCALE		0x10
#define	BU_HN_AUTOSCALE		0x20


/**
 * Convert digital sizes to human readable form.  Based off the
 * BSD function humanize_number(3).
 * Upon success, the humanize_number function returns the number of characters
 * that would have been stored in buf (excluding the terminating NUL) if buf
 * was large enough, or -1 upon failure.  Even upon failure, the contents of
 * buf may be modified.  If BU_HN_GETSCALE is specified, the prefix index
 * number will be returned instead.
 */
BU_EXPORT extern int bu_humanize_number(char *buf, size_t len, int64_t quotient, const char *suffix, size_t scale, int flags);


/*
 * Converts the number given in 'str', which may be given in a humanized
 * form (as described in BSD's humanize_number(3), but with some limitations),
 * to an int64_t without units.
 * In case of success, 0 is returned and *size holds the value.
 * Otherwise, -1 is returned and *size is untouched.
 */
BU_EXPORT extern int bu_dehumanize_number(const char *str, int64_t *size);


__END_DECLS

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
