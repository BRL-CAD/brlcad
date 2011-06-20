/*                         N O I S E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup noise */
/** @{ */
/** @file libbn/noise.c
 *
 * These noise functions provide mostly random noise at the integer
 * lattice points.  The functions should be evaluated at non-integer
 * locations for their nature to be realized.
 *
 * Conatins contributed code from:
 * F. Kenton Musgrave
 * Robert Skinner
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/**
 * @brief interpolate smoothly from 0 .. 1
 *
 * SMOOTHSTEP() takes a value in the range [0:1] and provides a number
 * in the same range indicating the amount of (a) present in a smooth
 * interpolation transition between (a) and (b)
 */
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2*(x)))


/**
 * @brief
 * fold space to extend the domain over which we can take noise
 * values.
 *
 *@n x, y, z are set to the noise space location for the source point.
 *@n ix, iy, iz are the integer lattice point (integer portion of x, y, z)
 *@n fx, fy, fz are the fractional lattice distance above ix, iy, iz
 *
 * The noise function has a finite domain, which can be exceeded when
 * using fractal textures with very high frequencies.  This routine is
 * designed to extend the effective domain of the function, albeit by
 * introducing periodicity. -FKM 4/93
 */
static void
filter_args(fastf_t *src, fastf_t *p, fastf_t *f, int *ip)
{
    register int i;
    point_t dst;
    static unsigned long max2x = ~((unsigned long)0);
    static unsigned long max = (~((unsigned long)0)) >> 1;

    for (i=0; i < 3; i++) {
	/* assure values are positive */
	if (src[i] < 0) dst[i] = -src[i];
	else dst[i] = src[i];


	/* fold space */
	while (dst[i] > max || dst[i]<0) {
	    if (dst[i] > max) {
		dst[i] = max2x - dst[i];
	    } else {
		dst[i] = -dst[i];
	    }
	}

    }

    p[X] = dst[0];
    ip[X] = floor(p[X]);
    f[X] = p[X] - ip[X];

    p[Y] = dst[1];
    ip[Y] = floor(p[Y]);
    f[Y] = p[Y] - ip[Y];

    p[Z] = dst[2];
    ip[Z] = floor(p[Z]);
    f[Z] = p[Z] - ip[Z];
}


#define MAXSIZE 267	/* 255 + 3 * (4 values) */

/**
 * The RTable maps numbers into the range [-1..1]
 */
static double RTable[MAXSIZE];

#define INCRSUM(m, s, x, y, z)	((s)*(RTable[m]*0.5		\
				      + RTable[m+1]*(x)		\
				      + RTable[m+2]*(y)		\
				      + RTable[m+3]*(z)))


/**
 * A heavily magic-number protected version of the hashtable.
 *
 * This table is used to convert integers into repeatable random
 * results for indicies into RTable.
 */
struct str_ht {
    long magic;
    char hashTableValid;
    long *hashTableMagic1;
    short *hashTable;
    long *hashTableMagic2;
    long magic_end;
};

static struct str_ht ht;

#define MAGIC_STRHT1 1771561
#define MAGIC_STRHT2 1651771
#define MAGIC_TAB1 9823
#define MAGIC_TAB2 784642
#define CK_HT() {							\
	BU_CKMAG(&ht.magic, MAGIC_STRHT1, "struct str_ht ht 1");	\
	BU_CKMAG(&ht.magic_end, MAGIC_STRHT2, "struct str_ht ht 2");	\
	BU_CKMAG(ht.hashTableMagic1, MAGIC_TAB1, "hashTable Magic 1");	\
	BU_CKMAG(ht.hashTableMagic2, MAGIC_TAB2, "hashTable Magic 2");	\
	if (ht.hashTable != (short *)&ht.hashTableMagic1[1])		\
	    bu_bomb("ht.hashTable changed rel ht.hashTableMagic1");	\
	if (ht.hashTableMagic2 != (long *)&ht.hashTable[4096])		\
	    bu_bomb("ht.hashTable changed rel ht.hashTableMagic2");	\
    }

