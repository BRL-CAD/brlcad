/*             O B J _ P A R S E R _ S T A T E . H
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

#ifndef ARL_OBJ_PARSER_STATE_H
#define ARL_OBJ_PARSER_STATE_H

#include "common.h"

#include "obj_parser.h"
#include "bu.h"

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstring>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

inline bool operator<(const obj_polygonal_attributes_t &lhs,
		      const obj_polygonal_attributes_t &rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(obj_polygonal_attributes_t)) < 0;
}


namespace arl {
namespace obj_parser {
namespace detail {

inline char *createCopyOfString(const char *source, const size_t strSize)
{
    char *copy = (char*)bu_malloc(sizeof(char) * strSize, "createCopyOfString");
    bu_strlcpy(copy, source, strSize);

    return copy;
}

typedef void *parser_type;

template<typename T, std::size_t N>
struct tuple {
    T v[N];
};


/* recursive tuple comparison */
template<typename T, std::size_t N, std::size_t i>
struct tuple_compare {
    bool compare(tuple<T, N> &lhs, const tuple<T, N> &rhs) {
	return lhs[i] == rhs[i] && tuple_compare<T, N, i - 1>(lhs, rhs);
    }
};

/* tuple comparison base case */
template<typename T, std::size_t N>
struct tuple_compare<T, N, 0> {
    void compare(tuple<T, N> &lhs, const tuple<T, N> &rhs) {
	return lhs[0] == rhs[0];
    }
};


template<typename T, std::size_t N>
inline bool operator==(const tuple<T, N> &lhs, const tuple<T, N> &rhs)
{
    return tuple_compare<T, N, N>::compare(lhs, rhs);
}

/**
 *  Basic parser object, persistant across multiple file parsings
 *
 *  Lifetime is controlled by the user via obj_parser_create
 *  and obj_parser_destroy.
 */
template<typename charT=char,
	 typename traits=std::char_traits<char>,
	 typename Allocator=std::allocator<char> >
struct basic_obj_parser {
    typedef std::basic_string<charT, traits, Allocator> string_type;
    // include paths etc

    string_type last_error;
};

/* contents of an obj file */
template<typename PrecisionT,
	 typename charT=char,
	 typename traits=std::char_traits<char>,
	 typename Allocator=std::allocator<char> >
struct basic_obj_contents {
    typedef PrecisionT precision_type;
    typedef tuple<precision_type, 4> gvertex_t;
    typedef tuple<precision_type, 3> tvertex_t;
    typedef tuple<precision_type, 3> nvertex_t;

    typedef size_t polygonal_v_index_type;
    typedef tuple<size_t, 2> polygonal_tv_index_type;
    typedef tuple<size_t, 2> polygonal_nv_index_type;
    typedef tuple<size_t, 3> polygonal_tnv_index_type;

    // indexloc_t := {start, length}
    typedef std::pair<size_t, size_t> indexloc_t;
    typedef std::vector<indexloc_t> indexloc_vec_type;

    typedef std::vector<size_t> indexvec_type;
    typedef std::vector<indexvec_type> indexvec_vec_type;

    typedef std::vector<polygonal_v_index_type> polygonal_v_indexvec_type;
    typedef std::vector<polygonal_tv_index_type> polygonal_tv_indexvec_type;
    typedef std::vector<polygonal_nv_index_type> polygonal_nv_indexvec_type;
    typedef std::vector<polygonal_tnv_index_type> polygonal_tnv_indexvec_type;

    typedef std::basic_string<charT, traits, Allocator> string_type;

    typedef std::vector<string_type> stringvec_type;
    typedef std::vector<const charT *> charvec_type;

    typedef std::vector<obj_polygonal_attributes_t> polyattributes_vec_type;

    // all vertices
    std::vector<gvertex_t> gvertices_list;
    std::vector<nvertex_t> tvertices_list;
    std::vector<tvertex_t> nvertices_list;

