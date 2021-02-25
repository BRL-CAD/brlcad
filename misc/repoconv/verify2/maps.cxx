#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <string>

int
read_mmap_file(std::map<std::string,std::set<std::string>> &map, const char *file, int rev, std::set<std::string> *non_cad_revs)
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

	    if (id1 == std::string("d41d8cd98f00b204e9800998ecf8427e") || id2 == std::string("d41d8cd98f00b204e9800998ecf8427e")) {
		//std::cout << "empty MD5 content hash found\n";
		continue;
	    }

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
		    map[id1].insert(id2);
	    } else {
		if (id1.length())
		map[id2].insert(id1);
	    }
	}

	in_stream.close();
    }

    return 0;
}

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

	    if (id1 == std::string("d41d8cd98f00b204e9800998ecf8427e") || id2 == std::string("d41d8cd98f00b204e9800998ecf8427e")) {
		//std::cout << "empty MD5 content hash found\n";
		continue;
	    }


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
    std::map<std::string,std::set<std::string>>::iterator s_it;

    if (argc != 7) {
	std::cerr << "maps svn_diffs.txt git_diffs.txt git_rev_map svn_logs.txt git_logs.txt non_cad.txt\n";
	return -1;
    }

    std::set<std::string> non_cad_revs;
    read_set_file(non_cad_revs, argv[6]);

    std::map<std::string,std::set<std::string>> contents_to_rev;
    std::map<std::string, std::string> rev_to_contents;
    read_mmap_file(contents_to_rev, argv[1], 0, &non_cad_revs);
    read_map_file(rev_to_contents, argv[1], 1, &non_cad_revs);

    std::map<std::string,std::set<std::string>> contents_to_sha1;
    std::map<std::string, std::string> sha1_to_contents;
    read_mmap_file(contents_to_sha1, argv[2], 0, NULL);
    read_map_file(sha1_to_contents, argv[2], 1, NULL);

    std::map<std::string,std::string> sha1_rev;
    std::map<std::string,std::string> rev_sha1;
    read_map_file(sha1_rev, argv[3], 0, NULL);
    read_map_file(rev_sha1, argv[3], 1, NULL);

    std::map<std::string, std::set<std::string>> msg_to_rev;
    std::map<std::string, std::string> rev_to_msg;
    read_mmap_file(msg_to_rev, argv[4], 0, &non_cad_revs);
    read_map_file(rev_to_msg, argv[4], 1, &non_cad_revs);

    std::map<std::string, std::set<std::string>> msg_to_sha1;
    std::map<std::string, std::string> sha1_to_msg;
    read_mmap_file(msg_to_sha1, argv[5], 0, NULL);
    read_map_file(sha1_to_msg, argv[5], 1, NULL);


    // First pass - find any revs that don't have unique commit message matches
    std::set<std::string> stage1;
    std::map<std::string, int> mcnt;
    int ucnt = 0;
    for (b_it = rev_to_msg.begin(); b_it != rev_to_msg.end(); b_it++) {
	// If the rev->msg mapping isn't unique, we can't rule anything out here.
	if (msg_to_rev[b_it->second].size() > 1) {
	    stage1.insert(b_it->first);
	    continue;
	}
	// If the rev msg doesn't have a corresponding sha1 that is unique,
	// we also can't rule anything out.  However, if the mapping exists
	// and is unique go with it.
	if (msg_to_sha1.find(b_it->second) == msg_to_sha1.end() || msg_to_sha1[b_it->second].size() > 1) {
	    stage1.insert(b_it->first);
	} else {
	    ucnt++;
	    //std::cout << "Unique mapping: " << b_it->first << " -> " << *msg_to_sha1[b_it->second].begin() << "\n";
	}
    }

    std::cout << "Unique commit message mappings found: " << ucnt << "\n";
    std::cout << "Remaining: " << stage1.size() << "\n";

    std::map<std::string, std::string> no_content_matches_with_sha1;
    std::set<std::string> no_content_matches_no_sha1;
    std::map<std::string, std::string> uniq_content_matches;
    std::set<std::string> multiple_content_matches_mapped;
    std::set<std::string> multiple_content_matches_unmapped;

    std::set<std::string>::iterator d_it;
    for (d_it = stage1.begin(); d_it != stage1.end(); d_it++) {
	std::string revc = rev_to_contents[*d_it];
	if (contents_to_sha1.find(revc) == contents_to_sha1.end()) {
	    if (rev_sha1.find(*d_it) != rev_sha1.end()) {
		no_content_matches_with_sha1[*d_it] = rev_sha1[*d_it];
	    } else {
		no_content_matches_no_sha1.insert(*d_it);
	    }
	    continue;
	}
	std::set<std::string> sha1s = contents_to_sha1[revc];
	if (sha1s.size() == 1) {
	    if (sha1_rev.find(*sha1s.begin()) == sha1_rev.end()) {
		uniq_content_matches[*d_it] = *sha1s.begin();
	    } else {
		// Validate
		std::string frev = sha1_rev[*sha1s.begin()];
		if (frev != *d_it) {
		    std::cout << "Misassigned rev! " << *d_it << "->" << *sha1s.begin() << "\n";
		}
	    }
	} else {
	    // If we're multi-matching, see if one of the matches has been assigned
	    // a revision.  If so, go with it
	    if (rev_sha1.find(*d_it) == rev_sha1.end()) {
		multiple_content_matches_unmapped.insert(*d_it);
	    } else {
		multiple_content_matches_mapped.insert(*d_it);
	    }
	}
    }
    std::ofstream mm("matched.txt");
    std::cout << "Matched unmapped total: " << uniq_content_matches.size() << "\n";
    for (b_it = uniq_content_matches.begin(); b_it != uniq_content_matches.end(); b_it++) {
	mm << b_it->second << ";" << b_it->first << "\n";
    }
    mm.close();

    std::map<std::string,std::string>::iterator w_it;
    std::ofstream wnm("unmatched_with_sha1.txt");
    std::cout << "Unmatched with sha1 total: " << no_content_matches_with_sha1.size() << "\n";
    for (w_it = no_content_matches_with_sha1.begin(); w_it != no_content_matches_with_sha1.end(); w_it++) {
	// Write out the pairings that don't report matching content for further validation
	wnm << w_it->first << ";" << w_it->second << "\n";
    }
    wnm.close();

    std::ofstream nnm("unmatched_no_sha1.txt");
    std::cout << "Unmatched no sha1 total: " << no_content_matches_no_sha1.size() << "\n";
    for (d_it = no_content_matches_no_sha1.begin(); d_it != no_content_matches_no_sha1.end(); d_it++) {
	nnm << *d_it << "\n";
    }
    nnm.close();

    std::ofstream mmm("multiple_matched.txt");
    std::cout << "Multiple content matches unmapped total: " << multiple_content_matches_unmapped.size() << "\n";
    for (d_it = multiple_content_matches_unmapped.begin(); d_it != multiple_content_matches_unmapped.end(); d_it++) {
	mmm << *d_it << " -> ";
	std::string revc = rev_to_contents[*d_it];
	std::set<std::string> sha1s = contents_to_sha1[revc];
	std::set<std::string>::iterator c_it;
	for (c_it = sha1s.begin(); c_it != sha1s.end(); c_it++) {
	    mmm << *c_it << " ";
	}
	mmm << "\n";
    }
    mmm.close();
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
