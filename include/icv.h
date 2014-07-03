/*                           I C V . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
/** @file icv.h
 *
 * Functions provided by the LIBICV image processing library.
 *
 */

#ifndef ICV_H
#define ICV_H

#include "common.h"

__BEGIN_DECLS

#ifndef ICV_EXPORT
#  if defined(ICV_DLL_EXPORTS) && defined(ICV_DLL_IMPORTS)
#    error "Only ICV_DLL_EXPORTS or ICV_DLL_IMPORTS can be defined, not both."
#  elif defined(ICV_DLL_EXPORTS)
#    define ICV_EXPORT __declspec(dllexport)
#  elif defined(ICV_DLL_IMPORTS)
#    define ICV_EXPORT __declspec(dllimport)
#  else
#    define ICV_EXPORT
#  endif
#endif

/** @addtogroup image */
/** @ingroup data */
/** @{ */

typedef enum {
    ICV_IMAGE_AUTO,
    ICV_IMAGE_PIX,
    ICV_IMAGE_BW,
    ICV_IMAGE_DPIX,
    ICV_IMAGE_ALIAS,
    ICV_IMAGE_BMP,
    ICV_IMAGE_CI,
    ICV_IMAGE_ORLE,
    ICV_IMAGE_PNG,
    ICV_IMAGE_PPM,
    ICV_IMAGE_PS,
    ICV_IMAGE_RLE,
    ICV_IMAGE_SPM,
    ICV_IMAGE_SUN,
    ICV_IMAGE_YUV,
    ICV_IMAGE_UNKNOWN
} ICV_IMAGE_FORMAT;

typedef enum {
    ICV_COLOR_SPACE_RGB,
    ICV_COLOR_SPACE_GRAY
    /* Add here for format addition like CMYKA, HSV, others  */
} ICV_COLOR_SPACE;

typedef enum {
    ICV_DATA_DOUBLE,
    ICV_DATA_UCHAR
} ICV_DATA;

/* Define Various Flags */
#define ICV_NULL_IMAGE 0X0001
#define ICV_SANITIZED 0X0002
#define ICV_OPERATIONS_MODE 0x0004
#define ICV_UNDEFINED_1 0x0008

struct icv_image {
    uint32_t magic;
    ICV_COLOR_SPACE color_space;
    double *data;
    float gamma_corr;
    int width, height, channels, alpha_channel;
    uint16_t flags;
};


typedef struct icv_image icv_image_t;
#define ICV_IMAGE_NULL ((struct icv_image *)0)

/**
 * asserts the integrity of a icv_image_file struct.
 */
#define ICV_CK_IMAGE(_i) ICV_CKMAG(_i, ICV_IMAGE_MAGIC, "icv_image")

/**
 * initializes a icv_image_file struct without allocating any memory.
 */
#define ICV_IMAGE_INIT(_i) { \
	    (_i)->magic = ICV_IMAGE_MAGIC; \
	    (_i)->width = (_i)->height = (_i)->channels = (_i)->alpha_channel = 0; \
	    (_i)->gamma_corr = 0.0; \
	    (_i)->data = NULL; \
    }

/**
 * returns truthfully whether a icv_image_file has been initialized.
 */
#define ICV_IMAGE_IS_INITIALIZED(_i) (((struct icv_image *)(_i) != ICV_IMAGE_NULL) && LIKELY((_i)->magic == ICV_IMAGE_MAGIC))

/* Validation Macros */
/**
 * Validates input icv_struct, if failure (in validation) returns -1
 */
#define ICV_IMAGE_VAL_INT(_i)  if (!ICV_IMAGE_IS_INITIALIZED(_i)) return -1

/**
 * Validates input icv_struct, if failure (in validation) returns NULL
 */
#define ICV_IMAGE_VAL_PTR(_i) if (!ICV_IMAGE_IS_INITIALIZED(_i)) return NULL


/* Data conversion MACROS  */
/**
 * Converts to double (icv data) type from unsigned char(8bit).
 */
#define ICV_CONV_8BIT(data) ((double)(data))/255.0

/** @file libicv/fileformat.c
 *
 * image read/write routines
 *
 * read/write images in a variety of formats.
 *
 */

/**
 * Finds the Image format based on heuristics depending on the file
 * name.
 * @param filename Filename of the image whose format is to be  know
 * @param trimmedname Buffer for storing filename after removing
 * extensions
 * @return File Format
 */
ICV_EXPORT extern ICV_IMAGE_FORMAT icv_guess_file_format(const char *filename, char *trimmedname);

