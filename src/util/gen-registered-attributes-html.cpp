// see db5_types.c for registered attribute info

// scheme: parse guts of function "int db5_standardize_attribute"
//  in file "db5_types.c"

#include "common.h"

#include "raytrace.h"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <sys/stat.h>

using namespace std;

// local struct
struct attr_t {
    const string name;   // the  "standard" name
    int index;           // the official index number for he "standard" name
    string def;
    bool binary;

    string example;
    set<string> aliases; // as found in the original list
    attr_t(const string& a, const int idx, const string& d)
        : name(a)
        , index(idx)
        , def(d)
        , binary(false)
        {}
};

// local funcs
static bool file_exists(const std::string& fname);
static void gen_attr_html_page(const std::string& fname,
                               map<string,attr_t>& sttrs, map<string,attr_t>& rttrs);
static void get_standard_attrs(const string& fname, map<string,attr_t>& attrs);
static void open_file_read(ifstream& fs, const string& f);
static void open_file_write(ofstream& fs, const string& f);
// FIXME: following to be added later (see rough function body below):
//static void get_registered_attrs(const string& fname, map<string,attr_t>& attrs);

// local vars
static bool debug(true);

int
main()
{
    map<string,attr_t> sattrs; // standard attributes
    map<string,attr_t> rattrs; // registered attributes
    string fname("db5_types.c");

    get_standard_attrs(fname, sattrs);
    //get_registered_attrs(fname, rattrs);

    // write the html file
    string ofil("t.html");
    gen_attr_html_page(ofil, sattrs, rattrs);
    return 0;
}

void
get_standard_attrs(const string& fname, map<string,attr_t>& attrs)
{
    ifstream fi;
    open_file_read(fi, fname.c_str());

    set<string> aliases;

    bool found_list(false);
    int linenum(0);
    string line;
    while (!getline(fi, line).eof()) {
        ++linenum;

        if (0 && debug) {
            printf("DEBUG(%s, %u): line %u:\n", __FILE__, __LINE__, linenum);
            printf("  {{%s}}\n", line.c_str());
        }

        // check for begining of list
        if (!found_list) {
            if (line.find("begin-standard-attributes-list") == string::npos)
                continue;
            found_list = true;
            continue;
        }

        // quit at end of list
        size_t idx = line.find("end-standard-attributes-list");
        if (idx != string::npos)
            break;

        // look for attr def lines
        idx = line.find("BU_STR_EQUIV");
        if (idx == string::npos)
            continue;

        // extract the attribute between the double quotes
        idx = line.find_first_of('"', idx+1);
        size_t ridx = line.find_last_of('"');
        string attr = line.substr(idx+1, ridx - idx - 1);
        // ensure it's lower case
        for (size_t i = 0; i < attr.size(); ++i)
            attr[i] = tolower(attr[i]);
        if (debug)
            printf("DEBUG: Found attribute '%s'\n", attr.c_str());
        aliases.insert(attr);
    }

    // now associate names with standard name
    for (set<string>::iterator i = aliases.begin(); i != aliases.end(); ++i) {
        if (debug)
            printf("DEBUG: found alias in list '%s'\n", (*i).c_str());
        const string& alias(*i);
        // get info on standard name and index
        int idx = db5_standardize_attribute(alias.c_str());
        if (idx == ATTR_NULL) {
            bu_log("unexpected ATTR_NULL for attr alias '%s'", alias.c_str());
            bu_bomb("unexpected ATTR_NULL for attr alias\n");
        }

        string std_attr = db5_standard_attribute(idx);
        if (debug)
            printf("DEBUG:  Standard attr for index %d is '%s'\n", idx, std_attr.c_str());
        // need a definition or format
        string def = db5_standard_attribute_def(idx);
        // is it binary? (a kludge for now)
        bool binary = def.find("binary") == string::npos ? false : true;

        if (attrs.find(std_attr) == attrs.end()) {
            attr_t d(std_attr, idx, def);
            d.binary = binary;
            attrs.insert(make_pair(std_attr,d));
        }

        // so is this an alias or the real deal?
        if (alias == std_attr) {
            ; // do nothing
        } else {
            map<string,attr_t>::iterator j = attrs.find(std_attr);
            if (j != attrs.end()) {
                attr_t& d = j->second;
                d.aliases.insert(alias);
            } else {
                bu_log("unexpected no-find for std attr '%s'", std_attr.c_str());
                bu_bomb("unexpected no-find for std_attr\n");
            }
        }
    }
    if (debug)
        printf("DEBUG:  Finished collecting attrs...\n");

} // get_standard_attrs

void
open_file_write(ofstream& fs, const string& f)
{
  fs.open(f.c_str());
  if (fs.bad()) {
    fs.close();
    printf("File '%s' open error.\n", f.c_str());
    exit(1);
  }
} // open_file_write

void
open_file_read(ifstream& fs, const string& f)
{
  if (!file_exists(f)) {
      printf("File '%s' not found.\n", f.c_str());
      exit(1);
  }

  fs.open(f.c_str());
  if (fs.bad()) {
    fs.close();
    printf("File '%s' close error.\n", f.c_str());
    exit(1);
  }
} // open_file_read

bool
file_exists(const std::string& fname)
{
    // see <bits/stat.h>
    struct stat buf;
    return (!stat(fname.c_str(), &buf)
            && (S_ISREG(buf.st_mode)|S_ISDIR(buf.st_mode)));
} // file_exists

