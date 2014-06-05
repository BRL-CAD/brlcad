/*                  O B J _ G R A M M A R . Y Y
 * BRL-CAD
 *
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
%include {
/*                  O B J _ G R A M M A R . Y Y
 * BRL-CAD
 *
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

#include "obj_parser.h"
#include "obj_parser_state.h"
#include "obj_grammar_decls.h"
#include "obj_rules.h"

#include <assert.h>
#include <iostream>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <vector>
#include <set>
#include <string>
#include <cstddef>
#include <sys/types.h>

#define SET_SYNTAX_ERROR \
    static_cast<obj::objCombinedState*> \
	(perplexGetExtra(scanner))->parser_state.syntaxError = true;

#define YYERROR SET_SYNTAX_ERROR

/**
 *  Use namespaces here to avoid multiple symbol name clashes
 */
void obj_parser_error(yyscan_t scanner, const char *s);

namespace cad {
namespace gcv {
namespace obj {

static const char vertex_ref_err[] = "Invalid vertex reference";
static const char texture_ref_err[] = "Invalid texture reference";
static const char normal_ref_err[] = "Invalid normal reference";
static const char lod_range_err[] = "LOD value must be between 0 and 100";
static const char smooth_range_err[] = "negative smoothing group id";
static const char face_length_err[] =
    "Faces must contain at least 3 references";
static const char line_length_err[] =
    "Lines must contain at least 2 references";

typedef objCombinedState::contents_type contents_type;
typedef objCombinedState::parser_state_type parser_state_type;

/**
 *  convenience routines
 */
inline static parser_state_type & get_state(yyscan_t scanner)
{
    return static_cast<objCombinedState*>(obj_parser_get_state(scanner))->
	parser_state;
}

inline static objCombinedState::basic_parser_type &get_parser(yyscan_t scanner)
{
    return *(static_cast<objCombinedState*>(obj_parser_get_state(scanner))->
	    basic_parser);
}

inline static contents_type & get_contents(yyscan_t scanner)
{
    return *(static_cast<objCombinedState*>(obj_parser_get_state(scanner))->
	    contents);
}

inline static objCombinedState & get_extra(yyscan_t scanner)
{
    return *static_cast<objCombinedState*>(obj_parser_get_state(scanner));
}

inline static size_t real_index(int val, std::size_t nvert)
{
    return ((val < 0) ? (nvert - size_t(std::abs(val))) : (size_t(val - 1)));
}

template<typename charT>
static bool index_check(int raw, std::size_t index,
			size_t vertices, const charT *log,
			yyscan_t scanner)
{
    if (!raw || index >= vertices) {
	std::string err;
        char buf[256];
	// stringstream gives internal compiler error with gcc 4.2, so
	// using snprintf instead.
        snprintf(buf, sizeof(buf), "index '%d': ", raw);
        err = buf;
        err += log;
        obj_parser_error(scanner, err.c_str());
        return true;
    }
    return false;
}

} /* namespace obj */
} /* namespace gcv */
} /* namespace cad */

using namespace cad::gcv;

/**
 *  Error reporting function as required by yacc
 */
void obj_parser_error(yyscan_t scanner, const char *s)
{
    if (!obj::get_state(scanner).syntaxError) {
	obj::verbose_output_formatter(obj::get_state(scanner), s);
    }
}

/* trying to be reentrant, so no static/global non-constant data
 * and no calls to non-reentrant functions
 *
 * returns 1 on syntax error, 0 otherwise
 * returns error message in
 *     ((objCombinedState*)perplexGetExtra(scanner))->parser_state.err.str()
 */
