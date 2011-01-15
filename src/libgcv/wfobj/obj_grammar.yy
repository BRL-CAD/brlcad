/*                  O B J _ G R A M M A R . Y Y
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
%{
/*                  O B J _ G R A M M A R . Y Y 
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
#include "obj_grammar.h"
#include "obj_rules.h"

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

/**
 *  Use namespaces here to avoid multiple symbol name clashes
 */
void obj_parser_error(yyscan_t scanner, const char *s);

namespace arl {
namespace obj_parser {
namespace detail {

static const char vertex_ref_err[] = "Invalid vertex reference";
static const char texture_ref_err[] = "Invalid texture reference";
static const char normal_ref_err[] = "Invalid normal reference";
static const char lod_range_err[] = "LOD value must be between 0 and 100";
static const char smooth_range_err[] = "negative smoothing group id";
static const char face_length_err[] = "Faces must contain at least 3 references";
static const char line_length_err[] = "Lines must contain at least 2 references";
  
typedef parser_extra::contents_type contents_type;
typedef parser_extra::parser_state_type parser_state_type;
  
/**
 *  convenience routines
 */
inline static parser_state_type & get_state(yyscan_t scanner) {
	return static_cast<parser_extra*>(obj_parser_get_extra(scanner))->parser_state;
}

inline static parser_extra::parser_type & get_parser(yyscan_t scanner) {
	return *(static_cast<parser_extra*>(obj_parser_get_extra(scanner))->parser);
}

inline static contents_type & get_contents(yyscan_t scanner) {
	return *(static_cast<parser_extra*>(obj_parser_get_extra(scanner))->contents);
}

inline static parser_extra & get_extra(yyscan_t scanner) {
	return *static_cast<parser_extra*>(obj_parser_get_extra(scanner));
}

inline static size_t real_index(int val, std::size_t nvert) {
	return (val<0?nvert-size_t(std::abs(val)):size_t(val-1));
}

template<typename charT>
inline static bool index_check(int raw, std::size_t index,
				   size_t vertices, const charT *log, yyscan_t scanner)
{
    if (!raw || index >= vertices) {
	std::stringstream err;
	err << "index '" << raw << "': " << log;
        std::string str = err.str();
        obj_parser_error(scanner, str.c_str());
        return true;
    }
    return false;
}

} /* namespace detail */
} /* namespcae obj_parser */
} /* namespace arl */

using namespace arl::obj_parser;
  
/**
 *  Error reporting function as required by yacc
 */
void obj_parser_error(yyscan_t scanner, const char *s)
{
    detail::verbose_output_formatter(detail::get_state(scanner), s);
}

%}

%pure-parser

%parse-param {yyscan_t scanner}
%lex-param {yyscan_t scanner}

%union {
    float real;
    int integer;
    int reference[3];
    bool toggle;
    size_t index;
}


%token <real> FLOAT
%token <integer> INTEGER
%token <reference> V_REFERENCE
%token <reference> TV_REFERENCE
%token <reference> NV_REFERENCE
%token <reference> TNV_REFERENCE

%token ID
%token VERTEX
%token T_VERTEX
%token N_VERTEX
%token POINT
%token LINE
%token FACE
%token GROUP
%token SMOOTH
%token OBJECT
%token USEMTL
%token MTLLIB
%token USEMAP
%token MAPLIB
%token BEVEL
%token C_INTERP
%token D_INTERP
%token LOD
%token SHADOW_OBJ
%token TRACE_OBJ
%token ON
%token OFF

%type <real> coord
%type <toggle> toggle
%type <index> p_v_reference_list
%type <index> l_v_reference_list
%type <index> l_tv_reference_list
%type <index> f_v_reference_list
%type <index> f_tv_reference_list
%type <index> f_nv_reference_list
%type <index> f_tnv_reference_list
%%

statement_list: '\n'
| statement '\n'
| statement_list '\n'
| statement_list statement '\n'
;
  
statement: vertex
| t_vertex
| n_vertex
| point
| line
| face
| group
| smooth
| object
| usemtl
| mtllib
| usemap
| maplib
| shadow_obj
| trace_obj
| bevel
| c_interp
| d_interp
| lod
;

coord: FLOAT { $$ = float($1); }
| INTEGER { $$ = float($1); }
;

vertex
: VERTEX coord coord coord
{
    detail::obj_contents::gvertex_t vertex = {{$2, $3, $4, 1}};
    detail::get_contents(scanner).gvertices_list.push_back(vertex);
}
| VERTEX coord coord coord coord
{
    detail::obj_contents::gvertex_t vertex = {{$2, $3, $4, $5}};
    detail::get_contents(scanner).gvertices_list.push_back(vertex);
}
;

