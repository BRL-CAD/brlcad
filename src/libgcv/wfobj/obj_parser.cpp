/*                  O B J _ P A R S E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2013 United States Government as represented by
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
/* This file implements the obj parser interface defined in obj_parser.h.
 *
 * Code that wants to use the interface should create a handle of type
 * obj_parser_t and pass it to obj_parser_create() to initialize it.
 *
 * Parsing is done using the obj_parse() or obj_fparse() functions, which take
 * an input, a parser handle, and an obj_contents_t to return the parsed file
 * contents in.
 */

#include "common.h"
#include "obj_parser.h"
#include "obj_parser_state.h"

#include "obj_grammar_decls.h"
#include "obj_rules.h"

#include <cerrno>
#include <cstdio>
#include <sstream>
#include <iostream>

extern int obj_parser_parse(yyscan_t);

namespace cad {
namespace gcv {
namespace obj {

struct no_close {
    void operator()(FILE*) {}
};

template<typename ObjContentsT>
static int set_stream(FILE *stream, basic_parser_state<ObjContentsT> &state)
{
    typedef basic_parser_state<ObjContentsT> state_type;
    typedef typename state_type::file_node file_node_type;
    typedef typename state_type::string_type string_type;

    file_node_type node;
    node.dir = ".";
    node.lineno = 1;
    node.file.reset(stream, no_close());
    state.file_stack.push_back(node);

    return 0;
}


template<typename ObjContentsT>
static int open_file(
	const typename basic_parser_state<ObjContentsT>::string_type &filename,
	basic_parser_state<ObjContentsT> &state)
{
    typedef basic_parser_state<ObjContentsT> state_type;
    typedef typename state_type::file_node file_node_type;
    typedef typename state_type::string_type string_type;

    file_node_type node;
    node.path = filename;

    typename string_type::size_type loc = filename.find_last_of('/');

    if (loc == string_type::npos) {
	node.dir = ".";
    } else {
	node.dir = filename.substr(0, loc);
    }

    node.lineno = 1;

    FILE *file = fopen(filename.c_str(), "r");

    if (!file) {
	return errno;
    }

    node.file.reset(file, fclose);

    state.file_stack.push_back(node);

    return 0;
}


struct lex_sentry {
    lex_sentry(yyscan_t s) : scanner(s) {}

    ~lex_sentry(void) {
	obj_parser_lex_destroy(scanner);
    }

