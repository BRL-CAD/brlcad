/*                        S P H M A P . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbn */
/*@{*/

/** @file sphmap.c
 *  Common Subroutines for Spherical Data Structures/Texture Maps Subroutines
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "spm.h"

/*
 *		S P M _ I N I T
 *
 *  Return a sphere map structure initialized for N points around
 *  the equator.  Malloc the storage and fill in the pointers.
 *  This code leaves a ring of "triangular" pixels at the poles.
 *  An alternative would be to have the pole region map to a
 *  single pixel.
 *
 *  Returns SPM_NULL on error.
 */
spm_map_t *
spm_init(int N, int elsize)
{
	int	i, nx, total, index;
	register spm_map_t *mapp;

	mapp = (spm_map_t *)bu_malloc( sizeof(spm_map_t), "spm_map_t");
	if( mapp == SPM_NULL )
		return( SPM_NULL );
	bzero( (char *)mapp, sizeof(spm_map_t) );

	mapp->elsize = elsize;
	mapp->ny = N/2;
	mapp->nx = (int *) bu_malloc( (unsigned)(N/2 * sizeof(*(mapp->nx))), "sph nx" );
	if( mapp->nx == NULL ) {
		spm_free( mapp );
		return( SPM_NULL );
	}
	mapp->xbin = (unsigned char **) bu_malloc( (unsigned)(N/2 * sizeof(char *)), "sph xbin" );
	if( mapp->xbin == NULL ) {
		spm_free( mapp );
		return( SPM_NULL );
	}

	total = 0;
	for( i = 0; i < N/4; i++ ) {
		nx = ceil( N*cos( i*bn_twopi/N ) );
		if( nx > N ) nx = N;
		mapp->nx[ N/4 + i ] = nx;
		mapp->nx[ N/4 - i -1 ] = nx;

		total += 2*nx;
	}

	mapp->_data = (unsigned char *) bu_calloc( (unsigned)total, elsize, "spm_init data" );
	if( mapp->_data == NULL ) {
		spm_free( mapp );
		return( SPM_NULL );
	}

	index = 0;
	for( i = 0; i < N/2; i++ ) {
		mapp->xbin[i] = &((mapp->_data)[index]);
		index += elsize * mapp->nx[i];
	}
	mapp->magic = SPM_MAGIC;
	return( mapp );
}

/*
 *		S P M _ F R E E
 *
 *  Free the storage associated with a sphere structure.
 */
void
spm_free(spm_map_t *mp)
{
	RT_CK_SPM(mp);
	if( mp == SPM_NULL )
		return;

	if( mp->_data != NULL )  {
		(void) bu_free( (char *)mp->_data, "sph _data" );
		mp->_data = NULL;
	}

	if( mp->nx != NULL )  {
		(void) bu_free( (char *)mp->nx, "sph nx" );
		mp->nx = NULL;
	}

	if( mp->xbin != NULL )  {
		(void) bu_free( (char *)mp->xbin, "sph xbin" );
		mp->xbin = NULL;
	}

	(void) bu_free( (char *)mp, "spm_map_t" );
}

/*
 *		S P M _ R E A D
 *
 *  Read the value of the pixel at the given normalized (u,v)
 *  coordinates.  It does NOT check the sanity of the coords.
 *
 *  0.0 <= u < 1.0	Left to Right
 *  0.0 <= v < 1.0	Bottom to Top
 */
void
spm_read(register spm_map_t *mapp, register unsigned char *valp, double u, double v)
{
	int	x, y;
	register unsigned char *cp;
	register int	i;

	RT_CK_SPM(mapp);

	y = v * mapp->ny;
	x = u * mapp->nx[y];
	cp = &(mapp->xbin[y][x*mapp->elsize]);

	i = mapp->elsize;
	while( i-- > 0 ) {
		*valp++ = *cp++;
	}
}

/*
 *		S P M _ W R I T E
 *
 *  Write the value of the pixel at the given normalized (u,v)
 *  coordinates.  It does NOT check the sanity of the coords.
 *
 *  0.0 <= u < 1.0	Left to Right
 *  0.0 <= v < 1.0	Bottom to Top
 */
void
spm_write(register spm_map_t *mapp, register unsigned char *valp, double u, double v)
{
	int	x, y;
	register unsigned char *cp;
	register int	i;

	RT_CK_SPM(mapp);

	y = v * mapp->ny;
	x = u * mapp->nx[y];
	cp = &(mapp->xbin[y][x*mapp->elsize]);

	i = mapp->elsize;
	while( i-- > 0 ) {
		*cp++ = *valp++;
	}
}

/*
 *		S P M _ G E T
 *
 *  Return a pointer to the storage element indexed by (u,v)
 *  coordinates.  It does NOT check the sanity of the coords.
 *
 *  0.0 <= u < 1.0	Left to Right
 *  0.0 <= v < 1.0	Bottom to Top
 */
char *
spm_get(register spm_map_t *mapp, double u, double v)
{
	int	x, y;
	register unsigned char *cp;

	RT_CK_SPM(mapp);

	y = v * mapp->ny;
	x = u * mapp->nx[y];
	cp = &(mapp->xbin[y][x*mapp->elsize]);

	return( (char *)cp );
}

/*
 *		S P M _ L O A D
 *
 *  Read a saved sphere map from a file ("-" for stdin) into
 *  the given map structure.
 *  This does not check for conformity of size, etc.
 *  Returns -1 on error, else 0.
 */
