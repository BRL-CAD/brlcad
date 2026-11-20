/*                        J P E G . C
 * BRL-CAD
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#define JPEG_INTERNAL_OPTIONS
#include <jpeglib.h>

#include "bio.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "icv_private.h"

/* Pixel storage and the scanline entry points below use 8-bit samples. */
#if BITS_IN_JSAMPLE != 8
#  error "ICV JPEG requires the 8-bit libjpeg API"
#endif

const struct bu_mime_capability *
jpeg_capabilities(size_t *count)
{
    static const char *dct_features[] = {
#ifdef D_PROGRESSIVE_SUPPORTED
	"progressive",
#endif
#ifdef D_ARITH_CODING_SUPPORTED
	"arithmetic",
#endif
	NULL
    };
    static const struct bu_mime_trait gray[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("channels", "1"),
	BU_MIME_TRAIT_SET("jpeg.frame-features", dct_features, sizeof(dct_features) / sizeof(dct_features[0]) - 1)};
    static const struct bu_mime_trait rgb[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("channels", "3"),
	BU_MIME_TRAIT_SET("jpeg.frame-features", dct_features, sizeof(dct_features) / sizeof(dct_features[0]) - 1)};
#ifdef D_LOSSLESS_SUPPORTED
    static const char *lossless[] = {"lossless"};
    /* The existing reader requests RGB output. Lossless decoding cannot
     * perform the gray/YCbCr-to-RGB conversions used by the DCT reader. */
    static const struct bu_mime_trait lossless_rgb[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("channels", "3"), BU_MIME_TRAIT_STRING("color-space", "rgb"),
	BU_MIME_TRAIT_SET("jpeg.frame-features", lossless, 1)};
#endif
    static const struct bu_mime_trait output[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("channels", "3"), BU_MIME_TRAIT_STRING("color-space", "ycbcr"),
	BU_MIME_TRAIT_STRING("jpeg.sampling-factors", "1x1,1x1,1x1"), BU_MIME_TRAIT_SET("jpeg.frame-features", NULL, 0)};
    static const unsigned int inputs = ICV_CODEC_READ | ICV_CODEC_READ_MEM;
    static const struct bu_mime_capability formats[] = {
	BU_MIME_CAPABILITY("image/jpeg", NULL, 0, ICV_CODEC_METADATA),
	{"image/jpeg", gray, sizeof(gray) / sizeof(gray[0]), inputs, BU_MIME_DEFER_INPUT_VALIDATION},
	{"image/jpeg", rgb, sizeof(rgb) / sizeof(rgb[0]), inputs, BU_MIME_DEFER_INPUT_VALIDATION},
#ifdef D_LOSSLESS_SUPPORTED
	{"image/jpeg", lossless_rgb, sizeof(lossless_rgb) / sizeof(lossless_rgb[0]), inputs, BU_MIME_DEFER_INPUT_VALIDATION},
#endif
	BU_MIME_CAPABILITY("image/jpeg", output, sizeof(output) / sizeof(output[0]), ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM)
    };
    *count = sizeof(formats) / sizeof(formats[0]);
    return formats;
}

static const char *
jpeg_color_name(J_COLOR_SPACE color)
{
    switch (color) {
	case JCS_GRAYSCALE: return "gray";
	case JCS_RGB: return "rgb";
	case JCS_YCbCr: return "ycbcr";
	case JCS_CMYK: return "cmyk";
	case JCS_YCCK: return "ycck";
	default: return NULL;
    }
}

static int
jpeg_header_traits(const struct jpeg_decompress_struct *header, struct bu_mime_traits *traits)
{
    const char *features[3];
    size_t feature_count = 0;
    struct bu_vls number = BU_VLS_INIT_ZERO;
    struct bu_vls sampling = BU_VLS_INIT_ZERO;
    const char *color = jpeg_color_name(header->jpeg_color_space);
    if (header->progressive_mode) features[feature_count++] = "progressive";
    if (header->arith_code) features[feature_count++] = "arithmetic";
    /* Lossless SOS uses a nonzero predictor and Se=0; sequential DCT uses
     * Ss=0 and Se=63. This also works with older libjpeg public structures. */
    if (!header->progressive_mode && header->Ss && !header->Se) features[feature_count++] = "lossless";
    int valid = bu_mime_traits_add(traits, "encoding", "binary") &&
	bu_mime_traits_add_set(traits, "jpeg.frame-features", features, feature_count);
    bu_vls_printf(&number, "%d", header->data_precision);
    valid = valid && bu_mime_traits_add(traits, "bit-depth", bu_vls_cstr(&number));
    bu_vls_sprintf(&number, "%d", header->num_components);
    valid = valid && bu_mime_traits_add(traits, "channels", bu_vls_cstr(&number));
    for (int i = 0; i < header->num_components; ++i) {
	if (i) bu_vls_putc(&sampling, ',');
	bu_vls_printf(&sampling, "%dx%d", header->comp_info[i].h_samp_factor, header->comp_info[i].v_samp_factor);
    }
    valid = valid && bu_mime_traits_add(traits, "jpeg.sampling-factors", bu_vls_cstr(&sampling)) &&
	(!color || bu_mime_traits_add(traits, "color-space", color));
    bu_vls_free(&number);
    bu_vls_free(&sampling);
    return valid;
}

