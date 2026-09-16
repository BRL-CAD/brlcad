/*                      F I L E F O R M A T . C
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
#include <sys/stat.h>	/* for file mode info in WRMODE */
#include "png.h"

#include "bio.h"

#include "bu/str.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/path.h"
#include "bn.h"
#include "vmath.h"
#include "icv_private.h"

/* private functions */

#define ICV_IMAGE_ALLOC_STDLIB 0x0008
#define ICV_DATA_ALLOC_STDLIB 0x0010
#define ICV_OWNERSHIP_FLAGS (ICV_IMAGE_ALLOC_STDLIB | ICV_DATA_ALLOC_STDLIB)

void
icv_image_data_free(icv_image_t *img, const char *label)
{
    if (!img || !img->data)
	return;

    if (img->flags & ICV_DATA_ALLOC_STDLIB)
	free(img->data);
    else
	bu_free(img->data, label);

    img->data = NULL;
    img->flags &= (uint16_t)~ICV_DATA_ALLOC_STDLIB;
}


void
icv_image_data_set_bu(icv_image_t *img, double *data)
{
    if (!img)
	return;

    img->data = data;
    img->flags &= (uint16_t)~ICV_DATA_ALLOC_STDLIB;
}


void
icv_image_data_set_stdlib(icv_image_t *img, double *data)
{
    if (!img)
	return;

    img->data = data;
    if (data)
	img->flags |= ICV_DATA_ALLOC_STDLIB;
    else
	img->flags &= (uint16_t)~ICV_DATA_ALLOC_STDLIB;
}


int
icv_image_data_realloc(icv_image_t *img, size_t size, const char *label)
{
    double *data;

    if (!img || size == 0)
	return -1;

    if (img->flags & ICV_DATA_ALLOC_STDLIB) {
	data = (double *)realloc(img->data, size);
	if (!data)
	    return -1;
	img->data = data;
	return 0;
    }

    img->data = (double *)bu_realloc(img->data, size, label);
    return 0;
}

/*
 * Attempt to guess the file type. Understands ImageMagick style
 * FMT:filename as being preferred, but will attempt to guess based on
 * extension as well.
 */
static bu_mime_image_t
icv_guess_file_format(const char *filename, struct bu_vls *trimmedname)
{
    static const struct {
	const char *prefix;
	bu_mime_image_t format;
    } prefixed_formats[] = {
	{"PIX:", BU_MIME_IMAGE_PIX},
	{"PNG:", BU_MIME_IMAGE_PNG},
	{"JPEG:", BU_MIME_IMAGE_JPEG},
	{"PPM:", BU_MIME_IMAGE_PPM},
	{"BMP:", BU_MIME_IMAGE_BMP},
	{"BW:", BU_MIME_IMAGE_BW},
	{"DPIX:", BU_MIME_IMAGE_DPIX},
	{"RLE:", BU_MIME_IMAGE_RLE}
    };
    struct bu_vls ext = BU_VLS_INIT_ZERO;
    int mime_type = -1;
    size_t i = 0;

    /* If we have no filename, there's nothing to go on */
    if (!filename)
	return BU_MIME_IMAGE_PIX;

    /* look for the FMT: header */
    for (i = 0; i < sizeof(prefixed_formats) / sizeof(prefixed_formats[0]); i++) {
	size_t prefix_len = strlen(prefixed_formats[i].prefix);
	if (!bu_strncmp(filename, prefixed_formats[i].prefix, prefix_len)) {
	    if (trimmedname)
		bu_vls_sprintf(trimmedname, "%s", filename + prefix_len);
	    return prefixed_formats[i].format;
	}
    }

    /* no format header found, copy the name as it is */
    if (trimmedname)
	bu_vls_sprintf(trimmedname, "%s", filename);

    /* and guess based on extension */
    if (bu_path_component(&ext, filename, BU_PATH_EXT)) {
	mime_type = bu_file_mime(bu_vls_cstr(&ext), BU_MIME_IMAGE);
    }
    bu_vls_free(&ext);
    if (mime_type >= 0)
	return (bu_mime_image_t)mime_type;

    /* defaulting to PIX */
    return BU_MIME_IMAGE_PIX;
}