    yyscan_t scanner;
};

} /* namespace obj */

__BEGIN_DECLS

static void*
bu_malloc_wrapper(size_t size)
{
    return bu_malloc(size, "alloc lemon parser object");
}

static void
bu_free_wrapper(void *ptr)
{
    return bu_free(ptr, "free lemon parser object");
}

static void createParser(obj::parser_type *parser)
{
    *parser = ParseAlloc(bu_malloc_wrapper);
}

static void destroyParser(obj::parser_type *parser)
{
    ParseFree(*parser, bu_free_wrapper);
}

static void destroyScanner(yyscan_t *scanner)
{
    struct extra_t *extra =
	static_cast<struct extra_t*>(obj_parser_get_extra(*scanner));

    BU_PUT(extra, struct extra_t);
    obj_parser_set_extra(*scanner, NULL);

    obj_parser_lex_destroy(*scanner);
}

static void setScannerExtra(yyscan_t scanner, obj::objCombinedState *state)
{
    struct extra_t *extra;

    BU_GET(extra, struct extra_t);
    extra->state = static_cast<void*>(state);

    obj_parser_set_extra(scanner, static_cast<void*>(extra));
}

int obj_parser_create(obj_parser_t *parser)
{
    int err = 0;

    try {
	parser->p = new obj::objParser;
    } catch (std::bad_alloc &) {
	err = ENOMEM;
    } catch (...) {
	bu_bomb("unexpected error encountered\n");
    }

    return err;
}


void obj_parser_destroy(obj_parser_t parser)
{
    try {
	delete static_cast<obj::objParser*>(parser.p);
	parser.p = 0;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }
}


int obj_parse(const char *filename, obj_parser_t parser,
	      obj_contents_t *contents)
{
    using obj::objParser;
    using obj::objFileContents;
    using obj::objCombinedState;

    objParser *p = static_cast<objParser*>(parser.p);

    int err = 0;

    try {
	std::auto_ptr<objFileContents> sentry(new objFileContents);

	objCombinedState state(p, sentry.get());

	if ((err =
	     obj::open_file(std::string(filename), state.parser_state))) {
	    return err;
	}

	yyscan_t scanner;

	scanner = perplexFileScanner(state.parser_state.file_stack.back().file.get());
	setScannerExtra(scanner, &state);

	state.parser = NULL;
	createParser(&(state.parser));

	err = obj_parser_parse(scanner);

	p->last_error = state.parser_state.err.str();

	destroyParser(&(state.parser));
	destroyScanner(&scanner);

	if (err == 2) {
	    return ENOMEM;
	}

	if (err != 0) {
	    return -1;
	}

	contents->p = sentry.release();
    } catch(std::bad_alloc &) {
	err = ENOMEM;
    } catch(std::exception &ex) {
	p->last_error = ex.what();
	err = -1;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return err;
}


int obj_fparse(FILE *stream, obj_parser_t parser, obj_contents_t *contents)
{
    using obj::objParser;
    using obj::objFileContents;
    using obj::objCombinedState;

    objParser *p = static_cast<objParser*>(parser.p);

    int err = 0;

    try {
	std::auto_ptr<objFileContents> sentry(new objFileContents);

	objCombinedState state(p, sentry.get());

	if ((err = obj::set_stream(stream, state.parser_state))) {
	    return err;
	}

	yyscan_t scanner;

	scanner = perplexFileScanner(state.parser_state.file_stack.back().file.get());
	setScannerExtra(scanner, &state);

	state.parser = NULL;
	createParser(&(state.parser));

	err = obj_parser_parse(scanner);

	p->last_error = state.parser_state.err.str();

	destroyParser(&(state.parser));
	destroyScanner(&scanner);

	if (err == 2) {
	    return ENOMEM;
	}

	if (err != 0) {
	    return -1;
	}

	contents->p = sentry.release();
    } catch(std::bad_alloc &) {
	err = ENOMEM;
    } catch(std::exception &ex) {
	p->last_error = ex.what();
	err = -1;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return err;
}


const char * obj_parse_error(obj_parser_t parser)
{
    const char *err = 0;

    try {
	obj::objParser *p = static_cast<obj::objParser*>(parser.p);

	if (!(p->last_error.empty())) {
	    err = p->last_error.c_str();
	}
    } catch (...) {
	bu_bomb("unexpected error encountered\n");
    }

    return err;
}


int obj_contents_destroy(obj_contents_t contents)
{
    try {
	delete static_cast<obj::objFileContents*>(contents.p);
	contents.p = 0;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_vertices(obj_contents_t contents, const float (*val_arr[])[4])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<float, 4> > to size_t arr[][4]
	if (val_arr && c->gvertices_list.size()) {
	    *val_arr = &(c->gvertices_list.front().v);
	}

	return c->gvertices_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_texture_coord(obj_contents_t contents, const float (*val_arr[])[3])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<float, 3> > to size_t arr[][3]
	if (val_arr && c->tvertices_list.size()) {
	    *val_arr = &(c->tvertices_list.front().v);
	}

	return c->tvertices_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_normals(obj_contents_t contents, const float (*val_arr[])[3])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<float, 3> > to size_t arr[][3]
	if (val_arr && c->nvertices_list.size()) {
	    *val_arr = &(c->nvertices_list.front().v);
	}

	return c->nvertices_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_groups(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->groupchar_set.size()) {
	    *val_arr = &(c->groupchar_set.front());
	}

	return c->groupchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_num_groupsets(obj_contents_t contents)
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	return c->groupindex_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_groupset(obj_contents_t contents, size_t n,
		    const size_t (*index_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (index_arr && c->groupindex_set[n].size()) {
	    *index_arr = &(c->groupindex_set[n].front());
	}

	return c->groupindex_set[n].size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_objects(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->objectchar_set.size()) {
	    *val_arr = &(c->objectchar_set.front());
	}

	return c->objectchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_materials(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->materialchar_set.size()) {
	    *val_arr = &(c->materialchar_set.front());
	}

	return c->materialchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_materiallibs(obj_contents_t contents,
			const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->materiallibchar_set.size()) {
	    *val_arr = &(c->materiallibchar_set.front());
	}

	return c->materiallibchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_num_materiallibsets(obj_contents_t contents)
{
    try {
	obj::objFileContents *c =
	static_cast<obj::objFileContents*>(contents.p);

	return c->materiallibindex_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_materiallibset(obj_contents_t contents, size_t n, const size_t (*index_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (index_arr && c->materiallibindex_set[n].size()) {
	    *index_arr = &(c->materiallibindex_set[n].front());
	}

	return c->materiallibindex_set[n].size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_texmaps(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->texmapchar_set.size()) {
	    *val_arr = &(c->texmapchar_set.front());
	}

	return c->texmapchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_texmaplibs(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->texmaplibchar_set.size()) {
	    *val_arr = &(c->texmaplibchar_set.front());
	}

	return c->texmaplibchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_num_texmaplibsets(obj_contents_t contents)
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	return c->texmaplibindex_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_texmaplibset(obj_contents_t contents, size_t n,
			const size_t (*index_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (index_arr && c->texmaplibindex_set[n].size()) {
	    *index_arr = &(c->texmaplibindex_set[n].front());
	}

	return c->texmaplibindex_set[n].size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_shadow_objs(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->shadow_objchar_set.size()) {
	    *val_arr = &(c->shadow_objchar_set.front());
	}

	return c->shadow_objchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_trace_objs(obj_contents_t contents, const char * const (*val_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (val_arr && c->trace_objchar_set.size()) {
	    *val_arr = &(c->trace_objchar_set.front());
	}

	return c->trace_objchar_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_attributes(obj_contents_t contents,
				const obj_polygonal_attributes_t (*attr_list[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attr_list && c->polyattributes_set.size()) {
	    *attr_list = &(c->polyattributes_set.front());
	}

	return c->polyattributes_set.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_points(obj_contents_t contents,
			      const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->point_v_attr_list.size()) {
	    *attindex_arr = &(c->point_v_attr_list.front());
	}

	return c->point_v_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_point_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (index_arr && c->point_v_loclist[face].second) {
	    *index_arr = &(c->point_v_indexlist[c->point_v_loclist[face].first]);
	}

	return c->point_v_loclist[face].second;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_lines(obj_contents_t contents,
			     const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->line_v_attr_list.size()) {
	    *attindex_arr = &(c->line_v_attr_list.front());
	}

	return c->line_v_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_line_vertices(obj_contents_t contents, size_t face,
				     const size_t (*index_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (index_arr && c->line_v_loclist[face].second) {
	    *index_arr = &(c->line_v_indexlist[c->line_v_loclist[face].first]);
	}

	return c->line_v_loclist[face].second;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tv_lines(obj_contents_t contents,
			      const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->line_tv_attr_list.size()) {
	    *attindex_arr = &(c->line_tv_attr_list.front());
	}

	return c->line_tv_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tv_line_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<size_t, 2> > to size_t arr[][2]
	if (index_arr && c->line_tv_loclist[face].second) {
	    *index_arr = &(c->line_tv_indexlist[c->line_tv_loclist[face].first].v);
	}

	return c->line_tv_loclist[face].second;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_faces(obj_contents_t contents,
			     const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->polygonal_v_attr_list.size()) {
	    *attindex_arr = &(c->polygonal_v_attr_list.front());
	}

	return c->polygonal_v_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_v_face_vertices(obj_contents_t contents, size_t face,
				     const size_t (*index_arr[]))
{
    using obj::objFileContents;

    try {
	objFileContents *c = static_cast<objFileContents*>(contents.p);

	size_t start = c->polygonal_v_loclist[face].first;
	size_t length = c->polygonal_v_loclist[face].second;

	if (index_arr != NULL && length > 0) {
	    *index_arr = &(c->pologonal_v_indexlist[start]);
	}

	return length;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tv_faces(obj_contents_t contents,
			      const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->polygonal_tv_attr_list.size()) {
	    *attindex_arr = &(c->polygonal_tv_attr_list.front());
	}

	return c->polygonal_tv_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tv_face_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<size_t, 2> > to size_t arr[][2]
	if (index_arr && c->polygonal_tv_loclist[face].second) {
	    *index_arr = &(c->pologonal_tv_indexlist[c->polygonal_tv_loclist[face].first].v);
	}

	return c->polygonal_tv_loclist[face].second;

    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_nv_faces(obj_contents_t contents,
			      const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->polygonal_nv_attr_list.size()) {
	    *attindex_arr = &(c->polygonal_nv_attr_list.front());
	}

	return c->polygonal_nv_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_nv_face_vertices(obj_contents_t contents, size_t face,
				      const size_t (*index_arr[])[2])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<size_t, 2> > to size_t arr[][2]
	if (index_arr && c->polygonal_nv_loclist[face].second) {
	    *index_arr = &(c->pologonal_nv_indexlist[c->polygonal_nv_loclist[face].first].v);
	}

	return c->polygonal_nv_loclist[face].second;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tnv_faces(obj_contents_t contents,
			       const size_t (*attindex_arr[]))
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	if (attindex_arr && c->polygonal_tnv_attr_list.size()) {
	    *attindex_arr = &(c->polygonal_tnv_attr_list.front());
	}

	return c->polygonal_tnv_attr_list.size();
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}


size_t obj_polygonal_tnv_face_vertices(obj_contents_t contents, size_t face,
				       const size_t (*index_arr[])[3])
{
    try {
	obj::objFileContents *c =
	    static_cast<obj::objFileContents*>(contents.p);

	// coerce vector<tuple<size_t, 3> > to size_t arr[][3]
	if (index_arr && c->polygonal_tnv_loclist[face].second) {
	    *index_arr = &(c->pologonal_tnv_indexlist[c->polygonal_tnv_loclist[face].first].v);
	}

	return c->polygonal_tnv_loclist[face].second;
    } catch(...) {
	bu_bomb("unexpected error encountered\n");
    }

    return 0;
}

__END_DECLS

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
