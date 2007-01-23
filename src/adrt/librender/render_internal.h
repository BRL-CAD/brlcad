/*               R E N D E R _ I N T E R N A L . H
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
/** @file render_internal.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RENDER_INTERNAL_H
#define _RENDER_INTERNAL_H

#include "tie.h"

#define RENDER_METHOD_FLAT	0
#define RENDER_METHOD_GRID	1
#define RENDER_METHOD_NORMAL	2
#define RENDER_METHOD_PHONG	3
#define RENDER_METHOD_PATH	4
#define RENDER_METHOD_PLANE	5
#define RENDER_METHOD_COMPONENT	6
#define RENDER_METHOD_SPALL	7
#define RENDER_METHOD_DEPTH	8


#define RENDER_MAX_DEPTH	24


struct render_s;
typedef void render_work_t(struct render_s *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);
typedef void render_free_t(struct render_s *render);

typedef struct render_s {
  render_work_t *work;
  render_free_t *free;
  void *data;
} render_t;

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