int
spm_load(spm_map_t *mapp, char *filename)
{
	int	y, total;
	FILE	*fp;

	RT_CK_SPM(mapp);

	if( strcmp( filename, "-" ) == 0 )
		fp = stdin;
	else  {
		bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		fp = fopen( filename, "r" );
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		if( fp == NULL )
			return( -1 );
	}

	total = 0;
	for( y = 0; y < mapp->ny; y++ )
		total += mapp->nx[y];

	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	y = fread( (char *)mapp->_data, mapp->elsize, total, fp );	/* res_syscall */
	(void) fclose( fp );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	if( y != total )
		return( -1 );

	return( 0 );
}

/*
 *		S P M _ S A V E
 *
 *  Write a loaded sphere map to the given file ("-" for stdout).
 *  Returns -1 on error, else 0.
 */
int
spm_save(spm_map_t *mapp, char *filename)
{
	int	i;
	int	got;
	FILE	*fp;

	RT_CK_SPM(mapp);

	if( strcmp( filename, "-" ) == 0 )
		fp = stdout;
	else  {
		bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		fp = fopen( filename, "w" );			/* res_syscall */
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		if( fp == NULL )
			return( -1 );
	}

	for( i = 0; i < mapp->ny; i++ ) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		got = fwrite( (char *)mapp->xbin[i], mapp->elsize,	/* res_syscall */
		    mapp->nx[i], fp );
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		if( got != mapp->nx[i] ) {
			bu_log("spm_save(%s): write error\n", filename);
			bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		    	(void) fclose( fp );
			bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		    	return( -1 );
		}
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	(void) fclose( fp );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	return( 0 );
}

/*
 *		S P M _ P I X _ L O A D
 *
 *  Load an 'nx' by 'ny' pix file and filter it into the
 *  given sphere structure.
 *  Returns -1 on error, else 0.
 */
int
spm_px_load(spm_map_t *mapp, char *filename, int nx, int ny)
{
	int	i, j;			/* index input file */
	int	x, y;			/* index texture map */
	double	j_per_y, i_per_x;	/* ratios */
	int	nj, ni;			/* ints of ratios */
	unsigned char *cp;
	unsigned char *buffer;
	unsigned long	red, green, blue;
	long	count;
	FILE	*fp;

	RT_CK_SPM(mapp);

	if( strcmp( filename, "-" ) == 0 )
		fp = stdin;
	else  {
		bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		fp = fopen( filename, "r" );
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		if( fp == NULL )
			return( -1 );
	}

	/* Shamelessly suck it all in */
	buffer = (unsigned char *)bu_malloc( (unsigned)(nx*nx*3), "spm_px_load buffer" );
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	i = fread( (char *)buffer, 3, nx*ny, fp );	/* res_syscall */
	(void) fclose( fp );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	if( i != nx*ny )  {
		bu_log("spm_px_load(%s) read error\n", filename);
		return( -1 );
	}

	j_per_y = (double)ny / (double)mapp->ny;
	nj = (int)j_per_y;
	/* for each bin */
	for( y = 0; y < mapp->ny; y++ ) {
		i_per_x = (double)nx / (double)mapp->nx[y];
		ni = (int)i_per_x;
		/* for each cell in bin */
		for( x = 0; x < mapp->nx[y]; x++ ) {
			/* Average pixels from the input file */
			red = green = blue = 0;
			count = 0;
			for( j = y*j_per_y; j < y*j_per_y+nj; j++ ) {
				for( i = x*i_per_x; i < x*i_per_x+ni; i++ ) {
					red = red + (unsigned long)buffer[ 3*(j*nx+i) ];
					green = green + (unsigned long)buffer[ 3*(j*nx+i)+1 ];
					blue = blue + (unsigned long)buffer[ 3*(j*nx+i)+2 ];
					count++;
				}
			}
			/* Save the color */
			cp = &(mapp->xbin[y][x*3]);
			*cp++ = (unsigned char)(red/count);
			*cp++ = (unsigned char)(green/count);
			*cp++ = (unsigned char)(blue/count);
		}
	}
	(void) bu_free( (char *)buffer, "spm buffer" );

	return( 0 );
}

/*
 *		S P M _ P I X _ S A V E
 *
 *  Save a sphere structure as an 'nx' by 'ny' pix file.
 *  Returns -1 on error, else 0.
 */
int
spm_px_save(spm_map_t *mapp, char *filename, int nx, int ny)
{
	int	x, y;
	FILE	*fp;
	unsigned char pixel[3];
	int	got;

	RT_CK_SPM(mapp);

	if( strcmp( filename, "-" ) == 0 )
		fp = stdout;
	else  {
		bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
		fp = fopen( filename, "w" );
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		if( fp == NULL )
			return( -1 );
	}

	for( y = 0; y < ny; y++ ) {
		for( x = 0; x < nx; x++ ) {
			spm_read( mapp, pixel, (double)x/(double)nx, (double)y/(double)ny );
			bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
			got = fwrite( (char *)pixel, sizeof(pixel), 1, fp );	/* res_syscall */
			bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
			if( got != 1 )  {
				bu_log("spm_px_save(%s): write error\n", filename);
				bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
				(void) fclose( fp );
				bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
				return( -1 );
			}
		}
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	(void) fclose( fp );
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	return( 0 );
}

/*
 * 		S P M _ D U M P
 *
 *  Display a sphere structure on stderr.
 *  Used for debugging.
 */
void
spm_dump(spm_map_t *mp, int verbose)
{
	int	i;

	RT_CK_SPM(mp);

	bu_log("elsize = %d\n", mp->elsize );
	bu_log("ny = %d\n", mp->ny );
	bu_log("_data = 0x%x\n", mp->_data );
	if( !verbose )  return;
	for( i = 0; i < mp->ny; i++ ) {
		bu_log("  nx[%d] = %3d, xbin[%d] = 0x%x\n",
			i, mp->nx[i], i, mp->xbin[i] );
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
