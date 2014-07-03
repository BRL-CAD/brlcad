/*                           L E X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
#include <ctype.h>
#include <string.h>
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

static int bu_lex_reading_comment = 0;


HIDDEN char *
lex_getone(int *used, struct bu_vls *rtstr)
{
    register char *cp;
    register char *sp;
    register char tc;
    register char *unit;
    int number;

    number = 1;
    *used = 0;

    BU_CK_VLS(rtstr);
    cp = bu_vls_addr(rtstr);
    while (1) {
	if (bu_lex_reading_comment) {
	    do {
		/* looking at two consecutive chars */
		tc = *cp; cp++;
		if (!tc) {
		    return 0;
		}
	    } while (!(tc == '*' && *cp == '/'));
	    cp++;   /* skip the '/' */
	    bu_lex_reading_comment = 0;
	}
	/* skip leading blanks */
	while (*cp && isspace((int)(*cp))) cp++;
	/* is this a comment? '#' to end of line is */
	if (!*cp || *cp == '#') {
	    return 0;

	}
	/* is this a 'C' multi-line comment? */
	if (*cp == '/' && *(cp+1) == '*') {
	    cp += 2;
	    bu_lex_reading_comment = 1;
	} else /* we are done reading comments */ {
	    break;
	}
    }
    /* cp points to the first non-blank character */
    sp = cp;		/* start pointer */
    while (*cp) {

	tc = *cp; cp++;
	/*
	 * Numbers come in the following forms
	 * [0-9]*
	 * [0-9]*.[0-9][0-9]*
	 * [0-9]*.[0-9][0-9]*{e|E}{+|-}[0-9][0-9]*
	 */
	if (number) {
	    /*
	     * We have not seen anything to make this NOT
	     * a number.
	     */
	    if (isdigit((int)tc)) {
		if (number == 5 || number == 6) number = 7;
		if (number == 3) number = 4;
		if (number == 1) number = 2;
		continue;
	    }
	    if (number == 2 && tc == '.') {
		/*
		 * [0-9][0-9]*.
		 */
		number = 3;
		continue;
	    }
	    if (number == 4 && (tc == 'e' || tc == 'E')) {
		/*
		 * [0-9][0-9]*.[0-9][0-9]*{e|E}
		 */
		number = 5;
		continue;
	    }
	    if (number == 5 && (tc == '+' || tc == '-')) {
		/*
		 * [0-9][0-9]*.[0-9][0-9]*{e|E}{+|-}
		 */
		number = 6;
		continue;
	    }
	    if (number == 3) break;
	    number = 0;
	}
	if (!isalnum((int)tc) && tc != '.' && tc != '_') break;
    }
    if (number == 6) --cp;	/* subtract off the + or - */
    if (number == 3) --cp;  /* subtract off the . */
    /*
     * All spaces have been skipped. (sp)
     * if we had NUMBER. or NUMBERe{+|-} that has be replaced (cp)
     */
    *used = cp - sp -1;
    if (*used == 0)
	*used = 1;

    unit = (char *)bu_malloc((size_t)(*used+1), "unit token");
    bu_strlcpy(unit, sp, (size_t)(*used+1));
    *used = sp-bu_vls_addr(rtstr) + *used;

    if (*used == 0)
	*used = 1;

    return unit;
}


int
bu_lex(
    union bu_lex_token *token,
    struct bu_vls *rtstr,
    struct bu_lex_key *keywords,
    struct bu_lex_key *symbols)
{
    char *unit;
    char *cp;
    int used;

    /*
     * get a unit of information from rtstr.
     */
    used = 0;
    unit = lex_getone(&used, rtstr);

    /*
     * Was line empty or commented out.
     */
    if (!unit) {
	if (used) bu_bomb("bu_lex: Null unit, and something used.\n");
	return BU_LEX_NEED_MORE;
    }

    /*
     * Decide if this unit is a symbol, number or identifier.
     */
    if (isdigit((int)(*unit))) {
	/*
	 * Humm, this could be a number.
	 * octal -- 0[0-7]*
	 * hex   -- 0x[0-9a-f]*
	 * dec   -- [0-9][0-9]*
	 * dbl   -- [0-9][0-9]*.[0-9]*{{E|e}{+|-}[0-9][0-9]*}
	 */
	if (*unit == '0') {
	    /* any of the above */
	    /*
	     * octal -- 0[0-7]*
	     */
	    for (cp=unit; *cp && *cp>='0' && *cp <='7'; cp++);
	    if (!*cp) {
		/* We have an octal value */
		token->type = BU_LEX_INT;
		sscanf(unit, "%o", (unsigned int *)&token->t_int.value);
		bu_free(unit, "unit token");
		return used;
	    }
	    /*
	     * if it is not an octal number, maybe it is
	     * a hex number?"
	     * hex   -- 0x[0-9a-f]*
	     */
	    cp=unit+1;
	    if (*cp == 'x' || *cp == 'X') {
		for (;*cp && isxdigit((int)(*cp));cp++);
		if (!*cp) {
		    token->type = BU_LEX_INT;
		    sscanf(unit, "%x", (unsigned int *)&token->t_int.value);
		    bu_free(unit, "unit token");
		    return used;
		}
	    }
	}
	/*
	 * This could be a decimal number, a double or an identifier.
	 * dec   -- [0-9][0-9]*
	 */
	for (cp=unit; *cp && isdigit((int)(*cp)); cp++);
	if (!*cp) {
	    token->type = BU_LEX_INT;
	    sscanf(unit, "%d", &token->t_int.value);
	    bu_free(unit, "unit token");
	    return used;
	}
	/*
	 * if we are here, then this is either a double or
	 * an identifier.
	 * dbl   -- [0-9][0-9]*.[0-9]*{{E|e}{+|-}[0-9][0-9]*}
	 *
	 * *cp should be a '.'
	 */
	if (*cp == '.') {
	    for (cp++;*cp &&isdigit((int)(*cp));cp++);
	    if (*cp == 'e' || *cp == 'E') cp++;
	    if (*cp == '+' || *cp == '-') cp++;
	    for (;*cp &&isdigit((int)(*cp));cp++);
	    if (!*cp) {
		token->type = BU_LEX_DOUBLE;
		sscanf(unit, "%lg", &token->t_dbl.value);
		bu_free(unit, "unit token");
		return used;
	    }
	}
	/*
	 * Oh well, I guess it was not a number.  That means it
	 * must be something else.
	 */
    }
    /*
     * We either have an identifier, keyword, or symbol.
     */
    if (symbols) {
	if (!*(unit+1)) {
	    /* single character, good choice for a symbol. */
	    register struct bu_lex_key *sp;
	    for (sp=symbols;sp->tok_val;sp++) {
		if (*sp->string == *unit) {
		    token->type = BU_LEX_SYMBOL;
		    token->t_key.value = sp->tok_val;
		    bu_free(unit, "unit token");
		    return used;
		}
	    }
	}
    }
    if (keywords) {
	register struct bu_lex_key *kp;
	for (kp=keywords;kp->tok_val; kp++) {
	    if (BU_STR_EQUAL(kp->string, unit)) {
		token->type = BU_LEX_KEYWORD;
		token->t_key.value = kp->tok_val;
		bu_free(unit, "unit token");
		return used;
	    }
	}
    }
    token->type = BU_LEX_IDENT;
    token->t_id.value = unit;
    return used;
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
