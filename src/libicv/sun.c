/*                           S U N . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file sun.c
 *
 * Sun rasterfile writing support.
 */

#include "common.h"

#include <stdint.h>
#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "icv_private.h"

#define SUN_MAP_SIZE 256
#define SUN_CUBE_LEVELS 6
#define SUN_CUBE_SIZE (SUN_CUBE_LEVELS * SUN_CUBE_LEVELS * SUN_CUBE_LEVELS)
#define SUN_EXTRA_LEVELS 10
#define SUN_RASTER_MAGIC 0x59a66a95
#define SUN_RASTER_DEPTH 8
#define SUN_RASTER_TYPE_STANDARD 1
#define SUN_RASTER_MAPTYPE_EQUAL_RGB 1
#define SUN_RASTER_HEADER_WORDS 8
#define SUN_RASTER_HEADER_SIZE (SUN_RASTER_HEADER_WORDS * sizeof(uint32_t))
#define SUN_DITHER_CUBE_MAGNITUDE 51.0
#define SUN_DITHER_PRIMARY_MAGNITUDE 8.0

static const unsigned char sun_extra_values[SUN_EXTRA_LEVELS] = {
    17, 34, 68, 85, 119, 136, 170, 187, 221, 238
};
static const unsigned char sun_red_indices[16] = {
    0, 216, 217, 1, 218, 219, 2, 220, 221, 3, 222, 223, 4, 224, 225, 5
};
static const unsigned char sun_green_indices[16] = {
    0, 226, 227, 6, 228, 229, 12, 230, 231, 18, 232, 233, 24, 234, 235, 30
};
static const unsigned char sun_blue_indices[16] = {
    0, 236, 237, 36, 238, 239, 72, 240, 241, 108, 242, 243, 144, 244, 245, 180
};
static const unsigned char sun_gray_indices[16] = {
    0, 246, 247, 43, 248, 249, 86, 250, 251, 129, 252, 253, 172, 254, 255, 215
};
static const double sun_dither_noise[] = {
    0.0, 0.1, -0.4, 0.2, -0.3, 0.3, -0.2, 0.4, -0.1, 0.5
};

struct sun_dither_state {
    size_t noise_index;
};