int obj_parser_parse(yyscan_t scanner)
{
    using obj::objCombinedState;
    using obj::parser_type;

    int yychar;
    struct extra_t *extra = static_cast<struct extra_t*>(perplexGetExtra(scanner));
    YYSTYPE *tokenData = &extra->tokenData;
    objCombinedState *state = static_cast<objCombinedState*>(extra->state);
    parser_type parser = state->parser;
    bool &error = state->parser_state.syntaxError;

    error = false;

    while ((yychar = obj_parser_lex(scanner)) != YYEOF) {
	Parse(parser, yychar, *tokenData, scanner);
    }

    if (error) {
	return -1;
    }

    return 0;
}

void printToken(YYSTYPE token)
{
    std::cerr << "{";
    std::cerr << "\n    real: " << token.real;
    std::cerr << "\n    integer: " << token.integer;
    std::cerr << "\n    reference: {";
    std::cerr << token.reference[0] << ", ";
    std::cerr << token.reference[1] << ", ";
    std::cerr << token.reference[2] << "}";
    std::cerr << "\n    toggle: " << token.toggle;
    std::cerr << "\n    index: " << token.index;
    std::cerr << "\n    string: " << token.string;
    std::cerr << "\n}" << std::endl;
}

} /* include */

%extra_argument { yyscan_t scanner }

%destructor statement_list {
    if (UNLIKELY(scanner == NULL)) {
	$$.integer = 0;
    }
}

%stack_overflow {
    std::cerr << "Error: Parser experienced stack overflow. Last token was:\n";
    printToken(yypMinor->yy0);
}

%token_type {YYSTYPE}

%syntax_error {
    /* only report first error */
    if (!obj::get_state(scanner).syntaxError) {
	SET_SYNTAX_ERROR;

	std::cerr << "Error: Parser experienced a syntax error.\n";
	std::cerr << "Last token (type " << yymajor << ") was:\n";
	printToken(yyminor.yy0);
    }
}

start ::= statement_list.

statement_list ::= EOL.
statement_list ::= statement EOL.
statement_list ::= statement_list EOL.
statement_list ::= statement_list statement EOL.

statement ::= vertex.
statement ::= t_vertex.
statement ::= n_vertex.
statement ::= point.
statement ::= line.
statement ::= face.
statement ::= group.
statement ::= smooth.
statement ::= object.
statement ::= usemtl.
statement ::= mtllib.
statement ::= usemap.
statement ::= maplib.
statement ::= shadow_obj.
statement ::= trace_obj.
statement ::= bevel.
statement ::= c_interp.
statement ::= d_interp.
statement ::= lod.

coord(A) ::= FLOAT(B). { A.real = B.real; }
coord(A) ::= INTEGER(B). { A.real = B.integer; }

vertex ::= VERTEX coord(A) coord(B) coord(C) opt_color.
{
    obj::objFileContents::gvertex_t vertex = {{A.real, B.real, C.real, 1}};
    obj::get_contents(scanner).gvertices_list.push_back(vertex);
}
vertex ::= VERTEX coord(A) coord(B) coord(C) coord(D) opt_color.
{
    obj::objFileContents::gvertex_t vertex =
	{{A.real, B.real, C.real, D.real}};

    obj::get_contents(scanner).gvertices_list.push_back(vertex);
}

/* Per-vertex colors are a non-standard OBJ extension. Accept but ignore
 * them.
 */
opt_color ::= /* empty */.
opt_color ::= coord coord coord.

t_vertex ::= T_VERTEX coord(A).
{
    obj::objFileContents::tvertex_t vertex = {{A.real, 0, 0}};
    obj::get_contents(scanner).tvertices_list.push_back(vertex);
}
t_vertex ::= T_VERTEX coord(A) coord(B).
{
    obj::objFileContents::tvertex_t vertex = {{A.real, B.real, 0}};
    obj::get_contents(scanner).tvertices_list.push_back(vertex);
}
t_vertex ::= T_VERTEX coord(A) coord(B) coord(C).
{
    obj::objFileContents::tvertex_t vertex = {{A.real, B.real, C.real}};
    obj::get_contents(scanner).tvertices_list.push_back(vertex);
}

