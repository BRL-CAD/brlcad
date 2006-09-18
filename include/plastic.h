/*                          P L A S T I C . H
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
/** @addtogroup liboptical */
/*@{*/
/** @file plastic.h
 *
 */
#ifndef plastic_h
#define plastic_h

#define PL_MAGIC        0xbeef00d
#define PL_NULL ((struct phong_specific *)0)
#define PL_O(m) offsetof(struct phong_specific, m)

/* Local information */
struct phong_specific {
	int	magic;
	int	shine;
	double	wgt_specular;
	double	wgt_diffuse;
	double	transmit;       /**< @brief Moss "transparency" */
	double	reflect;        /**< @brief Moss "transmission" */
	double	refrac_index;
	double	extinction;
	double	emission[3];
	struct	mfuncs *mfp;
};

extern struct bu_structparse phong_parse[];
#endif
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

