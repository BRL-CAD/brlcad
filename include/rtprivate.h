/*                     R T P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup rt */
/*@{*/
/** @file rtprivate.h
 *
 *@brief
 *	Things to support the "rt" program and the link to liboptical
 *
 */

#ifndef __RTPRIVATE_H__
#define __RTPRIVATE_H__

#include "common.h"

#include <stdio.h>

#include "raytrace.h"
#include "optical.h"

/* default parseable title length  (v4 was 132) */
#define RT_BUFSIZE 1024

/* do.c */
extern void do_ae(double azim, double elev);
extern int do_frame( int framenumber );
extern void def_tree( register struct rt_i *rtip );
extern void do_prep( struct rt_i *rtip );
extern int old_way( FILE *fp );

/* opt.c */
extern int get_args( int argc, char **argv );
extern int		query_x;
extern int		query_y;
extern int		Query_one_pixel;
extern int		query_rdebug;
extern int		query_debug;
extern int		benchmark;

/* view.c */
#if 0
extern void view_eol(register struct application *ap);
extern void do_run( int a, int b );
extern void view_2init( register struct application *ap, char	*framename);
extern void view_setup(struct rt_i	*rtip);
extern void view_pixel(register struct application *ap);
extern void view_cleanup(struct rt_i	*rtip);
extern int view_end(struct application *ap);
extern int view_init(register struct application *ap,
		     char *file,
		     char *obj,
		     int minus_o);
#else
extern void view_eol();
extern void do_run();
extern void view_2init();
extern void view_setup();
extern void view_pixel();
extern void view_cleanup();
extern void view_end();
extern int view_init();

#endif

#endif  /* __RTPRIVATE_H__ */
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

