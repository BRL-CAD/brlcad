/*                 D B 5 _ A T T R S . C P P
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

#include "db5_attrs_private.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <map>
#include <set>

#include "boost/tokenizer.hpp"

// boost tokenizer reusable parts
typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
static const boost::char_separator<char> my_sep(" \t\n\r;,");

using namespace std;

namespace db5_attrs_private {

    struct db5_attr_ctype {
        int attr_type;    // from enum in 'raytrace.h'
        bool is_binary;   // false for ASCII attributes; true for binary attributes
        int attr_subtype; // from enum in 'db5_attrs_private.h'

        // names should be specified with alphanumeric characters
        // (lower-case letters, no white space) and will act as unique
        // keys to an object's attribute list
        const char *name;         // the "standard" name
        const char *description;
        const char *examples;
        // comma-delimited list of alternative names for this attribute:
        const char *aliases;
        // property name as found in file 'attributes.xml'; user-defined attrs may not have this defined:
        const char *property;

    };

    // this is the master source of standard and registered attributes
    static const db5_attr_ctype db5_attr_ctype_table[] = {
        { ATTR_REGION, false, ATTR_STANDARD,
          "region",
          "Region Flag: boolean",
          "Yes, R, 1, 0", // example
          "", // aliases, if any
          ""  // property, if any
        },
        { ATTR_REGION_ID, false, ATTR_STANDARD,
          "region_id",
          "an integer",
          "0, -1, and positive integers", // examples
          "id",  // aliases, if any
          "Region Identifier Number"  // property, if any
        },
        { ATTR_MATERIAL_ID, false, ATTR_STANDARD,
          "material_id",
          "zero or positive integer (user-defined)",
          "", // examples
          "giftmater,mat",  // aliases, if any
          "Material Identifier Number"  // property, if any
        },
        { ATTR_AIR, false, ATTR_STANDARD,
          "aircode",
          "an integer (application defined)",
          "'0', '1', or '-2'", // examples
          "air",  // aliases, if any
          "Air Code"  // property, if any
        },
        { ATTR_LOS, false, ATTR_STANDARD,
          "los",
          "an integer in the inclusive range: 0 to 100",
          "'24' or '100'", // examples
          "",  // aliases, if any
          "Line of Sight Thickness Equivalence"  // property, if any
        },
        { ATTR_COLOR, false, ATTR_STANDARD,
          "color",
          "a 3-tuple of RGB values",
          "\"0 255 255\"", // examples
          "rgb",  // aliases, if any
          "Color"  // property, if any
        },
        { ATTR_SHADER, false, ATTR_STANDARD,
          "shader",
          "a string of shader characteristics in a standard format",
          "", // examples
          "oshader",  // aliases, if any
          "Shader Name"  // property, if any
        },
        { ATTR_INHERIT, false, ATTR_STANDARD,
          "inherit",
          "boolean",
          "Yes, 1, 0", // examples
          "",  // aliases, if any
          "Inherit Properties"  // property, if any
        },
        { ATTR_TIMESTAMP, true, ATTR_STANDARD, /* first binary attribute */
          "mtime",

          "a binary time stamp for an object's last mod time (the"
          " time is displayed in human-readable form with the 'attr' command)",

          "", // examples
          "timestamp,time_stamp,modtime,mod_time",  // aliases, if any
          "Time Stamp"  // property, if any
        },
    };

    static const size_t nattrs = sizeof(db5_attr_ctype_table)/sizeof(db5_attr_ctype);

    map<string,int> name2int;
    map<int,db5_attr_t> int2attr;
    map<string,db5_attr_t> name2attr;

    void load_maps();
    void get_tokens(set<string>& toks, const string& str,
                    const boost::char_separator<char>& sep);
} // namespace db5_attrs_private

using namespace db5_attrs_private;

void
db5_attrs_private::load_maps()
{
    if (!int2attr.empty())
        return;

    // don't allow duplicate names
    set<string> used_names;
    // don't allow duplicate attr_types
    set<int> used_types;
    for (size_t i = 0; i < nattrs; ++i) {
        const db5_attr_ctype &a = db5_attr_ctype_table[i];
        const string name      = a.name;
        const string desc      = a.description;
        const string examp     = a.examples;
        const string Aliases   = a.aliases;
        const string prop      = a.property;

        // check for invalid subtype
        const int st = a.attr_subtype;
        if (st != ATTR_STANDARD && st != ATTR_REGISTERED) {
            bu_log("unknown attr_subtype '%d'\n", st);
            bu_bomb("unknown attr_subtype\n");
        }

        if (used_names.find(name) == used_names.end()) {
            used_names.insert(name);
        } else {
            bu_log("duplicate attr name '%s'\n", name.c_str());
            bu_bomb("duplicate attr name\n");
        }

        if (used_types.find(a.attr_type) == used_types.end()) {
            used_types.insert(a.attr_type);
        } else {
            bu_log("duplicate attr_type '%d'\n", a.attr_type);
            bu_bomb("duplicate attr_type\n");
        }

        set<string> aliases;
        get_tokens(aliases, Aliases, my_sep);
        for (set<string>::iterator j = aliases.begin(); j != aliases.end(); ++j) {
            const string& n(*j);
            if (used_names.find(n) == used_names.end()) {
                used_names.insert(n);
            } else {
                bu_log("duplicate attr alias '%s'\n", n.c_str());
                bu_bomb("duplicate attr alias\n");
            }
        }
        db5_attr_t ap(a.is_binary, a.attr_subtype, name, desc, examp, aliases, prop);

        // prepare the maps
        name2int.insert(make_pair(name, a.attr_type));

        // also include aliases
        for (set<string>::iterator j = ap.aliases.begin(); j != ap.aliases.end(); ++j)
            name2int.insert(make_pair(*j, a.attr_type));

        int2attr.insert(make_pair(a.attr_type, ap));
        name2attr.insert(make_pair(name, ap));
    }

}

int
db5_attr_is_standard_attribute(const char *attr_want)
{
    string aname(attr_want);
    for (size_t i = 0; i < aname.size(); ++i)
        aname[i] = tolower(aname[i]);

    if (name2int.empty())
        load_maps();

    if (name2int.find(aname) != name2int.end())
        return name2int[aname];
    else
        return ATTR_NULL;
}

int
db5_attr_standardize_attribute(const char *attr_want)
{
    string aname(attr_want);
    for (size_t i = 0; i < aname.size(); ++i)
        aname[i] = tolower(aname[i]);

    if (name2int.empty())
        load_maps();

    if (name2int.find(aname) != name2int.end())
        return name2int[aname];
    else
        return ATTR_NULL;
}

const char *
db5_attr_standard_attribute(const int attr_index)
{
    if (int2attr.empty())
        load_maps();

    if (int2attr.find(attr_index) != int2attr.end())
        return int2attr[attr_index].name.c_str();
    else
        return NULL;
}


void
db5_attrs_private::get_tokens(set<string>& toks,
                              const string& str,
                              const boost::char_separator<char>& sep)
{
  // from boost tokenizer file char_sep_example_1.cpp
  tokenizer tokens(str, sep);

  toks.clear();
  for (tokenizer::iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    toks.insert(*it);
  }
} // get_tokens
