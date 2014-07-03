/*                      R E A D S T R G . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file iges/readstrg.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a character string of the form "nHstring" where n
 * is the length of "string". If "id" is not the null string, then
 * "id" is printed followed by "string".  If "id" is null, then the
 * only action taken is to read the string (effectively skipping the
 * field).
 *
 * "eof" is the "end-of-field" delimiter
 * "eor" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readstrg(char *id)
{
    int i = (-1), length = 0, done = 0, lencard;
    char num[80];

    if (card[counter] == eof) {
	/* This is an empty field */
	counter++;
	return;
    } else if (card[counter] == eor) /* Up against the end of record */
	return;

    if (card[72] == 'P')
	lencard = PARAMLEN;
    else
	lencard = CARDLEN;

    if (counter > lencard)
	Readrec(++currec);

    if (!id) {
	return;
    }

    if (*id != '\0')
	bu_log("%s", id);

    while (!done) {
	while ((num[++i] = card[counter++]) != 'H' &&
	       counter <= lencard);
	if (counter > lencard)
	    Readrec(++currec);
	if (num[i] == 'H')
	    done = 1;
    }
    num[++i] = '\0';
    length = atoi(num);
    for (i = 0; i < length; i++) {
	if (counter > lencard)
	    Readrec(++currec);
	if (*id != '\0')
	    bu_log("%c", card[counter]);
	counter++;
    }
    if (*id != '\0')
	bu_log("%c", '\n');

    while (card[counter] != eof && card[counter] != eor) {
	if (counter < lencard)
	    counter++;
	else
	    Readrec(++currec);
    }

    if (card[counter] == eof) {
	counter++;
	if (counter > lencard)
	    Readrec(++ currec);
    }
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
