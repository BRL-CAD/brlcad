/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * buildmap.c - Build a color map from the RLE file color map.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sat Jan 24 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include <math.h>

/*****************************************************************
 * TAG( buildmap )
 * 
 * Returns a color map that can easily be used to map the pixel values in
 * an RLE file.  Map is built from the color map in the input file.
 * Inputs:
 * 	the_hdr:	rle_hdr structure containing color map.
 *	minmap:		Minimum number of channels in output map.
 *	orig_gamma:	Adjust color map for this image gamma value
 *			(1.0 means no adjustment).
 *	new_gamma:	Gamma of new display.
 * Outputs:
 * 	Returns an array of pointers to arrays of rle_pixels.  The array
 *	of pointers contains max(ncolors, ncmap) elements, each 
 *	array of pixels contains 2^cmaplen elements.  The pixel arrays
 *	should be considered read-only.
 * Assumptions:
 * 	[None]
 * Algorithm:
 *	Ensure that there are at least ncolors rows in the map, and
 *	that each has at least 256 elements in it (largest map that can
 *	be addressed by an rle_pixel).
 */
rle_pixel **
buildmap( the_hdr, minmap, orig_gamma, new_gamma )
rle_hdr *the_hdr;
int minmap;
double orig_gamma;
double new_gamma;
{
    rle_pixel ** cmap, * gammap;
    double gamma;
    register int i, j;
    int maplen, cmaplen, nmap;

    if ( the_hdr->ncmap == 0 )	/* make identity map */
    {
	nmap = (minmap < the_hdr->ncolors) ? the_hdr->ncolors : minmap;
	cmap = (rle_pixel **)malloc( nmap * sizeof(rle_pixel *) );
	cmap[0] = (rle_pixel *)malloc( nmap * 256 * sizeof(rle_pixel) );
	for ( j = 1; j < nmap; j++ )
	    cmap[j] = cmap[j-1] + 256;
	for ( i = 0; i < 256; i++ )
	    for ( j = 0; j < nmap; j++ )
		cmap[j][i] = i;
	maplen = 256;
    }
    else			/* make map from the_hdr */
    {
	/* Map is at least 256 long */
	cmaplen = (1 << the_hdr->cmaplen);
	if ( cmaplen < 256 )
	    maplen = 256;
	else
	    maplen = cmaplen;

	/* Nmap is max( minmap, the_hdr->ncmap, the_hdr->ncolors ). */
	nmap = minmap;
	if ( nmap < the_hdr->ncmap )
	    nmap = the_hdr->ncmap;
	if ( nmap < the_hdr->ncolors )
	    nmap = the_hdr->ncolors;
	
	/* Allocate memory for the map and secondary pointers. */
	cmap = (rle_pixel **)malloc( nmap * sizeof(rle_pixel *) );
	cmap[0] = (rle_pixel *)malloc( nmap * maplen * sizeof(rle_pixel) );
	for ( i = 1; i < nmap; i++ )
	    cmap[i] = cmap[0] + i * maplen;
	
	/* Fill it in. */
	for ( i = 0; i < maplen; i++ )
	{
	    for ( j = 0; j < the_hdr->ncmap; j++ )
		if ( i < cmaplen )
		    cmap[j][i] = the_hdr->cmap[j*cmaplen + i] >> 8;
		else
		    cmap[j][i] = i;
	    for ( ; j < nmap; j++ )
		cmap[j][i] = cmap[j-1][i];
	}
    }
    
    /* Gamma compensate if requested */
    if ( orig_gamma == 0 )
    {
	char *v;
	if ( (v = rle_getcom( "image_gamma", the_hdr )) != NULL )
	{
	    orig_gamma = atof( v );
	    /* Protect against bogus information */
	    if ( orig_gamma == 0.0 )
		orig_gamma = 1.0;
	    else
		orig_gamma = 1.0 / orig_gamma;
	}
	else if ( (v = rle_getcom( "display_gamma", the_hdr )) != NULL)
	{
	    orig_gamma = atof( v );
	    /* Protect */
	    if ( orig_gamma == 0.0 )
		orig_gamma = 1.0;
	}
	else
	    orig_gamma = 1.0;
    }

    /* Now, compensate for the gamma of the new display, too. */
    if ( new_gamma != 0.0 )
	gamma = orig_gamma / new_gamma;
    else
	gamma = orig_gamma;

    if ( gamma != 1.0 )
    {
	gammap = (rle_pixel *)malloc( 256 * sizeof(rle_pixel) );
	for ( i = 0; i < 256; i++ )
	    gammap[i] = (int)(0.5 + 255.0 * pow( i / 255.0, gamma ));
	for ( i = 0; i < nmap; i++ )
	    for ( j = 0; j < maplen; j++ )
		cmap[i][j] = gammap[cmap[i][j]];
	free( gammap );
    }

    return cmap;
}
