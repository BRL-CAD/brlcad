/*                  G I T 2 G O U R C E . C P P
 * BRL-CAD
 *
 * Published in 2021 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file git2gource.cpp
 *
 * Read a git log file written using the gource --log-command git format and
 * generate the gource custom input format
 *
 */

#include <string>
#include <iostream>
#include <fstream>

bool
only_num(std::string &s)
{
    if (!s.length())
	return false;

    for (size_t i = 0; i < s.length(); i++)
	if (!isdigit(s[i])) return false;

    return true;
}

int
main(int argc, const char *argv[])
{
    if (argc != 2)
	return -1;

    std::ifstream log;
    log.open(argv[1]);
    if (!log.is_open()) {
	return -1;
    }

    std::string sfile;
    std::string author;
    std::string timestamp;
    while (std::getline(log, sfile)) {
	if (!sfile.compare(0, 5, "user:")) {
	    author = sfile.substr(5, std::string::npos);
	    continue;
	}
	if (only_num(sfile)) {
	    timestamp = sfile;
	    continue;
	}

	if (!sfile.length()) {
	    author = std::string("");
	    timestamp = std::string("");
	    continue;
	}
	std::string s1 = sfile.substr(37, std::string::npos);
	std::string mode = s1.substr(0, 1);
	std::string path = s1.substr(2, std::string::npos);
	std::cout << timestamp << "|" << author << "|" << mode << "|" << path << "\n";
    }

    log.close();

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

