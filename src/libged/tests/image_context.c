/* T E S T _ M I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "ged.h"
#include "icv.h"

int
main(int argc, const char **argv)
{
    struct ged session = {0};
    struct icv_context *context;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const unsigned char pixels[] = {255, 0, 0};
    icv_image_t *image;
    int status = 0;
    bu_setprogname(argv[0]);
    if (argc != 2)
	return 1;
    ged_init(&session);
    context = ged_image_context(&session);
    if (!context || context != ged_image_context(&session) ||
	!icv_load_plugin(context, argv[1], &messages) ||
	!icv_supports(ged_image_context(&session), "image/vnd.example.codec", ICV_CODEC_READ | ICV_CODEC_WRITE)) {
	status = 1;
    } else {
	image = icv_read_mem_with_context(ged_image_context(&session), pixels,
	    sizeof(pixels), "image/vnd.brlcad.pix", NULL, 1, 1, &messages);
	if (!image)
	    status = 1;
	else
	    icv_destroy(image);
    }
    if (status)
	bu_log("Session image context failed: %s\n", bu_vls_cstr(&messages));
    ged_free(&session);
    bu_vls_free(&messages);
    return status;
}