    // unordered set s.t. the default (possibly empty) value is first
    // these are indexed in interface for space consideration
    // The *char_set is just a vector of const char * from *_set[i].c_str()
    stringvec_type group_set;
    charvec_type groupchar_set;
    indexvec_vec_type groupindex_set;

    stringvec_type object_set;
    charvec_type objectchar_set;

    stringvec_type material_set;
    charvec_type materialchar_set;

    stringvec_type materiallib_set;
    charvec_type materiallibchar_set;
    indexvec_vec_type materiallibindex_set;

    stringvec_type texmap_set;
    charvec_type texmapchar_set;

    stringvec_type texmaplib_set;
    charvec_type texmaplibchar_set;
    indexvec_vec_type texmaplibindex_set;

    stringvec_type shadow_obj_set;
    charvec_type shadow_objchar_set;

    stringvec_type trace_obj_set;
    charvec_type trace_objchar_set;

    // unordered set of polygonal attributes
    polyattributes_vec_type polyattributes_set;

    // point_*_attr_list is list of indices into polyattributes_set
    // point_*_indexlist is a vector of n-tuples of indices
    // point_*_attr_list and point_*_indexlist are synced
    indexvec_type point_v_attr_list;
    indexloc_vec_type point_v_loclist;
    polygonal_v_indexvec_type point_v_indexlist;

    // line_*_attr_list is list of indices into polyattributes_set
    // line_*_indexlist is a vector of n-tuples of indices
    // line_*_attr_list and line_*_indexlist are synced
    indexvec_type line_v_attr_list;
    indexloc_vec_type line_v_loclist;
    polygonal_v_indexvec_type line_v_indexlist;

    indexvec_type line_tv_attr_list;
    indexloc_vec_type line_tv_loclist;
    polygonal_tv_indexvec_type line_tv_indexlist;

    // polygonal_*_attr_list is list of indices into polyattributes_set
    // pologonal_*_indexlist is a vector of n-tuples of indices
    // polygonal_*_attr_list and pologonal_*_indexlist are synced
    indexvec_type polygonal_v_attr_list;
    indexloc_vec_type polygonal_v_loclist;
    polygonal_v_indexvec_type pologonal_v_indexlist;

    indexvec_type polygonal_tv_attr_list;
    indexloc_vec_type polygonal_tv_loclist;
    polygonal_tv_indexvec_type pologonal_tv_indexlist;

    indexvec_type polygonal_nv_attr_list;
    indexloc_vec_type polygonal_nv_loclist;
    polygonal_nv_indexvec_type pologonal_nv_indexlist;

    indexvec_type polygonal_tnv_attr_list;
    indexloc_vec_type polygonal_tnv_loclist;
    polygonal_tnv_indexvec_type pologonal_tnv_indexlist;
}; /* basic_obj_contents */


template<typename ObjContentsT>
struct basic_parser_state {
    typedef ObjContentsT contents_type;
    typedef typename contents_type::string_type string_type;

    typedef std::basic_stringstream<typename string_type::value_type,
				    typename string_type::traits_type,
				    typename string_type::allocator_type>
					stringstream_type;

    typedef std::size_t index_type;

    typedef tuple<index_type, 1> index1_type;
    typedef tuple<index_type, 2> index2_type;
    typedef tuple<index_type, 3> index3_type;

    typedef typename contents_type::polygonal_v_indexvec_type
	polygonal_v_indexvec_type;

    typedef typename contents_type::polygonal_tv_indexvec_type
	polygonal_tv_indexvec_type;

    typedef typename contents_type::polygonal_nv_indexvec_type
	polygonal_nv_indexvec_type;

    typedef typename contents_type::polygonal_tnv_indexvec_type
	polygonal_tnv_indexvec_type;

    typedef typename contents_type::indexvec_type indexvec_type;
    typedef typename contents_type::indexvec_vec_type indexvec_vec_type;
    typedef typename indexvec_vec_type::size_type indexvec_vec_index_type;