/* begin public functions */

icv_image_t *
icv_read(const char *filename, bu_mime_image_t format, size_t width, size_t height)
{
    struct bu_vls ifilename = BU_VLS_INIT_ZERO;
    const char *ifname = filename;

    if (format == BU_MIME_IMAGE_AUTO) {
	format = icv_guess_file_format(filename, &ifilename);
	if (filename)
	    ifname = bu_vls_cstr(&ifilename);
    }

    FILE *fp = (!ifname) ? stdin : fopen(ifname, "rb");
    if (!fp) {
	bu_log("ERROR: Cannot open file %s for reading\n", filename);
	bu_vls_free(&ifilename);
	return NULL;
    }
    if (!ifname)
	setmode(fileno(fp), O_BINARY);

    icv_image_t *oimg = NULL;
    switch (format) {
	case BU_MIME_IMAGE_PNG:
	    oimg = png_read(fp);
	    break;
	case BU_MIME_IMAGE_PIX:
	    oimg = pix_read(fp, width, height);
	    break;
	case BU_MIME_IMAGE_BW :
	    oimg = bw_read(fp, width, height);
	    break;
	case BU_MIME_IMAGE_DPIX :
	    oimg = dpix_read(fp, width, height);
	    break;
	case BU_MIME_IMAGE_PPM :
	    oimg = ppm_read(fp);
	    break;
	case BU_MIME_IMAGE_RLE :
	    oimg = rle_read(fp);
	    break;
	case BU_MIME_IMAGE_JPEG :
	    oimg = jpeg_read(fp);
	    break;
	default:
	    bu_log("icv_read not implemented for this format\n");
	    fclose(fp);
	    bu_vls_free(&ifilename);
	    return NULL;
    }

    if (fp != stdin) {
	fflush(fp);
	fclose(fp);
    }
    bu_vls_free(&ifilename);
    return oimg;
}

icv_image_t *
icv_read_mem(const unsigned char *buffer, size_t size, bu_mime_image_t format, size_t width, size_t height)
{
    if (!buffer || size == 0) {
	return NULL;
    }

    (void)width;
    (void)height;

    icv_image_t *oimg = NULL;
    switch (format) {
	case BU_MIME_IMAGE_JPEG:
	    oimg = jpeg_read_mem(buffer, size);
	    break;
	case BU_MIME_IMAGE_PNG:
	    oimg = png_read_mem(buffer, size);
	    break;
	case BU_MIME_IMAGE_PIX:
	    oimg = pix_read_mem(buffer, size, width, height);
	    break;
	case BU_MIME_IMAGE_BW:
	    oimg = bw_read_mem(buffer, size, width, height);
	    break;
	case BU_MIME_IMAGE_DPIX:
	    oimg = dpix_read_mem(buffer, size, width, height);
	    break;
	case BU_MIME_IMAGE_PPM:
	    oimg = ppm_read_mem(buffer, size);
	    break;
	case BU_MIME_IMAGE_RLE:
	    oimg = rle_read_mem(buffer, size);
	    break;
	default:
	    bu_log("icv_read_mem not implemented for this format\n");
	    return NULL;
    }

    return oimg;
}


int
icv_write(icv_image_t *bif, const char *filename, bu_mime_image_t format)
{
    int ret = 0;
    struct bu_vls ofilename = BU_VLS_INIT_ZERO;
    const char *ofname = filename;

    if (format == BU_MIME_IMAGE_AUTO) {
	format = icv_guess_file_format(filename, &ofilename);
	if (filename)
	    ofname = bu_vls_cstr(&ofilename);
    }

    ICV_IMAGE_VAL_INT(bif);

    FILE *fp = (ofname==NULL) ? stdout : fopen(ofname, "wb");
    if (UNLIKELY(fp==NULL)) {
	perror("fopen");
	bu_log("ERROR: icv_write failed to get a FILE pointer for %s\n", filename);
	return BRLCAD_ERROR;
    }

    switch (format) {
	/* case BU_MIME_IMAGE_BMP:
	   return bmp_write(bif, ofname); */
	case BU_MIME_IMAGE_PPM:
	    ret = ppm_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_PNG:
	    ret = png_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_PIX:
	    ret = pix_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_BW:
	    ret = bw_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_DPIX :
	    ret = dpix_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_RLE :
	    ret = rle_write(bif, fp);
	    break;
	case BU_MIME_IMAGE_JPEG :
	    // The icv write function doesn't expose a quality setting - we
	    // try to make a good trade-off between file size and quality.
	    // There is little point in 100% quality, since JPEG is inherently
	    // lossy - it gives us larger file sizes for little in return.
	    ret = jpeg_write(bif, fp, 90);
	    break;
	default:
	    ret = pix_write(bif, fp);
    }

    if (fp != stdout) {
	fflush(fp);
	fclose(fp);
    }

    bu_vls_free(&ofilename);
    return ret;
}