/**
 * Map integer point into repeatable random number [0..4095].  We
 * actually only use the first 8 bits of the final value extracted
 * from this table.  It's not quite clear that we really need this big
 * a table.  The extra size does provide some extra randomness for
 * intermediate results.
 */
#define Hash3d(a, b, c)					\
    ht.hashTable[					\
	ht.hashTable[					\
	    ht.hashTable[(a) & 0xfff] ^ ((b) & 0xfff)	\
	    ]  ^ ((c) & 0xfff)				\
	]


void
bn_noise_init(void)
{
    int i, j, k, temp;
    int rndtabi = BN_RAND_TABSIZE - 1;

    bu_semaphore_acquire(BU_SEM_BN_NOISE);

    if (ht.hashTableValid) {
	bu_semaphore_release(BU_SEM_BN_NOISE);
	return;
    }

    BN_RANDSEED(rndtabi, (BN_RAND_TABSIZE-1));
    ht.hashTableMagic1 = (long *) bu_malloc(
	2*sizeof(long) + 4096*sizeof(short int),
	"noise hashTable");
    ht.hashTable = (short *)&ht.hashTableMagic1[1];
    ht.hashTableMagic2 = (long *)&ht.hashTable[4096];

    *ht.hashTableMagic1 = MAGIC_TAB1;
    *ht.hashTableMagic2 = MAGIC_TAB2;

    ht.magic_end = MAGIC_STRHT2;
    ht.magic = MAGIC_STRHT1;

    for (i = 0; i < 4096; i++)
	ht.hashTable[i] = i;

    /* scramble the hash table */
    for (i = 4095; i > 0; i--) {
	j = (int)(BN_RANDOM(rndtabi) * 4096.0);

	temp = ht.hashTable[i];
	ht.hashTable[i] = ht.hashTable[j];
	ht.hashTable[j] = temp;
    }

    BN_RANDSEED(k, 13);

    for (i = 0; i < MAXSIZE; i++)
	RTable[i] = BN_RANDOM(k) * 2.0 - 1.0;


    ht.hashTableValid = 1;

    bu_semaphore_release(BU_SEM_BN_NOISE);


    CK_HT();
}


/**
 *@brief
 * Robert Skinner's Perlin-style "Noise" function
 *
 * Results are in the range [-0.5 .. 0.5].  Unlike many
 * implementations, this function provides random noise at the integer
 * lattice values.  However this produces much poorer quality and
 * should be avoided if possible.
 *
 * The power distribution of the result has no particular shape,
 * though it isn't as flat as the literature would have one believe.
 */
double
bn_noise_perlin(fastf_t *point)
{
    register int jx, jy, jz;
    int ix, iy, iz;	/* lower integer lattice point */
    double x, y, z;	/* corrected point */
    double fx, fy, fz;	/* distance above integer lattice point */
    double sx, sy, sz, tx, ty, tz;
    double sum;
    short m;
    point_t p, f;
    int ip[3];

    if (!ht.hashTableValid)
	bn_noise_init();

    /* IS: const fastf_t *, point_t, point_t, int[3] */
    /* NE: fastf_t *, fastf_t *, fastf_t *, int * */
    filter_args(point, p, f, ip);
    ix = ip[X];
    iy = ip[Y];
    iz = ip[Z];

    fx = f[X];
    fy = f[Y];
    fz = f[Z];

    x = p[X];
    y = p[Y];
    z = p[Z];

    jx = ix + 1; /* (jx, jy, jz) = integer lattice point above (ix, iy, iz) */
    jy = iy + 1;
    jz = iz + 1;

    sx = SMOOTHSTEP(fx);
    sy = SMOOTHSTEP(fy);
    sz = SMOOTHSTEP(fz);

    /* the complement values of sx, sy, sz */
    tx = 1.0 - sx;
    ty = 1.0 - sy;
    tz = 1.0 - sz;

    /*
     * interpolate!
     */
    /* get a repeatable random # 0..4096 & 0xFF*/
    m = Hash3d(ix, iy, iz) & 0xFF;
    sum = INCRSUM(m, (tx*ty*tz), (x-ix), (y-iy), (z-iz));

    m = Hash3d(jx, iy, iz) & 0xFF;
    sum += INCRSUM(m, (sx*ty*tz), (x-jx), (y-iy), (z-iz));

    m = Hash3d(ix, jy, iz) & 0xFF;
    sum += INCRSUM(m, (tx*sy*tz), (x-ix), (y-jy), (z-iz));

    m = Hash3d(jx, jy, iz) & 0xFF;
    sum += INCRSUM(m, (sx*sy*tz), (x-jx), (y-jy), (z-iz));

    m = Hash3d(ix, iy, jz) & 0xFF;
    sum += INCRSUM(m, (tx*ty*sz), (x-ix), (y-iy), (z-jz));

    m = Hash3d(jx, iy, jz) & 0xFF;
    sum += INCRSUM(m, (sx*ty*sz), (x-jx), (y-iy), (z-jz));

    m = Hash3d(ix, jy, jz) & 0xFF;
    sum += INCRSUM(m, (tx*sy*sz), (x-ix), (y-jy), (z-jz));

    m = Hash3d(jx, jy, jz) & 0xFF;
    sum += INCRSUM(m, (sx*sy*sz), (x-jx), (y-jy), (z-jz));

    return sum;

}

