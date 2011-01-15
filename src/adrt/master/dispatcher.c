/*                    D I S P A T C H E R . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file dispatcher.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#include "bio.h"

#include "tie.h"
#include "adrt.h"

#include "tienet_master.h"
#include "dispatcher.h"
#include "camera.h"

#ifdef HAVE_SYS_SYSINFO_H
#  include <sys/sysinfo.h>
#elif defined(HAVE_SYS_SYSCTL_H)
#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#  endif
#  include <sys/sysctl.h>
#endif

uint16_t dispatcher_frame;
tienet_buffer_t dispatcher_mesg;


void
master_dispatcher_init ()
{
    TIENET_BUFFER_INIT(dispatcher_mesg);
    dispatcher_frame = 1;
}


void
master_dispatcher_free ()
{
    TIENET_BUFFER_FREE(dispatcher_mesg);
}


void
master_dispatcher_generate (void *data, int data_len, int image_w, int image_h, int image_format)
{
    int i, n, size;
    camera_tile_t tile;

    size = data_len + sizeof (camera_tile_t);

    TIENET_BUFFER_SIZE(dispatcher_mesg, size);

    tienet_master_begin ();

    /* Copy data payload to front */
    bcopy(data, dispatcher_mesg.data, data_len);

    tile.size_x = image_w / DISPATCHER_TILE_NUM;
    tile.size_y = image_h / DISPATCHER_TILE_NUM;
    tile.format = image_format;
    tile.frame = dispatcher_frame;

    for (i = 0; i < image_h; i += tile.size_y)
    {
	tile.orig_y = i;
	for (n = 0; n < image_w; n += tile.size_x)
	{
	    tile.orig_x = n;
	    TCOPY(camera_tile_t, &tile, 0, dispatcher_mesg.data, data_len);
	    tienet_master_push(dispatcher_mesg.data, size);
	}
    }

    tienet_master_end ();

    dispatcher_frame = (dispatcher_frame + 1) % (1<<14);
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
