// see db5_types.c for registered attribute info

// scheme: parse guts of function "int db5_standardize_attribute"
//  in file "db5_types.c"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <set>
#include <sys/stat.h>

using namespace std;

// local funcs
static void get_attrs(const string& fname, set<string>& attrs);
static void open_file_read(ifstream& fs, const string& f);
static void open_file_write(ofstream& fs, const string& f);
static bool file_exists(const std::string& fname);

// local vars
static bool debug(false);

int
main()
{
    set<string> attrs;
    string fname("db5_types.c");

    get_attrs(fname, attrs);

    // write the html file
    string ofil("t.html");
    ofstream fo;
    open_file_write(fo, ofil);

    return 0;
}

void
get_attrs(const string& fname, set<string>& attrs)
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
        size_t idx = line.find("end-registered-attributes-list");
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
        attrs.insert(attr);
    }
} // get_attrs

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