    typedef std::set<string_type> stringset_type;
    typedef typename contents_type::stringvec_type stringvec_type;
    typedef typename contents_type::charvec_type charvec_type;
    typedef typename stringvec_type::size_type stringvec_index_type;

    typedef std::map<string_type, typename charvec_type::size_type>
	string_index_map_type;

    typedef std::map<stringset_type, indexvec_vec_index_type>
	groupset_index_map_type;

    typedef std::map<string_type, stringvec_index_type>
	object_index_map_type;

    typedef std::map<string_type, stringvec_index_type>
	material_index_map_type;

    typedef std::map<stringset_type, indexvec_vec_index_type>
	materiallibset_index_map_type;

    typedef std::map<string_type, stringvec_index_type>
	texmap_index_map_type;

    typedef std::map<stringset_type, indexvec_vec_index_type>
	texmaplibset_index_map_type;

    typedef std::map<string_type, stringvec_index_type>
	shadow_obj_index_map_type;

    typedef std::map<string_type, stringvec_index_type>
	trace_obj_index_map_type;

    typedef typename contents_type::polyattributes_vec_type
	polyattributes_vec_type;

    typedef typename polyattributes_vec_type::size_type
	polyattributes_vec_index_type;

    typedef std::map<obj_polygonal_attributes_t, polyattributes_vec_index_type>
	polyattributes_index_map_type;

    struct file_node {
	string_type path;
	string_type dir;
	std::size_t lineno;
	shared_ptr<FILE> file;
    };

    std::vector<file_node> file_stack;
    stringstream_type err;
    bool syntaxError;

    /**
     *  Working contents for content construction during parse
     */
    string_type working_string;
    stringset_type working_stringset;
    obj_polygonal_attributes_t working_polyattributes;

    // index from strings to content group_set indices
    string_index_map_type group_index_map;
    indexvec_vec_index_type current_groupset;
    groupset_index_map_type groupset_index_map;

    // current object index and mapping into contents
    stringvec_index_type current_object;
    object_index_map_type object_index_map;

    // current material index and mapping into contents
    stringvec_index_type current_material;
    material_index_map_type material_index_map;

    // current materiallib index and mapping into contents
    string_index_map_type materiallib_index_map;
    indexvec_vec_index_type current_materiallib;
    materiallibset_index_map_type materiallibset_index_map;

    // current texmap index and mapping into contents
    stringvec_index_type current_texmap;
    texmap_index_map_type texmap_index_map;

    // current texmaplib index and mapping into contents
    string_index_map_type texmaplib_index_map;
    indexvec_vec_index_type current_texmaplib;
    texmaplibset_index_map_type texmaplibset_index_map;

    // current shadow_obj index and mapping into contents
    stringvec_index_type current_shadow_obj;
    shadow_obj_index_map_type shadow_obj_index_map;

    // current trace_obj index and mapping into contents
    stringvec_index_type current_trace_obj;
    trace_obj_index_map_type trace_obj_index_map;

    bool polyattributes_dirty;
    polyattributes_vec_index_type current_polyattributes;
    polyattributes_index_map_type polyattributes_index_map;
}; /* basic_parser_state */


/**
 *  Composition object for dealing with lex/lacc reentrant interface.
 *
 *  Lifetime is only until the parse completion of a single file and 
 *  it's includes.
 */
template<typename PrecisionT,
	 typename charT=char,
	 typename traits=std::char_traits<char>,
	 typename Allocator=std::allocator<char> >
struct basic_parser_extra {
    typedef charT char_type;
    typedef traits traits_type;
    typedef Allocator allocator;

    typedef basic_obj_parser<charT, traits, Allocator> basic_parser_type;
    typedef basic_obj_contents<PrecisionT, charT, traits, Allocator>
	contents_type;

    typedef basic_parser_state<contents_type> parser_state_type;