t_vertex
: T_VERTEX coord
{
    detail::obj_contents::tvertex_t vertex = {{$2, 0, 0}};
    detail::get_contents(scanner).tvertices_list.push_back(vertex);
}
| T_VERTEX coord coord
{
    detail::obj_contents::tvertex_t vertex = {{$2, $3, 0}};
    detail::get_contents(scanner).tvertices_list.push_back(vertex);
}
| T_VERTEX coord coord coord
{
    detail::obj_contents::tvertex_t vertex = {{$2, $3, $4}};
    detail::get_contents(scanner).tvertices_list.push_back(vertex);
}
;

n_vertex
: N_VERTEX coord coord coord
{
    detail::obj_contents::nvertex_t vertex = {{$2, $3, $4}};
    detail::get_contents(scanner).nvertices_list.push_back(vertex);
}
;

p_v_reference_list
: INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1, num_gvertices);

    if (detail::index_check($1, gindex, num_gvertices, detail::vertex_ref_err, scanner)) {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).point_v_indexlist.size();
    detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
| V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1[0], num_gvertices);

    if (detail::index_check($1[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = detail::get_contents(scanner).point_v_indexlist.size();
    detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
| p_v_reference_list INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2, num_gvertices);

    if (detail::index_check($2, gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;

    $$ = $1;
    detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
| p_v_reference_list V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2[0], num_gvertices);

    if (detail::index_check($2[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = $1;
    detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
}
;

l_v_reference_list
: INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1, num_gvertices);

    if (detail::index_check($1, gindex, num_gvertices, detail::vertex_ref_err, scanner)) {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).line_v_indexlist.size();
    detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
| V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1[0], num_gvertices);

    if (detail::index_check($1[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = detail::get_contents(scanner).line_v_indexlist.size();
    detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
| l_v_reference_list INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2, num_gvertices);

    if (detail::index_check($2, gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;

    $$ = $1;
    detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
| l_v_reference_list V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2[0], num_gvertices);

    if (detail::index_check($2[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = $1;
    detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
}
;

l_tv_reference_list
: TV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

    detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index($1[0], num_gvertices),
        detail::real_index($1[1], num_tvertices)
    }};

    if (detail::index_check($1[0], tv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($1[1], tv_index.v[1], num_tvertices, detail::texture_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).line_tv_indexlist.size();
    detail::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
}
| l_tv_reference_list TV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

    detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index($2[0], num_gvertices),
        detail::real_index($2[1], num_tvertices)
    }};

    if (detail::index_check($2[0], tv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($2[1], tv_index.v[1], num_tvertices, detail::texture_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = $1;
    detail::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
}
;

f_v_reference_list
: INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1, num_gvertices);

    if (detail::index_check($1, gindex, num_gvertices, detail::vertex_ref_err, scanner)) {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).pologonal_v_indexlist.size();
    detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
| V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($1[0], num_gvertices);

    if (detail::index_check($1[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = detail::get_contents(scanner).pologonal_v_indexlist.size();
    detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
| f_v_reference_list INTEGER
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2, num_gvertices);

    if (detail::index_check($2, gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;

    $$ = $1;
    detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
| f_v_reference_list V_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    detail::contents_type::polygonal_v_index_type gindex = detail::real_index($2[0], num_gvertices);

    if (detail::index_check($2[0], gindex, num_gvertices, detail::vertex_ref_err, scanner))
        YYERROR;
      
    $$ = $1;
    detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
}
;

f_tv_reference_list
: TV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

    detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index($1[0], num_gvertices),
        detail::real_index($1[1], num_tvertices)
    }};

    if (detail::index_check($1[0], tv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($1[1], tv_index.v[1], num_tvertices, detail::texture_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).pologonal_tv_indexlist.size();
    detail::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
}
| f_tv_reference_list TV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

    detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index($2[0], num_gvertices),
        detail::real_index($2[1], num_tvertices)
    }};

    if (detail::index_check($2[0], tv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($2[1], tv_index.v[1], num_tvertices, detail::texture_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = $1;
    detail::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
}
;

f_nv_reference_list
: NV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

    detail::contents_type::polygonal_nv_index_type nv_index = {{
        detail::real_index($1[0], num_gvertices),
        detail::real_index($1[2], num_nvertices)
    }};

    if (detail::index_check($1[0], nv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($1[2], nv_index.v[1], num_nvertices, detail::normal_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).pologonal_nv_indexlist.size();
    detail::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
}
| f_nv_reference_list NV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

    detail::contents_type::polygonal_nv_index_type nv_index = {{
        detail::real_index($2[0], num_gvertices),
        detail::real_index($2[2], num_nvertices)
    }};

    if (detail::index_check($2[0], nv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($2[2], nv_index.v[1], num_nvertices, detail::normal_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = $1;
    detail::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
}
;

f_tnv_reference_list
: TNV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();
    std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

    detail::contents_type::polygonal_tnv_index_type tnv_index = {{
        detail::real_index($1[0], num_gvertices),
        detail::real_index($1[1], num_tvertices),
        detail::real_index($1[2], num_nvertices)
    }};

    if (detail::index_check($1[0], tnv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($1[1], tnv_index.v[1], num_tvertices, detail::texture_ref_err, scanner) ||
        detail::index_check($1[2], tnv_index.v[2], num_nvertices, detail::normal_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = detail::get_contents(scanner).pologonal_tnv_indexlist.size();
    detail::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
}
| f_tnv_reference_list TNV_REFERENCE
{
    std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
    std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();
    std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

    detail::contents_type::polygonal_tnv_index_type tnv_index = {{
        detail::real_index($2[0], num_gvertices),
        detail::real_index($2[1], num_tvertices),
        detail::real_index($2[2], num_nvertices)
    }};

    if (detail::index_check($2[0], tnv_index.v[0], num_gvertices, detail::vertex_ref_err, scanner) ||
        detail::index_check($2[1], tnv_index.v[1], num_tvertices, detail::texture_ref_err, scanner) ||
        detail::index_check($2[2], tnv_index.v[2], num_nvertices, detail::normal_ref_err, scanner))
    {
        YYERROR;
    }
      
    $$ = $1;
    detail::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
}
;

point
: POINT p_v_reference_list
{
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).point_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).point_v_loclist.
        resize(detail::get_contents(scanner).point_v_loclist.size()+1);
    detail::get_contents(scanner).point_v_loclist.back().first = $2;
    detail::get_contents(scanner).point_v_loclist.back().second =
        detail::get_contents(scanner).point_v_indexlist.size()-$2;
}
;

line
: LINE l_v_reference_list
{
    if (detail::get_contents(scanner).line_v_indexlist.size()-$2 < 2) {
        obj_parser_error(scanner, detail::line_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).line_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).line_v_loclist.
        resize(detail::get_contents(scanner).line_v_loclist.size()+1);
    detail::get_contents(scanner).line_v_loclist.back().first = $2;
    detail::get_contents(scanner).line_v_loclist.back().second =
        detail::get_contents(scanner).line_v_indexlist.size()-$2;
}
|
LINE l_tv_reference_list
{
    if (detail::get_contents(scanner).line_tv_indexlist.size()-$2 < 3) {
        obj_parser_error(scanner, detail::line_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).line_tv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).line_tv_loclist.
        resize(detail::get_contents(scanner).line_tv_loclist.size()+1);
    detail::get_contents(scanner).line_tv_loclist.back().first = $2;
    detail::get_contents(scanner).line_tv_loclist.back().second =
        detail::get_contents(scanner).line_tv_indexlist.size()-$2;
}
;
  
face
: FACE f_v_reference_list
{
    if (detail::get_contents(scanner).pologonal_v_indexlist.size()-$2 < 3) {
        obj_parser_error(scanner, detail::face_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).polygonal_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).polygonal_v_loclist.
        resize(detail::get_contents(scanner).polygonal_v_loclist.size()+1);
    detail::get_contents(scanner).polygonal_v_loclist.back().first = $2;
    detail::get_contents(scanner).polygonal_v_loclist.back().second =
        detail::get_contents(scanner).pologonal_v_indexlist.size()-$2;
}
| FACE f_tv_reference_list
{
    if (detail::get_contents(scanner).pologonal_tv_indexlist.size()-$2 < 3) {
        obj_parser_error(scanner, detail::face_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).polygonal_tv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).polygonal_tv_loclist.
        resize(detail::get_contents(scanner).polygonal_tv_loclist.size()+1);
    detail::get_contents(scanner).polygonal_tv_loclist.back().first = $2;
    detail::get_contents(scanner).polygonal_tv_loclist.back().second =
        detail::get_contents(scanner).pologonal_tv_indexlist.size()-$2;
}
| FACE f_nv_reference_list
{
    if (detail::get_contents(scanner).pologonal_nv_indexlist.size()-$2 < 3) {
        obj_parser_error(scanner, detail::face_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).polygonal_nv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).polygonal_nv_loclist.
        resize(detail::get_contents(scanner).polygonal_nv_loclist.size()+1);
    detail::get_contents(scanner).polygonal_nv_loclist.back().first = $2;
    detail::get_contents(scanner).polygonal_nv_loclist.back().second =
        detail::get_contents(scanner).pologonal_nv_indexlist.size()-$2;
}
| FACE f_tnv_reference_list
{
    if (detail::get_contents(scanner).pologonal_tnv_indexlist.size()-$2 < 3) {
        obj_parser_error(scanner, detail::face_length_err);
        YYERROR;
    }
      
    if (detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
    detail::get_contents(scanner).polygonal_tnv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

    detail::get_contents(scanner).polygonal_tnv_loclist.
        resize(detail::get_contents(scanner).polygonal_tnv_loclist.size()+1);
    detail::get_contents(scanner).polygonal_tnv_loclist.back().first = $2;
    detail::get_contents(scanner).polygonal_tnv_loclist.back().second =
        detail::get_contents(scanner).pologonal_tnv_indexlist.size()-$2;
}
;

group: GROUP id_list
{
    detail::set_working_groupset(detail::get_extra(scanner));
}
;

smooth: SMOOTH INTEGER
{
    if ($2 < 0) {
        obj_parser_error(scanner, detail::smooth_range_err);
        YYERROR;
    }

    if (detail::get_state(scanner).working_polyattributes.smooth_group != 
        static_cast<unsigned int>($2))
    {
        detail::get_state(scanner).working_polyattributes.smooth_group = 
	    static_cast<unsigned int>($2);
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
| SMOOTH OFF
{
    if (detail::get_state(scanner).working_polyattributes.smooth_group != 0) {
        detail::get_state(scanner).working_polyattributes.smooth_group = 0;
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
;
  
object: OBJECT ID
{
    detail::set_working_object(detail::get_extra(scanner));
}
;

usemtl: USEMTL ID
{
    detail::set_working_material(detail::get_extra(scanner));
}
;

mtllib: MTLLIB id_list
{
    detail::set_working_materiallib(detail::get_extra(scanner));
}
;

usemap: USEMAP ID
{
    detail::set_working_texmap(detail::get_extra(scanner));
}
| USEMAP OFF
{
    detail::get_state(scanner).working_string.clear();
    detail::set_working_texmap(detail::get_extra(scanner));
}
;

maplib: MAPLIB id_list
{
    detail::set_working_texmaplib(detail::get_extra(scanner));
}
;
  
shadow_obj: SHADOW_OBJ ID
{
    detail::set_working_shadow_obj(detail::get_extra(scanner));
}
;

trace_obj: TRACE_OBJ ID
{
    detail::set_working_trace_obj(detail::get_extra(scanner));
}
;

bevel: BEVEL toggle
{
    if (detail::get_state(scanner).working_polyattributes.bevel != $2) {
        detail::get_state(scanner).working_polyattributes.bevel = $2;
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
;

c_interp: C_INTERP toggle
{
    if (detail::get_state(scanner).working_polyattributes.c_interp != $2) {
        detail::get_state(scanner).working_polyattributes.c_interp = $2;
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
;

d_interp: D_INTERP toggle
{
    if (detail::get_state(scanner).working_polyattributes.d_interp != $2) {
        detail::get_state(scanner).working_polyattributes.d_interp = $2;
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
;

lod: LOD INTEGER
{
    if (!($2 >= 0 && $2 <= 100)) {
        obj_parser_error(scanner, detail::lod_range_err);
        YYERROR;
    }
      
    unsigned char tmp = $2;
      
    if (detail::get_state(scanner).working_polyattributes.lod != tmp) {
        detail::get_state(scanner).working_polyattributes.lod = tmp;
        detail::get_state(scanner).polyattributes_dirty = true;
    }
}
;
  
id_list: ID
{
    detail::get_state(scanner).working_stringset.
        insert(detail::get_state(scanner).working_string);
    detail::get_state(scanner).working_string.clear();
}
| id_list ID
{
    detail::get_state(scanner).working_stringset.
        insert(detail::get_state(scanner).working_string);
    detail::get_state(scanner).working_string.clear();
}
;

toggle: ON
{
    $$ = true;
}
| OFF
{
    $$ = false;
}
;
  
%%

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
