/*                      R A N D M T . C
 * BRL-CAD
 *
 * A C-program for MT19937: Real number version
 *   genrand() generates one pseudorandom real number (double)
 * which is uniformly distributed on [0, 1]-interval, for each
 * call. sgenrand(seed) set initial values to the working area
 * of 624 words. Before genrand(), sgenrand(seed) must be
 * called once. (seed is any 32-bit integer except for 0).
 * Integer generator is obtained by modifying two lines.
 *   Coded by Takuji Nishimura, considering the suggestions by
 * Topher Cooper and Marc Rieffel in July-Aug. 1997.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 *
 * Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
 * Any feedback is very welcome. For any question, comments,
 * see http://www.math.keio.ac.jp/matumoto/emt.html or email
 * matumoto@math.keio.ac.jp
 */

#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/randmt.h"

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */


/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

#define MERSENNE_MAGIC 0x4D54524E

struct _internal_state_s {
    uint32_t magic;
    int mti;		/* state index */
    uint32_t mt[N];	/* state vector */
};

static struct _internal_state_s global_state = { MERSENNE_MAGIC, N+1, {0} };

void *
bn_randmt_state_create(void)
{
    struct _internal_state_s *is;

    BU_ALLOC(is, struct _internal_state_s);
    is->magic = MERSENNE_MAGIC;
    is->mti = N+1;
    return (void *)is;
}

/* initializing the array with a NONZERO seed */
void
bn_randmt_state_seed(struct _internal_state_s *is, uint32_t seed)
{
    /*
     * setting initial seeds to mt[N] using
     * the generator Line 25 of Table 1 in
     * [KNUTH 1981, The Art of Computer Programming
     *    Vol. 2 (2nd Ed.), pp102]
     */
    is->mt[0] = seed & 0xffffffff;
    for (is->mti = 1; is->mti<N; is->mti++)
	is->mt[is->mti] = (69069 * is->mt[is->mti-1]) & 0xffffffff;
}

double
bn_randmt_state(struct _internal_state_s *is)
{
    uint32_t y;
    static uint32_t mag01[2]={0x0, MATRIX_A};

    /* generate N words at one time */
    if (is->mti >= N) {
	int kk;

	/* if sgenrand() has not been called, a default initial seed is used */
	if (is->mti == N+1)
	    bn_randmt_seed(4357);

	for (kk = 0; kk < N-M; kk++) {
	    y = (is->mt[kk]&UPPER_MASK)|(is->mt[kk+1]&LOWER_MASK);
	    is->mt[kk] = is->mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
	}

	for (; kk < N-1; kk++) {
	    y = (is->mt[kk]&UPPER_MASK)|(is->mt[kk+1]&LOWER_MASK);
	    is->mt[kk] = is->mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
	}

	y = (is->mt[N-1]&UPPER_MASK)|(is->mt[0]&LOWER_MASK);
	is->mt[N-1] = is->mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];

	is->mti = 0;
    }

    y = is->mt[is->mti++];
    y ^= TEMPERING_SHIFT_U(y);
    y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
    y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
    y ^= TEMPERING_SHIFT_L(y);

    return (double)y / (uint32_t)0xffffffff;
}

/* straight binhex would result in a 8+4992 character encoding. We should be able
 * to compress it quite a bit with better encoding? maybe uuencode? */
void
bn_randmt_state_serialize(struct _internal_state_s *UNUSED(is), struct bu_vls *UNUSED(s))
{
    bu_bomb("Not implemented yet.\n");
}

void
bn_randmt_state_deserialize(struct _internal_state_s *UNUSED(is), struct bu_vls *UNUSED(s))
{
    bu_bomb("Not implemented yet.\n");
}

void
bn_randmt_seed(unsigned long seed)
{
    bn_randmt_state_seed(&global_state, (uint32_t)seed);
}

double bn_randmt(void)
{
    return bn_randmt_state(&global_state);
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
