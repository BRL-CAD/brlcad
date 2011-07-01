/*                        S P H M A P . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "spm.h"


void
bn_spm_free(bn_spm_map_t *mp)
{
    BN_CK_SPM_MAP(mp);
    if (mp == BN_SPM_MAP_NULL)
	return;

    if (mp->_data != NULL) {
	(void) bu_free((char *)mp->_data, "sph _data");
	mp->_data = NULL;
    }

    if (mp->nx != NULL) {
	(void) bu_free((char *)mp->nx, "sph nx");
	mp->nx = NULL;
    }

    if (mp->xbin != NULL) {
	(void) bu_free((char *)mp->xbin, "sph xbin");
	mp->xbin = NULL;
    }

    (void) bu_free((char *)mp, "bn_spm_map_t");
}


bn_spm_map_t *
bn_spm_init(int N, int elsize)
{
    int i, nx, total, idx;
    register bn_spm_map_t *mapp;

    mapp = (bn_spm_map_t *)bu_malloc(sizeof(bn_spm_map_t), "bn_spm_map_t");
    if (mapp == BN_SPM_MAP_NULL)
	return BN_SPM_MAP_NULL;
    memset((char *)mapp, 0, sizeof(bn_spm_map_t));

    mapp->elsize = elsize;
    mapp->ny = N/2;
    mapp->nx = (int *) bu_malloc((unsigned)(N/2 * sizeof(*(mapp->nx))), "sph nx");
    if (mapp->nx == NULL) {
	bn_spm_free(mapp);
	return BN_SPM_MAP_NULL;
    }
    mapp->xbin = (unsigned char **) bu_malloc((unsigned)(N/2 * sizeof(char *)), "sph xbin");
    if (mapp->xbin == NULL) {
	bn_spm_free(mapp);
	return BN_SPM_MAP_NULL;
    }

    total = 0;
    for (i = 0; i < N/4; i++) {
	nx = ceil(N*cos(i*bn_twopi/N));
	if (nx > N) nx = N;
	mapp->nx[ N/4 + i ] = nx;
	mapp->nx[ N/4 - i -1 ] = nx;

	total += 2*nx;
    }

    mapp->_data = (unsigned char *) bu_calloc((unsigned)total, elsize, "bn_spm_init data");
    if (mapp->_data == NULL) {
	bn_spm_free(mapp);
	return BN_SPM_MAP_NULL;
    }

    idx = 0;
    for (i = 0; i < N/2; i++) {
	mapp->xbin[i] = &((mapp->_data)[idx]);
	idx += elsize * mapp->nx[i];
    }
    mapp->magic = BN_SPM_MAGIC;
    return mapp;
}


void
bn_spm_read(register bn_spm_map_t *mapp, register unsigned char *valp, double u, double v)
{
    int x, y;
    register unsigned char *cp;
    register int i;

    BN_CK_SPM_MAP(mapp);

    y = v * mapp->ny;
    x = u * mapp->nx[y];
    cp = &(mapp->xbin[y][x*mapp->elsize]);

    i = mapp->elsize;
    while (i-- > 0) {
	*valp++ = *cp++;
    }
}


void
bn_spm_write(register bn_spm_map_t *mapp, register unsigned char *valp, double u, double v)
{
    int x, y;
    register unsigned char *cp;
    register int i;

    BN_CK_SPM_MAP(mapp);

    y = v * mapp->ny;
    x = u * mapp->nx[y];
    cp = &(mapp->xbin[y][x*mapp->elsize]);

    i = mapp->elsize;
    while (i-- > 0) {
	*cp++ = *valp++;
    }
}


char *
bn_spm_get(register bn_spm_map_t *mapp, double u, double v)
{
    int x, y;
    register unsigned char *cp;

    BN_CK_SPM_MAP(mapp);

    y = v * mapp->ny;
    x = u * mapp->nx[y];
    cp = &(mapp->xbin[y][x*mapp->elsize]);

    return (char *)cp;
}


int
bn_spm_load(bn_spm_map_t *mapp, char *filename)
{
    int y, total;
    FILE *fp;

    BN_CK_SPM_MAP(mapp);

    if (BU_STR_EQUAL(filename, "-"))
	fp = stdin;
    else {
	bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	fp = fopen(filename, "rb");
	bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	if (fp == NULL)
	    return -1;
    }

    total = 0;
    for (y = 0; y < mapp->ny; y++)
	total += mapp->nx[y];

    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
    y = (int)fread((char *)mapp->_data, mapp->elsize, total, fp);	/* res_syscall */
    (void) fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */

    if (y != total)
	return -1;

    return 0;
}