/**
 * Vector-valued "Noise"
 */
void
bn_noise_vec(fastf_t *point, fastf_t *result)
{
    register int jx, jy, jz;
    int ix, iy, iz;		/* lower integer lattice point */
    double x, y, z;		/* corrected point */
    double px, py, pz, s;
    double sx, sy, sz, tx, ty, tz;
    short m;
    point_t p, f;
    int ip[3];


    if (! ht.hashTableValid) bn_noise_init();


    /* sets:
     * x, y, z to range [0..maxval],
     * ix, iy, iz to integer portion,
     * fx, fy, fz to fractional portion
     */
    filter_args(point, p, f, ip);
    ix = ip[X];
    iy = ip[Y];
    iz = ip[Z];

    x = p[X];
    y = p[Y];
    z = p[Z];

    jx = ix+1;   jy = iy + 1;   jz = iz + 1;

    sx = SMOOTHSTEP(x - ix);
    sy = SMOOTHSTEP(y - iy);
    sz = SMOOTHSTEP(z - iz);

    /* the complement values of sx, sy, sz */
    tx = 1.0 - sx;
    ty = 1.0 - sy;
    tz = 1.0 - sz;

    /*
     * interpolate!
     */
    m = Hash3d(ix, iy, iz) & 0xFF;
    px = x-ix;
    py = y-iy;
    pz = z-iz;
    s = tx*ty*tz;
    result[0] = INCRSUM(m, s, px, py, pz);
    result[1] = INCRSUM(m+4, s, px, py, pz);
    result[2] = INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(jx, iy, iz) & 0xFF;
    px = x-jx;
    s = sx*ty*tz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(jx, jy, iz) & 0xFF;
    py = y-jy;
    s = sx*sy*tz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(ix, jy, iz) & 0xFF;
    px = x-ix;
    s = tx*sy*tz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(ix, jy, jz) & 0xFF;
    pz = z-jz;
    s = tx*sy*sz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(jx, jy, jz) & 0xFF;
    px = x-jx;
    s = sx*sy*sz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(jx, iy, jz) & 0xFF;
    py = y-iy;
    s = sx*ty*sz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);

    m = Hash3d(ix, iy, jz) & 0xFF;
    px = x-ix;
    s = tx*ty*sz;
    result[0] += INCRSUM(m, s, px, py, pz);
    result[1] += INCRSUM(m+4, s, px, py, pz);
    result[2] += INCRSUM(m+8, s, px, py, pz);
}
/*************************************************************
 *@brief
 * Spectral Noise functions
 *
 *************************************************************
 *
 * The Spectral Noise functions cache the values of the term:
 *
 *@code
		    (-h_val)
		freq
		*@endcode
		* Which on some systems is rather expensive to compute.
		*
		*************************************************************/
