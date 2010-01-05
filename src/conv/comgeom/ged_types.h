/*                     G E D _ T Y P E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file ged_types.h
 * type definition for new "C" declaration:  "homog_t".
 *
 * This is to make declairing Homogeneous Transform matricies easier.
 */
typedef	float	mat_t[4*4];
typedef	float	*matp_t;
typedef	float	vect_t[4];
typedef	float	*vectp_t;

enum axis {
    X = 0,
    Y = 1,
    Z = 2,
    H = 3
};

#define ALPHA	0
#define BETA	1
#define GAMMA	2

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
