/*                           P P M . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file ppm.c
 *
 * This file contains reading and writing routines for ppm format.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "icv_private.h"
#include "../libbu/mime_pnm.h"

#define PPM_CHANNELS 3
static int
ppm_header_read(struct mime_pnm_input *input, struct mime_pnm_header *header)
{
    return mime_pnm_header_read(input, header) && (header->kind == '3' || header->kind == '6');
}

enum icv_metadata_status
ppm_metadata(const struct bu_mime_probe_input *probe, const char *UNUSED(type),
    struct icv_image_metadata *info, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    struct mime_pnm_input input = {NULL, probe->data, probe->data ? probe->data + probe->size : NULL, 0};
    struct mime_pnm_header header;
    if (!ppm_header_read(&input, &header))
	return input.ended && !probe->complete ? ICV_METADATA_INCOMPLETE : ICV_METADATA_ERROR;
    if (!mime_pnm_profile_matches(&header, traits, observations))
	return ICV_METADATA_ERROR;
    info->width = header.width;
    info->height = header.height;
    info->channels = PPM_CHANNELS;
    info->color_space = ICV_METADATA_COLOR_RGB;
    return ICV_METADATA_MATCH;
}

static icv_image_t *
ppm_decode(struct mime_pnm_input *input, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    struct mime_pnm_header header;
    if (!ppm_header_read(input, &header) ||
	header.height > SIZE_MAX / header.width / PPM_CHANNELS / sizeof(double)) {
	bu_log("PPM: invalid dimensions or sample range\n");
	return NULL;
    }
    if (!mime_pnm_profile_matches(&header, traits, observations)) {
	bu_log("PPM: requested profile does not match the input header\n");
	return NULL;
    }
    size_t width = header.width, height = header.height, maxval = header.maxval;
    icv_image_t *image = icv_create(width, height, ICV_COLOR_SPACE_RGB);
    if (!image)
	return NULL;
    for (size_t row = 0; row < height; ++row) {
	size_t offset = (height - row - 1) * width * PPM_CHANNELS;
	for (size_t sample = 0; sample < width * PPM_CHANNELS; ++sample) {
	    size_t value;
	    if (header.kind == '3') {
		if (!mime_pnm_number(input, &value))
		    goto invalid;
	    } else {
		int byte = mime_pnm_byte(input);
		if (byte == EOF)
		    goto invalid;
		value = (size_t)byte;
		if (maxval > MIME_PNM_BYTE_MAX) {
		    byte = mime_pnm_byte(input);
		    if (byte == EOF)
			goto invalid;
		    value = value * (MIME_PNM_BYTE_MAX + 1) + (size_t)byte;
		}
	    }
	    if (value > maxval)
		goto invalid;
	    image->data[offset + sample] = (double)value / (double)maxval;
	}
    }
    return image;
invalid:
    bu_log("PPM: truncated or out-of-range raster data\n");
    icv_destroy(image);
    return NULL;
}

icv_image_t *
ppm_read(FILE *stream, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    if (!stream)
	return NULL;
    struct mime_pnm_input input = {stream, NULL, NULL, 0};
    icv_image_t *image = ppm_decode(&input, traits, observations);
    if (ferror(stream) && image) {
	icv_destroy(image);
	return NULL;
    }
    return image;
}

icv_image_t *
ppm_read_mem(const unsigned char *buffer, size_t size, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    if (!buffer || !size)
	return NULL;
    struct mime_pnm_input input = {NULL, buffer, buffer + size, 0};
    return ppm_decode(&input, traits, observations);
}

static int
ppm_encode(icv_image_t *image, FILE *stream, unsigned char **buffer, size_t *size)
{
    if (!stream && (!buffer || !size))
	return BRLCAD_ERROR;
    if (buffer)
	*buffer = NULL;
    if (size)
	*size = 0;
    icv_image_t *rgb = icv_image_for_write(image, ICV_COLOR_SPACE_RGB, PPM_CHANNELS);
    if (!rgb)
	return BRLCAD_ERROR;
    struct bu_vls header = BU_VLS_INIT_ZERO;
    bu_vls_printf(&header, "P6\n%zu %zu\n%d\n", rgb->width, rgb->height, MIME_PNM_BYTE_MAX);
    size_t header_size = bu_vls_strlen(&header);
    size_t row_size = rgb->width * PPM_CHANNELS;
    size_t raster_size = row_size * rgb->height;
    unsigned char *pixels = NULL;
    int status = BRLCAD_ERROR;
    if (raster_size > SIZE_MAX - header_size)
	goto cleanup;
    pixels = icv_data2uchar(rgb);
    if (!pixels)
	goto cleanup;
    if (stream) {
	if (fwrite(bu_vls_cstr(&header), 1, header_size, stream) != header_size)
	    goto cleanup;
    } else {
	*size = header_size + raster_size;
	*buffer = (unsigned char *)bu_malloc(*size, "PPM output");
	memcpy(*buffer, bu_vls_cstr(&header), header_size);
    }
    for (size_t row = 0; row < rgb->height; ++row) {
	const unsigned char *source = pixels + (rgb->height - row - 1) * row_size;
	if (stream) {
	    if (fwrite(source, 1, row_size, stream) != row_size)
		goto cleanup;
	} else {
	    memcpy(*buffer + header_size + row * row_size, source, row_size);
	}
    }
    status = BRLCAD_OK;
cleanup:
    bu_free(pixels, "PPM pixels");
    bu_vls_free(&header);
    icv_destroy(rgb);
    return status;
}

int
ppm_write(icv_image_t *image, FILE *stream)
{
    return stream ? ppm_encode(image, stream, NULL, NULL) : BRLCAD_ERROR;
}

int
ppm_write_mem(icv_image_t *image, unsigned char **buffer, size_t *size)
{
    return ppm_encode(image, NULL, buffer, size);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
