/*                  F B S E R V _ W I N 3 2 . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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
/** @file mged/fbserv_win32.c
 *
 * This code was developed by modifying the stand-alone version of
 * fbserv to work within MGED.
 *
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "../libfb/pkgtypes.h"
#include "./fbserv.h"


/******** Here's where the hooks lead *********/

void
rfbopen(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbclose(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbfree(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbclear(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbread(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbwrite(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * R F B R E A D R E C T
 */
void
rfbreadrect(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * R F B W R I T E R E C T
 */
void
rfbwriterect(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * R F B B W R E A D R E C T
 */
void
rfbbwreadrect(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * R F B B W W R I T E R E C T
 */
void
rfbbwwriterect(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbcursor(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbgetcursor(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbsetcursor(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*OLD*/
void
rfbscursor(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*OLD*/
void
rfbwindow(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*OLD*/
void
rfbzoom(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbview(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbgetview(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbrmap(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * R F B W M A P
 *
 * Accept a color map sent by the client, and write it to the framebuffer.
 * Network format is to send each entry as a network (IBM) order 2-byte
 * short, 256 red shorts, followed by 256 green and 256 blue, for a total
 * of 3*256*2 bytes.
 */
void
rfbwmap(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbflush(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


void
rfbpoll(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * At one time at least we couldn't send a zero length PKG
 * message back and forth, so we receive a dummy long here.
 */
void
rfbhelp(pcp, buf)
    struct pkg_conn *pcp;
    char *buf;
{
    return;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
