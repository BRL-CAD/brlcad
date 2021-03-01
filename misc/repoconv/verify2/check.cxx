#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <string>

int
read_map_file(std::map<std::string,std::string> &map, const char *file, int rev, std::set<std::string> *non_cad_revs)
{
    std::ifstream in_stream(file, std::ifstream::binary);
    if (in_stream.good()) {
	std::string line;
	while (std::getline(in_stream, line)) {
	    // Skip empty lines
	    if (!line.length()) {
		continue;
	    }

	    size_t spos = line.find_first_of(";");
	    if (spos == std::string::npos) {
		std::cerr << "Invalid map line!: " << line << "\n";
		exit(-1);
	    }

	    std::string id1 = line.substr(0, spos);
	    std::string id2 = line.substr(spos+1, std::string::npos);


	    if (non_cad_revs) {
		// If we have a set to check, skip anything having to do with
		// a non-cad revision
		if (non_cad_revs->find(id1) != non_cad_revs->end())
		    continue;
		if (non_cad_revs->find(id2) != non_cad_revs->end())
		    continue;
	    }


	    if (!rev) {
		if (id2.length())
		    map[id1] = id2;
	    } else {
		if (id1.length())
		    map[id2] = id1;
	    }
	}

	in_stream.close();
    }

    return 0;
}

int
read_set_file(std::set<std::string> &set, const char *file)
{
    std::ifstream in_stream(file, std::ifstream::binary);
    if (in_stream.good()) {
	std::string line;
	while (std::getline(in_stream, line)) {
	    // Skip empty lines
	    if (!line.length()) {
		continue;
	    }
	    if (line.length())
		set.insert(line);
	}
	in_stream.close();
    }

    return 0;
}

int
main(int argc, const char **argv)
{
    std::map<std::string,std::string>::iterator b_it;

    if (argc != 4) {
	std::cerr << "check svn_diffs.txt git_diffs.txt non_cad.txt\n";
	return -1;
    }

    std::set<std::string> non_cad_revs;
    read_set_file(non_cad_revs, argv[6]);

    std::map<std::string, std::string> svn_rev_to_contents;
    read_map_file(svn_rev_to_contents, argv[1], 1, &non_cad_revs);

    std::map<std::string, std::string> git_rev_to_contents;
    read_map_file(git_rev_to_contents, argv[2], 1, NULL);

    // First pass - find any revs that don't have unique commit message matches
    for (b_it = git_rev_to_contents.begin(); b_it != git_rev_to_contents.end(); b_it++) {
	std::string svn_md5 = svn_rev_to_contents[b_it->first];
	if (svn_md5 != b_it->second) {
	    std::cout << b_it->first << "\n";
	}
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
