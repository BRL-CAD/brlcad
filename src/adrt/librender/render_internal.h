/*               R E N D E R _ I N T E R N A L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2010 United States Government as represented by
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
 */

#ifndef _RENDER_INTERNAL_H
#define _RENDER_INTERNAL_H

#include "tie.h"

#define RENDER_METHOD_COMPONENT	0x01
#define RENDER_METHOD_CUT	0x02
#define RENDER_METHOD_DEPTH	0x03
#define RENDER_METHOD_FLAT	0x04
#define RENDER_METHOD_FLOS	0x05
#define RENDER_METHOD_GRID	0x06
#define RENDER_METHOD_NORMAL	0x07
#define RENDER_METHOD_PHONG	0x08
#define RENDER_METHOD_PATH	0x09
#define RENDER_METHOD_SPALL	0x0A
#define RENDER_METHOD_SURFEL	0x0B

#define RENDER_METHOD_PLANE	RENDER_METHOD_CUT


#define RENDER_MAX_DEPTH	24


#define RENDER_SHADER(name) \
	BU_EXPORT BU_EXTERN(int render_##name##_init, (render_t *, char *));

struct render_s;
typedef void render_work_t(struct render_s *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);
typedef void render_free_t(struct render_s *render);

typedef struct render_s {
    char name[256];
    tie_t *tie;
    render_work_t *work;
    render_free_t *free;
    void *data;
    struct render_s *next;
    char *shader;
} render_t;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