n_vertex ::= N_VERTEX coord(A) coord(B) coord(C).
{
    obj::objFileContents::nvertex_t vertex = {{A.real, B.real, C.real}};
    obj::get_contents(scanner).nvertices_list.push_back(vertex);
}

p_v_reference_list(A) ::= INTEGER(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.integer, num_gvertices);

    if (obj::index_check(B.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).point_v_indexlist.size();
    obj::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
p_v_reference_list(A) ::= V_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.reference[0], num_gvertices);

    if (obj::index_check(B.reference[0], gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).point_v_indexlist.size();
    obj::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
p_v_reference_list(A) ::= p_v_reference_list(B) INTEGER(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.integer, num_gvertices);

    if (obj::index_check(C.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
p_v_reference_list(A) ::= p_v_reference_list(B) V_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.reference[0], num_gvertices);

    if (obj::index_check(C.reference[0], gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).point_v_indexlist.push_back(gindex);
}

l_v_reference_list(A) ::= INTEGER(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.integer, num_gvertices);

    if (obj::index_check(B.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).line_v_indexlist.size();
    obj::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
l_v_reference_list(A) ::= V_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.reference[0], num_gvertices);

    if (obj::index_check(B.reference[0], gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).line_v_indexlist.size();
    obj::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
l_v_reference_list(A) ::= l_v_reference_list(B) INTEGER(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.integer, num_gvertices);

    if (obj::index_check(C.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
l_v_reference_list(A) ::= l_v_reference_list(B) V_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.reference[0], num_gvertices);

    if (obj::index_check(C.reference[0], gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).line_v_indexlist.push_back(gindex);
}

l_tv_reference_list(A) ::= TV_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    obj::contents_type::polygonal_tv_index_type tv_index = {{
	obj::real_index(B.reference[0], num_gvertices),
	obj::real_index(B.reference[1], num_tvertices)
    }};

    if (obj::index_check(B.reference[0], tv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(B.reference[1], tv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).line_tv_indexlist.size();
    obj::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
}
l_tv_reference_list(A) ::= l_tv_reference_list(B) TV_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    obj::contents_type::polygonal_tv_index_type tv_index = {{
	obj::real_index(C.reference[0], num_gvertices),
	obj::real_index(C.reference[1], num_tvertices)
    }};

    if (obj::index_check(C.reference[0], tv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(C.reference[1], tv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
}

f_v_reference_list(A) ::= INTEGER(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.integer, num_gvertices);

    if (obj::index_check(B.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).pologonal_v_indexlist.size();
    obj::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
f_v_reference_list(A) ::= V_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(B.reference[0], num_gvertices);

    if (obj::index_check(B.reference[0], gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).pologonal_v_indexlist.size();
    obj::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
f_v_reference_list(A) ::= f_v_reference_list(B) INTEGER(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.integer, num_gvertices);

    if (obj::index_check(C.integer, gindex, num_gvertices,
		obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
f_v_reference_list(A) ::= f_v_reference_list(B) V_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    obj::contents_type::polygonal_v_index_type gindex =
	obj::real_index(C.reference[0], num_gvertices);

    if (obj::index_check(C.reference[0], gindex, num_gvertices,
	obj::vertex_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}

f_tv_reference_list(A) ::= TV_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    obj::contents_type::polygonal_tv_index_type tv_index = {{
	obj::real_index(B.reference[0], num_gvertices),
	obj::real_index(B.reference[1], num_tvertices)
    }};

    if (obj::index_check(B.reference[0], tv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(B.reference[1], tv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).pologonal_tv_indexlist.size();
    obj::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
}
f_tv_reference_list(A) ::= f_tv_reference_list(B) TV_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    obj::contents_type::polygonal_tv_index_type tv_index = {{
	obj::real_index(C.reference[0], num_gvertices),
	obj::real_index(C.reference[1], num_tvertices)
    }};

    if (obj::index_check(C.reference[0], tv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(C.reference[1], tv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
}

f_nv_reference_list(A) ::= NV_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_nvertices =
	obj::get_contents(scanner).nvertices_list.size();

    obj::contents_type::polygonal_nv_index_type nv_index = {{
	obj::real_index(B.reference[0], num_gvertices),
	obj::real_index(B.reference[2], num_nvertices)
    }};

    if (obj::index_check(B.reference[0], nv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(B.reference[2], nv_index.v[1], num_nvertices,
	obj::normal_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).pologonal_nv_indexlist.size();
    obj::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
}
f_nv_reference_list(A) ::= f_nv_reference_list(B) NV_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_nvertices =
	obj::get_contents(scanner).nvertices_list.size();

    obj::contents_type::polygonal_nv_index_type nv_index = {{
	obj::real_index(C.reference[0], num_gvertices),
	obj::real_index(C.reference[2], num_nvertices)
    }};

    if (obj::index_check(C.reference[0], nv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(C.reference[2], nv_index.v[1], num_nvertices,
	obj::normal_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
}

f_tnv_reference_list(A) ::= TNV_REFERENCE(B).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    std::size_t num_nvertices =
	obj::get_contents(scanner).nvertices_list.size();

    obj::contents_type::polygonal_tnv_index_type tnv_index = {{
	obj::real_index(B.reference[0], num_gvertices),
	obj::real_index(B.reference[1], num_tvertices),
	obj::real_index(B.reference[2], num_nvertices)
    }};

    if (obj::index_check(B.reference[0], tnv_index.v[0], num_gvertices,
		obj::vertex_ref_err, scanner) ||
	obj::index_check(B.reference[1], tnv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner) ||
	obj::index_check(B.reference[2], tnv_index.v[2], num_nvertices,
	obj::normal_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = obj::get_contents(scanner).pologonal_tnv_indexlist.size();
    obj::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
}
f_tnv_reference_list(A) ::= f_tnv_reference_list(B) TNV_REFERENCE(C).
{
    std::size_t num_gvertices =
	obj::get_contents(scanner).gvertices_list.size();

    std::size_t num_tvertices =
	obj::get_contents(scanner).tvertices_list.size();

    std::size_t num_nvertices =
	obj::get_contents(scanner).nvertices_list.size();

    obj::contents_type::polygonal_tnv_index_type tnv_index = {{
	obj::real_index(C.reference[0], num_gvertices),
	obj::real_index(C.reference[1], num_tvertices),
	obj::real_index(C.reference[2], num_nvertices)
    }};

    if (obj::index_check(C.reference[0], tnv_index.v[0], num_gvertices,
	obj::vertex_ref_err, scanner) ||
	obj::index_check(C.reference[1], tnv_index.v[1], num_tvertices,
	obj::texture_ref_err, scanner) ||
	obj::index_check(C.reference[2], tnv_index.v[2], num_nvertices,
	obj::normal_ref_err, scanner))
    {
	YYERROR;
    }

    A.index = B.index;
    obj::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
}

point ::= POINT p_v_reference_list(A).
{
    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }
    obj::get_contents(scanner).point_v_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).point_v_loclist.
	resize(obj::get_contents(scanner).point_v_loclist.size() + 1);

    obj::get_contents(scanner).point_v_loclist.back().first = A.index;

    obj::get_contents(scanner).point_v_loclist.back().second =
	obj::get_contents(scanner).point_v_indexlist.size() - A.index;
}

line ::= LINE l_v_reference_list(A).
{
    if (obj::get_contents(scanner).line_v_indexlist.size() - A.index < 2) {
	obj_parser_error(scanner, obj::line_length_err);
	YYERROR;
    }

    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }
    obj::get_contents(scanner).line_v_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).line_v_loclist.
	resize(obj::get_contents(scanner).line_v_loclist.size() + 1);

    obj::get_contents(scanner).line_v_loclist.back().first = A.index;

    obj::get_contents(scanner).line_v_loclist.back().second =
	obj::get_contents(scanner).line_v_indexlist.size() - A.index;
}
line ::= LINE l_tv_reference_list(A).
{
    if (obj::get_contents(scanner).line_tv_indexlist.size() - A.index < 3) {
	obj_parser_error(scanner, obj::line_length_err);
	YYERROR;
    }

    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }
    obj::get_contents(scanner).line_tv_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).line_tv_loclist.
	resize(obj::get_contents(scanner).line_tv_loclist.size() + 1);

    obj::get_contents(scanner).line_tv_loclist.back().first = A.index;

    obj::get_contents(scanner).line_tv_loclist.back().second =
	obj::get_contents(scanner).line_tv_indexlist.size() - A.index;
}

face ::= FACE f_v_reference_list(A).
{
    using obj::get_contents;
    using obj::get_state;
    using obj::get_extra;
    using obj::set_working_polygattributes;
    using obj::face_length_err;
    using obj::contents_type;
    using obj::parser_state_type;

    contents_type &contents = get_contents(scanner);
    parser_state_type &state = get_state(scanner);

    size_t numReferences = contents.pologonal_v_indexlist.size() - A.index;

    if (numReferences < 3) {
	obj_parser_error(scanner, face_length_err);
	YYERROR;
    }

    if (state.polyattributes_dirty) {
	set_working_polygattributes(get_extra(scanner));
    }

    contents.polygonal_v_attr_list.push_back(state.current_polyattributes);

    /* add new */
    size_t currSize = contents.polygonal_v_loclist.size();

    contents.polygonal_v_loclist.resize(currSize + 1);

    contents.polygonal_v_loclist.back().first = A.index;
    contents.polygonal_v_loclist.back().second = numReferences;
}
face ::= FACE f_tv_reference_list(A).
{
    if (obj::get_contents(scanner).pologonal_tv_indexlist.size() -
	A.index < 3)
    {
	obj_parser_error(scanner, obj::face_length_err);
	YYERROR;
    }

    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }
    obj::get_contents(scanner).polygonal_tv_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).polygonal_tv_loclist.
	resize(obj::get_contents(scanner).polygonal_tv_loclist.size() + 1);

    obj::get_contents(scanner).polygonal_tv_loclist.back().first = A.index;

    obj::get_contents(scanner).polygonal_tv_loclist.back().second =
	obj::get_contents(scanner).pologonal_tv_indexlist.size() - A.index;
}
face ::= FACE f_nv_reference_list(A).
{
    if (obj::get_contents(scanner).pologonal_nv_indexlist.size() -
	A.index < 3)
    {
	obj_parser_error(scanner, obj::face_length_err);
	YYERROR;
    }

    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }

    obj::get_contents(scanner).polygonal_nv_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).polygonal_nv_loclist.
	resize(obj::get_contents(scanner).polygonal_nv_loclist.size() + 1);

    obj::get_contents(scanner).polygonal_nv_loclist.back().first =
	A.index;

    obj::get_contents(scanner).polygonal_nv_loclist.back().second =
	obj::get_contents(scanner).pologonal_nv_indexlist.size() - A.index;
}
face ::= FACE f_tnv_reference_list(A).
{
    if (obj::get_contents(scanner).pologonal_tnv_indexlist.size() -
	A.index < 3)
    {
	obj_parser_error(scanner, obj::face_length_err);
	YYERROR;
    }

    if (obj::get_state(scanner).polyattributes_dirty) {
	obj::set_working_polygattributes(obj::get_extra(scanner));
    }
    obj::get_contents(scanner).polygonal_tnv_attr_list.
	push_back(obj::get_state(scanner).current_polyattributes);

    obj::get_contents(scanner).polygonal_tnv_loclist.
	resize(obj::get_contents(scanner).polygonal_tnv_loclist.size()+1);

    obj::get_contents(scanner).polygonal_tnv_loclist.back().first =
	A.index;

    obj::get_contents(scanner).polygonal_tnv_loclist.back().second =
	obj::get_contents(scanner).pologonal_tnv_indexlist.size() - A.index;
}

group ::= GROUP id_list.
{
    obj::set_working_groupset(obj::get_extra(scanner));
}

smooth ::= SMOOTH INTEGER(A).
{
    if (A.integer < 0) {
	obj_parser_error(scanner, obj::smooth_range_err);
	YYERROR;
    }

    if (obj::get_state(scanner).working_polyattributes.smooth_group !=
	static_cast<unsigned int>(A.integer))
    {
	obj::get_state(scanner).working_polyattributes.smooth_group =
	    static_cast<unsigned int>(A.integer);

	obj::get_state(scanner).polyattributes_dirty = true;
    }
}
smooth ::= SMOOTH OFF.
{
    if (obj::get_state(scanner).working_polyattributes.smooth_group != 0) {
	obj::get_state(scanner).working_polyattributes.smooth_group = 0;
	obj::get_state(scanner).polyattributes_dirty = true;
    }
}

object ::= OBJECT.
{
    obj::get_state(scanner).working_string.clear();
    obj::set_working_object(obj::get_extra(scanner));
}
object ::= OBJECT ID.
{
    obj::set_working_object(obj::get_extra(scanner));
}

usemtl ::= USEMTL ID.
{
    obj::set_working_material(obj::get_extra(scanner));
}

mtllib ::= MTLLIB id_list.
{
    obj::set_working_materiallib(obj::get_extra(scanner));
}

usemap ::= USEMAP ID.
{
    obj::set_working_texmap(obj::get_extra(scanner));
}
usemap ::= USEMAP OFF.
{
    obj::get_state(scanner).working_string.clear();
    obj::set_working_texmap(obj::get_extra(scanner));
}

maplib ::= MAPLIB id_list.
{
    obj::set_working_texmaplib(obj::get_extra(scanner));
}

shadow_obj ::= SHADOW_OBJ ID.
{
    obj::set_working_shadow_obj(obj::get_extra(scanner));
}

trace_obj ::= TRACE_OBJ ID.
{
    obj::set_working_trace_obj(obj::get_extra(scanner));
}

bevel ::= BEVEL toggle(A).
{
    if (obj::get_state(scanner).working_polyattributes.bevel != A.toggle) {
	obj::get_state(scanner).working_polyattributes.bevel = A.toggle;
	obj::get_state(scanner).polyattributes_dirty = true;
    }
}

c_interp ::= C_INTERP toggle(A).
{
    if (obj::get_state(scanner).working_polyattributes.c_interp != A.toggle) {
	obj::get_state(scanner).working_polyattributes.c_interp = A.toggle;
	obj::get_state(scanner).polyattributes_dirty = true;
    }
}

d_interp ::= D_INTERP toggle(A).
{
    if (obj::get_state(scanner).working_polyattributes.d_interp != A.toggle) {
	obj::get_state(scanner).working_polyattributes.d_interp = A.toggle;
	obj::get_state(scanner).polyattributes_dirty = true;
    }
}

lod ::= LOD INTEGER(A).
{
    if (!(A.integer >= 0 && A.integer <= 100)) {
	obj_parser_error(scanner, obj::lod_range_err);
	YYERROR;
    }

    unsigned char tmp = A.integer;

    if (obj::get_state(scanner).working_polyattributes.lod != tmp) {
	obj::get_state(scanner).working_polyattributes.lod = tmp;
	obj::get_state(scanner).polyattributes_dirty = true;
    }
}

id_list ::= ID(A).
{
    obj::get_state(scanner).working_stringset.insert(A.string);

    A.string[0] = '\0';
}
id_list ::= id_list ID(A).
{
    obj::get_state(scanner).working_stringset.insert(A.string);

    A.string[0] = '\0';
}

toggle(A) ::= ON.
{
    A.toggle = true;
}
toggle(A) ::= OFF.
{
    A.toggle = false;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
