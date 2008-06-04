/*                          A D R T . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file adrt.h
 *
 * Brief description
 *
 */

#ifndef _ADRT_H
#define _ADRT_H

#define	ADRT_PORT		1982
#define	ADRT_USE_COMPRESSION	1
#define ADRT_MAX_WORKSPACE_NUM	64

/* Pixel Format: 0 = unsigned char, 1 =  tfloat */
#define	ADRT_PIXEL_FMT		0

/* these are contained in an ADRT_NETOP_WORK message */
enum
{
    ADRT_WORK_INIT = 100,
    ADRT_WORK_STATUS,
    ADRT_WORK_FRAME_ATTR,
    ADRT_WORK_FRAME,
    ADRT_WORK_SHOTLINE,
    ADRT_WORK_SPALL,
    ADRT_WORK_SELECT,
    ADRT_WORK_MINMAX
};

/* top level messages */
enum
{
    ADRT_NETOP_NOP = 200,
    ADRT_NETOP_INIT,
    ADRT_NETOP_REQWID,
    ADRT_NETOP_LOAD,
    ADRT_NETOP_WORK,
    ADRT_NETOP_MESG,
    ADRT_NETOP_QUIT,
    ADRT_NETOP_SHUTDOWN
};

#define ADRT_VER_KEY		0
#define ADRT_VER_DETAIL		"ADRT - Advanced Distributed Ray Tracer"

#define ADRT_MESH_HIT	0x1
#define ADRT_MESH_SELECT	0x2

#define ADRT_NAME_SIZE 256

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
