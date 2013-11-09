/*                 D B 5 _ A T T R S _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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
/** @file db5_attrs_private.h
 *
 * These are declarations for functions that are for internal use by
 * LIBRT programs and functions but are not public API.  NO CODE
 * outside of LIBRT should use these functions.
 *
 */
#ifndef DB5_ATTRS_PRIVATE_H
#define DB5_ATTRS_PRIVATE_H

#include "raytrace.h" /* for the ATTR_x enum definitons, the master source */

#if defined(__cplusplus)
  #include <map>
  #include <string>
  #include <set>
/*
  #include <boost/container/flat_map.hpp>
  #include <boost/container/flat_set.hpp>
*/
#endif

#if defined(__cplusplus)
extern "C" {
#endif

    int db5_attr_is_standard_attribute(const char *attr_want);
    int db5_attr_standardize_attribute(const char *attr_want);
    const char *db5_attr_standard_attribute(const int attr_index);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
namespace db5_attrs_private {

    enum { ATTR_STANDARD, ATTR_REGISTERED };

    struct db5_attr_t {
        bool is_binary; /* false for ASCII attributes; true for binary attributes */
        int atype;      /* from enum above */

	/*
	  names should be specified with alphanumeric charcters
	  (lower-case letters, no white space) and will act as unique
	  keys to an object's attribute list
        */
	std::string name;         /* the "standard" name */
        std::string description;
        std::string examples;
        std::set<std::string> aliases; /* comma-delimited list of alternative names for this attribute */

	db5_attr_t()
	    {}
        db5_attr_t(const int b, const int a, const std::string& n,
		   const std::string& d, const std::string& e,
		   const std::set<std::string>& s)
	: is_binary(b ? true : false)
	, atype(a)
	, name(n)
	, description(d)
	, examples(e)
	, aliases(s)
	    {}
    };

    extern std::map<int,db5_attr_t>         int2attr;
    extern std::map<std::string,db5_attr_t> name2attr;
}
#endif

#endif /* #ifndef DB5_ATTRS_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
