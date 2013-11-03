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
    const string alias; // as found in the original list
    string sname;       // the  "standard" name
    string sdef;
    int sindex;          // the official index number for he "standard" name
    attr_t(const string& a)
        : alias(a)
        , sname("")
        , sdef("")
        , sindex(-1) // indicates unknown
        {}
};

// local funcs
static bool file_exists(const std::string& fname);
static void gen_attr_html_page(const std::string& fname,
                               map<string,attr_t>& sttrs, map<string,attr_t>& rttrs);
static void get_registered_attrs(const string& fname, map<string,attr_t>& attrs);
static void get_standard_attrs(const string& fname, map<string,attr_t>& attrs);
static void open_file_read(ifstream& fs, const string& f);
static void open_file_write(ofstream& fs, const string& f);

// local vars
static bool debug(false);

int
main()
{
    map<string,attr_t> sattrs; // standard attributes
    map<string,attr_t> rattrs; // registered attributes
    string fname("db5_types.c");

    get_standard_attrs(fname, sattrs);
    get_registered_attrs(fname, rattrs);

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
        printf("Found attribute '%s'\n", attr.c_str());
        // need a definition or format (empty for now)
        attr_t d(attr);
        attrs.insert(make_pair(attr,d));
    }

    // now associate names with standard name

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
        "</head>\n"
        "<body>\n"
        "  <h1>Standard Attributes</h2>\n"
        "  <p>Following is a list of the BRL-CAD standard attributes.  Users should\n"
        "  not assign values to them in other than their example format.</p>\n"

        "  <p>If a user wishes to register an attribute to protect its use for models\n"
        "  transferred to other BRL-CAD users, submit the attribute along with a description\n"
        "  of its intended use to <mailto>...\n"
        "  Its approval will be formal when it appears in a separate list following the\n"
        "  standard attributes.</p>\n"

        ;

    // the list
    fo << "  <h3>Standard Attributes</h3>\n";
    fo << "  <ol>\n";

    // need a table here
    for (map<string,attr_t>::iterator i = sattrs.begin(); i != sattrs.end(); ++i) {
        const string& a(i->first);
        attr_t& d(i->second);
        fo << "    <li>" << a << " (" << d.alias << ")</li>\n";
    }
    fo << "  </ol>\n";

    fo << "  <h3>Registered Attributes</h3>\n";
    if (rattrs.empty()) {
        fo << "    <p>None at this time.</p>\n";
    } else {
        fo << "  <ol>\n";
        // need a table here
        for (map<string,attr_t>::iterator i = rattrs.begin(); i != rattrs.end(); ++i) {
            const string& a(i->first);
            attr_t& d(i->second);
            fo << "    <li>" << a << " (" << d.alias << ")</li>\n";
        }
        fo << "  </ol>\n";
    }

    fo <<
        "</body>\n"
        "</html>\n"
        ;

} // gen_attr_html_page

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
