/*                         C O U N T . C
 * BRL-CAD
 *
 * Copyright (C) 2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file count.c
 *
 * Basic counting structure and functions.
 *
 * Author -
 *   Christopher Sean Morrison
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h> /* for isspace() */

#include "machine.h"
#include "bu.h"

#include "./count.h"


static counter_t *counter = NULL;

static void incr_token(id)
{
    token_t *t;
    
    /* allocate and initialize on first use */
    if (!counter) {
	counter = bu_malloc(sizeof(counter_t), "count()");
	INIT_COUNTER_T(*counter);
    }

    t = &(counter->token);

    do {
	/* found existing */
	if (t->id == id) {
	    t->count++;
	    return;
	}

	/* add new */
	if (t->next == NULL) {
	    t->next = bu_calloc(1, sizeof(token_t), "incr_token()");
	    t->next->id = id;
	    t->next->count = 1;
	    return;
	}

	t = t->next;
    } while (t != (token_t*)NULL);
}


void count(long int id, const char *text)
{
    int i;
    static char previous = 0;

    /* allocate and initialize on first use */
    if (!counter) {
	counter = bu_malloc(sizeof(counter_t), "count()");
	INIT_COUNTER_T(*counter);
    }

    for (i=0; text[i] != '\0'; i++) {
	if ((text[i] == '\n') || (text[i] == '\r')) {
	    counter->lines++;
	    counter->column = 0;
	} else if (text[i] == '\t') {
	    counter->column += 8 - ((counter->column-1) % 8);
	} else {
	    counter->column++;
	}

	if (isspace(text[i]) && !isspace(previous)) {
	    counter->words++;
	}

	counter->bytes++;
	previous = text[i];
    }

    incr_token(id);
}


void freecount()
{
    token_t *t = (token_t*)NULL;
    token_t *next = (token_t*)NULL;

    if (!counter) {
	return;
    }

    t = (counter->token).next;
    
    while (t != (token_t*)NULL) {
	next = t->next;
	bu_free(t, "freecount()");
	t = next;
    }
    
    bu_free(counter, "freecount()");
}
    
    

long int get_column()
{
    if (counter)
	return counter->column;
    return 0;
}

long int get_bytes()
{
    if (counter)
	return counter->bytes;
    return 0;
}

long int get_words()
{
    if (counter)
	return counter->words;
    return 0;
}

long int get_lines()
{
    if (counter)
	return counter->lines;
    return 0;
}

long int count_get_token(long int id)
{
    token_t *t;

    if (!counter) {
	return 0;
    }

    for (t = &(counter->token); t != (token_t*)NULL; t = t->next) {
	if (t->id == id) {
	    return t->count;
	}
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