/**
 * This function allocates memory for an image and returns the
 * resultant image.
 *
 * @param width Width of the image to be created
 * @param height Height of the image to be created
 * @param color_space Color space of the image (RGB, grayscale)
 * @return Image structure with allocated space and zeroed data array
 */
ICV_EXPORT extern icv_image_t *icv_create(int width, int height, ICV_COLOR_SPACE color_space);

/**
 * Write an image line to the data of ICV struct. Can handle unsigned
 * char buffers.
 *
 * Note : This function requires memory allocation for ICV_UCHAR_DATA,
 * which in turn acquires BU_SEM_SYSCALL semaphore.
 *
 * @param bif ICV struct where data is to be written
 * @param y Index of the line at which data is to be written. 0 for
 * the first line
 * @data Line Data to be written
 * @type Type of data, e.g., uint8 data specify ICV_DATA_UCHAR or 1
 * @return on success 0, on failure -1
 */
ICV_EXPORT int icv_writeline(icv_image_t *bif, int y, void *data, ICV_DATA type);

/**
 * Writes a pixel to the specified coordinates in the data of ICV
 * struct.
 *
 * @param bif ICV struct where data is to be written
 * @param x x-dir coordinate of the pixel
 * @param y y-dir coordinate of the pixel. (0,0) coordinate is taken
 * as bottom left
 * @data Data to be written
 * @return on success 0, on failure -1
 */
ICV_EXPORT int icv_writepixel(icv_image_t *bif, int x, int y, double *data);

/**
 * Saves Image to a file or streams to stdout in respective format
 *
 * To stream it to stdout pass NULL pointer for filename.
 *
 * @param bif Image structure of file.
 * @param filename Filename of the file to be written.
 * @param format Specific format of the file to be written.
 * @return on success 0, on failure -1 with log messages.
 */
ICV_EXPORT extern int icv_write(icv_image_t *bif, const char*filename, ICV_IMAGE_FORMAT format);

/**
 * Load a file into an ICV struct. For most formats, this will be
 * called with format=ICV_IMAGE_AUTO.
 *
 * The data is packed in icv_image struct in double format with varied
 * channels as per the specification of image to be loaded.
 *
 * To read stream from stdin pass NULL pointer for filename.
 *
 * In case of bw and pix image if size is unknown pass 0 for width and
 * height. This will read the image till EOF is reached. The image
 * size of the output image will be : height = 1; width = size; where
 * size = total bytes read
 *
 * @param filename File to read
 * @param hint_format Probable format of the file, typically
 * ICV_IMAGE_AUTO
 * @param hint_width Width when passed as parameter from calling
 * program.
 * @param hint_height Height when passed as parameter from calling
 * program.
 * @return A newly allocated struct holding the loaded image info.
 */
ICV_EXPORT extern icv_image_t *icv_read(const char *filename, int format, int width, int height);

/**
 * This function zeroes all the data entries of an image
 * @param img Image Structure
 */
ICV_EXPORT extern icv_image_t *icv_zero(icv_image_t *bif);

/**
 * This function frees the allocated memory for a ICV Structure and
 * data.
 */
ICV_EXPORT extern int icv_destroy(icv_image_t *bif);

/** @file libicv/color_space.c
 *
 * This file contains routines to change the icv image from one
 * colorspace to other.
 *
 */

/**
 * Converts a single channel image to three channel image.  Replicates
 * the pixel as done by bw-pix utility returns a three channel image.
 * If a three channel image is passed, this function returns the same
 * image.
 */
ICV_EXPORT int icv_gray2rgb(icv_image_t *img);

typedef enum {
    ICV_COLOR_RGB,
    ICV_COLOR_R,
    ICV_COLOR_G,
    ICV_COLOR_B,
    ICV_COLOR_RG,
    ICV_COLOR_RB,
    ICV_COLOR_BG
} ICV_COLOR;

/**
 * converts image to single channel image by combining three weights
 * based on NTSC primaries and 6500 white.
 */
#define icv_rgb2gray_ntsc(_a) icv_rgb2gray(_a, ICV_COLOR_RGB, 0.30, 0.59, 0.11)

/**
 * converts image to single channel image by combining three weights
 * based on CRT phosphor and D6500 white.
 */
#define icv_rgb2gray_crt(_a) icv_rgb2gray(_a, ICV_COLOR_RGB, 0.26, 0.66, 0.08)