int
icv_write_mem(icv_image_t *bif, unsigned char **buffer, size_t *size, bu_mime_image_t format)
{
    int ret = 0;

    if (!bif || !buffer || !size) {
	return BRLCAD_ERROR;
    }

    *buffer = NULL;
    *size = 0;

    ICV_IMAGE_VAL_INT(bif);

    switch (format) {
	case BU_MIME_IMAGE_JPEG:
	    ret = jpeg_write_mem(bif, buffer, size, 90);
	    break;
	case BU_MIME_IMAGE_PNG:
	    ret = png_write_mem(bif, buffer, size);
	    break;
	case BU_MIME_IMAGE_PPM:
	    ret = ppm_write_mem(bif, buffer, size);
	    break;
	case BU_MIME_IMAGE_BW:
	    ret = bw_write_mem(bif, buffer, size);
	    break;
	case BU_MIME_IMAGE_PIX:
	    ret = pix_write_mem(bif, buffer, size);
	    break;
	case BU_MIME_IMAGE_DPIX:
	    ret = dpix_write_mem(bif, buffer, size);
	    break;
	case BU_MIME_IMAGE_RLE:
	    ret = rle_write_mem(bif, buffer, size);
	    break;
	default:
	    bu_log("ERROR: icv_write_mem not implemented for this format\n");
	    return BRLCAD_ERROR;
    }

    return ret;
}

int
icv_writeline(icv_image_t *bif, size_t y, void *data, ICV_DATA type)
{
    double *dst;
    size_t width_size;
    unsigned char *p=NULL;

    if (bif == NULL || data == NULL)
	return -1;

    ICV_IMAGE_VAL_INT(bif);

    if (y >= bif->height)
	return -1;

    width_size = (size_t) bif->width*bif->channels;
    dst = bif->data + width_size*y;

    if (type == ICV_DATA_UCHAR) {
	p = (unsigned char *)data;
	for (; width_size > 0; width_size--) {
	    *dst = ICV_CONV_8BIT(*p);
	    p++;
	    dst++;
	}
    } else
	memcpy(dst, data, width_size*sizeof(double));


    return 0;
}


int
icv_writepixel(icv_image_t *bif, size_t x, size_t y, double *data)
{
    double *dst;

    ICV_IMAGE_VAL_INT(bif);

    if (x >= bif->width)
	return -1;

    if (y >= bif->height)
	return -1;

    if (data == NULL)
	return -1;

    dst = bif->data + (y*bif->width + x)*bif->channels;

    /* can copy float to double also double to double */
    VMOVEN(dst, data, bif->channels);
    return 0;
}


static int
icv_channels_match_color_space(ICV_COLOR_SPACE color_space, size_t channels)
{
    switch (color_space) {
	case ICV_COLOR_SPACE_RGB:
	    return channels == 3 || channels == 4;
	case ICV_COLOR_SPACE_GRAY:
	    return channels == 1 || channels == 2;
	default:
	    return 0;
    }
}