struct fbm_spec {
    long magic;
    double octaves;
    double lacunarity;
    double h_val;
    double remainder;
    double *spec_wgts;
};
#define MAGIC_fbm_spec_wgt 0x837592

static struct fbm_spec *etbl = (struct fbm_spec *)NULL;
static int etbl_next = 0;
static int etbl_size = 0;

#define PSCALE(_p, _s) _p[0] *= _s; _p[1] *= _s; _p[2] *= _s
#define PCOPY(_d, _s) _d[0] = _s[0]; _d[1] = _s[1]; _d[2] = _s[2]


static struct fbm_spec *
build_spec_tbl(double h_val, double lacunarity, double octaves)
{
    struct fbm_spec *ep;
    double *spec_wgts;
    double frequency;
    int i;

    /* The right spectral weights table for these parameters has not
     * been pre-computed.  As a result, we compute the table now and
     * save it with the knowledge that we'll likely want it again
     * later.
     */

    /* allocate storage for new tables if needed */
    if (etbl_next >= etbl_size) {
	if (etbl_size) {
	    etbl_size *= 2;
	    etbl = (struct fbm_spec *)bu_realloc((char *)etbl,
						 etbl_size*sizeof(struct fbm_spec),
						 "spectral weights table");
	} else
	    etbl = (struct fbm_spec *)bu_calloc(etbl_size = 10,
						sizeof(struct fbm_spec),
						"spectral weights table");

	if (!etbl) abort();
    }

    /* set up the next available table */
    ep = &etbl[etbl_next];
    ep->magic = MAGIC_fbm_spec_wgt;	ep->octaves = octaves;
    ep->h_val = h_val;		ep->lacunarity = lacunarity;
    spec_wgts = ep->spec_wgts =
	(double *)bu_malloc(((int)(octaves+1)) * sizeof(double),
			    "spectral weights");

    /* precompute and store spectral weights table */
    for (frequency = 1.0, i=0; i < octaves; i++) {
	/* compute weight for each frequency */
	spec_wgts[i] = pow(frequency, -h_val);
	frequency *= lacunarity;
    }

    etbl_next++; /* saved for last in case we're running multi-threaded */
    return ep;
}

/**
 * The first order of business is to see if we have pre-computed the
 * spectral weights table for these parameters in a previous
 * invocation.  If not, the we compute them and save them for possible
 * future use
 */