/**
 * converts a three channel rgb image to single channel gray-image.
 * This function will combine or select planes of the image based on
 * the input arguments.
 *
 * A normal calling of this functions is as follows:
 * icv_image_rgb2gray(bif, 0, 0, 0, 0); where bif is the rgb image
 * to be converted.
 *
 * @param color Chooses color planes to be selected for combination.
 * This function will need color to be specified from
 *              ICV_COLOR_R
 *              ICV_COLOR_G
 *              ICV_COLOR_B
 *              ICV_COLOR_RG
 *              ICV_COLOR_RB
 *              ICV_COLOR_BG
 *              ICV_COLOR_RGB
 * @param rweight Weight for r-plane
 * @param gweight Weight for g-plane
 * @param bweight Weight for b-plane
 * @return 0 on success; on failure return 1
 *
 * User can specify weights in the arguments, for the selected color
 * planes. If 0 weight is chosen this utility assigns equal weights.
 *
 */
ICV_EXPORT int icv_rgb2gray(icv_image_t *img,
				  ICV_COLOR color,
				  double rweight,
				  double gweight,
				  double bweight);

/** @file libicv/crop.c
 *
 * This file contains functions for cropping images.
 * There are two types of cropping: rectangular and skeyed.
 */

/**
 * This function crops an input image.
 * Note : (0,0) corresponds to the Bottom Left of an Image.
 *
 * @param img Input image struct to be cropped.
 * @param xorig X-Coordinate of offset of image to be extracted from.
 * @param yorig Y-Coordinate of offset of image to be extracted from.
 * @param xnum Length of the output image to be extracted from input
 * data in horizontal direction.
 * @param ynum Length of the output image to be extracted from input
 * data in vertical direction.
 * @return 0 on success.
 */
ICV_EXPORT extern int icv_rect(icv_image_t *img, int xorig, int yorig, int xnum, int ynum);

/**
 * This function crops an input image.
 *
 * This can do a screwed cropping, i.e. given any four points of
 * quadrilateral in an image, map it to a rectangle of xnumXynum
 * dimension.
 *
 *        (ulx,uly)         (urx,ury)
 *             __________________
 *            /                 |
 *           /                  |
 *          /                   |
 *         /                    |
 *        /                     |
 *       /______________________|
 *     (llx,lly)             (lrx,lry)
 *
 * @return 0 on success; on failure -1; and logs the error message.
 */
ICV_EXPORT extern int icv_crop(icv_image_t *img,
			       int ulx, int uly,
			       int urx, int ury,
			       int lrx, int lry,
			       int llx, int lly,
			       unsigned int ynum,
			       unsigned int xnum);

/** @file libicv/operations.c
 *
 * This file contains routines for operations.
 *
 */

/**
 * This function sanitizes the image.
 *
 * It forces the image pixels to be in the prescribed range.
 *
 * All the pixels higher than the max range are set to MAX (1.0).
 * All the pixels lower than the min range are set to MIN (0.0).
 *
 * Note if an image(bif) is sanitized then,
 * (bif->flags&&ICV_SANITIZED)  is true.
 *
 */
ICV_EXPORT int icv_sanitize(icv_image_t* img);

/**
 * This adds a constant value to all the pixels of the image.  Also if
 * the flag ICV_OPERATIONS_MODE is set this doesn't sanitize the
 * image.
 *
 * Note to set the flag for a bif (icv_image struct);
 * bif->flags |= ICV_OPERATIONS_MODE;
 *
 */
ICV_EXPORT int icv_add_val(icv_image_t* img, double val);

/**
 * This multiplies all the pixels of the image with a constant Value.
 * Also if the flag ICV_OPERATIONS_MODE is set this doesn't sanitize
 * the image.
 */
ICV_EXPORT int icv_multiply_val(icv_image_t* img, double val);

/**
 * This divides all the pixels of the image with a constant Value.
 * Also if the flag ICV_OPERATIONS_MODE is set this doesn't sanitize
 * the image.
 */
ICV_EXPORT int icv_divide_val(icv_image_t* img, double val);

/**
 * This raises all the pixels of the image to a constant exponential
 * power.  Also if the flag ICV_OPERATIONS_MODE is set this doesn't
 * sanitize the image.
 */
ICV_EXPORT int icv_pow_val(icv_image_t* img, double val);

/**
 * This routine adds pixel value of one image to pixel value of other
 * pixel and inserts in the same index of the output image.
 *
 * Also it sanitizes the image.
 */
