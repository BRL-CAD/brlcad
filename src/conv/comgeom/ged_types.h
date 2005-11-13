/*                     G E D _ T Y P E S . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

#define	X	0
#define	Y	1
#define Z	2
#define H	3

#define ALPHA	0
#define BETA	1
#define GAMMA	2

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