struct fbm_spec *
find_spec_wgt(double h, double l, double o)
{
    struct fbm_spec *ep;
    int i;


    for (ep = etbl, i=0; i < etbl_next; i++, ep++) {
	if (ep->magic != MAGIC_fbm_spec_wgt)
	    bu_bomb("find_spec_wgt");
	if (ZERO(ep->lacunarity - l)
	    && ZERO(ep->h_val - h)
	    && ep->octaves > (o - SMALL_FASTF))
	{
	    return ep;
	}
    }

    /* we didn't find the table we wanted so we've got to semaphore on
     * the list to wait our turn to add what we want to the table.
     */

    bu_semaphore_acquire(BU_SEM_BN_NOISE);

    /* We search the list one more time in case the last process to
     * hold the semaphore just created the table we were about to add
     */
    for (ep = etbl, i=0; i < etbl_next; i++, ep++) {
	if (ep->magic != MAGIC_fbm_spec_wgt)
	    bu_bomb("find_spec_wgt");
	if (ZERO(ep->lacunarity - l)
	    && ZERO(ep->h_val - h)
	    && ep->octaves > (o - SMALL_FASTF))
	    break;
    }

    if (i >= etbl_next) ep = build_spec_tbl(h, l, o);

    bu_semaphore_release(BU_SEM_BN_NOISE);

    return ep;
}
/**
 * @brief
 * Procedural fBm evaluated at "point"; returns value stored in
 * "value".
 *
 * @param point          location to sample noise
 * @param ``h_val''      fractal increment parameter
 * @param ``lacunarity'' gap between successive frequencies
 * @param ``octaves''  	 number of frequencies in the fBm
 *
 * The spectral properties of the result are in the APPROXIMATE range
 * [-1..1] Depending upon the number of octaves computed, this range
 * may be exceeded.  Applications should clamp or scale the result to
 * their needs.  The results have a more-or-less gaussian
 * distribution.  Typical results for 1M samples include:
 *
 * @li Min -1.15246
 * @li Max  1.23146
 * @li Mean -0.0138744
 * @li s.d.  0.306642
 * @li Var 0.0940295
 *
 * The function call pow() is relatively expensive.  Therfore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
double
bn_noise_fbm(fastf_t *point, double h_val, double lacunarity, double octaves)
{
    struct fbm_spec *ep;
    double value, noise_remainder, *spec_wgts;
    point_t pt;
    int i, oct;

    /* The first order of business is to see if we have pre-computed
     * the spectral weights table for these parameters in a previous
     * invocation.  If not, the we compute them and save them for
     * possible future use
     */

    ep = find_spec_wgt(h_val, lacunarity, octaves);

    /* now we're ready to compute the fBm value */

    value = 0.0;            /* initialize vars to proper values */
    /* copy the point so we don't corrupt the caller's version */
    PCOPY(pt, point);

    spec_wgts = ep->spec_wgts;

    /* inner loop of spectral construction */
    oct=(int)octaves; /* save repeating double->int cast */
    for (i=0; i < oct; i++) {
	value += bn_noise_perlin(pt) * spec_wgts[i];
	PSCALE(pt, lacunarity);
    }

    noise_remainder = octaves - (int)octaves;
    if (!ZERO(noise_remainder)) {
	/* add in ``octaves'' noise_remainder ``i'' and spatial freq. are
	 * preset in loop above
	 */
	value += noise_remainder * bn_noise_perlin(pt) * spec_wgts[i];
    }

    return value;

} /* bn_noise_fbm() */


/**
 * @brief
 * Procedural turbulence evaluated at "point";
 *
 * @return turbulence value for point
 *
 * @param point          location to sample noise at
 * @param ``h_val''      fractal increment parameter
 * @param ``lacunarity'' gap between successive frequencies
 * @param ``octaves''    number of frequencies in the fBm
 *
 * The result is characterized by sharp, narrow trenches in low values
 * and a more fbm-like quality in the mid-high values.  Values are in
 * the APPROXIMATE range [0 .. 1] depending upon the number of octaves
 * evaluated.  Typical results:
 @code
 * Min  0.00857137
 * Max  1.26712
 * Mean 0.395122
 * s.d. 0.174796
 * Var  0.0305536
 @endcode
 * The function call pow() is relatively expensive.  Therfore, this
 * function pre-computes and saves the spectral weights in a table for
 * re-use in successive invocations.
 */
double
bn_noise_turb(fastf_t *point, double h_val, double lacunarity, double octaves)
{
    struct fbm_spec *ep;
    double value, noise_remainder, *spec_wgts;
    point_t pt;
    int i, oct;


    /* The first order of business is to see if we have pre-computed
     * the spectral weights table for these parameters in a previous
     * invocation.  If not, the we compute them and save them for
     * possible future use
     */

#define CACHE_SPECTRAL_WGTS 1
#ifdef CACHE_SPECTRAL_WGTS

    ep = find_spec_wgt(h_val, lacunarity, octaves);

    /* now we're ready to compute the fBm value */

    value = 0.0;            /* initialize vars to proper values */

    /* copy the point so we don't corrupt the caller's copy of the
     * variable
     */
    PCOPY(pt, point);
    spec_wgts = ep->spec_wgts;

    /* inner loop of spectral construction */
    oct=(int)octaves; /* save repeating double->int cast */
    for (i=0; i < oct; i++) {
	value += fabs(bn_noise_perlin(pt)) * spec_wgts[i];
	PSCALE(pt, lacunarity);
    }

    noise_remainder = octaves - (int)octaves;
    if (!ZERO(noise_remainder)) {
	/* add in ``octaves'' noise_remainder ``i'' and spatial freq. are
	 * preset in loop above
	 */
	value += noise_remainder * bn_noise_perlin(pt) * spec_wgts[i];
    }
#else
    PCOPY(pt, point);

    value = 0.0;      /* initialize vars to proper values */
    frequency = 1.0;

    oct=(int)octaves; /* save repeating double->int cast */
    for (i=0; i < oct; i++) {
	value += fabs(bn_noise_perlin(pt)) * pow(frequency, -h_val);
	frequency *= lacunarity;
	PSCALE(pt, lacunarity);
    }

    noise_remainder = octaves - (int)octaves;
    if (noise_remainder) {
	/* add in ``octaves'' noise_remainder ``i'' and spatial freq. are
	 * preset in loop above
	 */
	value += noise_remainder * bn_noise_perlin(pt) * pow(frequency, -h_val);
    }
#endif
    return value;

} /* bn_noise_turb() */

