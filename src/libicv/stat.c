/*                          S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file libicv/stat.c
 *
 * This file contains image statistics and histogram routines.
 *
 */

#include "bu.h"
#include "icv.h"

HIDDEN size_t **
icv_init_bins(icv_image_t* img, int n_bins)
{
    int c;
    int i;
    size_t **bins;

    bins = (size_t**) bu_malloc(sizeof(size_t*)*img->channels, "icv_init_bins : Histogram Bins");
    for (c = 0; c <= img->channels; c++) {
	bins[c] = (size_t*) bu_malloc(sizeof(size_t)*n_bins, "icv_init_bins : Histogram Array for Channels");
	for (i=0; i<n_bins; i++) {
	    bins[c][i] = 0;
	}
    }
    return bins;
}

size_t **
icv_hist(icv_image_t* img, int n_bins)
{
    long int i;
    int j;
    double *data;
    int temp;
    long int size;
    size_t **bins;
    size = img->width*img->height;
    data = img->data;

    ICV_IMAGE_VAL_PTR(img);

    bins = icv_init_bins(img, n_bins);

    for (i=0; i<=size; i++) {
	for (j=0; j < img->channels; j++) {
	    temp = (*data++)*n_bins;
	    bins[j][temp]++;
	}
    }
    return bins;
}

double *icv_max(icv_image_t* img)
{
    double *data = NULL;
    size_t size;
    double *max; /**< An array of size channels. */
    int i;

    ICV_IMAGE_VAL_PTR(img);

    max = (double *)bu_malloc(sizeof(double)*img->channels, "max values");

    for (i=0; i<img->channels; i++)
	max[i] = 0.0;

    data = img->data;

    for (size = img->width*img->height; size>0; size--)
	for (i=0; i<img->channels; i++)
	    if (max[i] > *data++)
		max[i] = *(data-1);

    return max;
}

double *icv_sum(icv_image_t* img)
{
    double *data = NULL;

    double *sum; /**< An array of size channels. */
    int i;
    size_t size,j;

    ICV_IMAGE_VAL_PTR(img);

    sum = (double *)bu_malloc(sizeof(double)*img->channels, "sum values");

    for (i=0; i<img->channels; i++)
	sum[i] = 0.0;

    data = img->data;
    size = (size_t)img->width*img->height;

    for (j=0; j<size; j++)
	for (i=0; i<img->channels; i++)
	    sum[i] += *data++;

    return sum;
}

double *icv_mean(icv_image_t* img)
{
    double *mean;
    size_t size;
    int i;

    ICV_IMAGE_VAL_PTR(img);

    mean = icv_sum(img); /**< receives sum from icv_image_sum*/
    size = (size_t)img->width*img->height;

    for (i=0; i<img->channels; i++)
	mean[i]/=size;

    return mean;
}

double *icv_min(icv_image_t* img)
{
    double *data = NULL;
    size_t size;
    double *min; /**< An array of size channels. */
    int i;

    ICV_IMAGE_VAL_PTR(img);

    min = (double *)bu_malloc(sizeof(double)*img->channels, "min values");

    for (i=0; i<img->channels; i++)
	min[i] = 1.0;

    data = img->data;

    for (size = (size_t)img->width*img->height; size>0; size--) {
	for (i=0; i<img->channels; i++)
	    if (min[i] < *data++)
		min[i] = *(data-1);
    }

    return min;
}

double *icv_var(icv_image_t* img, size_t** bins, int n_bins)
{
    int i,c;
    double *var;
    double *mean;
    size_t size;
    double d;

    ICV_IMAGE_VAL_PTR(img);

    var = (double *) bu_malloc(sizeof(double)*img->channels, "variance values");

    size = (size_t) img->height*img->width;

    mean = icv_mean(img);
    for (i=0; i < n_bins; i++) {
	for (c=0 ; c<img->channels; c++) {
	    d = (double)i - n_bins*mean[c];
	    var[c] += bins[c][i] * d * d;
	}
    }

    for (c=0 ; c<img->channels; c++) {
	var[c] /= size;
    }

    return var;
}

double *icv_skew(icv_image_t* img, size_t** bins, int n_bins)
{
    int i,c;
    double *skew;
    double *mean;
    size_t size;
    double d;

    ICV_IMAGE_VAL_PTR(img);

    skew = (double *)bu_malloc(sizeof(double)*img->channels, "skewness values");

    size = (size_t) img->height*img->width;

    mean = icv_mean(img);
    for (i=0; i < n_bins; i++) {
	for (c=0 ; c < img->channels; c++) {
	    d = (double)i - n_bins*mean[c];
	    skew[c] += bins[c][i] * d * d *d;
	}
    }

    for (c=0 ; c < img->channels; c++) {
	skew[c] /= size;
    }

    return skew;
}

int *icv_median(icv_image_t* img, size_t** bins, int n_bins)
{
    int i,c;
    int *median;
    double *sum;
    double *partial_sum;

    ICV_IMAGE_VAL_PTR(img);

    median = (int *)bu_malloc(sizeof(int)*img->channels, "median values");
    partial_sum = (double *)bu_malloc(sizeof(int)*img->channels, "partial sum values");

    sum = icv_sum(img);

    for (c=0; c<img->channels; c++) {
	partial_sum[c] = 0;
	for (i=0; i < n_bins; i++) {
	    if (partial_sum[c] < sum[c]/2) {
		partial_sum[c] += i*bins[c][i];
		median[c] = i;
	    } else break;
	}
    }

    bu_free(partial_sum, "icv_median : partial sum values\n");

    return median;
}

int *icv_mode(icv_image_t* img, size_t** bins, int n_bins)
{
    int i,c;
    int *mode;

    ICV_IMAGE_VAL_PTR(img);

    mode = (int *) bu_malloc(sizeof(int)*img->channels, "mode values");

    for (c=0; c < img->channels; c++) {
	mode[c] = 0;
	for (i=0; i < n_bins; i++)
	    if (bins[c][mode[c]] < bins[c][i])
		mode[c] = i;
    }
    return mode;
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
