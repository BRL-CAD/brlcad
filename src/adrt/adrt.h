/*                          A D R T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
 *
 */

#ifndef ADRT_ADRT_H
#define ADRT_ADRT_H

#include "common.h"

#include "adrt_struct.h"

#define	ADRT_PORT		1982
#define	ADRT_USE_COMPRESSION	1
#define ADRT_MAX_WORKSPACE_NUM	64

/* Pixel Format: 0 = unsigned char, 1 =  TFLOAT */
#define	ADRT_PIXEL_FMT		0

/* these are contained in an ADRT_NETOP_WORK message */
#define ADRT_WORK_BASE 0x10
enum
{
    ADRT_WORK_INIT = ADRT_WORK_BASE,	/* 10 */
    ADRT_WORK_STATUS,			/* 11 */
    ADRT_WORK_FRAME_ATTR,		/* 12 */
    ADRT_WORK_FRAME,			/* 13 */
    ADRT_WORK_SHOTLINE,			/* 14 */
    ADRT_WORK_SPALL,			/* 15 */
    ADRT_WORK_SELECT,			/* 16 */
    ADRT_WORK_MINMAX,			/* 17 */
    ADRT_WORK_END
};


#define ADRT_NETOP_BASE 0x20
/* top level messages */
enum
{
    ADRT_NETOP_NOP = ADRT_NETOP_BASE,	/* 20 */
    ADRT_NETOP_INIT,			/* 21 */
    ADRT_NETOP_REQWID,			/* 22 */
    ADRT_NETOP_LOAD,			/* 23 */
    ADRT_NETOP_WORK,			/* 24 */
    ADRT_NETOP_MESG,			/* 25 */
    ADRT_NETOP_QUIT,			/* 26 */
    ADRT_NETOP_SHUTDOWN,		/* 27 */
    ADRT_NETOP_END
};


/* fill in a human readable version of the adrt op (for debugging) */
#define ADRT_MESSAGE_NAME(op) \
    (op >= ADRT_WORK_BASE && op <= ADRT_WORK_END) ? adrt_work_table[op-ADRT_WORK_BASE] : \
    (op >= ADRT_NETOP_BASE && op <= ADRT_NETOP_END) ? adrt_netop_table[op-ADRT_NETOP_BASE] : \
    "Unknown"

/* use the high bit to indicate if mode has changed */
#define ADRT_MESSAGE_MODE_CHANGE(x) (x |= 0x80)
#define ADRT_MESSAGE_MODE_CHANGEP(x) (x & 0x80)
#define ADRT_MESSAGE_MODE(x) (x & ~0x80)

#define ADRT_VER_KEY		0
#define ADRT_VER_DETAIL		"ADRT - Advanced Distributed Ray Tracer"

#define ADRT_MESH_HIT	0x1
#define ADRT_MESH_SELECT	0x2

#define ADRT_NAME_SIZE 256

#define ADRT_LOAD_FORMAT_G 42
#define ADRT_LOAD_FORMAT_REG 43
#define ADRT_LOAD_FORMAT_KDTREE 44

RENDER_EXPORT extern int load_g(struct tie_s *tie, const char *db, int argc, const char **argv, struct adrt_mesh_s **);

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
