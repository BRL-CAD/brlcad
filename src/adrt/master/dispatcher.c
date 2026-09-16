/*                    D I S P A T C H E R . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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
 * Work dispatcher: splits a frame into tiles and pushes each tile as
 * a work unit onto the tienet master work buffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_SYSINFO_H
#  include <sys/sysinfo.h>
#elif defined(HAVE_SYS_SYSCTL_H)

#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
/* sys/sysctl.h may need help with c90-era BSD/XOPEN API */
#    ifndef _U_LONG
#      define u_long unsigned long
#    endif
#    ifndef _U_INT
#      define u_int unsigned int
#    endif
#    ifndef _U_SHORT
#      define u_short unsigned short
#    endif
#    ifndef _U_CHAR
#      define u_char unsigned char
#    endif
#  endif

#  include <sys/sysctl.h>
#endif

#include "bio.h"

#include "bu/malloc.h"
#include "rt/tie.h"
#include "adrt.h"

#include "tienet.h"
#include "tienet_master.h"
#include "dispatcher.h"
#include "camera.h"


uint16_t dispatcher_frame;
tienet_buffer_t dispatcher_mesg;


void
master_dispatcher_init (void)
{
    TIENET_BUFFER_INIT(dispatcher_mesg);
    dispatcher_frame = 1;
}


void
master_dispatcher_free (void)
{
    TIENET_BUFFER_FREE(dispatcher_mesg);
}


void
master_dispatcher_generate (void *data, int data_len, int image_w, int image_h, int image_format)
{
    int tx, ty, base_x, base_y, size;
    camera_tile_t tile;

    size = data_len + sizeof (camera_tile_t);

    TIENET_BUFFER_SIZE(dispatcher_mesg, size);

    tienet_master_begin ();

    /* Copy data payload to front */
    memcpy(dispatcher_mesg.data, data, data_len);

    /*
     * Emit exactly DISPATCHER_TILE_NUM x DISPATCHER_TILE_NUM tiles (which is
     * what master.tile_num expects for the frame-complete count).  The base
     * tile size floors the division; the last row/column absorbs the remainder
     * so the tiles cover the whole image without gaps or writing past its
     * edges when the dimensions are not evenly divisible.
     */
    base_x = image_w / DISPATCHER_TILE_NUM;
    base_y = image_h / DISPATCHER_TILE_NUM;
    tile.format = image_format;
    tile.frame = dispatcher_frame;

    for (ty = 0; ty < DISPATCHER_TILE_NUM; ty++) {
	tile.orig_y = (uint16_t)(ty * base_y);
	tile.size_y = (uint16_t)((ty == DISPATCHER_TILE_NUM - 1) ? (image_h - ty * base_y) : base_y);
	for (tx = 0; tx < DISPATCHER_TILE_NUM; tx++) {
	    tile.orig_x = (uint16_t)(tx * base_x);
	    tile.size_x = (uint16_t)((tx == DISPATCHER_TILE_NUM - 1) ? (image_w - tx * base_x) : base_x);
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
