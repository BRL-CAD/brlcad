/*                        T I E N E T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file tienet.h
 *
 *
 */

#ifndef _TIENET_H
#define _TIENET_H

#include "adrt.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SELECT_H
# include <sys/select.h>
#endif

#define TIENET_OP(name,cmd)  \
static int tienet_##name(int socket, void* data, size_t size) \
{ \
    fd_set	 set; \
    unsigned int ind = 0; \
    int		 r; \
\
    FD_ZERO(&set); \
    FD_SET(socket, &set); \
\
    do { \
	select(socket+1, NULL, &set, NULL, NULL); \
	r = cmd(socket, &((char*)data)[ind], size-ind); \
	ind += r; \
	if (r <= 0) return 1;	/* Error, socket is probably dead */ \
    } while (ind < size); \
\
    return 0; \
}

TIENET_OP(send,write)
TIENET_OP(recv,read)

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