ICV_EXPORT icv_image_t *icv_add(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine subtracts pixel value of one image from pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 - img2)
 *
 */
ICV_EXPORT icv_image_t *icv_sub(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine multiplies pixel value of one image to pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 * img2)
 *
 */
ICV_EXPORT icv_image_t *icv_multiply(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine divides pixel value of one image from pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 / img2)
 *
 */
ICV_EXPORT icv_image_t *icv_divide(icv_image_t *img1, icv_image_t *img2);

/**
 * Change the saturation of image pixels.  If sat is set to 0.0 the
 * result will be monochromatic, if sat is made 1.0, the color will
 * not change, if sat is made greater than 1.0, the amount of color is
 * increased.
 *
 * @param img RGB Image to be saturated.
 * @param sat Saturation value.
 */
ICV_EXPORT int icv_saturate(icv_image_t* img, double sat);


/** @file libicv/filter.c
 *
 * This file contains routines for image filtering. This is done
 * mainly using the convolution of images. Both Gray Scale and RGB
 * images are taken care.
 *
 */


typedef enum {
    ICV_FILTER_LOW_PASS,
    ICV_FILTER_LAPLACIAN,
    ICV_FILTER_HORIZONTAL_GRAD,
    ICV_FILTER_VERTICAL_GRAD,
    ICV_FILTER_HIGH_PASS,
    ICV_FILTER_NULL,
    ICV_FILTER_BOXCAR_AVERAGE
} ICV_FILTER;

typedef enum {
    ICV_FILTER3_LOW_PASS,
    ICV_FILTER3_HIGH_PASS,
    ICV_FILTER3_BOXCAR_AVERAGE,
    ICV_FILTER3_ANIMATION_SMEAR,
    ICV_FILTER3_NULL
} ICV_FILTER3;

/**
 * Filters an image with the specified filter type. Basically
 * convolves kernel with the image.  Does zero_padding for outbound
 * pixels.
 *
 * @param img Image to be filtered.
 * @param filter_type Type of filter to be used.
 *
 */
ICV_EXPORT extern int icv_filter(icv_image_t *img, ICV_FILTER filter_type);


/**
 * Filters a set of three image with the specified filter type.  Does
 * zero_padding for outbound pixels.  Finds the resultant pixel with
 * the help of neighboring pixels in all the three images.
 *
 *
 * @return Resultant image.
 *
 */
ICV_EXPORT extern icv_image_t *icv_filter3(icv_image_t *old_img,
					       icv_image_t *curr_img,
					       icv_image_t *new_img,
					       ICV_FILTER3 filter_type);


/**
 * icv_fade will darken a pix by a certain fraction.
 *
 * Fades an image in place.
 *
 * @param img ICV Image to be faded.
 * @param fraction should be between 0 to 1. Amount by which the image
 * is needed to faded.
 */
ICV_EXPORT extern int icv_fade(icv_image_t *img, double fraction);

/** @file libicv/stat.c
 *
 * This file contains image statistics and histogram routines.
 *
 */


/**
 * This function calculates the histogram of different channels
 * separately.
 *
 * @param img Image of which histogram is to found.
 * @param n_bins number of bins required.
 * @return Histogram of size_t type array. This 2-dimension array
 * is of size c X n_bins where c is the channels in the image.
 *
 */
ICV_EXPORT size_t **icv_hist(icv_image_t* img, int n_bins);

/**
 * Finds the minimum value in each channel of the image.
 *
 * @return a double array of size channels. Each element contains min
 * value of the channel.
 *
 * e.g. min = icv_min(bif);
 * min[0] gives the minimum value of all the pixels in first bin.
 * and so on.
 *
 */
ICV_EXPORT double *icv_min(icv_image_t* img);

/**
 * Finds the average value in each channel of the image.
 *
 * @return a double array of size channels. Each elements contains
 * average value of the channel.
 *
 * e.g. mean = icv_mean(bif);
 * mean[0] gives the average value of all the pixels in first channel
 * and so on.
 *
 */
ICV_EXPORT double *icv_mean(icv_image_t* img);

/**
 * Finds the sum of all the pixel values for each channel of the image
 *
 * @return a double array of size channels. Each element contains sum
 * value of the channel.
 *
 * e.g. sum = icv_sum(bif);
 * sum[0] gives the sum of all the pixels in first channel
 * and so on.
 *
 */
ICV_EXPORT double *icv_sum(icv_image_t* img);

