/*                     C A M E R A . H
 *
 * @file camera.h
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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
 *
 *  Comments -
 *      Utilities Library - Camera Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _UTIL_CAMERA_H
#define _UTIL_CAMERA_H


#include "adrt_common.h"
#include "tienet.h"
#include "cdb.h"


#define	UTIL_CAMERA_DOF_SAMPLES	13


typedef struct util_camera_view_s {
  TIE_3 step_x;
  TIE_3 step_y;
  TIE_3 pos;
  TIE_3 top_l;
} util_camera_view_t;


typedef struct util_camera_s {
  TIE_3 pos;
  TIE_3 focus;
  tfloat tilt;
  tfloat fov;
  tfloat dof;
  int thread_num;
  int view_num;
  util_camera_view_t *view_list;
} util_camera_t;


typedef struct util_camera_thread_data_s {
  common_db_t *db;
  util_camera_t *camera;
  tie_t *tie;
  common_work_t work;
  void *res_buf;
  unsigned int *scanline;
  pthread_mutex_t mut;
} util_camera_thread_data_t;


void util_camera_init(util_camera_t *camera, int threads);
void util_camera_free(util_camera_t *camera);
void util_camera_prep(util_camera_t *camera, common_db_t *db);
void util_camera_render(util_camera_t *camera, common_db_t *db, tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len);

#endif