/**
 *@brief
 * A ridged noise pattern
 *
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed p338
 */
double
bn_noise_ridged(fastf_t *point, double h_val, double lacunarity, double octaves, double offset)
{
    struct fbm_spec *ep;
    double result, weight, noise_signal, *spec_wgts;
    point_t pt;
    int i;

    /* The first order of business is to see if we have pre-computed
     * the spectral weights table for these parameters in a previous
     * invocation.  If not, the we compute them and save them for
     * possible future use
     */

    ep = find_spec_wgt(h_val, lacunarity, octaves);

    /* copy the point so we don't corrupt the caller's copy of the
     * variable
     */
    PCOPY(pt, point);
    spec_wgts = ep->spec_wgts;


    /* get first octave */
    noise_signal = bn_noise_perlin(pt);

    /* get absolute value of noise_signal (this creates the ridges) */
    if (noise_signal < 0.0) noise_signal = -noise_signal;

    /* invert and translate (note that "offset shoudl be ~= 1.0 */
    noise_signal = offset - noise_signal;

    /* square the noise_signal, to increase "sharpness" of ridges */
    noise_signal *= noise_signal;

    /* assign initial value */
    result = noise_signal;
    weight = 1.0;

    for (i=1; i < octaves; i++) {
	PSCALE(pt, lacunarity);

	noise_signal = bn_noise_perlin(pt);

	if (noise_signal < 0.0) noise_signal = - noise_signal;
	noise_signal = offset - noise_signal;

	/* weight the contribution */
	noise_signal *= weight;
	result += noise_signal * spec_wgts[i];
    }
    return result;
}

/**
 *
 * From "Texturing and Modeling, A Procedural Approach" 2nd ed
 */
double
bn_noise_mf(fastf_t *point, double h_val, double lacunarity, double octaves, double offset)
{
    double frequency = 1.0;
    struct fbm_spec *ep;
    double result, weight, noise_signal, *spec_wgts;
    point_t pt;
    int i;

    /* The first order of business is to see if we have pre-computed
     * the spectral weights table for these parameters in a previous
     * invocation.  If not, the we compute them and save them for
     * possible future use
     */

    ep = find_spec_wgt(h_val, lacunarity, octaves);

    /* copy the point so we don't corrupt the caller's copy of the
     * variable.
     */
    PCOPY(pt, point);
    spec_wgts = ep->spec_wgts;
    offset = 1.0;

    result = (bn_noise_perlin(pt) + offset) * spec_wgts[0];
    weight = result;

    for (i=1; i < octaves; i++) {
	PSCALE(pt, lacunarity);

	if (weight > 1.0) weight = 1.0;

	noise_signal = (bn_noise_perlin(pt) + offset) * spec_wgts[i];

	noise_signal += fabs(bn_noise_perlin(pt)) * pow(frequency, -h_val);
	frequency *= lacunarity;
	PSCALE(pt, lacunarity);
    }
    return result;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
