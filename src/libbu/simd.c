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
    int d;
    /* since we're PIC, we need to stash EBX on ia32 */
#ifdef __i386__
    asm ("pushl %%ebx;cpuid;popl %%ebx;movl $0,%%eax": "=d" (d): "a" (0x1));
#else
    asm ("cpuid;movl $0,%%eax": "=d"(d): "a"(0x1));
#endif
    if(d & 0x1<<26)
	return BU_SIMD_SSE2;
    if(d & 0x1<<25)
	return BU_SIMD_SSE;
    if(d & 0x1<<24)
	return BU_SIMD_MMX;
#endif
    return BU_SIMD_NONE;
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