int
bn_spm_save(bn_spm_map_t *mapp, char *filename)
{
    int i;
    int got;
    FILE *fp;

    BN_CK_SPM_MAP(mapp);

    if (BU_STR_EQUAL(filename, "-"))
	fp = stdout;
    else {
	bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	fp = fopen(filename, "wb");			/* res_syscall */
	bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	if (fp == NULL)
	    return -1;
    }

    for (i = 0; i < mapp->ny; i++) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	got = (int)fwrite((char *)mapp->xbin[i], mapp->elsize,	/* res_syscall */
			  mapp->nx[i], fp);
	bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	if (got != mapp->nx[i]) {
	    bu_log("WARNING: Unable to write SPM to [%s]\n", filename);
	    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	    (void) fclose(fp);
	    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	    return -1;
	}
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
    (void) fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */

    return 0;
}


int
bn_spm_pix_load(bn_spm_map_t *mapp, char *filename, int nx, int ny)
{
    int i, j;			/* index input file */
    int x, y;			/* index texture map */
    double j_per_y, i_per_x;	/* ratios */
    int nj, ni;			/* ints of ratios */
    unsigned char *cp;
    unsigned char *buffer;
    unsigned long red, green, blue;
    long count;
    FILE *fp;

    BN_CK_SPM_MAP(mapp);

    if (BU_STR_EQUAL(filename, "-"))
	fp = stdin;
    else {
	bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	fp = fopen(filename, "rb");
	bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	if (fp == NULL)
	    return -1;
    }

    /* Shamelessly suck it all in */
    buffer = (unsigned char *)bu_malloc((unsigned)(nx*nx*3), "bn_spm_pix_load buffer");
    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
    i = (int)fread((char *)buffer, 3, nx*ny, fp);	/* res_syscall */
    (void)fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
    if (i != nx*ny) {
	bu_log("bn_spm_pix_load(%s) read error\n", filename);
	return -1;
    }

    j_per_y = (double)ny / (double)mapp->ny;
    nj = (int)j_per_y;
    /* for each bin */
    for (y = 0; y < mapp->ny; y++) {
	i_per_x = (double)nx / (double)mapp->nx[y];
	ni = (int)i_per_x;
	/* for each cell in bin */
	for (x = 0; x < mapp->nx[y]; x++) {
	    /* Average pixels from the input file */
	    red = green = blue = 0;
	    count = 0;
	    for (j = y*j_per_y; j < y*j_per_y+nj; j++) {
		for (i = x*i_per_x; i < x*i_per_x+ni; i++) {
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
    (void) bu_free((char *)buffer, "bn_spm buffer");

    return 0;
}


int
bn_spm_pix_save(bn_spm_map_t *mapp, char *filename, int nx, int ny)
{
    int x, y;
    FILE *fp;
    unsigned char pixel[3];
    int got;

    BN_CK_SPM_MAP(mapp);

    if (BU_STR_EQUAL(filename, "-"))
	fp = stdout;
    else {
	bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	fp = fopen(filename, "wb");
	bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	if (fp == NULL)
	    return -1;
    }

    for (y = 0; y < ny; y++) {
	for (x = 0; x < nx; x++) {
	    bn_spm_read(mapp, pixel, (double)x/(double)nx, (double)y/(double)ny);
	    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
	    got = (int)fwrite((char *)pixel, sizeof(pixel), 1, fp);	/* res_syscall */
	    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
	    if (got != 1) {
		bu_log("bn_spm_px_save(%s): write error\n", filename);
		bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
		(void) fclose(fp);
		bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */
		return -1;
	    }
	}
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);		/* lock */
    (void) fclose(fp);
    bu_semaphore_release(BU_SEM_SYSCALL);		/* unlock */

    return 0;
}


void
bn_spm_dump(bn_spm_map_t *mp, int verbose)
{
    int i;

    BN_CK_SPM_MAP(mp);

    bu_log("elsize = %d\n", mp->elsize);
    bu_log("ny = %d\n", mp->ny);
    bu_log("_data = %p\n", (void *)mp->_data);
    if (!verbose) return;
    for (i = 0; i < mp->ny; i++) {
	bu_log(" nx[%d] = %3d, xbin[%d] = %p\n",
	       i, mp->nx[i], i, (void *)mp->xbin[i]);
    }
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