icv_image_t *
icv_create_with_channels(size_t width, size_t height, ICV_COLOR_SPACE color_space, size_t channels)
{
    icv_image_t *bif;
    size_t data_size;

    if (!icv_channels_match_color_space(color_space, channels)) {
	bu_log("icv_create_with_channels : invalid color space/channel combination\n");
	return NULL;
    }

    if (width == 0 || height == 0) {
	bu_log("icv_create_with_channels : image dimensions must be greater than zero\n");
	return NULL;
    }

    /* Prevent integer overflows on size calculations for massive images. */
    if (width > 0 && height > (size_t)-1 / width / channels / sizeof(double)) {
	bu_log("icv_create_with_channels : image dimensions excessively large, causing integer overflow\n");
	return NULL;
    }

    data_size = height * width * channels * sizeof(double);
    bif = (icv_image_t *)calloc(1, sizeof(struct icv_image));
    if (!bif) {
	bu_log("icv_create_with_channels : image structure allocation failed\n");
	return NULL;
    }

    ICV_IMAGE_INIT(bif);
    bif->width = width;
    bif->height = height;
    bif->color_space = color_space;
    bif->channels = channels;
    bif->alpha_channel = (channels == 2 || channels == 4) ? 1 : 0;
    bif->flags = ICV_IMAGE_ALLOC_STDLIB;
    icv_image_data_set_stdlib(bif, (double *)calloc(1, data_size));
    if (!bif->data) {
	free(bif);
	bu_log("icv_create_with_channels : image data allocation failed\n");
	return NULL;
    }

    return icv_zero(bif);
}

icv_image_t *
icv_create(size_t width, size_t height, ICV_COLOR_SPACE color_space)
{
    icv_image_t *bif;
    size_t channels = 0;

    switch (color_space) {
	case ICV_COLOR_SPACE_RGB :
	    channels = 3;
	    break;
	case ICV_COLOR_SPACE_GRAY :
	    channels = 1;
	    break;
	default :
	    bu_log("icv_create : Color Space Not Defined\n");
	    return NULL;
    }

    bif = icv_create_with_channels(width, height, color_space, channels);
    if (!bif) {
	bu_log("icv_create : image allocation failed\n");
	return NULL;
    }

    return bif;
}

static struct icv_render_info *
icv_render_info_clone(const struct icv_render_info *src)
{
    struct icv_render_info *dst;

    if (!src)
	return NULL;

    dst = icv_render_info_create();
    if (src->db_filename)
	dst->db_filename = bu_strdup(src->db_filename);
    if (src->objects)
	dst->objects = bu_strdup(src->objects);
    memcpy(dst->viewrotscale, src->viewrotscale, sizeof(dst->viewrotscale));
    memcpy(dst->eye_model, src->eye_model, sizeof(dst->eye_model));
    dst->viewsize = src->viewsize;
    dst->aspect = src->aspect;
    dst->perspective = src->perspective;

    return dst;
}

icv_image_t *
icv_clone(const icv_image_t *src)
{
    icv_image_t *dst;
    size_t size;

    if (!ICV_IMAGE_IS_INITIALIZED(src))
	return NULL;

    dst = icv_create_with_channels(src->width, src->height, src->color_space, src->channels);
    if (!dst)
	return NULL;

    dst->gamma_corr = src->gamma_corr;
    dst->flags = (uint16_t)((src->flags & ~ICV_OWNERSHIP_FLAGS) | (dst->flags & ICV_OWNERSHIP_FLAGS));
    size = src->width * src->height * src->channels;
    if (size && !src->data) {
	icv_destroy(dst);
	return NULL;
    }
    if (size)
	memcpy(dst->data, src->data, size * sizeof(double));
    dst->render_info = icv_render_info_clone(src->render_info);

    return dst;
}

