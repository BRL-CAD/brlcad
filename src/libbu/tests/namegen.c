/*                       N A M E G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
#include <limits.h>
#include <ctype.h>
#include "bu.h"
#include "bn.h"
#include "string.h"


int
main(int argc, const char **argv)
{
    int ret = -1;
    const char *instr = NULL;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    long increment_states[10];
    const char *time_incrementers[3] = {"d:0:1:12:0","d:2:0:59:0","d:2:0:59:0"};
    const char *regex_str = "([0-9]+):([0-9]+):([0-9]+).*($)";

    /* Sanity check */
    if (argc < 2)
	bu_exit(1, "ERROR: wrong number of parameters");

    instr = argv[1];
    if (strlen(instr) <= 0) {
	bu_exit(1, "invalid string: %s\n", argv[1]);
    }

    switch (argc) {
	case 2:
	    ret = bu_namegen(&out, instr, regex_str, time_incrementers, increment_states);
	    break;
	default:
	    break;
    }

    return ret;
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
