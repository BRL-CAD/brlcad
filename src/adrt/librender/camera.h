/*                        C A M E R A . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file librender/camera.h
 *
 */

#ifndef _RENDER_CAMERA_H
#define _RENDER_CAMERA_H

#include "tie.h"
#include "adrt.h"
#include "render.h"

#define	RENDER_CAMERA_DOF_SAMPLES	13
#define RENDER_CAMERA_PERSPECTIVE	0x0
#define RENDER_CAMERA_ORTHOGRAPHIC	0x1
#define	RENDER_CAMERA_BGR		0.00
#define	RENDER_CAMERA_BGG		0.05
#define	RENDER_CAMERA_BGB		0.15

#define RENDER_CAMERA_BIT_DEPTH_24	0
#define RENDER_CAMERA_BIT_DEPTH_128	1


typedef struct render_camera_view_s
{
    vect_t step_x;
    vect_t step_y;
    vect_t pos;
    vect_t top_l;
} render_camera_view_t;


typedef struct render_camera_s
{
    uint8_t type;
    point_t pos;
    vect_t focus;
    fastf_t tilt;
    fastf_t fov;
    fastf_t gridsize;
    fastf_t aspect;
    fastf_t dof;
    uint8_t thread_num;
    uint16_t view_num;
    render_camera_view_t *view_list;
    render_t render;
    uint32_t rm;
    uint16_t w;
    uint16_t h;
} render_camera_t;


typedef struct camera_tile_s
{
    uint16_t orig_x;
    uint16_t orig_y;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t format;
    uint16_t frame;
} camera_tile_t;


typedef struct render_camera_thread_data_s
{
    render_camera_t *camera;
    struct tie_s *tie;
    camera_tile_t *tile;
    void *res_buf;
    unsigned int *scanline;
} render_camera_thread_data_t;


BU_EXPORT extern void render_camera_init(render_camera_t *camera, int threads);
BU_EXPORT extern void render_camera_free(render_camera_t *camera);
BU_EXPORT extern void render_camera_prep(render_camera_t *camera);
BU_EXPORT extern void render_camera_render(render_camera_t *camera, struct tie_s *tie, camera_tile_t *tile, tienet_buffer_t *result);

BU_EXPORT extern int render_shader_init(render_t *, const char *name, const char *buf);
BU_EXPORT extern const char *render_shader_load_plugin(const char *filename);
/* r is passed in so something ... sane(?) can be done if the shader being
 * unloaded is in use. */
BU_EXPORT extern int render_shader_unload_plugin(render_t *r, const char *name);

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
