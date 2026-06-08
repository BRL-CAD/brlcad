/*                     T E S T _ W A V E L E T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "test_util.h"


static int
test_wavelet_1d(void)
{
    int failures = 0;
    const char *test = "wavelet_1d";
    double dbuf[8] = {2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0};
    double dorig[8] = {2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0};
    float fbuf[16] = {
	0.0f, 10.0f, 2.0f, 12.0f, 4.0f, 14.0f, 6.0f, 16.0f,
	8.0f, 18.0f, 10.0f, 20.0f, 12.0f, 22.0f, 14.0f, 24.0f
    };
    float forig[16] = {
	0.0f, 10.0f, 2.0f, 12.0f, 4.0f, 14.0f, 6.0f, 16.0f,
	8.0f, 18.0f, 10.0f, 20.0f, 12.0f, 22.0f, 14.0f, 24.0f
    };
    int ibuf[8] = {16, 24, 32, 40, 48, 56, 64, 72};
    int iorig[8] = {16, 24, 32, 40, 48, 56, 64, 72};
    float ftmp[8];
    int itmp[4];
    size_t i;

    bn_wlt_haar_1d_double_decompose(NULL, dbuf, 8, 1, 1);
    bn_wlt_haar_1d_double_reconstruct(NULL, dbuf, 8, 1, 1, 8);
    for (i = 0; i < 8; i++) {
	if (!scalar_close(dbuf[i], dorig[i], 1.0e-12)) {
	    report_failure(test, "double round-trip mismatch at index %zu", i);
	    failures++;
	    break;
	}
    }

    bn_wlt_haar_1d_float_decompose(ftmp, fbuf, 8, 2, 2);
    bn_wlt_haar_1d_float_reconstruct(ftmp, fbuf, 8, 2, 2, 8);
    for (i = 0; i < 16; i++) {
	if (!scalar_close(fbuf[i], forig[i], 1.0e-5)) {
	    report_failure(test, "float round-trip mismatch at index %zu", i);
	    failures++;
	    break;
	}
    }

    bn_wlt_haar_1d_int_decompose(itmp, ibuf, 8, 1, 1);
    bn_wlt_haar_1d_int_reconstruct(itmp, ibuf, 8, 1, 1, 8);
    for (i = 0; i < 8; i++) {
	if (ibuf[i] != iorig[i]) {
	    report_failure(test, "int round-trip mismatch at index %zu", i);
	    failures++;
	    break;
	}
    }

    return failures;
}


static int
test_wavelet_2d(void)
{
    int failures = 0;
    const char *test = "wavelet_2d";
    double dbuf[16] = {
	2.0, 4.0, 6.0, 8.0,
	10.0, 12.0, 14.0, 16.0,
	18.0, 20.0, 22.0, 24.0,
	26.0, 28.0, 30.0, 32.0
    };
    double dorig[16] = {
	2.0, 4.0, 6.0, 8.0,
	10.0, 12.0, 14.0, 16.0,
	18.0, 20.0, 22.0, 24.0,
	26.0, 28.0, 30.0, 32.0
    };
    double dtmp[8];
    float fbuf1[32] = {
	0.0f, 100.0f, 2.0f, 102.0f, 4.0f, 104.0f, 6.0f, 106.0f,
	8.0f, 108.0f, 10.0f, 110.0f, 12.0f, 112.0f, 14.0f, 114.0f,
	16.0f, 116.0f, 18.0f, 118.0f, 20.0f, 120.0f, 22.0f, 122.0f,
	24.0f, 124.0f, 26.0f, 126.0f, 28.0f, 128.0f, 30.0f, 130.0f
    };
    float fbuf2[32];
    float ftmp1[8];
    float ftmp2[8];
    int ibuf[16] = {
	16, 24, 32, 40,
	48, 56, 64, 72,
	80, 88, 96, 104,
	112, 120, 128, 136
    };
    int iorig[16] = {
	16, 24, 32, 40,
	48, 56, 64, 72,
	80, 88, 96, 104,
	112, 120, 128, 136
    };
    int itmp[8];
    size_t i;

    memcpy(fbuf2, fbuf1, sizeof(fbuf1));

    bn_wlt_haar_2d_double_decompose(dtmp, dbuf, 4, 1, 1);
    bn_wlt_haar_2d_double_reconstruct(dtmp, dbuf, 4, 1, 1, 4);
    for (i = 0; i < 16; i++) {
	if (!scalar_close(dbuf[i], dorig[i], 1.0e-12)) {
	    report_failure(test, "2d double round-trip mismatch at index %zu", i);
	    failures++;
	    break;
	}
    }

    bn_wlt_haar_2d_float_decompose(ftmp1, fbuf1, 4, 2, 1);
    bn_wlt_haar_2d_float_decompose2(ftmp2, fbuf2, 4, 4, 2, 1);
    for (i = 0; i < 32; i++) {
	if (!scalar_close(fbuf1[i], fbuf2[i], 1.0e-6)) {
	    report_failure(test, "2d float square decompose and decompose2 results diverged at index %zu", i);
	    failures++;
	    break;
	}
    }

    bn_wlt_haar_2d_int_decompose(itmp, ibuf, 4, 1, 1);
    bn_wlt_haar_2d_int_reconstruct(itmp, ibuf, 4, 1, 1, 4);
    for (i = 0; i < 16; i++) {
	if (ibuf[i] != iorig[i]) {
	    report_failure(test, "2d int round-trip mismatch at index %zu", i);
	    failures++;
	    break;
	}
    }

    return failures;
}


static const struct bn_api_case wavelet_cases[] = {
    {"1d", test_wavelet_1d},
    {"2d", test_wavelet_2d},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    return bn_api_dispatch(argc, argv, wavelet_cases);
}
