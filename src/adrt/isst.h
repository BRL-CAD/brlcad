/*
 * ISST
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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
 */
/** @file isst.h
 *
 *
 */


struct isst_s {
    struct tie_s *tie;
    struct render_camera_s camera;
    struct camera_tile_s tile;
    struct adrt_mesh_s *meshes;
    tienet_buffer_t buffer_image;
    int ogl, sflags, w, h, gs, ui;
    double dt, fps, uic;
    double rotx, roty;
    int texid;
    void *texdata;
    vect_t camera_pos_init;
    vect_t camera_focus_init;
    int64_t t1;
    int64_t t2;
    int dirty;
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
