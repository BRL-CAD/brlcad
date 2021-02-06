/*                            O P . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

/* interface header */
#include "rt/op.h"

/* implementation headers */
#include <ctype.h>


db_op_t
db_str2op(const char *str)
{
    db_op_t ret = DB_OP_NULL;

    if (!str)
	return ret;

    /* skip any leading whitespace */
    while (*str && isspace((int)(*str)))
	str++;

    if (str[0] == '\0')
	return ret;

    if (isprint(str[0])) {
	/* single byte/char */
	switch (str[0]) {
	    case 'u':
		ret = DB_OP_UNION;
		break;
	    case '-':
	    case '\\':
		ret = DB_OP_SUBTRACT;
		break;
	    case '+':
	    case 'n':
	    case 'x':
		ret = DB_OP_INTERSECT;
		break;
	}
	return ret;
    }

    /* check if multibyte */

    if (((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x88 && (unsigned char)str[2] == 0xAA) /* union symbol */
	|| ((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x8B && (unsigned char)str[2] == 0x83) /* n-ary union */
	)
    {
	ret = DB_OP_UNION;
    } else if (((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x80 && (unsigned char)str[2] > 0x89 && (unsigned char)str[2] < 0x96)
	       /* first check matches unicode symbol variants starting
		* with hyphen, non-breaking hypen, figure dash, en dash,
		* em dash, and horizontal bar.
		*/
	       || ((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x88 && (unsigned char)str[2] == 0x92) /* minus sign */
	       || ((unsigned char)str[0] == 0xEF && (unsigned char)str[1] == 0xB9 && (unsigned char)str[2] == 0x98) /* small em dash */
	       || ((unsigned char)str[0] == 0xEF && (unsigned char)str[1] == 0xB9 && (unsigned char)str[2] == 0xA3) /* small hypen minus */
	       || ((unsigned char)str[0] == 0xEF && (unsigned char)str[1] == 0xBC && (unsigned char)str[2] == 0x8D) /* fullwidth hyphen-minus */
	       || ((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x88 && (unsigned char)str[2] == 0x96) /* set minus */
	       || ((unsigned char)str[0] == 0xCB && (unsigned char)str[1] == 0x97) /* utf-16, modifier minus sign */
	)
    {
	ret = DB_OP_SUBTRACT;
    } else if (((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x88 && (unsigned char)str[2] == 0xA9) /* intersection symbol */
	       || ((unsigned char)str[0] == 0xE2 && (unsigned char)str[1] == 0x8B && (unsigned char)str[2] == 0x82) /* n-ary intersection */
	       || ((unsigned char)str[0] == 0xCB && (unsigned char)str[1] == 0x96) /* utf-16, modifier plus sign */
	       || ((unsigned char)str[0] == 0xC3 && (unsigned char)str[1] == 0x97) /* utf-16, multiplication sign */
	)
    {
	ret = DB_OP_INTERSECT;
    }

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
