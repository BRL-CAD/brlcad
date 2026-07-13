/*                  I C V _ P R I V A T E . H
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
#include "bu/mime.h"
#include "bio.h" /* for O_BINARY */
#include "icv.h"

#ifndef ICV_PRIVATE_H
#define ICV_PRIVATE_H

__BEGIN_DECLS

/* Internal image allocation helpers.  icv_create is the public
 * color-space constructor; these preserve exact channel layouts for
 * internal operations that must not silently drop alpha data.
 */
extern icv_image_t *icv_create_with_channels(size_t width, size_t height, ICV_COLOR_SPACE color_space, size_t channels);
extern icv_image_t *icv_clone(const icv_image_t *src);
extern icv_image_t *icv_image_for_write(const icv_image_t *src, ICV_COLOR_SPACE color_space, size_t channels);
extern void icv_image_data_free(icv_image_t *img, const char *label);
extern void icv_image_data_set_bu(icv_image_t *img, double *data);
extern void icv_image_data_set_stdlib(icv_image_t *img, double *data);
extern int icv_image_data_realloc(icv_image_t *img, size_t size, const char *label);

/* defined in bw.c */
extern icv_image_t *bw_read(FILE *fp, size_t width, size_t height);
extern icv_image_t *bw_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height);
extern int bw_write(icv_image_t *bif, FILE *fp);
extern int bw_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

/* defined in pix.c */
extern icv_image_t *pix_read(FILE *fp, size_t width, size_t height);
extern icv_image_t *pix_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height);
extern int pix_write(icv_image_t *bif, FILE *fp);
extern int pix_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

/* defined in dpix.c */
extern icv_image_t *dpix_read(FILE *fp, size_t width, size_t height);
extern icv_image_t *dpix_read_mem(const unsigned char *buffer, size_t size, size_t width, size_t height);
extern int dpix_write(icv_image_t *bif, FILE *fp);
extern int dpix_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

/* defined in jpeg.c */
extern icv_image_t *jpeg_read(FILE *fp);
extern icv_image_t *jpeg_read_mem(const unsigned char *buffer, size_t size);
extern int jpeg_write(icv_image_t *bif, FILE *fp, int quality);
extern int jpeg_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize, int quality);

/* defined in png.cpp */
extern icv_image_t* png_read(FILE *fp);
extern icv_image_t* png_read_mem(const unsigned char *buffer, size_t size);
extern int png_write(icv_image_t *bif, FILE *fp);
extern int png_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

/* defined in ppm.c */
extern icv_image_t* ppm_read(FILE *fp);
extern icv_image_t* ppm_read_mem(const unsigned char *buffer, size_t size);
extern int ppm_write(icv_image_t *bif, FILE *fp);
extern int ppm_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

/* defined in rle.cpp */
ICV_EXPORT extern icv_image_t* rle_read(FILE *fp);
ICV_EXPORT extern icv_image_t* rle_read_mem(const unsigned char *buffer, size_t size);
ICV_EXPORT extern int rle_write(icv_image_t *bif, FILE *fp);
ICV_EXPORT extern int rle_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize);

__END_DECLS

#endif /* ICV_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