static void
sun_store_be32(unsigned char *out, uint32_t value)
{
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

static void
sun_colormap(unsigned char *map)
{
    size_t index;

    memset(map, 0, 3 * SUN_MAP_SIZE);
    for (index = 0; index < SUN_CUBE_SIZE; index++) {
	map[index] = (unsigned char)((index % SUN_CUBE_LEVELS) * 51);
	map[SUN_MAP_SIZE + index] = (unsigned char)(((index / SUN_CUBE_LEVELS) % SUN_CUBE_LEVELS) * 51);
	map[2 * SUN_MAP_SIZE + index] = (unsigned char)((index / (SUN_CUBE_LEVELS * SUN_CUBE_LEVELS)) * 51);
    }
    for (index = 0; index < SUN_EXTRA_LEVELS; index++) {
	map[SUN_CUBE_SIZE + index] = sun_extra_values[index];
	map[SUN_MAP_SIZE + SUN_CUBE_SIZE + SUN_EXTRA_LEVELS + index] = sun_extra_values[index];
	map[2 * SUN_MAP_SIZE + SUN_CUBE_SIZE + 2 * SUN_EXTRA_LEVELS + index] = sun_extra_values[index];
	map[SUN_CUBE_SIZE + 3 * SUN_EXTRA_LEVELS + index] = sun_extra_values[index];
	map[SUN_MAP_SIZE + SUN_CUBE_SIZE + 3 * SUN_EXTRA_LEVELS + index] = sun_extra_values[index];
	map[2 * SUN_MAP_SIZE + SUN_CUBE_SIZE + 3 * SUN_EXTRA_LEVELS + index] = sun_extra_values[index];
    }
}

static double
sun_noise(struct sun_dither_state *state)
{
    size_t count = sizeof(sun_dither_noise) / sizeof(sun_dither_noise[0]);

    if (state->noise_index < count)
	return sun_dither_noise[state->noise_index++];
    state->noise_index = 0;
    return sun_dither_noise[0];
}

static int
sun_clamp_dither(unsigned char value, double noise, double magnitude)
{
    double result = value + noise * magnitude;

    if (result > 255.0)
	return 255;
    if (result < 0.0)
	return 0;
    return (int)result;
}

static unsigned char
sun_palette_index(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char red_level = (unsigned char)(((int)red + 26) / 51);
    unsigned char green_level = (unsigned char)(((int)green + 26) / 51);
    unsigned char blue_level = (unsigned char)(((int)blue + 26) / 51);

    if (red_level == green_level) {
	if (red_level == blue_level)
	    return sun_gray_indices[(((int)red + (int)green + (int)blue) / 3) >> 4];
	if (red_level == 0)
	    return sun_blue_indices[blue >> 4];
    } else if (green_level == blue_level && green_level == 0) {
	return sun_red_indices[red >> 4];
    } else if (red_level == blue_level && red_level == 0) {
	return sun_green_indices[green >> 4];
    }
    return (unsigned char)(red_level + green_level * SUN_CUBE_LEVELS +
	blue_level * SUN_CUBE_LEVELS * SUN_CUBE_LEVELS);
}

static unsigned char
sun_dithered_palette_index(unsigned char red, unsigned char green,
	unsigned char blue, struct sun_dither_state *state)
{
    double red_noise = sun_noise(state);
    double green_noise = sun_noise(state);
    double blue_noise = sun_noise(state);
    int dithered_red = sun_clamp_dither(red, red_noise, SUN_DITHER_CUBE_MAGNITUDE);
    int dithered_green = sun_clamp_dither(green, green_noise, SUN_DITHER_CUBE_MAGNITUDE);
    int dithered_blue = sun_clamp_dither(blue, blue_noise, SUN_DITHER_CUBE_MAGNITUDE);
    int red_level = (dithered_red + 26) / 51;
    int green_level = (dithered_green + 26) / 51;
    int blue_level = (dithered_blue + 26) / 51;

    if (red_level == green_level) {
	if (red_level == blue_level) {
	    dithered_red = sun_clamp_dither(red, red_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	    dithered_green = sun_clamp_dither(green, green_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	    dithered_blue = sun_clamp_dither(blue, blue_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	    return sun_gray_indices[((dithered_red + dithered_green + dithered_blue) / 3) >> 4];
	}
	if (red_level == 0) {
	    dithered_blue = sun_clamp_dither(blue, blue_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	    return sun_blue_indices[dithered_blue >> 4];
	}
    } else if (green_level == blue_level && green_level == 0) {
	dithered_red = sun_clamp_dither(red, red_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	return sun_red_indices[dithered_red >> 4];
    } else if (red_level == blue_level && red_level == 0) {
	dithered_green = sun_clamp_dither(green, green_noise, SUN_DITHER_PRIMARY_MAGNITUDE);
	return sun_green_indices[dithered_green >> 4];
    }
    return (unsigned char)(red_level + green_level * SUN_CUBE_LEVELS +
	blue_level * SUN_CUBE_LEVELS * SUN_CUBE_LEVELS);
}

int
sun_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize,
	int dither)
{
    struct sun_dither_state dither_state = {0};
    unsigned char header[SUN_RASTER_HEADER_SIZE];
    uint32_t header_words[SUN_RASTER_HEADER_WORDS];
    unsigned char *pixels;
    unsigned char *output;
    icv_image_t *wimg;
    size_t image_size;
    size_t row_stride;
    size_t row;

    if (UNLIKELY(!bif || !outbuffer || !outsize))
	return BRLCAD_ERROR;
    *outbuffer = NULL;
    *outsize = 0;
    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (wimg == NULL) {
	bu_log("sun_write_mem: color space conflict\n");
	return BRLCAD_ERROR;
    }
    if (wimg->width > UINT32_MAX || wimg->height > UINT32_MAX ||
	wimg->width == SIZE_MAX ||
	(wimg->height && wimg->width > SIZE_MAX / wimg->height / 3)) {
	bu_log("sun_write_mem: image dimensions exceed rasterfile limits\n");
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }
    row_stride = (wimg->width + 1) & ~(size_t)1;
    if (wimg->height && row_stride > SIZE_MAX / wimg->height) {
	bu_log("sun_write_mem: image dimensions exceed rasterfile limits\n");
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }
    image_size = row_stride * wimg->height;
    if (image_size > UINT32_MAX ||
	image_size > SIZE_MAX - SUN_RASTER_HEADER_SIZE - 3 * SUN_MAP_SIZE) {
	bu_log("sun_write_mem: image dimensions exceed rasterfile limits\n");
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }
    pixels = icv_data2uchar(wimg);
    if (pixels == NULL) {
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }
    *outsize = SUN_RASTER_HEADER_SIZE + 3 * SUN_MAP_SIZE + image_size;
    *outbuffer = (unsigned char *)bu_calloc(*outsize, 1, "Sun rasterfile");
    output = *outbuffer;

    header_words[0] = SUN_RASTER_MAGIC;
    header_words[1] = (uint32_t)wimg->width;
    header_words[2] = (uint32_t)wimg->height;
    header_words[3] = SUN_RASTER_DEPTH;
    header_words[4] = (uint32_t)image_size;
    header_words[5] = SUN_RASTER_TYPE_STANDARD;
    header_words[6] = SUN_RASTER_MAPTYPE_EQUAL_RGB;
    header_words[7] = 3 * SUN_MAP_SIZE;
    for (row = 0; row < SUN_RASTER_HEADER_WORDS; row++)
	sun_store_be32(&header[row * sizeof(uint32_t)], header_words[row]);
    memcpy(output, header, sizeof(header));
    output += sizeof(header);
    sun_colormap(output);
    output += 3 * SUN_MAP_SIZE;

    for (row = 0; row < wimg->height; row++) {
	size_t source_row = wimg->height - 1 - row;
	size_t column;

	for (column = 0; column < wimg->width; column++) {
	    size_t source = (source_row * wimg->width + column) * 3;
	    output[column] = dither
		? sun_dithered_palette_index(pixels[source], pixels[source + 1], pixels[source + 2], &dither_state)
		: sun_palette_index(pixels[source], pixels[source + 1], pixels[source + 2]);
	}
	output += row_stride;
    }

    bu_free(pixels, "Sun source pixels");
    icv_destroy(wimg);
    return BRLCAD_OK;
}

int
sun_write(icv_image_t *bif, FILE *fp, int dither)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    size_t written;

    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;
    if (sun_write_mem(bif, &buffer, &size, dither) != BRLCAD_OK)
	return BRLCAD_ERROR;
    written = fwrite(buffer, 1, size, fp);
    bu_free(buffer, "Sun rasterfile");
    if (written != size) {
	bu_log("sun_write: short write\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
icv_write_sun(icv_image_t *bif, const char *filename, int dither)
{
    FILE *fp = filename ? fopen(filename, "wb") : stdout;
    int ret;

    if (!fp) {
	perror("fopen");
	bu_log("icv_write_sun: cannot open %s\n", filename);
	return BRLCAD_ERROR;
    }
    ret = sun_write(bif, fp, dither);
    if (fp != stdout && fclose(fp) != 0) {
	bu_log("icv_write_sun: error closing %s\n", filename);
	return BRLCAD_ERROR;
    }
    return ret;
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
