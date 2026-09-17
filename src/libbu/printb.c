/*                        P R I N T B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/vls.h"

void
bu_vls_printb(struct bu_vls *vls, const char *s, register long unsigned int v, register const char *bits)
{
    register int i, any = 0;
    register unsigned char c;

    if (!vls)
	return;

    if (!s)
	s = "";

    if (!bits) {
	bu_vls_printf(vls, "%s=x%lx <>", s, v);
	return;
    }

    if ((unsigned char)*bits == 8) {
	bits++;
	bu_vls_printf(vls, "%s=0%lo <", s, v);
    } else {
	if (*bits)
	    bits++;
	bu_vls_printf(vls, "%s=x%lx <", s, v);
    }

    while (*bits) {
	i = (unsigned char)*bits++;
	if (i == 0)
	    break;
	if (i > 0 && i <= (int)(sizeof(long unsigned int) * 8) && (v & (1UL << (i - 1)))) {
	    if (any)
		bu_vls_putc(vls, ',');
	    any = 1;
	    while ((c = (unsigned char)*bits) > 32) {
		bu_vls_putc(vls, (char)c);
		bits++;
	    }
	} else {
	    while ((unsigned char)*bits > 32)
		bits++;
	}
    }
    bu_vls_putc(vls, '>');
}


void
bu_printb(const char *s, register long unsigned int v, register const char *bits)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    bu_vls_printb(&str, s, v, bits);
    bu_log("%s", bu_vls_addr(&str));
    bu_vls_free(&str);
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
