/*                        M A S T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file master.h
 *
 * Author -
 *   Justin Shumaker
 *
 */

#ifndef _ISST_MASTER_H
#define _ISST_MASTER_H

#include "tie.h"

extern void isst_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host);

extern TIE_3 isst_master_camera_position;
extern TIE_3 isst_master_shot_position;
extern TIE_3 isst_master_shot_direction;
extern TIE_3 isst_master_in_hit;
extern tfloat isst_master_camera_azimuth;
extern tfloat isst_master_camera_elevation;
extern tfloat isst_master_spall_angle;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
