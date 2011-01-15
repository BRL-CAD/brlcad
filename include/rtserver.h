/*                      R T S E R V E R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup rtserver */
/** @{ */
/** @file rtserver.h
 *
 * @brief
 *	header file for the rtserver
 *
 */

/* Attempt to handle different sizes of the TCL ClientData on differing acjitectures */
#if SIZEOF_VOID_P == SIZEOF_INT
typedef int CLIENTDATA_INT;
#elif SIZEOF_VOID_P == SIZEOF_LONG
typedef long CLIENTDATA_INT;
#elif SIZEOF_VOID_P == SIZEOF_LONG_LONG
typedef long long CLIENTDATA_INT;
#else
#define CLIENTDATA_INT "ERROR: could not determine size of void*";
#endif


extern void getApplication(struct application **ap);
extern void freeApplication(struct application *ap);
extern void get_model_extents( int sessionid, point_t min, point_t max );
extern void rts_set_verbosity( int v );
extern int rts_hit(struct application *ap, struct partition *partHeadp, struct seg *segs);
extern int rts_miss(struct application *ap);
extern void getApplication(struct application **ap);
extern void freeApplication(struct application *ap);
extern void rts_set_verbosity( int v );
extern void rts_close_session( int sessionid );
extern int rts_open_session();
extern int rts_load_geometry( char *filename, int num_trees, char **objects );
extern void printHits(struct bu_vlb *vlb);
extern void get_model_extents( int sessionid, point_t min, point_t max );
extern void rts_shootray( struct application *ap );

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