void
gen_attr_html_page(const std::string& fname,
                   map<string,attr_t>& sattrs,
                   map<string,attr_t>& rattrs
                   )
{
    ofstream fo;
    open_file_write(fo, fname);

    fo <<
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <title>brlcad-attributes.html</title>\n"
        "  <meta charset = \"UTF-8\" />\n"
        "  <style type = \"text/css\">\n"
        "  table, td, th {\n"
        "    border: 1px solid black;\n"
        "  }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>BRL-CAD Standard and User-Registered Attributes</h2>\n"
        "  <p>Following are lists of the BRL-CAD standard and user-registered attribute names\n"
        "  along with their value definitions and aliases (if any).  Users should\n"
        "  not assign values to them in other than their defined format.\n"
        "  (Note that attribute names are not case-sensitive although their canonical form is\n"
        "  lower-case.)</p>\n"

        "  <p>Some attributes have ASCII names but binary values (e.g., 'mtime').  Their values cannot\n"
        "  be modifed by a user with the 'attr' command.  In some cases, but not all, their\n"
        "  values may be shown in a human readable form with the 'attr' command.)</p>\n"

        "  <p>If a user wishes to register an attribute to protect its use for models\n"
        "  transferred to other BRL-CAD users, submit the attribute along with a description\n"
        "  of its intended use to the\n"
        "  <a href=\"mailto:brlcad-devel@lists.sourceforge.net\">BRL-CAD developers</a>.\n"
        "  Its approval will be formal when it appears in the separate, registered attribute\n"
        "  table following the standard attribute table.</p>\n"
        ;

    // need a table here (5 columns at the moment)
    fo <<
        "  <h3>BRL-CAD Standard Attributes</h3>\n"
        "  <table>\n"
        "    <tr>\n"
        "      <th>Attribute</th>\n"
        "      <th>Binary?</th>\n"
        "      <th>Definition</th>\n"
        "      <th>Example</th>\n"
        "      <th>Aliases</th>\n"
        "    </tr>\n"
        ;

    for (map<string,attr_t>::iterator i = sattrs.begin(); i != sattrs.end(); ++i) {
        const string& a(i->first);
        attr_t& d(i->second);
        fo <<
            "    <tr>\n"
            "      <td>" << a     << "</td>\n"
            "      <td>" << (d.binary ? "yes" : "")     << "</td>\n"
            "      <td>" << d.def << "</td>\n"
            "      <td>" << d.example << "</td>\n"
            "      <td>"
            ;
        if (!d.aliases.empty()) {
            for (set<string>::iterator j = d.aliases.begin();
                 j != d.aliases.end(); ++j) {
                if (j != d.aliases.begin())
                    fo << ", ";
                fo << *j;
            }
        }
        fo <<
            "</td>\n"
            "    </tr>\n"
            ;
    }
    fo << "  </table>\n";

    fo << "  <h3>User-Registered Attributes</h3>\n";
    if (rattrs.empty()) {
        fo << "    <p>None at this time.</p>\n";
    } else {
        // need a table here
        fo <<
            "  <table>\n"
            "    <tr>\n"
            "      <th>Attribute</th>\n"
            "      <th>Binary?</th>\n"
            "      <th>Definition</th>\n"
            "      <th>Example</th>\n"
            "      <th>Aliases</th>\n"
            "    </tr>\n"
            ;
        for (map<string,attr_t>::iterator i = rattrs.begin(); i != rattrs.end(); ++i) {
            const string& a(i->first);
            attr_t& d(i->second);
            fo <<
                "    <tr>\n"
                "      <td>" << a     << "</td>\n"
                "      <td>" << (d.binary ? "yes" : "")     << "</td>\n"
                "      <td>" << d.def << "</td>\n"
                "      <td>" << d.example << "</td>\n"
                "      <td>"
                ;
            if (!d.aliases.empty()) {
                for (set<string>::iterator j = d.aliases.begin();
                     j != d.aliases.end(); ++i) {
                    if (j != d.aliases.begin())
                        fo << ", ";
                    fo << *j;
                }
            }
            fo <<
                "</td>\n"
                "    </tr>\n"
                ;
        }
        fo << "  </table>\n";
    }

    fo <<
        "</body>\n"
        "</html>\n"
        ;

    fo.close();

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_html_page

// FIXME: complete this function later when supporting functions in db5_types.c are ready
/*
void
get_registered_attrs(const string& fname, map<string,attr_t>& attrs)
{

    ifstream fi;
    open_file_read(fi, fname.c_str());

    bool found_list(false);
    int linenum(0);
    string line;
    while (!getline(fi, line).eof()) {
        ++linenum;

        if (debug) {
            printf("DEBUG(%s, %u): line %u:\n", __FILE__, __LINE__, linenum);
            printf("  {{%s}}\n", line.c_str());
        }

        // check for begining of list
        if (!found_list) {
            if (line.find("begin-registered-attributes-list") == string::npos)
                continue;
            found_list = true;
            continue;
        }

        // quit at end of list
        size_t idx = line.find("end-registerd-attributes-list");
        if (idx != string::npos)
            break;

        // look for attr def lines
        idx = line.find("BU_STR_EQUIV");
        if (idx == string::npos)
            continue;

        // extract the attribute between the double quotes
        idx = line.find_first_of('"', idx+1);
        size_t ridx = line.find_last_of('"');
        string attr = line.substr(idx+1, ridx - idx - 1);
        // ensure it's lower case
        for (size_t i = 0; i < attr.size(); ++i)
            attr[i] = tolower(attr[i]);
        printf("Found attribute '%s'\n", attr.c_str());
        // need a definition or format
        string def;
        attrs.insert(make_pair(attr,def));
    }
} // get_registered_attrs
*/
