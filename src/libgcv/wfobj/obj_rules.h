/*                     O B J _ R U L E S . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
/** @file obj_rules.h
 *
 * Necessary declarations to use re2c scanner.
 *
 */

#ifndef LIBGCV_WFOBJ_OBJ_RULES_H
#define LIBGCV_WFOBJ_OBJ_RULES_H

#include "common.h"
#include "obj_token_type.h"

enum YYCONDTYPE {
    INITIAL,
    id_state,
    toggle_id_state,
    id_list_state
};
#define CONDTYPE enum YYCONDTYPE

__BEGIN_DECLS

#define PERPLEX_LEXER obj_parser_lex

#define PERPLEX_ON_ENTER \
    using obj::objCombinedState; \
    struct extra_t *extra = static_cast<struct extra_t*>(yyextra); \
    YYSTYPE *yylval = &extra->tokenData; \
    objCombinedState *combinedState = static_cast<objCombinedState*>(extra->state);

#include "obj_scanner.h"

typedef perplex_t yyscan_t;

struct extra_t {
    void *state;
    YYSTYPE tokenData;
};

void obj_parser_lex_destroy(yyscan_t scanner);
void *obj_parser_get_state(yyscan_t scanner);
void *obj_parser_get_extra(yyscan_t scanner);
void obj_parser_set_extra(yyscan_t scanner, void *extra);

__END_DECLS

#endif /* LIBGCV_WFOBJ_OBJ_RULES_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
