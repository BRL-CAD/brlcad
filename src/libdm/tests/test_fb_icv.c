/*                   T E S T _ F B _ I C V . C
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

#include <stdio.h>
#include "bu/app.h"
#include "dm.h"
#include "icv.h"

int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);
    struct fb *framebuffer = fb_open("/dev/mem", 2, 1);
    if (!framebuffer)
	return 1;
    for (size_t channels = 1; channels <= 4; ++channels) {
	int gray = channels <= 2;
	icv_image_t *image = icv_create_with_channels(2, 1,
	    gray ? ICV_COLOR_SPACE_GRAY : ICV_COLOR_SPACE_RGB, channels);
	if (!image)
	    return 1;
	for (size_t channel = 0; channel < channels; ++channel) {
	    image->data[channel] = gray ? 0.25 : channel == 0 ? 1.0 : channel == 1 ? 0.5 : 0.0;
	    image->data[channels + channel] = 1.0;
	}
	if (image->alpha_channel)
	    image->data[channels - 1] = 0.5;
	if (fb_read_icv(framebuffer, image, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, NULL))
	    return 1;
	unsigned char pixels[6];
	if (fb_read(framebuffer, 0, 0, pixels, 2) != 2)
	    return 1;
	for (size_t channel = 0; channel < 3; ++channel) {
	    double alpha = image->alpha_channel ? 0.5 : 1.0;
	    int expected = (int)(255.0 * alpha * image->data[gray ? 0 : channel]);
	    int difference = (int)pixels[channel] - expected;
	    if (difference < -1 || difference > 1 || pixels[3 + channel] != 255) {
		fprintf(stderr, "Framebuffer conversion failed for %zu channels\n", channels);
		return 1;
	    }
	}
	if (image->channels != channels || (image->alpha_channel && !EQUAL(image->data[channels - 1], 0.5)))
	    return 1;
	icv_destroy(image);
    }
    return fb_close(framebuffer) ? 1 : 0;
}
