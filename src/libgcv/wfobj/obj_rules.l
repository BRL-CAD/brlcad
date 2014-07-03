/*                   O B J _ R U L E S . R E
 * Copyright (c) 2010-2014 United States Government as represented by
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>

#include "bu.h"
#include "bio.h"
#include "obj_grammar_decls.h"
#include "obj_parser_state.h"
#include "obj_rules.h"

namespace cad {
namespace gcv {
namespace obj {

/**
 *  convenience routines
 */
template<typename T>
inline static objCombinedState::parser_state_type &get_state(T combinedState)
{
    return static_cast<objCombinedState*>(combinedState)->parser_state;
}

/**
 *  Local functions
 */
static bool split_reference(const char *s, int val[3]);

} /* namespace obj */
} /* namespace gcv */
} /* namespace cad */

using namespace cad::gcv;

void *obj_parser_get_extra(yyscan_t scanner)
{
    return perplexGetExtra(scanner);
}

void *obj_parser_get_state(yyscan_t scanner)
{
    return static_cast<struct extra_t*>(perplexGetExtra(scanner))->state;
}

void obj_parser_set_extra(yyscan_t scanner, void *extra)
{
    perplexSetExtra(scanner, extra);
}

void obj_parser_lex_destroy(yyscan_t scanner)
{
    perplexFree(scanner);
}

%%
vertex       = "v";
t_vertex     = "vt";
n_vertex     = "vn";
point        = "p";
line         = "l";
face         = "f";
group        = "g";
object       = "o";
smooth       = "s";
integer      = [+-]?([0-9]+);
dseq         = ([0-9]+);
dseq_opt     = ([0-9]*);
frac         = (((dseq_opt)"."(dseq))|(dseq)".");
exp          = ([eE][+-]?(dseq));
exp_opt      = ((exp)?);
fsuff        = [flFL];
fsuff_opt    = ((fsuff)?);
hpref        = ('0'[xX]);
hdseq        = ([0-9]+);
hdseq_opt    = ([0-9]*);
hfrac        = (((hdseq_opt)"."(hdseq))|((hdseq)"."));
bexp         = ([pP][+-]?(dseq));
dfc          = (((frac)(exp_opt)(fsuff_opt))|((dseq)(exp)(fsuff_opt)));
hfc          = (((hpref)(hfrac)(bexp)(fsuff_opt))|((hpref)(hdseq)(bexp)(fsuff_opt)));
real         = [+-]?((dfc)|(hfc));
usemtl       = "usemtl";
mtllib       = "mtllib";
usemap       = "usemap";
maplib       = "maplib";
bevel        = "bevel";
c_interp     = "c_interp";
d_interp     = "d_interp";
lod          = "lod";
shadow_obj   = "shadow_obj";
trace_obj    = "trace_obj";
off          = "off";
on           = "on";
v_reference  = (integer)"/""/"?;
v_tv_reference  = (integer)"/"(integer);
v_nt_reference  = (integer)"//"(integer);
v_tnv_reference_list  = (integer)"/"(integer)"/"(integer);

wspace       = [ \t];
id           = [!-~]+;
newline      = ["\r\n""\n"];
comment      = "#"[^"\r\n""\n"]*(newline);

<INITIAL> {

vertex            { return VERTEX; }
t_vertex          { return T_VERTEX; }
n_vertex          { return N_VERTEX; }
point             { return POINT; }
line              { return LINE; }
face              { return FACE; }
group => id_list_state {
    return GROUP;
}
object => id_state {
    return OBJECT;
}
smooth            {
    return SMOOTH;
}
integer           {
    yylval->integer = atoi(yytext);
    return INTEGER;
}
real              {
    yylval->real = atof(yytext);
    return FLOAT;
}
usemtl => id_state {
    return USEMTL;
}
mtllib => id_list_state {
    return MTLLIB;
}
usemap => toggle_id_state {
    return USEMAP;
}
maplib => id_list_state {
    return MAPLIB;
}
bevel             {
    return BEVEL;
}
c_interp {
    return C_INTERP;
}
d_interp {
    return D_INTERP;
}
lod {
    return LOD;
}
shadow_obj => id_state {
    return SHADOW_OBJ;
}
trace_obj => id_state {
    return TRACE_OBJ;
}
on {
    return ON;
}
off {
    return OFF;
}
v_reference {
    if (obj::split_reference(yytext, yylval->reference))
	return V_REFERENCE;
    return YYEOF;
}
v_tv_reference {
    if (obj::split_reference(yytext, yylval->reference))
	return TV_REFERENCE;
    return YYEOF;
}
v_nt_reference {
    if (obj::split_reference(yytext, yylval->reference))
	return NV_REFERENCE;
    return YYEOF;
}
v_tnv_reference_list {
    if (obj::split_reference(yytext, yylval->reference))
	return TNV_REFERENCE;
    return YYEOF;
}

wspace { IGNORE_TOKEN; }

comment|newline {
    ++(obj::get_state(combinedState).file_stack.back().lineno);
    return EOL;
}

. { IGNORE_TOKEN; }

} /* INITIAL */

<id_state> {
id => INITIAL {
    /* Keywords are valid identifiers here
     * Goto initial state after single token
     */
    obj::get_state(combinedState).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    return ID;
}

wspace { IGNORE_TOKEN; }

comment|newline => INITIAL {
    /* Goto initial state when we hit newline */
    ++(obj::get_state(combinedState).file_stack.back().lineno);
    return EOL;
}

. { IGNORE_TOKEN; }

} /* id_state */

<toggle_id_state> {
off => INITIAL {
    // off is a valid token, not an id
    return OFF;
}
id => INITIAL {
    /* Keywords are valid identifiers here
     * Goto initial state after single token
     */
    obj::get_state(combinedState).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    return ID;
}

wspace { IGNORE_TOKEN; }

comment|newline => INITIAL {
    // Goto initial state when we hit newline
    ++(obj::get_state(combinedState).file_stack.back().lineno);
    return EOL;
}

. { IGNORE_TOKEN; }

} /* toggle_id_state */

<id_list_state> {
id {
    // Keywords are valid identifiers here
    obj::get_state(combinedState).working_string = yytext;
    bu_strlcpy(yylval->string, yytext, TOKEN_STRING_LEN);

    return ID;
}

wspace { IGNORE_TOKEN; }

comment|newline => INITIAL {
    // Goto initial state when we hit newline
    ++(obj::get_state(combinedState).file_stack.back().lineno);
    return EOL;
}

. { IGNORE_TOKEN; }

} /* id_list_state */

%%
namespace cad {
namespace gcv {
namespace obj {

bool split_reference(const char *s, int val[3])
{
    memset(val, 0, sizeof(int)*3);

    char *endptr;
    val[0] = strtol(s, &endptr, 0);
    if (*endptr == 0)
	return true;

    if (*endptr != '/')
	return false;
    ++endptr;

    val[1] = strtol(endptr, &endptr, 0);
    if (*endptr == 0)
	return true;

    if (*endptr != '/')
	return false;
    ++endptr;

    val[2] = strtol(endptr, &endptr, 0);

    return (*endptr == 0);
}

} /* namespace obj */
} /* namespace gcv */
} /* namespace cad */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
