/*                        F B S E R V . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file mged/fbserv.h
 * Package Handlers.
 */
#ifndef __FBSERV_H__
#define __FBSERV_H__

#include "fbmsg.h"

void rfbopen(struct pkg_conn *pcp, char *buf), rfbclose(struct pkg_conn *pcp, char *buf), rfbclear(struct pkg_conn *pcp, char *buf), rfbread(struct pkg_conn *pcp, char *buf), rfbwrite(struct pkg_conn *pcp, char *buf);
void rfbcursor(struct pkg_conn *pcp, char *buf), rfbgetcursor(struct pkg_conn *pcp, char *buf);
void rfbrmap(struct pkg_conn *pcp, char *buf), rfbwmap(struct pkg_conn *pcp, char *buf);
void rfbhelp(struct pkg_conn *pcp, char *buf);
void rfbreadrect(struct pkg_conn *pcp, char *buf), rfbwriterect(struct pkg_conn *pcp, char *buf);
void rfbbwreadrect(struct pkg_conn *pcp, char *buf), rfbbwwriterect(struct pkg_conn *pcp, char *buf);
void rfbpoll(struct pkg_conn *pcp, char *buf), rfbflush(struct pkg_conn *pcp, char *buf), rfbfree(struct pkg_conn *pcp, char *buf);
void rfbview(struct pkg_conn *pcp, char *buf), rfbgetview(struct pkg_conn *pcp, char *buf);
void rfbsetcursor(struct pkg_conn *pcp, char *buf);
void rfbunknown(struct pkg_conn *pcp, char *buf);
/* Old Routines */
void rfbscursor(struct pkg_conn *pcp, char *buf), rfbwindow(struct pkg_conn *pcp, char *buf), rfbzoom(struct pkg_conn *pcp, char *buf);

static struct pkg_switch pkg_switch[] = {
    { MSG_FBOPEN,			rfbopen,	"Open Framebuffer", NULL },
    { MSG_FBCLOSE,			rfbclose,	"Close Framebuffer", NULL },
    { MSG_FBCLEAR,			rfbclear,	"Clear Framebuffer", NULL },
    { MSG_FBREAD,			rfbread,	"Read Pixels", NULL },
    { MSG_FBWRITE,			rfbwrite,	"Write Pixels", NULL },
    { MSG_FBWRITE + MSG_NORETURN,	rfbwrite,	"Asynch write", NULL },
    { MSG_FBCURSOR,			rfbcursor,	"Cursor", NULL },
    { MSG_FBGETCURSOR,			rfbgetcursor,	"Get Cursor", NULL },	   /*NEW*/
    { MSG_FBSCURSOR,			rfbscursor,	"Screen Cursor", NULL }, /*OLD*/
    { MSG_FBWINDOW,			rfbwindow,	"Window", NULL },	   /*OLD*/
    { MSG_FBZOOM,			rfbzoom,	"Zoom", NULL },	   /*OLD*/
    { MSG_FBVIEW,			rfbview,	"View", NULL },	   /*NEW*/
    { MSG_FBGETVIEW,			rfbgetview,	"Get View", NULL },	   /*NEW*/
    { MSG_FBRMAP,			rfbrmap,	"R Map", NULL },
    { MSG_FBWMAP,			rfbwmap,	"W Map", NULL },
    { MSG_FBHELP,			rfbhelp,	"Help Request", NULL },
    { MSG_ERROR,			rfbunknown,	"Error Message", NULL },
    { MSG_CLOSE,			rfbunknown,	"Close Connection", NULL },
    { MSG_FBREADRECT,			rfbreadrect,	"Read Rectangle", NULL },
    { MSG_FBWRITERECT,			rfbwriterect,	"Write Rectangle", NULL },
    { MSG_FBWRITERECT + MSG_NORETURN,	rfbwriterect,	"Write Rectangle", NULL },
    { MSG_FBBWREADRECT, 		rfbbwreadrect,	"Read BW Rectangle", NULL },
    { MSG_FBBWWRITERECT,		rfbbwwriterect,	"Write BW Rectangle", NULL },
    { MSG_FBBWWRITERECT + MSG_NORETURN,	rfbbwwriterect,	"Write BW Rectangle", NULL },
    { MSG_FBFLUSH,			rfbflush,	"Flush Output", NULL },
    { MSG_FBFLUSH + MSG_NORETURN, 	rfbflush,	"Flush Output", NULL },
    { MSG_FBFREE,			rfbfree,	"Free Resources", NULL },
    { MSG_FBPOLL,			rfbpoll,	"Handle Events", NULL },
    { MSG_FBSETCURSOR,			rfbsetcursor,	"Set Cursor Shape", NULL },
    { MSG_FBSETCURSOR + MSG_NORETURN,	rfbsetcursor,	"Set Cursor Shape", NULL },
    { 0,				NULL,		NULL, NULL }
};


#endif  /* __FBSERV_H__ */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
