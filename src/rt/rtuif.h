/*                         R T U I F . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file rt/rtuif.h
 *
 * Private API header for developing applications that utilize rt's
 * common Ray Trace User Interface Framework (RTUIF).
 *
 * While not (yet) public API, it's a useable inteface for rapidly
 * developing applications involving grids of rays.  It provides the
 * same command-line inteface as 'rt' and is the foundation for
 * numerous BRL-CAD ray tracing based applications such as rtweight,
 * rtarea, and others.  See viewdummy.c for an example template.
 *
 */

#ifndef __RTUIF_H__
#define __RTUIF_H__

#include "common.h"

#include <stdio.h>

#include "raytrace.h"
#include "optical.h"


/**
 * Called by main() at the start of a run.  Should set up rayhit() and
 * raymiss() in the application structure.  The rayhit() callback
 * function is called via the a_hit linkage from rt_shootray() when a
 * ray hits.  The raymiss() callback function is called via the a_miss
 * linkage from rt_shootray() when a ray misses.
 *
 * Returns 1 if framebuffer should be opened, else 0.
 */
extern int view_init(struct application *ap, char *file, char *obj, int minus_o, int minus_F);

/**
 * Called by do_prep(), just before rt_prep() is called, in
 * “do.c”. This allows the lighting model to get set up for this
 * frame, e.g., generate lights, associate materials routines, etc.
 */
extern void view_setup(struct rt_i *rtip);

/**
 * Called at the beginning of a frame. Called by do_frame() just
 * before raytracing starts.
 */
extern void view_2init(struct application *ap, char *framename);

/**
 * Called by worker() after the end of proccessing for each pixel.
 */
extern void view_pixel(struct application *ap);

/**
 * Called after the end of each ray trace scanline.
 */
extern void view_eol(struct application *ap);

/**
 * Called in do_frame() at the end of a frame, just after raytracing
 * completes.
 */
extern void view_end(struct application *ap);

/**
 * Called before rt_clean() in do.c, for releasing resources allocated
 * and opened during ray tracing.
 */
extern void view_cleanup(struct rt_i *rtip);

#endif  /* __RTUIF_H__ */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
