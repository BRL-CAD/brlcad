/*                          A D R T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
#define ADRT_WORK_BASE 10
enum
{
    ADRT_WORK_INIT = ADRT_WORK_BASE,
    ADRT_WORK_STATUS,
    ADRT_WORK_FRAME_ATTR,
    ADRT_WORK_FRAME,
    ADRT_WORK_SHOTLINE,
    ADRT_WORK_SPALL,
    ADRT_WORK_SELECT,
    ADRT_WORK_MINMAX,
    ADRT_WORK_END
};
static char *adrt_work_table[40] = {
    "ADRT_WORK_INIT",
    "ADRT_WORK_STATUS",
    "ADRT_WORK_FRAME_ATTR",
    "ADRT_WORK_FRAME",
    "ADRT_WORK_SHOTLINE",
    "ADRT_WORK_SPALL",
    "ADRT_WORK_SELECT",
    "ADRT_WORK_MINMAX",
    NULL};



#define ADRT_NETOP_BASE 50
/* top level messages */
enum
{
    ADRT_NETOP_NOP = ADRT_NETOP_BASE,
    ADRT_NETOP_INIT,
    ADRT_NETOP_REQWID,
    ADRT_NETOP_LOAD,
    ADRT_NETOP_WORK,
    ADRT_NETOP_MESG,
    ADRT_NETOP_QUIT,
    ADRT_NETOP_SHUTDOWN,
    ADRT_NETOP_END
};
static char *adrt_netop_table[40] = {
    "ADRT_NETOP_NOP",
    "ADRT_NETOP_INIT",
    "ADRT_NETOP_REQWID",
    "ADRT_NETOP_LOAD",
    "ADRT_NETOP_WORK",
    "ADRT_NETOP_MESG",
    "ADRT_NETOP_QUIT",
    "ADRT_NETOP_SHUTDOWN",
    NULL};

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

#define ADRT_LOAD_FORMAT_MYSQL_F 0x64
#define ADRT_LOAD_FORMAT_G 42
#define ADRT_LOAD_FORMAT_REG 43
#define ADRT_LOAD_FORMAT_KDTREE 44

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