icv_image_t *
icv_image_for_write(const icv_image_t *src, ICV_COLOR_SPACE color_space, size_t channels)
{
    icv_image_t *dst;
    size_t npix, i;

    if (!ICV_IMAGE_IS_INITIALIZED(src) || !src->data)
	return NULL;

    if (!icv_channels_match_color_space(color_space, channels))
	return NULL;

    /* Writers prepare a temporary image so format-required color conversion
     * never mutates caller-owned icv_image_t state.  Alpha is preserved only
     * when the target format can represent it; raw BW/PIX/PPM/DPIX/JPEG
     * requests intentionally project to one or three channels. */
    if (color_space == ICV_COLOR_SPACE_GRAY && channels == 1) {
	dst = icv_clone(src);
	if (!dst)
	    return NULL;
	if (dst->color_space == ICV_COLOR_SPACE_RGB) {
	    if (icv_rgb2gray(dst, ICV_COLOR_RGB, 0, 0, 0) != 0) {
		icv_destroy(dst);
		return NULL;
	    }
	} else if (dst->color_space == ICV_COLOR_SPACE_GRAY && dst->channels == 2) {
	    icv_image_t *gray = icv_create_with_channels(dst->width, dst->height, ICV_COLOR_SPACE_GRAY, 1);
	    if (!gray) {
		icv_destroy(dst);
		return NULL;
	    }
	    npix = dst->width * dst->height;
	    for (i = 0; i < npix; i++)
		gray->data[i] = dst->data[i * dst->channels];
	    icv_destroy(dst);
	    dst = gray;
	}
	if (dst->color_space == ICV_COLOR_SPACE_GRAY && dst->channels == 1)
	    return dst;
	icv_destroy(dst);
	return NULL;
    }

    if (color_space == ICV_COLOR_SPACE_RGB && channels == 3) {
	if (src->color_space == ICV_COLOR_SPACE_GRAY) {
	    dst = icv_clone(src);
	    if (!dst)
		return NULL;
	    if (icv_gray2rgb(dst) != 0) {
		icv_destroy(dst);
		return NULL;
	    }
	    if (dst->channels == 3)
		return dst;
	    icv_destroy(dst);
	    return NULL;
	}

	if (src->color_space == ICV_COLOR_SPACE_RGB && (src->channels == 3 || src->channels == 4)) {
	    dst = icv_create_with_channels(src->width, src->height, ICV_COLOR_SPACE_RGB, 3);
	    if (!dst)
		return NULL;
	    npix = src->width * src->height;
	    for (i = 0; i < npix; i++) {
		dst->data[i * 3 + 0] = src->data[i * src->channels + 0];
		dst->data[i * 3 + 1] = src->data[i * src->channels + 1];
		dst->data[i * 3 + 2] = src->data[i * src->channels + 2];
	    }
	    return dst;
	}
    }

    return NULL;
}


icv_image_t *
icv_zero(icv_image_t *bif)
{
    double *data;
    size_t size, i;

    ICV_IMAGE_VAL_PTR(bif);

    data = bif->data;
    size = bif->width * bif->height * bif->channels;
    for (i = 0; i < size; i++)
	*data++ = 0;

    return bif;
}


int
icv_destroy(icv_image_t *bif)
{
    ICV_IMAGE_VAL_INT(bif);

    icv_image_data_free(bif, "Image Data");
    if (bif->render_info)
	icv_render_info_destroy(bif->render_info);
    if (bif->flags & ICV_IMAGE_ALLOC_STDLIB)
	free(bif);
    else
	bu_free(bif, "ICV IMAGE Structure");
    return 0;
}


struct icv_render_info *
icv_render_info_create(void)
{
    struct icv_render_info *info;
    BU_ALLOC(info, struct icv_render_info);
    info->db_filename = NULL;
    info->objects = NULL;
    /* zero all numeric fields */
    {
	int i;
	for (i = 0; i < 16; i++)
	    info->viewrotscale[i] = 0.0;
    }
    info->eye_model[0] = info->eye_model[1] = info->eye_model[2] = 0.0;
    info->viewsize = 0.0;
    info->aspect = 1.0;
    info->perspective = 0.0;
    return info;
}


int
icv_render_info_destroy(struct icv_render_info *info)
{
    if (!info)
	return 0;
    if (info->db_filename)
	bu_free(info->db_filename, "render_info db_filename");
    if (info->objects)
	bu_free(info->objects, "render_info objects");
    bu_free(info, "icv_render_info");
    return 0;
}


int
icv_image_set_render_info(icv_image_t *img, struct icv_render_info *info)
{
    if (!img)
	return -1;
    if (img->render_info)
	icv_render_info_destroy(img->render_info);
    img->render_info = info;
    return 0;
}


struct icv_render_info *
icv_image_get_render_info(const icv_image_t *img)
{
    if (!img)
	return NULL;
    return img->render_info;
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
