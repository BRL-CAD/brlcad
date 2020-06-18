/*                       R E A D C N V . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/readcnv.c
 *
 * This routine reads the next field in "card" buffer. It expects the
 * field to contain a string representing a "float". The string is
 * read and converted to type "fastf_t", multiplied by "conv_factor",
 * and returned in "inum".  If "id" is not the null string, then "id"
 * is printed followed by the number.  "conv_factor" is a factor to
 * convert to mm and multiply by a scale factor.
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readcnv(fastf_t *inum, char *id)
{
    int i = (-1), done = 0, lencard;
    char num[80];

    if (card[counter] == eofd) {
	/* This is an empty field */
	counter++;
	return;
    } else if (card[counter] == eord) /* Up against the end of record */
	return;

    if (card[72] == 'P')
	lencard = PARAMLEN;
    else
	lencard = CARDLEN;

    if (counter >= lencard)
	Readrec(++currec);

    while (!done) {
	while ((num[++i] = card[counter++]) != eofd && num[i] != eord
	       && counter <= lencard)
	    if (num[i] == 'D')
		num[i] = 'e';

	if (counter > lencard && num[i] != eord && num[i] != eofd)
	    Readrec(++currec);
	else
	    done = 1;
    }

    if (num[i] == eord)
	counter--;

    num[++i] = '\0';
    *inum = atof(num) * conv_factor;
    if (*id != '\0')
	bu_log("%s%g\n", id, *inum);
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