/**
 * Finds the max value in each channel of the image.
 *
 * @return a double array of size channels. Each element contains max
 * value of the channel.
 *
 * e.g. max = icv_max(bif);
 * max[0] gives the maximum value of all the pixels in first bin.
 * and so on.
 *
 */
ICV_EXPORT double *icv_max(icv_image_t* img);

/**
 * Calculates mode of the values of each channel.
 * Mode value are calculated for quantified data which is sent as
 * bins(histogram Information). For any image mode is a 'c' length
 * array where c is the number of channels.
 *
 * To calculate the mode of an icv_image, a default call is as follows
 *   icv_mode(img, icv_hist(img, n_bins), n_bins);
 *
 * This call first calculates the histogram of the image. then finds
 * the mode values from histogram of each channel.
 *
 */
ICV_EXPORT int *icv_mode(icv_image_t* img, size_t** bins, int n_bins);

/**
 * Calculates median of the values of each channel.
 * Median value are calculated for quantified data which is sent as
 * bins(histogram information). For any image mode is a 'c' length
 * array, where c is the number of channels.
 *
 * To calculate the median of an icv_image, a default call is as
 * follows :
 *    icv_median(img, icv_hist(img, n_bins), n_bins);
 *
 * This call first calculates the histogram of the image. then finds
 * the mode values from histogram of each channel.
 *
 */
ICV_EXPORT int *icv_median(icv_image_t* img, size_t** bins, int n_bins);

/**
 * Calculates the skewness in data.
 *
 * To calculate the skewness in an icv_image, a default call is as
 * follows :
 *   icv_skew(img, icv_hist(img, n_bins), n_bins);
 *
 * @return c length double array where c is the number of channels in
 * the img
 */
ICV_EXPORT double *icv_skew(icv_image_t* img, size_t** bins, int n_bins);

/**
 * Calculates the variance in data.
 *
 * To calculate the variance in an icv_image, a default call is as
 * follows :
 *    icv_variance(img, icv_hist(img, n_bins), n_bins);
 *
 * @return c length double array where c is the number of channels in
 * the img
 */
ICV_EXPORT double *icv_var(icv_image_t* img, size_t** bins, int n_bins);


/** @file size.c
 *
 * This file contains routines to scale down an image to a lower
 * resolution or scale up an image to an higher Resolution.
 *
 */

typedef enum {
    ICV_RESIZE_UNDERSAMPLE,
    ICV_RESIZE_SHRINK,
    ICV_RESIZE_NINTERP,
    ICV_RESIZE_BINTERP
} ICV_RESIZE_METHOD;

/**
 * This function resizes the given input image.
 * Mode of usage:
 * a) ICV_RESIZE_UNDERSAMPLE : This method undersamples the said image
 * e.g. icv_resize(bif, ICV_RESIZE_UNDERSAMPLE, 0, 0, 2);
 *  undersamples the image with a factor of 2.
 *
 * b) ICV_RESIZE_SHRINK : This Shrinks the image, keeping the light
 * energy per square area as constant.
 * e.g. icv_resize(bif, ICV_RESIZE_SHRINK,0,0,2);
 *  shrinks the image with a factor of 2.
 *
 * c) ICV_RESIZE_NINTERP : This interpolates using nearest neighbor
 * method.
 * e.g. icv_resize(bif, ICV_RESIZE_NINTERP,1024,1024,0);
 *  interpolates the output image to have the size of 1024X1024.
 *
 * d) ICV_RESIZE_BINTERP : This interpolates using bilinear
 * Interpolation Method.
 * e.g. icv_resize(bif, ICV_RESIZE_BINTERP,1024,1024,0);
 *  interpolates the output image to have the size of 1024X1024.
 *
 * resizes the image inplace.
 *
 * @param bif Image (packed in icv_image struct)
 * @param method One of the modes.
 * @param out_width Out Width.
 * @param out_height Out Height.
 * @param factor Integer type data representing the factor to be
 * shrunken
 * @return 0 on success and -1 on failure.
 */
ICV_EXPORT int icv_resize(icv_image_t *bif, ICV_RESIZE_METHOD method, unsigned int out_width, unsigned int out_height, unsigned int factor);

/** @} */
/* end image utilities */

/**
 * Rotate an image.
 * %s [-rifb | -a angle] [-# bytes] [-s squaresize] [-w width] [-n height] [-o outputfile] inputfile [> outputfile]
 *
 */
ICV_EXPORT extern int icv_rot(int argv, char **argc);

__END_DECLS

#endif /* ICV_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
