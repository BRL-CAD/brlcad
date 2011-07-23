/*                       H T E S T E R . C
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu.h"


#define NUM 3000
double orig[NUM], after[NUM];
unsigned char buf[NUM*8];

void
flpr(unsigned char *cp)
{
    unsigned int i;
    for (i=0; i<sizeof(double); i++) {
	putchar("0123456789ABCDEFx"[*cp>>4]);
	putchar("0123456789ABCDEFx"[*cp&0xF]);
	cp++;
    }
    return;
}

int
ckbytes(unsigned char *a, unsigned char *b, unsigned int n)
{
#ifndef vax
    while (n-- > 0) {
	if (*a++ != *b++)
	    return -1;	/* BAD */
    }
    return 0;			/* OK */
#else
    /* VAX floating point has bytes swapped, vis-a-vis normal VAX order */
    int i;
    for (i=0; i<n; i++) {
	if (a[i^1] != b[i^1])
	    return -1;	/* BAD */
    }
    return 0;			/* OK */
#endif
}

int
main(int argc, char **argv)
{
    int ret;
    unsigned int i;
    unsigned int nbytes;
    int len = sizeof(double);

#define A argv[1][1]
    if (argc != 2 || argv[1][0] != '-' || (A != 'o' && A != 'i' && A != 'v')) {
	bu_exit(1, "Usage:  htester [-i|-o|-v] < input\n");
    }

    /* First stage, generate the reference pattern */
    for (i=0; i<1000; i++) {
	orig[i] = ((double)i)/(7*16);
    }
    for (i=1000; i<2000; i++) {
	orig[i] = -1.0/(i-1000+1);
    }
    orig[2000] = -1;
    for (i=2001; i<2035; i++) {
	orig[i] = orig[i-1] * -0.1;
    }
    for (i=2035; i<3000; i++) {
	int hilow = (i&1) ? (-1) : 1;
	orig[i] = orig[i-1000] + hilow;
    }

    /* Second stage, write out, or read and compare */
    if (argv[1][1] == 'o') {
	/* Write out */
	htond((unsigned char *)buf, (unsigned char *)orig, NUM);
	ret = fwrite(buf, 8, NUM, stdout);
	if (ret != 8)
	    perror("fwrite");
	exit(0);
    }

    /* Read and compare */
    switch (len) {
	case 8:
	    nbytes = 6;
	    break;
	case 4:
	    /* untested */
	    nbytes = 4;
	    break;
	default:
	    bu_bomb("unknown and untested double size\n");
	    break;
    }
    ret = fread(buf, 8, NUM, stdin);
    if (ret != 8)
	perror("fwrite");

/* ntohd((char *)after, buf, NUM);	*//* bulk conversion */
    for (i=0; i<NUM; i++) {
	ntohd((unsigned char *)&after[i], (unsigned char *)&buf[i*8], 1);	/* incremental */
	/* Floating point compare */
	if ((orig[i] - after[i]) > -SMALL_FASTF && (orig[i] - after[i]) < SMALL_FASTF)
	    continue;

	/* Byte-for-byte compare */
	if (ckbytes((unsigned char *)&orig[i],
		    (unsigned char *)&after[i], nbytes) == 0)
	    continue;

	/* Wrong */
	printf("%4d: calc ", i);
	flpr((unsigned char *)&orig[i]);
	printf(" %g\n", orig[i]);
	printf("      aftr ");
	flpr((unsigned char *)&after[i]);
	printf(" %g\n", after[i]);
	printf("      buf  ");
	flpr(&buf[i*8]);
	printf("\n");
    }
    exit(0);
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
