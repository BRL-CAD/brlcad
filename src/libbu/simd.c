/*                         S I M D . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
#include "bu.h"

int
bu_simd_level()
{
#if defined(__GNUC__) && defined(__SSE__)
    int b=0, c=0, d=0;
    asm volatile(
    /* since we're PIC, we need to stash EBX on ia32 */
# ifdef __i386__
		    "pushl %%ebx;"
# endif
		    "cpuid;"
# ifdef __i386__
		    "popl %%ebx;"
# endif
		    : "=b"(b), "=c"(c), "=d" (d)
		    : "a" (0x1));

    if(c & 0x100000)
	return BU_SIMD_SSE4_2;
    if(c & 0x080000)
	return BU_SIMD_SSE4_1;
    if(c & 0x1)
	return BU_SIMD_SSE3;
    if(d & 0x1<<26)
	return BU_SIMD_SSE2;
    if(d & 0x1<<25)
	return BU_SIMD_SSE;
    if(d & 0x1<<24)
	return BU_SIMD_MMX;
#endif
    return BU_SIMD_NONE;
}

int
bu_simd_supported(int level)
{
    int l;

    if(level == 4)
	return 0;

    l = bu_simd_level();

    return l >= level;
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