static int
jpeg_input_profile(const struct jpeg_decompress_struct *header, const struct bu_mime_traits *requested, const struct bu_mime_traits *observations)
{
    struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    size_t count;
    const struct bu_mime_capability *formats = jpeg_capabilities(&count);
    int supported = 0;
    if (jpeg_header_traits(header, &observed) && bu_mime_traits_match(&observed, observations, requested))
	for (size_t i = 0; i < count; ++i) {
	    struct bu_mime_capability complete = formats[i];
	    complete.flags = 0;
	    if ((complete.operations & ICV_CODEC_READ) && bu_mime_capability_match(&complete, &observed, BU_MIME_INPUT)) {
		supported = 1;
		break;
	    }
	}
    bu_mime_traits_free(&observed);
    if (!supported) bu_log("JPEG: unsupported input profile or contradictory traits\n");
    return supported;
}

struct icv_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void
icv_jpeg_error_exit(j_common_ptr cinfo)
{
    struct icv_jpeg_error_mgr *myerr = (struct icv_jpeg_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

static void
icv_jpeg_quiet(j_common_ptr UNUSED(cinfo))
{
}

static void
icv_jpeg_source_noop(j_decompress_ptr UNUSED(cinfo))
{
}

static boolean
icv_jpeg_suspend(j_decompress_ptr UNUSED(cinfo))
{
    return FALSE;
}

static void
icv_jpeg_skip(j_decompress_ptr cinfo, long count)
{
    if (count <= 0) return;
    size_t available = cinfo->src->bytes_in_buffer;
    size_t consumed = (unsigned long)count > available ? available : (size_t)count;
    cinfo->src->next_input_byte += consumed;
    cinfo->src->bytes_in_buffer -= consumed;
}

static enum icv_metadata_status
jpeg_prefix_header(const struct bu_mime_probe_input *input, struct bu_mime_traits *traits,
    struct icv_image_metadata *info)
{
    struct jpeg_decompress_struct header;
    struct icv_jpeg_error_mgr error;
    struct jpeg_source_mgr source;
    memset(&header, 0, sizeof(header));
    header.err = jpeg_std_error(&error.pub);
    error.pub.error_exit = icv_jpeg_error_exit;
    error.pub.output_message = icv_jpeg_quiet;
    if (setjmp(error.setjmp_buffer)) {
	jpeg_destroy_decompress(&header);
	return ICV_METADATA_ERROR;
    }
    jpeg_create_decompress(&header);
    /* Suspend at the prefix boundary instead of inserting an artificial EOI.
     * This source is inspected once, without resuming skipped segments. */
    source.init_source = icv_jpeg_source_noop;
    source.fill_input_buffer = icv_jpeg_suspend;
    source.skip_input_data = icv_jpeg_skip;
    source.resync_to_restart = jpeg_resync_to_restart;
    source.term_source = icv_jpeg_source_noop;
    source.bytes_in_buffer = input->size;
    source.next_input_byte = input->data;
    header.src = &source;
    int status = jpeg_read_header(&header, TRUE);
    if (status == JPEG_SUSPENDED) {
	jpeg_destroy_decompress(&header);
	return input->complete ? ICV_METADATA_ERROR : ICV_METADATA_INCOMPLETE;
    }
    int valid = jpeg_header_traits(&header, traits);
    if (info && valid) {
	info->width = header.image_width;
	info->height = header.image_height;
	info->channels = header.num_components;
	switch (header.jpeg_color_space) {
	    case JCS_GRAYSCALE: info->color_space = ICV_METADATA_COLOR_GRAY; break;
	    case JCS_RGB: case JCS_YCbCr: info->color_space = ICV_METADATA_COLOR_RGB; break;
	    case JCS_CMYK: case JCS_YCCK: info->color_space = ICV_METADATA_COLOR_CMYK; break;
	    default: info->color_space = ICV_METADATA_COLOR_UNKNOWN; break;
	}
    }
    jpeg_destroy_decompress(&header);
    return valid ? ICV_METADATA_MATCH : ICV_METADATA_ERROR;
}

int
jpeg_inspect(const struct bu_mime_probe_input *input, const char *UNUSED(type), struct bu_mime_traits *traits)
{
    /* A malformed/partial JPEG header remains recognizable by its signature.
     * Decoder errors are reported by the operation that needs a valid file. */
    (void)jpeg_prefix_header(input, traits, NULL);
    return 1;
}

enum icv_metadata_status
jpeg_metadata(const struct bu_mime_probe_input *input, const char *UNUSED(type),
    struct icv_image_metadata *info, const struct bu_mime_traits *requested, const struct bu_mime_traits *observations)
{
    struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    enum icv_metadata_status status = jpeg_prefix_header(input, &observed, info);
    if (status == ICV_METADATA_MATCH && !bu_mime_traits_match(&observed, observations, requested))
	status = ICV_METADATA_ERROR;
    bu_mime_traits_free(&observed);
    return status;
}

static int
icv_jpeg_quality(int quality)
{
    const int min_quality = 1;
    const int max_quality = 100;

    if (quality < min_quality)
	return min_quality;
    if (quality > max_quality)
	return max_quality;
    return quality;
}

int
jpeg_write(icv_image_t *bif, FILE *fp, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *data = NULL;
    icv_image_t *wimg = NULL;
    JSAMPROW row_pointer[1];
    size_t row;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("jpeg_write : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("jpeg_write : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_compress(&cinfo);
	if (data) bu_free(data, "jpeg_write data");
	if (wimg) icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = (JDIMENSION)wimg->width;
    cinfo.image_height = (JDIMENSION)wimg->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, icv_jpeg_quality(quality), TRUE);

    // Force 4:4:4 (disable chroma subsampling) - for
    // raytraced images, which usually have sharp edges,
    // we want to preserve per-pixel color.
    cinfo.comp_info[0].h_samp_factor = 1; // Y
    cinfo.comp_info[0].v_samp_factor = 1;
    cinfo.comp_info[1].h_samp_factor = 1; // Cb
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1; // Cr
    cinfo.comp_info[2].v_samp_factor = 1;

    // better compression, same quality
    cinfo.optimize_coding = TRUE;

    // highest DCT precision
    cinfo.dct_method = JDCT_ISLOW;

    jpeg_start_compress(&cinfo, TRUE);

    data = icv_data2uchar(wimg);

    /* Write rows top-to-bottom (JPEG convention).
     * BRL-CAD images are stored bottom-up, so row 0 in data is the
     * bottom of the image.  We flip here so viewers display correctly. */
    while (cinfo.next_scanline < cinfo.image_height) {
	row = wimg->height - 1 - cinfo.next_scanline;
	row_pointer[0] = data + row * wimg->width * 3;
	jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    bu_free(data, "jpeg_write data");
    icv_destroy(wimg);
    return BRLCAD_OK;
}

int
jpeg_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *data = NULL;
    icv_image_t *wimg = NULL;
    JSAMPROW row_pointer[1];
    size_t row;
    unsigned char *mem_buf = NULL;
    unsigned long mem_size = 0;

    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize))
	return BRLCAD_ERROR;

    *outbuffer = NULL;
    *outsize = 0;

    wimg = icv_image_for_write(bif, ICV_COLOR_SPACE_RGB, 3);
    if (!wimg) {
	bu_log("jpeg_write_mem : Color Space conflict");
	return BRLCAD_ERROR;
    }

    if (wimg->channels != 3) {
	bu_log("jpeg_write_mem : Channel count conflict (expected 3, got %d)", (int)wimg->channels);
	icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_compress(&cinfo);
	if (mem_buf) free(mem_buf);
	if (data) bu_free(data, "jpeg_write_mem data");
	if (wimg) icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    jpeg_create_compress(&cinfo);

    // Write to a memory buffer instead of FILE*
    jpeg_mem_dest(&cinfo, &mem_buf, &mem_size);

    cinfo.image_width = (JDIMENSION)wimg->width;
    cinfo.image_height = (JDIMENSION)wimg->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, icv_jpeg_quality(quality), TRUE);

    // Force 4:4:4 (disable chroma subsampling) - for
    // raytraced images, which usually have sharp edges,
    // we want to preserve per-pixel color.
    cinfo.comp_info[0].h_samp_factor = 1; // Y
    cinfo.comp_info[0].v_samp_factor = 1;
    cinfo.comp_info[1].h_samp_factor = 1; // Cb
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1; // Cr
    cinfo.comp_info[2].v_samp_factor = 1;

    // better compression, same quality
    cinfo.optimize_coding = TRUE;

    // highest DCT precision
    cinfo.dct_method = JDCT_ISLOW;

    jpeg_start_compress(&cinfo, TRUE);

    data = icv_data2uchar(wimg);

    /* Write rows top-to-bottom (JPEG convention).
     * BRL-CAD images are stored bottom-up, so row 0 in data is the
     * bottom of the image.  We flip here so viewers display correctly. */
    while (cinfo.next_scanline < cinfo.image_height) {
	row = wimg->height - 1 - cinfo.next_scanline;
	row_pointer[0] = data + row * wimg->width * 3;
	jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    bu_free(data, "jpeg_write_mem data");
    icv_destroy(wimg);
    wimg = NULL;

    /* Libjpeg uses standard malloc() for its memory destination buffer, but
     * BRL-CAD expects tracked memory. We copy it into a bu_malloc'd buffer
     * and free the libjpeg one to maintain proper API boundaries. */
    if (mem_buf && mem_size > 0) {
	*outsize = (size_t)mem_size;
	*outbuffer = (unsigned char *)bu_malloc(*outsize, "icv_write_mem buffer");
	memcpy(*outbuffer, mem_buf, *outsize);
	free(mem_buf);
    } else {
	if (wimg) icv_destroy(wimg);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

icv_image_t *
jpeg_read(FILE *fp, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    struct jpeg_decompress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *image = NULL;
    JSAMPROW row_pointer[1];
    size_t width, height;
    icv_image_t *bif = NULL;

    if (UNLIKELY(!fp))
	return NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_decompress(&cinfo);
	if (image) bu_free(image, "jpeg_read image");
	if (bif) icv_destroy(bif);
	return NULL;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    if (!jpeg_input_profile(&cinfo, traits, observations)) {
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;

    if (cinfo.output_components != 3) {
	bu_log("jpeg_read: unexpected component count: %d\n",
	       cinfo.output_components);
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    if (width > 0 && height > (size_t)-1 / width / 3 / sizeof(double)) {
	bu_log("jpeg_read: dimensions excessively large, causing integer overflow\n");
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    image = (unsigned char *)bu_malloc(width * height * 3, "jpeg_read image");

    /* Read top-to-bottom (JPEG), store bottom-up (BRL-CAD convention) */
    while (cinfo.output_scanline < cinfo.output_height) {
	size_t row = height - 1 - cinfo.output_scanline;
	row_pointer[0] = image + row * width * 3;
	jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->width = width;
    bif->height = height;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    bif->magic = ICV_IMAGE_MAGIC;
    bif->data = icv_uchar2double(image, 3 * width * height);
    bu_free(image, "jpeg_read uchar data");

    return bif;
}

icv_image_t *
jpeg_read_mem(const unsigned char *buffer, size_t size, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    struct jpeg_decompress_struct cinfo;
    struct icv_jpeg_error_mgr jerr;
    unsigned char *image = NULL;
    JSAMPROW row_pointer[1];
    size_t width, height;
    icv_image_t *bif = NULL;

    if (UNLIKELY(!buffer || size == 0))
	return NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = icv_jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	jpeg_destroy_decompress(&cinfo);
	if (image) bu_free(image, "jpeg_read_mem image");
	if (bif) icv_destroy(bif);
	return NULL;
    }

    jpeg_create_decompress(&cinfo);

    // Libjpeg provides this for memory reading
    jpeg_mem_src(&cinfo, buffer, size);

    jpeg_read_header(&cinfo, TRUE);
    if (!jpeg_input_profile(&cinfo, traits, observations)) {
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;

    if (cinfo.output_components != 3) {
	bu_log("jpeg_read_mem: unexpected component count: %d\n",
	       cinfo.output_components);
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    if (width > 0 && height > (size_t)-1 / width / 3 / sizeof(double)) {
	bu_log("jpeg_read_mem: dimensions excessively large, causing integer overflow\n");
	jpeg_destroy_decompress(&cinfo);
	return NULL;
    }

    image = (unsigned char *)bu_malloc(width * height * 3, "jpeg_read_mem image");

    /* Read top-to-bottom (JPEG), store bottom-up (BRL-CAD convention) */
    while (cinfo.output_scanline < cinfo.output_height) {
	size_t row = height - 1 - cinfo.output_scanline;
	row_pointer[0] = image + row * width * 3;
	jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);
    bif->width = width;
    bif->height = height;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;
    bif->magic = ICV_IMAGE_MAGIC;
    bif->data = icv_uchar2double(image, 3 * width * height);
    bu_free(image, "jpeg_read_mem uchar data");

    return bif;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
