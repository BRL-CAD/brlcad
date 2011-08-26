/*                   O B J _ G R A M M A R . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file obj_grammar.h
 *
 * Necessary declarations to parse grammar.
 *
 */

#ifndef OBJ_GRAMMAR_H
#define OBJ_GRAMMAR_H

#include "common.h"
#include "obj_grammar.hpp"
#include "obj_rules.h"
#include "obj_token_type.h"

__BEGIN_DECLS

void *ParseAlloc(void *(*mallocProc)(size_t));
void Parse(void *parser, int tokenType, YYSTYPE tokenValue, yyscan_t scanner);
void ParseFree(void *p, void (*freeProc)(void*));
void ParseTrace(FILE *stream, char *prefix);

__END_DECLS

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
