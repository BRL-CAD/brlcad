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
	for(i=0; i<n_bins; i++) {
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

    bins = icv_init_bins(img, n_bins);

    for(i=0; i<=size; i++) {
	for(j=0; j < img->channels; j++) {
	    temp = (*data++)*n_bins;
	    bins[j][temp]++;
	}
    }
    return bins;
}

double *icv_image_max(icv_image_t* img)
{
    double *data = NULL;
    size_t size;
    double *max; /**< An array of size channels. */
    int i;

    max = bu_malloc(sizeof(double)*img->channels, "max values");

    for(i=0; i<img->channels; i++)
        max[i] = 0.0;

    data = img->data;

    for (size = img->width*img->height; size>0; size--)
        for (i=0; i<img->channels; i++)
            if (max[i] > *data++)
                max[i] = *(data-1);

    return max;
}

double *icv_image_sum(icv_image_t* img)
{
    double *data = NULL;
   
    double *sum; /**< An array of size channels. */
    int i;
    size_t size,j;
    sum = bu_malloc(sizeof(double)*img->channels, "sum values");

    for(i=0; i<img->channels; i++)
        sum[i] = 0.0;

    data = img->data;
    size = (size_t)img->width*img->height;
    
    for (j=0; j<size; j++)
        for (i=0; i<img->channels; i++)
            sum[i] += *data++;
            
    return sum;
}

double *icv_image_mean(icv_image_t* img)
{
    double *mean; 
    size_t size;
    int i;

    mean = icv_image_sum(img); /**< recieves sum from icv_image_sum*/
    size = (size_t)img->width*img->height;  
           
    for(i=0; i<img->channels; i++)
        mean[i]/=size;
    
    return mean;
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
