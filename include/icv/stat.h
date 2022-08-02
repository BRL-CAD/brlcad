/*                          S T A T . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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
/** @addtogroup icv_stat
 *
 * Image statistics and histogram routines.
 *
 */

#ifndef ICV_STAT_H
#define ICV_STAT_H

#include "common.h"
#include <stddef.h> /* for size_t */
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/stat.h */

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
ICV_EXPORT size_t **icv_hist(icv_image_t* img, size_t n_bins);

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
ICV_EXPORT int *icv_mode(icv_image_t* img, size_t** bins, size_t n_bins);

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
ICV_EXPORT int *icv_median(icv_image_t* img, size_t** bins, size_t n_bins);

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
ICV_EXPORT double *icv_skew(icv_image_t* img, size_t** bins, size_t n_bins);

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
ICV_EXPORT double *icv_var(icv_image_t* img, size_t** bins, size_t n_bins);

/** @} */

__END_DECLS

#endif /* ICV_STAT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