    parser_state_type parser_state;

    parser_type parser;
    basic_parser_type *basic_parser;
    contents_type *contents;

    basic_parser_extra(basic_parser_type *p, contents_type *c);
};


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_groupset(basic_parser_extra<PrecisionT, charT, traits,
					     Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator>
	extra_type;

    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;

    typename parser_state_type::groupset_index_map_type::const_iterator res =
	extra.parser_state.groupset_index_map.
	    find(extra.parser_state.working_stringset);

    if (res != extra.parser_state.groupset_index_map.end()) {
	extra.parser_state.current_groupset = res->second;
    } else {
	typename parser_state_type::indexvec_type new_grpset;
	new_grpset.reserve(extra.parser_state.working_stringset.size());

	// not already seen, set in groupset_index_map
	typename parser_state_type::stringset_type::const_iterator key =
	    extra.parser_state.working_stringset.begin();

	while (key != extra.parser_state.working_stringset.end()) {
	    typename parser_state_type::string_index_map_type::const_iterator
		prev = extra.parser_state.group_index_map.find(*key);

	    if (prev != extra.parser_state.group_index_map.end()) {
		new_grpset.push_back(prev->second);
	    } else {
		// never seen this group name before, add to contents and maps
		new_grpset.push_back(extra.contents->group_set.size());

		extra.contents->group_set.push_back(*key);

		char *groupString;
		groupString = createCopyOfString(key->c_str(), key->size() + 1); 
		extra.contents->groupchar_set.push_back(groupString);

		extra.parser_state.group_index_map[*key] = new_grpset.back();
	    }
	    ++key;
	}

	// sort the new index set to ensure unique for all ordering
	std::sort(new_grpset.begin(), new_grpset.end());

	// map this set of groupname strings to the location in contents for
	// fast lookup later if needed
	extra.parser_state.
	    groupset_index_map[extra.parser_state.working_stringset] =
		extra.contents->groupindex_set.size();

	// set the current working groupset
	extra.parser_state.current_groupset =
	    extra.contents->groupindex_set.size();

	// new_grpset now contains a unique set of indices into
	// contents->group_set;
	extra.contents->groupindex_set.push_back(new_grpset);
    }

    extra.parser_state.working_stringset.clear();

    extra.parser_state.working_polyattributes.groupset_index = 
	extra.parser_state.current_groupset;

    extra.parser_state.polyattributes_dirty = true;
} /* set working groupset */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_object(basic_parser_extra<PrecisionT, charT, traits,
					   Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;

    typename parser_state_type::object_index_map_type::iterator res =
	extra.parser_state.object_index_map.
	    find(extra.parser_state.working_string);

    if (res != extra.parser_state.object_index_map.end()) {
	extra.parser_state.current_object = res->second;
	extra.parser_state.working_string.clear();
    } else {
	extra.parser_state.current_object = extra.contents->object_set.size();

	string_type &working_string = extra.parser_state.working_string;

	extra.contents->object_set.push_back(working_string);

	char *objectString = createCopyOfString(working_string.c_str(),
	    working_string.size() + 1);

	extra.contents->objectchar_set.push_back(objectString);
    }

    extra.parser_state.working_polyattributes.object_index =
	extra.parser_state.current_object;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_object */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_material(basic_parser_extra<PrecisionT, charT, traits,
					     Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;

    typename parser_state_type::material_index_map_type::iterator res =
	extra.parser_state.material_index_map.
	    find(extra.parser_state.working_string);

    if (res != extra.parser_state.material_index_map.end()) {
	extra.parser_state.current_material = res->second;
	extra.parser_state.working_string.clear();
    } else {
	extra.parser_state.current_material =
	    extra.contents->material_set.size();

	string_type &working_string = extra.parser_state.working_string;

	extra.contents->material_set.push_back(working_string);

	char *materialString = createCopyOfString(working_string.c_str(),
	    working_string.size() + 1);

	extra.contents->materialchar_set.push_back(materialString);
    }

    extra.parser_state.working_polyattributes.material_index =
	extra.parser_state.current_material;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_material */

template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_materiallib(basic_parser_extra<PrecisionT, charT, traits,
						Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;
    typedef typename contents_type::stringvec_type stringvec_type;

    typename parser_state_type::materiallibset_index_map_type::const_iterator
	res = extra.parser_state.materiallibset_index_map.
	    find(extra.parser_state.working_stringset);

    if (res != extra.parser_state.materiallibset_index_map.end()) {
	extra.parser_state.current_materiallib = res->second;
	extra.parser_state.working_stringset.clear();
    } else {
	typename parser_state_type::indexvec_type new_mtllibset;
	new_mtllibset.reserve(extra.parser_state.working_stringset.size());

	// not already seen, set in materiallibset_index_map
	typename parser_state_type::stringset_type::const_iterator key =
	    extra.parser_state.working_stringset.begin();

	while (key != extra.parser_state.working_stringset.end()) {
	    typename parser_state_type::string_index_map_type::const_iterator
		prev = extra.parser_state.materiallib_index_map.find(*key);

	    if (prev != extra.parser_state.materiallib_index_map.end()) {
		new_mtllibset.push_back(prev->second);
	    } else {
		// never seen this materiallib name before, add to contents and
		// maps
		stringvec_type &matlib_set = extra.contents->materiallib_set;

		new_mtllibset.push_back(matlib_set.size());

		matlib_set.push_back(*key);

		string_type &matlib_name = matlib_set.back();

		char *matlibString = createCopyOfString(matlib_name.c_str(),
		    matlib_name.size() + 1);

		extra.contents->materiallibchar_set.push_back(matlibString);

		extra.parser_state.materiallib_index_map[*key] =
		    new_mtllibset.back();
	    }
	    ++key;
	}

	// sort the new index set to ensure unique for all ordering
	std::sort(new_mtllibset.begin(), new_mtllibset.end());

	// map this set of mtllibname strings to the location in contents for
	// fast lookup later if needed
	extra.parser_state.
	    materiallibset_index_map[extra.parser_state.working_stringset] =
		extra.contents->materiallibindex_set.size();

	// set the current working mtllibset
	extra.parser_state.current_materiallib =
	    extra.contents->materiallibindex_set.size();

	// new_mtllibset now contains a unique set of indices into
	// contents->materiallib_set;
	extra.contents->materiallibindex_set.
	    resize(extra.contents->materiallibindex_set.size() + 1);

	swap(extra.contents->materiallibindex_set.back(), new_mtllibset);
    }

    extra.parser_state.working_stringset.clear();

    extra.parser_state.working_polyattributes.materiallibset_index = 
	extra.parser_state.current_materiallib;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_materiallib */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_texmap(basic_parser_extra<PrecisionT, charT, traits,
					   Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;

    typename parser_state_type::texmap_index_map_type::iterator res =
	extra.parser_state.texmap_index_map.
	    find(extra.parser_state.working_string);

    if (res != extra.parser_state.texmap_index_map.end()) {
	extra.parser_state.current_texmap = res->second;
	extra.parser_state.working_string.clear();
    } else {
	extra.parser_state.current_texmap = extra.contents->texmap_set.size();

	string_type &working_string = extra.parser_state.working_string;

	extra.contents->texmap_set.push_back(working_string);

	char *texmapString = createCopyOfString(working_string.c_str(),
	    working_string.size() + 1);

	extra.contents->texmapchar_set.push_back(texmapString);
    }

    extra.parser_state.working_polyattributes.texmap_index =
	extra.parser_state.current_texmap;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_texmap */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_texmaplib(basic_parser_extra<PrecisionT, charT, traits,
					      Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::stringvec_type stringvec_type;

    typename parser_state_type::texmaplibset_index_map_type::const_iterator res
	= extra.parser_state.texmaplibset_index_map.
	    find(extra.parser_state.working_stringset);

    if (res != extra.parser_state.texmaplibset_index_map.end()) {
	extra.parser_state.current_texmaplib = res->second;
	extra.parser_state.working_stringset.clear();
    } else {
	typename parser_state_type::indexvec_type new_texlibset;
	new_texlibset.reserve(extra.parser_state.working_stringset.size());

	// not already seen, set in texmaplibset_index_map
	typename parser_state_type::stringset_type::const_iterator key =
	    extra.parser_state.working_stringset.begin();

	while (key != extra.parser_state.working_stringset.end()) {
	    typename parser_state_type::string_index_map_type::const_iterator
		prev = extra.parser_state.texmaplib_index_map.find(*key);

	    if (prev != extra.parser_state.texmaplib_index_map.end()) {
		new_texlibset.push_back(prev->second);
	    } else {
		stringvec_type &texmaplib_set = extra.contents->texmaplib_set;

		// never seen this texmaplib name before, add to contents and
		// maps
		new_texlibset.push_back(texmaplib_set.size());

		texmaplib_set.push_back(*key);

		char *texmapString = createCopyOfString(key->c_str(),
		    key->size() + 1);

		extra.contents->texmaplibchar_set.push_back(texmapString);

		extra.parser_state.texmaplib_index_map[*key] =
		    new_texlibset.back();
	    }
	    ++key;
	}

	// sort the new index set to ensure unique for all ordering
	std::sort(new_texlibset.begin(), new_texlibset.end());

	// map this set of mtllibname strings to the location in contents for
	// fast lookup later if needed
	extra.parser_state.
	    texmaplibset_index_map[extra.parser_state.working_stringset] =
		extra.contents->texmaplibindex_set.size();

	// set the current working texmaplibset
	extra.parser_state.current_texmaplib =
	    extra.contents->texmaplibindex_set.size();

	// new_texlibset now contains a unique set of indices into
	// contents->texmaplib_set;
	extra.contents->texmaplibindex_set.
	    resize(extra.contents->texmaplibindex_set.size() + 1);

	swap(extra.contents->texmaplibindex_set.back(), new_texlibset);
    }

    extra.parser_state.working_stringset.clear();

    extra.parser_state.working_polyattributes.texmaplibset_index = 
	extra.parser_state.current_texmaplib;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_texmaplib */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_shadow_obj(basic_parser_extra<PrecisionT, charT, traits,
					       Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;

    typename parser_state_type::shadow_obj_index_map_type::iterator res =
	extra.parser_state.shadow_obj_index_map.
	    find(extra.parser_state.working_string);

    if (res != extra.parser_state.shadow_obj_index_map.end()) {
	extra.parser_state.current_shadow_obj = res->second;
	extra.parser_state.working_string.clear();
    } else {
	extra.parser_state.current_shadow_obj =
	    extra.contents->shadow_obj_set.size();

	string_type &working_string = extra.parser_state.working_string;

	extra.contents->shadow_obj_set.push_back(working_string);

	char *shadowObjString = createCopyOfString(working_string.c_str(),
	    working_string.size() + 1);

	extra.contents->shadow_objchar_set.push_back(shadowObjString);
    }

    extra.parser_state.working_polyattributes.shadow_obj_index =
	extra.parser_state.current_shadow_obj;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_shadow_obj */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_trace_obj(basic_parser_extra<PrecisionT, charT, traits,
					      Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;
    typedef typename contents_type::string_type string_type;

    typename parser_state_type::trace_obj_index_map_type::iterator res =
	extra.parser_state.trace_obj_index_map.
	    find(extra.parser_state.working_string);

    if (res != extra.parser_state.trace_obj_index_map.end()) {
	extra.parser_state.current_trace_obj = res->second;
	extra.parser_state.working_string.clear();
    } else {
	extra.parser_state.current_trace_obj =
	    extra.contents->trace_obj_set.size();

	string_type &working_string = extra.parser_state.working_string;

	extra.contents->trace_obj_set.push_back(working_string);

	char *traceObjString = createCopyOfString(working_string.c_str(),
	    working_string.size() + 1);

	extra.contents->trace_objchar_set.push_back(traceObjString);
    }

    extra.parser_state.working_polyattributes.trace_obj_index =
	extra.parser_state.current_trace_obj;

    extra.parser_state.polyattributes_dirty = true;
} /* set_working_trace_obj */


template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
void set_working_polygattributes(basic_parser_extra<PrecisionT, charT, traits,
						    Allocator> &extra)
{
    typedef basic_parser_extra<PrecisionT, charT, traits, Allocator> extra_type;
    typedef typename extra_type::contents_type contents_type;
    typedef typename extra_type::parser_state_type parser_state_type;

    typename parser_state_type::polyattributes_index_map_type::iterator res =
	extra.parser_state.polyattributes_index_map.
	    find(extra.parser_state.working_polyattributes);

    if (res != extra.parser_state.polyattributes_index_map.end()) {
	extra.parser_state.current_polyattributes = res->second;
    } else {
	extra.parser_state.current_polyattributes =
	    extra.contents->polyattributes_set.size();

	extra.contents->polyattributes_set.
	    resize(extra.contents->polyattributes_set.size() + 1,
		   extra.parser_state.working_polyattributes);
    }
}


/**
 *  Set the inital values of the lookup tables to the first element in the
 *  contents
 */
template<typename PrecisionT,
	 typename charT,
	 typename traits,
	 typename Allocator>
basic_parser_extra<PrecisionT, charT, traits, Allocator>::
basic_parser_extra(basic_parser_type *p, contents_type *c)
: basic_parser(p)
, contents(c)
{
    parser_state.working_stringset.insert("default");
    // since the working string is empty, these will set to the default ""
    set_working_groupset(*this);
    set_working_object(*this);
    set_working_material(*this);
    parser_state.working_stringset.insert("");
    set_working_materiallib(*this);
    set_working_texmap(*this);
    parser_state.working_stringset.insert("");
    set_working_texmaplib(*this);
    set_working_shadow_obj(*this);
    set_working_trace_obj(*this);
    parser_state.working_polyattributes.smooth_group = 0;
    parser_state.working_polyattributes.bevel = false;
    parser_state.working_polyattributes.c_interp = false;
    parser_state.working_polyattributes.d_interp = false;
    parser_state.working_polyattributes.lod = 0;
    parser_state.polyattributes_dirty = true;
}


/**
 *  InputIterator is a model of pointer to FileNode concept
 *  OutputStreamT is a model of OutputStream
 */
template<typename InputIterator, typename OutputStreamT>
void include_chain_formatter(InputIterator first, InputIterator last,
			     OutputStreamT &str)
{
    if (first == last) {
	return;
    }

    str << "In file included from " << first->path;

    while (++first != last) {
	str << "\n                 from " << first->path;
    }

    str << "\n";
}


template<typename ParserStateT>
void verbose_output_formatter(ParserStateT &state, const char *s)
{
    if (state.file_stack.size() > 1) {
	include_chain_formatter(state.file_stack.begin(),
				state.file_stack.end(), state.err);
    }

    state.err << state.file_stack.back().path << ":"
	      << state.file_stack.back().lineno << ": " << s;
}


template<typename ParserStateT>
inline void output_formatter(ParserStateT &state, const char *s)
{
    state.err << state.file_stack.back().path << ":"
	      << state.file_stack.back().lineno << ": " << s;
}

typedef basic_obj_contents<float, char> objFileContents;
typedef basic_obj_parser<char> objParser;
typedef basic_parser_extra<float, char> objCombinedState;

} /* namespace detail */
} /* namespace obj_parser */
} /* namespace arl */


#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
