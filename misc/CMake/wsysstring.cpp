#include <fstream>
#include <iostream>
#include <cstring>
#include <string>

int
main(int argc, const char *argv[])
{
    bool have_x11 = false;
    bool have_win32 = false;
    bool have_aqua = false;

    if (argc != 2) {
	std::cerr << "Usage: wsysstring <file>\n";
	return -1;
    }

    std::string sfile;
    std::ifstream lib;
    lib.open(argv[1]);
    if (!lib.is_open()) {
	std::cerr << "Unable to open file" << argv[1] << "\n";
	return -1;
    }

    lib.seekg(0, std::ios::end);
    size_t llen = lib.tellg();
    lib.seekg(0, std::ios::beg);
    std::string lcontent;
    lcontent.reserve(llen);
    char buff[16384]; // 2^14
    std::streamsize ss;
    while (lib.read(buff, sizeof buff), ss = lib.gcount()) {
	lcontent.append(buff, ss);
    }

    // Have contents, find any printable substrings and check them
    size_t cpos = 0;
    std::string cstr;
    while (cpos < lcontent.length()) {
	char c = lcontent[cpos];
	if (isprint(c)) {
	    cstr.push_back(c);
	} else {
	    // Hit a non-printable - if we have a cstr, work it
	    // and reset
	    if (cstr.length() > 2) {
		if (cstr == std::string("x11")) {
		    have_x11 = true;
		}
		if (cstr == std::string("win32")) {
		    have_win32 = true;
		}
		if (cstr == std::string("aqua")) {
		    have_aqua = true;
		}
	    }
	    cstr.clear();
	}
	cpos++;
    }

    int mcnt = 0;
    mcnt += have_x11;
    mcnt += have_win32;
    mcnt += have_aqua;
    if (mcnt > 1) return -1;

    if (mcnt == 1) {
	if (have_x11) std::cout << "x11\n";
	if (have_win32) std::cout << "win32\n";
	if (have_aqua) std::cout << "aqua\n";
	return 1;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
