#include <cstddef>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <regex>

#define sfcmp(_s1, _s2) _s1.compare(0, _s2.size(), _s2) && _s1.size() >= _s2.size()
#define get_str(_s1, _s2) (!sfcmp(_s1, _s2)) ? _s1.substr(_s2.size(), _s1.size()-_s2.size()) : std::string()

bool chk_msg(std::string &cmsg)
{
    std::regex sync_txt(".*[Ss]ync[ ].*");
    std::regex rev_txt(".*[0-9][0-9][0-9][0-9][0-9].*");
    std::regex mfc_txt(".*MFC[ ].*");
    std::regex merge_txt("^[Mm]erge[ ].*");

    if (std::regex_match(cmsg, sync_txt)) return true;
    if (std::regex_match(cmsg, rev_txt)) return true;
    if (std::regex_match(cmsg, mfc_txt)) return true;
    if (std::regex_match(cmsg, merge_txt)) return true;
#if 0 
    std::regex pull_txt(".*[Pp]ull[ ].*");
    std::regex part_txt(".*[Pp]art[ ].*");
    std::regex update_txt(".*[Uu]pdate[ ].*");
    std::regex trunk_txt(".*trunk.*");

    if (std::regex_match(cmsg, pull_txt)) return true;
    if (std::regex_match(cmsg, part_txt)) return true;
    if (std::regex_match(cmsg, update_txt)) return true;
    if (std::regex_match(cmsg, trunk_txt)) return true;
#endif
    return false;
}


int main(int argc, const char **argv)
{
    std::string c1key("commit");
    std::string c2key("committer ");
    std::string markkey("mark");
    std::string dkey("data");
    std::string fkey("from");
    std::string mergekey("merge");
    std::set<std::string> files;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("./commits")) != NULL) {
	while ((ent = readdir(dir)) != NULL) {
	    std::string f = std::string("./commits/") + std::string(ent->d_name);
	    files.insert(f);
	}
    }
    std::set<std::string>::iterator f_it;
    for (f_it = files.begin(); f_it != files.end(); f_it++) {
	std::ifstream infile(*f_it);
	if (!infile.good()) return -1;
	std::string line;
	std::string cmsg;
	long int cmsg_len = 0;
	int have_merge = 0;
	while (std::getline(infile, line)) {

	    size_t spos = line.find_first_of(" ");

	    std::string key = line.substr(0, spos);
	    std::string val = line.substr(spos+1, std::string::npos);
	    if (key == dkey) {
		long int dlen = std::stol(val);
		char vbuff[dlen+1];
		infile.read(vbuff, dlen);
		vbuff[dlen] = '\0';
		cmsg = std::string(vbuff);
	    }
	    if (key == mergekey) {
		have_merge = 1;
	    }
	}
	if (chk_msg(cmsg) && !have_merge) {
	    std::cout << *f_it << ": " << cmsg << "\n";
	}
	infile.close();
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
