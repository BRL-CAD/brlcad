/*                    R E P O W O R K . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file repowork.cpp
 *
 * Utility functions and main processing loop
 *
 */

#include <filesystem>
#include <iostream>
#include <sstream>
#include <locale>

#include "cxxopts.hpp"
#include "repowork.h"


// https://stackoverflow.com/a/5607650
struct schars: std::ctype<char> {
    schars(): std::ctype<char>(get_table()) {}
    static std::ctype_base::mask const* get_table() {
        static const std::ctype<char>::mask *const_table= std::ctype<char>::classic_table();
        static std::ctype<char>::mask cmask[std::ctype<char>::table_size];
        std::memcpy(cmask, const_table, std::ctype<char>::table_size * sizeof(std::ctype<char>::mask));
        cmask[';'] = std::ctype_base::space;
        return &cmask[0];
    }
};


int
git_parse_commitish(git_commitish &gc, git_fi_data *s, std::string line)
{
       if (line.c_str()[0] == ':') {
        // If we start with a colon, we have a mark - translate it and zero
        // from_str.
        line.erase(0, 1); // Remove ":" prefix
	long omark = std::stol(line);
        gc.mark = s->mark_old_to_new[omark];
	if (s->mark_to_index.find(gc.mark) != s->mark_to_index.end()) {
	    gc.index = s->mark_to_index[gc.mark];
	    //std::cout << "Mark id :" << line << " -> " << gc.index << "\n";
	} else {
	    std::cerr << "Mark with no index:" << gc.mark << "\n";
	    exit(EXIT_FAILURE);
	}
        return 0;
    }
    if (!ficmp(line, std::string("refs/heads/"))) {
        gc.ref = std::stol(line);
        return 0;
    }
    if (line.length() == 40) {
        // Probably have a SHA1
        gc.sha1 = line;
        gc.index = s->mark_to_index[s->sha1_to_mark[gc.sha1]];
        //std::cout << "SHA1 id :" << gc.sha1 << " -> " << gc.mark << " -> " << gc.index << "\n";
        return 0;
    }

    return -1;
}

int
git_unpack_notes(git_fi_data *s, std::ifstream &infile, std::string &repo_path)
{
    // Iterate over the commits looking for note commits.  If we find one,
    // find its associated blob with data, read it, find the associated
    // commit, and stash it in a string in that container.
    for (size_t i = 0; i < s->commits.size(); i++) {
	if (s->commits[i].notes_commit) {
	    continue;
	}

	if (!s->have_sha1s) {
	    std::cerr << "Fatal - notes unpacking requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	    exit(1);
	}

	if (!s->commits[i].id.sha1.length()) {
	    std::cerr << "Warning - commit " << s->commits[i].id.mark << " has no sha1 info, skipping notes lookup\n";
	    continue;
	}

	// This is cheap and clunky, but I've not yet found a document
	// describing how to reliably unpack git notes...
	std::string git_notes_cmd = std::string("cd ") + repo_path + std::string(" && git log -1 ") + s->commits[i].id.sha1 + std::string(" --pretty=format:\"%N\" > ../sha1.txt && cd ..");
        if (std::system(git_notes_cmd.c_str())) {
            std::cout << "git_sha1_cmd failed\n";
	    exit(-1);
        }

	std::ifstream n("sha1.txt");
	if (!n.good()) {
	    std::cout << "sha1.txt read failed\n";
	    exit(-1);
	}
	std::string note((std::istreambuf_iterator<char>(n)), std::istreambuf_iterator<char>());

	// Replace git style branch name for master with SVN style name
	std::regex mstr_regex("svn:branch:master");
	std::string note_fixed = std::regex_replace(note, mstr_regex, "svn:branch:trunk");

	// Write the message to the commit's string;
	s->commits[i].notes_string = note_fixed;

	// SPECIAL PURPOSE CODE - should go away eventually.
	// For BRL-CAD specifically, this information contains
	// the SVN id associated with the commit.  We want to
	// use this info, so parse it out and store it.
	std::regex svnid("svn:revision:([0-9]+).*");
	std::smatch svnidvar;
	if (std::regex_search(note, svnidvar, svnid)) {
	    s->commits[i].svn_id = std::string(svnidvar[1]);
	    std::cout << "Identified revision " << s->commits[i].svn_id << "\n";

	    // Store the id->sha1 relationship for potential later use
	    if (s->commits[i].id.sha1.length()) {
		s->rev_to_sha1[s->commits[i].svn_id] = s->commits[i].id.sha1;
	    }

	    // We wrote the wrong SVN branch name for older dmtogl branches -
	    // names were deliberately collapsed in git conversion, but we
	    // should reflect the original SVN history in the metadata.  Undo
	    // the mapping for the label based on revision number.
	    long revnum = std::stol(s->commits[i].svn_id);
	    if (revnum < 36472) {
		std::regex bstr_regex("svn:branch:dmtogl");
		std::smatch bmatch;
		if (std::regex_search(s->commits[i].notes_string, bmatch, bstr_regex)) {
		    std::string branch_fixed = std::regex_replace(s->commits[i].notes_string, bstr_regex, "svn:branch:dmtogl-branch");
		    s->commits[i].notes_string = branch_fixed;
		}
	    }
	}


	n.close();
    }

    return 0;
}

// This is the first step of the note correction process - run it first
int
git_update_svn_revs(git_fi_data *s, std::string &svn_rev_map)
{
    if (!s->have_sha1s) {
	std::cerr << "Fatal - sha1 SVN note updating requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	exit(1);
    }
    // read maps
    std::ifstream infile_revs(svn_rev_map, std::ifstream::binary);
    if (svn_rev_map.length() && !infile_revs.good()) {
	std::cerr << "Could not open svn_rev_map file: " << svn_rev_map << "\n";
	exit(-1);
    }
    std::map<std::string, int> rmap;
    if (infile_revs.good()) {
	std::string line;
	while (std::getline(infile_revs, line)) {
	    // Skip empty lines
	    if (!line.length()) {
		continue;
	    }

	    size_t spos = line.find_first_of(";");
	    if (spos == std::string::npos) {
		std::cerr << "Invalid sha1;rev map line!: " << line << "\n";
		exit(-1);
	    }

	    std::string id1 = line.substr(0, spos);
	    std::string id2 = line.substr(spos+1, std::string::npos);
	    int rev = (id2.length()) ? std::stoi(id2) : -1;

	    std::cout << "sha1: \"" << id1 << "\" -> rev: \"" << rev << "\n";
	    rmap[id1] = rev;
	}

	infile_revs.close();
    }

    for (size_t i = 0; i < s->commits.size(); i++) {

	if (!s->commits[i].id.sha1.length()) {
	    continue;
	}
	if (rmap.find(s->commits[i].id.sha1) == rmap.end()) {
	    continue;
	}

	long nrev =  rmap[s->commits[i].id.sha1];
	s->commits[i].svn_id = nrev;

	if (nrev > 0) {
	    std::cout << "Assigning new SVN rev " << nrev << " to " << s->commits[i].id.sha1 << "\n";

	    // Note:  this isn't guaranteed to be unique...  setting it mostly for
	    // the cases where it is.
	    s->rev_to_sha1[s->commits[i].svn_id] = s->commits[i].id.sha1;
	}

	// Update the message
	std::stringstream ss(s->commits[i].commit_msg);
	std::string nmsg, cline;
	bool srev = false;

	while (std::getline(ss, cline, '\n')) {
	    std::regex svnline("^svn:.*");
	    bool smatch = std::regex_match(cline, svnline);
	    std::regex cvsline("^cvs:.*");
	    bool cmatch = std::regex_match(cline, cvsline);
	    std::regex svnrevline("^svn:revision:([0-9]+).*");
	    bool srmatch = std::regex_match(cline, svnrevline);

	    // If it's not a CVS or SVN line, just append and continue
	    if (!smatch && !cmatch) {
		nmsg.append(cline);
		nmsg.append("\n");
		continue;
	    }

	    if (srev && srmatch) {
		// Already handled the revision update per srev - if cline is
		// an old revision line, skip it.
		continue;
	    }
	    if (srev) {
		// Already done - just add back on whatever else is there.
		nmsg.append(cline);
		nmsg.append("\n");
		continue;
	    }

	    if (smatch) {
		// The SVN revision line comes first in the svn: set -
		// if we have found such a line, we're either replacing
		// the existing one, inserting a new line before the
		// current non-revision svn line, or skipping if the
		// nrev value is -1.
		if (nrev > 0) {
		    std::string nrevline = std::string("svn:revision:") + std::to_string(nrev);
		    nmsg.append(nrevline);
		    nmsg.append("\n");
		}
		// Any further svn:revision lines will be skipped - for
		// now at least, one to a commit.
		srev = true;

		// If SVN cline was the old rev line, we're done -
		// continue on.  Otherwise, there may be more
		// processing to do with another type of svn: line.
		if (srmatch)
		    continue;
	    }

	    if (cmatch) {
		// If we've gotten down to a cvs: line without handling
		// srev, insert the revision line
		std::string nrevline = std::string("svn:revision:") + std::to_string(nrev);
		nmsg.append(nrevline);
		nmsg.append("\n");
		srev = true;
	    }

	    // Anything else, just write and continue
	    nmsg.append(cline);
	    nmsg.append("\n");
	}

	s->commits[i].commit_msg = nmsg;

    }

    return 0;
}

// Pull information on SVN revisions (if any) out of commit notes
int
git_sync_svn_revs(git_fi_data *s)
{
    for (size_t i = 0; i < s->commits.size(); i++) {
	std::stringstream ss(s->commits[i].commit_msg);
	std::string cline;
	std::string rev = std::string("-1");
	while (std::getline(ss, cline, '\n')) {
	    std::regex svnrevline("^svn:revision:([0-9]+).*");
	    bool srmatch = std::regex_match(cline, svnrevline);
	    if (srmatch) {
		rev = cline.substr(13, std::string::npos);
		break;
	    }
	}
	s->commits[i].svn_id = rev;
    }

    return 0;
}


// This function is intended to handle map files with either sha1 or svn rev keys,
// and multiple branches mapping to one key either on multiple lines or by semicolon
// separated lists.
//
// If update_mode == 0, clear branch lines from the message when we don't have map
// information.  If update_mode == 1, leave intact anything not explicitly in the map.
int
git_assign_branch_labels(git_fi_data *s, std::string &svn_branch_map, int update_mode)
{
    int key_type = -1;

    if (!s->have_sha1s) {
	std::cerr << "Fatal - sha1 SVN note updating requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	exit(1);
    }

    std::ifstream infile_branches(svn_branch_map, std::ifstream::binary);
    if (svn_branch_map.length() && !infile_branches.good()) {
	std::cerr << "Could not open svn_branch_map file: " << svn_branch_map << "\n";
	exit(-1);
    }
    std::map<std::string, std::set<std::string>> bmap;
    if (infile_branches.good()) {
	std::string line;
	while (std::getline(infile_branches, line)) {
	    // Skip empty lines
	    if (!line.length()) {
		continue;
	    }

	    size_t spos = line.find_first_of(":");
	    if (spos == std::string::npos) {
		std::cerr << "Invalid sha1;branch map line!: " << line << "\n";
		exit(-1);
	    }

	    std::string id1 = line.substr(0, spos);
	    std::string id2 = line.substr(spos+1, std::string::npos);

	    if (key_type < 0) {
		key_type = (id1.length() == 40) ? 1 : 2;
	    }

	    std::cout << "key: \"" << id1 << "\" -> branch: \"" << id2 << "\n";

	    // Split into a vector, since there may be more than one branch
	    std::stringstream ss(id2);
	    std::ostringstream oss;
	    ss.imbue(std::locale(std::locale(), new schars()));
	    std::istream_iterator<std::string> b_begin(ss);
	    std::istream_iterator<std::string> b_end;
	    std::vector<std::string> branches_array(b_begin, b_end);
	    std::copy(branches_array.begin(), branches_array.end(), std::ostream_iterator<std::string>(oss, "\n"));
	    for (size_t i = 0; i < branches_array.size(); i++) {
		bmap[id1].insert(branches_array[i]);
	    }
	}

	infile_branches.close();
    }

    // Iterate over the commits looking for relevant commits, and update msg.
    for (size_t i = 0; i < s->commits.size(); i++) {
	if (update_mode == 0 && s->commits[i].svn_id.length()) {
	    // If we're in overwrite mode, don't go beyond the CVS era commits - 
	    // SVN era commits were assigned branches in the original process,
	    // and any alterations to them should be corrections.  The CVS era
	    // assignments were unreliable, and so should be removed.
	    long int rev = std::stol(s->commits[i].svn_id);
	    if (rev > 29886) {
		continue;
	    }
	}

	std::set<std::string> sbranches;
	if (key_type == 1) {
	    if (update_mode == 1 && !s->commits[i].id.sha1.length())
		continue;
	    if (update_mode == 1 && bmap.find(s->commits[i].id.sha1) == bmap.end())
		continue;
	    sbranches = bmap[s->commits[i].id.sha1];
	}

	if (key_type == 2) {
	    if (update_mode == 1 && !s->commits[i].svn_id.length())
		continue;
	    if (update_mode == 1 && bmap.find(s->commits[i].svn_id) == bmap.end())
		continue;
	    sbranches = bmap[s->commits[i].svn_id];
	}

	// Update the message
	std::stringstream ss(s->commits[i].commit_msg);
	std::string nmsg, cline;
	std::set<std::string>::iterator b_it;
	bool wbranch = false;

	while (std::getline(ss, cline, '\n')) {
	    std::regex svnbranchline("^svn:branch:.*");
	    bool sbmatch = std::regex_match(cline, svnbranchline);
	    std::regex cvsbranchline("^cvs:branch:.*");
	    bool cbmatch = std::regex_match(cline, cvsbranchline);

	    // If it's not a CVS or SVN branch line, just append and continue
	    if (!sbmatch && !cbmatch) {
		nmsg.append(cline);
		nmsg.append("\n");
		continue;
	    }

	    // If it's a branch label match and we've already written the branches, skip
	    if ((sbmatch || cbmatch) && wbranch) {
		continue;
	    }

	    // If it's a match and we've not yet written out branches, do so
	    if (sbmatch || cbmatch) {
		if (!sbranches.size()) {
		    std::cout << "Note: clearing branches on commit " << s->commits[i].id.sha1 << "\n";
		}
		for (b_it = sbranches.begin(); b_it != sbranches.end(); b_it++) {
		    if (sbmatch) {
			std::string nbranchline = std::string("svn:branch:") + *b_it;
			nmsg.append(nbranchline);
		    }
		    if (cbmatch) {
			std::string nbranchline = std::string("cvs:branch:") + *b_it;
			nmsg.append(nbranchline);
		    }
		    nmsg.append("\n");
		}
		wbranch = true;
		continue;
	    }

	    // Anything else, just write and continue
	    nmsg.append(cline);
	    nmsg.append("\n");
	}

	if (!wbranch) {
	    // If we're adding branch labels where they didn't exist, they're
	    // appended to the end
	    bool sbmatch = false;
	    bool cbmatch = false;
	    if (s->commits[i].svn_id.length()) {
		long int rev = std::stol(s->commits[i].svn_id);
		if (rev <= 29886) {
		    cbmatch = true;
		} else {
		    sbmatch = true;
		}
	    } else {
		cbmatch = true;
	    }
	    for (b_it = sbranches.begin(); b_it != sbranches.end(); b_it++) {
		if (sbmatch) {
		    std::string nbranchline = std::string("svn:branch:") + *b_it;
		    nmsg.append(nbranchline);
		}
		if (cbmatch) {
		    std::string nbranchline = std::string("cvs:branch:") + *b_it;
		    nmsg.append(nbranchline);
		}
		nmsg.append("\n");
	    }

	}

	s->commits[i].commit_msg = nmsg;

    }

    return 0;
}

int
git_set_tag_labels(git_fi_data *s, std::string &tag_list)
{
    if (!s->have_sha1s) {
	std::cerr << "Fatal - sha1 SVN note updating requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	exit(1);
    }

    std::ifstream infile_tag_list(tag_list, std::ifstream::binary);
    if (tag_list.length() && !infile_tag_list.good()) {
	std::cerr << "Could not open tag_list file: " << tag_list << "\n";
	exit(-1);
    }
    std::set<std::string> tag_sha1s;
    if (infile_tag_list.good()) {
	std::string line;
	while (std::getline(infile_tag_list, line)) {
	    // Skip anything the wrong length
	    if (line.length() != 40) {
		continue;
	    }
	    tag_sha1s.insert(line);
	    std::cout << "tag sha1: " << line << "\n";
	    bool valid = false;
	    for (size_t i = 0; i < s->commits.size(); i++) {
		if (s->commits[i].id.sha1 == line) {
		    valid = true;
		    break;
		}
	    }
	    if (!valid) {
		std::cout << "INVALID sha1 supplied for tag!\n";
	    }
	}

	infile_tag_list.close();
    }

    // Iterate over the commits
    for (size_t i = 0; i < s->commits.size(); i++) {

	if (!s->commits[i].id.sha1.length()) {
	    continue;
	}
	if (tag_sha1s.find(s->commits[i].id.sha1) == tag_sha1s.end()) {
	    continue;
	}

	// Update the message
	std::stringstream ss(s->commits[i].commit_msg);
	std::string nmsg, cline;

	while (std::getline(ss, cline, '\n')) {
	    std::regex svnbranchline("^svn:branch:.*");
	    bool sbmatch = std::regex_match(cline, svnbranchline);
	    if (sbmatch) {
		std::string tagname = cline.substr(11, std::string::npos);
		std::cout << "branch->tag " << s->commits[i].id.sha1 << "," << tagname << "\n";
		std::string nbranchline = std::string("svn:tag:") + tagname;
		nmsg.append(nbranchline);
		nmsg.append("\n");
		continue;
	    }
	    // Anything else, just write and continue
	    nmsg.append(cline);
	    nmsg.append("\n");
	}

	s->commits[i].commit_msg = nmsg;

    }

    return 0;
}

int
git_remove_commits(git_fi_data *s, std::string &remove_commits)
{
    if (!s->have_sha1s) {
	std::cerr << "Fatal - commit removal requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	exit(1);
    }

    std::ifstream infile_remove_commits(remove_commits, std::ifstream::binary);
    if (remove_commits.length() && !infile_remove_commits.good()) {
	std::cerr << "Could not open remove_commits file: " << remove_commits << "\n";
	exit(-1);
    }
    std::set<std::string> remove_sha1s;
    if (infile_remove_commits.good()) {
	std::string line;
	while (std::getline(infile_remove_commits, line)) {
	    // Skip anything the wrong length
	    if (line.length() != 40) {
		continue;
	    }
	    remove_sha1s.insert(line);
	    std::cout << "remove sha1: " << line << "\n";
	    bool valid = false;
	    for (size_t i = 0; i < s->commits.size(); i++) {
		if (s->commits[i].id.sha1 == line) {
		    valid = true;
		    break;
		}
	    }
	    if (!valid) {
		std::cout << "INVALID sha1 supplied for removal!\n";
	    }
	}

	infile_remove_commits.close();
    }


    std::set<std::string>::iterator r_it;
    for (r_it = remove_sha1s.begin(); r_it != remove_sha1s.end(); r_it++) {
	git_commitish rfrom;
	git_commitish rish;
	for (size_t i = 0; i < s->commits.size(); i++) {
	    if (s->commits[i].id.sha1 == *r_it) {
		rfrom = s->commits[i].from;
		rish = s->commits[i].id;
		s->commits[i].skip_commit = true;
		break;
	    }
	}
	// Update any references
	for (size_t i = 0; i < s->commits.size(); i++) {
	    if (s->commits[i].from == rish) {
		std::cout << *r_it << " removal: updating from commit for " << s->commits[i].id.sha1 << "\n";
		s->commits[i].from = rfrom;
	    }
	    for (size_t j = 0; j < s->commits[i].merges.size(); j++) {
		if (s->commits[i].merges[j] == rish) {
		    std::cout << *r_it << " removal: updating merge commit for " << s->commits[i].id.sha1 << "\n";
		    s->commits[i].merges[j] = rfrom;
		}
	    }
	}
    }

    return 0;
}

int
git_map_emails(git_fi_data *s, std::string &email_map)
{
    // read map
    std::ifstream infile(email_map, std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open email_map file: " << email_map << "\n";
	exit(-1);
    }

    std::map<std::string, std::string> email_id_map;

    std::string line;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    continue;
	}

	size_t spos = line.find_first_of(";");
	if (spos == std::string::npos) {
	    std::cerr << "Invalid email map line!: " << line << "\n";
	    exit(-1);
	}

	std::string id1 = line.substr(0, spos);
	std::string id2 = line.substr(spos+1, std::string::npos);

	std::cout << "id1: \"" << id1 << "\"\n";
	std::cout << "id2: \"" << id2 << "\"\n";
	email_id_map[id1] = id2;
    }

    // Iterate over the commits looking for note commits.  If we find one,
    // find its associated blob with data, read it, find the associated
    // commit, and stash it in a string in that container.
    for (size_t i = 0; i < s->commits.size(); i++) {
	git_commit_data *c = &(s->commits[i]);
	if (email_id_map.find(c->author) != email_id_map.end()) {
	    std::string nauthor = email_id_map[c->author];
	    c->author = nauthor;
	}
	if (email_id_map.find(c->committer) != email_id_map.end()) {
	    std::string ncommitter = email_id_map[c->committer];
	    //std::cerr << "Replaced committer \"" << c->committer << "\" with \"" << ncommitter << "\"\n";
	    c->committer = ncommitter;
	}
    }

    return 0;
}

int
git_map_svn_committers(git_fi_data *s, std::string &svn_map)
{
    // read map
    std::ifstream infile(svn_map, std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open svn_map file: " << svn_map << "\n";
	exit(-1);
    }

    // Create mapping of ids to svn committers 
    std::map<std::string, std::string> svn_committer_map;
    std::string line;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    continue;
	}

	size_t spos = line.find_first_of(" ");
	if (spos == std::string::npos) {
	    std::cerr << "Invalid svn map line!: " << line << "\n";
	    exit(-1);
	}

	std::string id = line.substr(0, spos);
	std::string committer = line.substr(spos+1, std::string::npos);

	svn_committer_map[id] = committer;
    }

    // Iterate over the commits and assign committers.
    for (size_t i = 0; i < s->commits.size(); i++) {
	git_commit_data *c = &(s->commits[i]);
	if (!c->svn_id.length()) {
	    continue;
	}
	if (svn_committer_map.find(c->svn_id) != svn_committer_map.end()) {
	    std::string svncommitter = svn_committer_map[c->svn_id];
	    //std::cerr << "Found SVN commit \"" << c->svn_id << "\" with committer \"" << svncommitter << "\"\n";
	    c->svn_committer = svncommitter;
	}
    }

    return 0;
}

void
read_key_sha1_map(git_fi_data *s, std::string &keysha1file)
{
    std::ifstream infile(keysha1file, std::ifstream::binary);
    if (!infile.good()) {
        std::cerr << "Could not open file: " << keysha1file << "\n";
        exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
        size_t cpos = line.find_first_of(":");
        std::string key = line.substr(0, cpos);
        std::string sha1 = line.substr(cpos+1, std::string::npos);
	s->sha12key[sha1] = key;
	s->key2sha1[key] = sha1;
    }
    infile.close();
}

void
read_key_cvsbranch_map(
	git_fi_data *s,
        std::string &branchfile)
{
    std::map<std::string, std::string> key2branch;
    std::ifstream infile(branchfile, std::ifstream::binary);
    if (!infile.good()) {
        std::cerr << "Could not open file: " << branchfile << "\n";
        exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
        size_t cpos = line.find_first_of(":");
        std::string key = line.substr(0, cpos);
        std::string branch = line.substr(cpos+1, std::string::npos);
        if (key2branch.find(key) != key2branch.end()) {
            std::string oldbranch = key2branch[key];
            if (oldbranch != branch) {
                std::cout << "WARNING: non-unique key maps to both branch " << oldbranch << " and branch "  << branch << ", overriding\n";
            }
        }
	if (s->key2sha1.find(key) != s->key2sha1.end()) {
	    s->key2cvsbranch[key] = branch;
	}
    }
    infile.close();
}

void
read_key_cvsauthor_map(	git_fi_data *s, std::string &authorfile)
{
    std::map<std::string, std::string> key2author;
    std::ifstream infile(authorfile, std::ifstream::binary);
    if (!infile.good()) {
        std::cerr << "Could not open file: " << authorfile << "\n";
        exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
        size_t cpos = line.find_first_of(":");
        std::string key = line.substr(0, cpos);
        std::string author = line.substr(cpos+1, std::string::npos);
        if (key2author.find(key) != key2author.end()) {
            std::string oldauthor = key2author[key];
            if (oldauthor != author) {
                std::cout << "WARNING: non-unique key maps to both author " << oldauthor << " and author "  << author << ", overriding\n";
            }
        }
	if (s->key2sha1.find(key) != s->key2sha1.end()) {
	    s->key2cvsauthor[key] = author;
	}
    }
    infile.close();
}

void
process_ls_tree(std::string &sha1)
{
    // read children
    std::ifstream tfile("tree.txt", std::ifstream::binary);
    if (!tfile.good()) {
	std::cerr << "Could not open tree file tree.txt\n";
	exit(-1);
    }
    std::string sha1tree = std::string("trees/") + sha1 + std::string("-tree.fi");
    std::ofstream ofile(sha1tree, std::ios::out | std::ios::binary);
    ofile << "deleteall\n";

    std::string tline;
    while (std::getline(tfile, tline)) {
	std::string ltree = tline;
	std::regex bregex(" blob ");
	std::string ltree2 = std::regex_replace(ltree, bregex, " ");
	std::regex sregex("^");
	ltree = std::regex_replace(ltree2, sregex , "M ");
	std::regex tregex("\t");
	ltree2 = std::regex_replace(ltree, tregex , " \"");
	ofile << ltree2 << "\"\n";
    }

    ofile.close();

    std::remove("tree.txt");
}

int
git_id_rebuild_commits(git_fi_data *s, std::string &id_file, std::string &repo_path, std::string &child_commits_file)
{
    {
	// read children
	std::ifstream cfile(child_commits_file, std::ifstream::binary);
	if (!cfile.good()) {
	    std::cerr << "Could not open child_commits_file file: " << child_commits_file << "\n";
	    exit(-1);
	}

	std::string rline;
	while (std::getline(cfile, rline)) {
	    // Skip empty lines
	    if (!rline.length()) {
		continue;
	    }

	    // First 40 characters are the key
	    std::string key = rline.substr(0, 40);
	    rline.erase(0,41); // Remove key and space
	    std::set<std::string> vals;
	    while (rline.length() >= 40) {
		std::string val = rline.substr(0, 40);
		vals.insert(val);
		rline.erase(0,41);
	    }
	    if (vals.size()) {
		s->children[key] = vals;
	    }
	}
    }

    {
	// read ids
	std::ifstream infile(id_file, std::ifstream::binary);
	if (!infile.good()) {
	    std::cerr << "Could not open id_file file: " << id_file << "\n";
	    exit(-1);
	}

	std::string line;
	while (std::getline(infile, line)) {
	    // Skip empty lines
	    if (!line.length()) {
		continue;
	    }

	    std::string sha1;
	    if (line.length() < 40) {
		// Given an svn revision - translate it to a sha1
		if (s->rev_to_sha1.find(line) == s->rev_to_sha1.end()) {
		    std::cerr << "SVN revision " << line << " could not be mapped to SHA1.  May need to re-export fast import file with --show-original-ids.\n";
		    exit(1);
		}
		sha1 = s->rev_to_sha1[line];
	    } else {
		sha1 = line;
	    }

	    s->rebuild_commits.insert(sha1);
	    std::cout << "rebuild commit: " << line << " -> " << sha1 << "\n";
	}
    }

    // Children of the rebuilt commits will need to fully define their
    // contents, unless they are also being rebuilt (in which case their
    // children will need to reset themselves.)
    std::set<std::string> rbc = s->rebuild_commits;
    while (rbc.size()) {
	std::string rb = *rbc.begin();
	rbc.erase(rb);
	std::cout << "Finding reset commit(s) for: " << rb << "\n";
	if (s->children.find(rb) == s->children.end()) {
	    // No child commits - no further work needed.
	    std::cout << "Leaf commit: " << rb << "\n";
	    continue;
	}
	std::set<std::string>::iterator c_it;
	std::set<std::string> rc = s->children[rb];
	while (rc.size()) {
	    std::string rcs = *rc.begin();
	    rc.erase(rcs);
	    if (s->rebuild_commits.find(rcs) == s->rebuild_commits.end()) {
		std::cout << "found reset commit: " << rcs << "\n";
		s->reset_commits.insert(rcs);
	    } else {
		if (s->children.find(rcs) != s->children.end()) {
		    rc.insert(s->children[rcs].begin(), s->children[rcs].end());
		}
	    }
	}
    }

    // Now that we know what the reset commits are, generate the trees that will
    // achieve this.
    std::set<std::string>::iterator s_it;
    for (s_it = s->reset_commits.begin(); s_it != s->reset_commits.end(); s_it++) {
	std::string sha1 = *s_it;
	std::string git_ls_tree_cmd = std::string("cd ") + repo_path + std::string(" && git ls-tree --full-tree -r ") + sha1 + std::string(" > ../tree.txt && cd ..");
	if (std::system(git_ls_tree_cmd.c_str())) {
	    std::cout << "git_ls_tree_cmd \"" << git_ls_tree_cmd << "\" failed\n";
	    exit(-1);
	}
	process_ls_tree(sha1);
    }

    return 0;
}


typedef int (*gitcmd_t)(git_fi_data *, std::ifstream &);

gitcmd_t
gitit_find_cmd(std::string &line, std::map<std::string, gitcmd_t> &cmdmap)
{
    gitcmd_t gc = NULL;
    std::map<std::string, gitcmd_t>::iterator c_it;
    for (c_it = cmdmap.begin(); c_it != cmdmap.end(); c_it++) {
	if (!ficmp(line, c_it->first)) {
	    gc = c_it->second;
	    break;
	}
    }
    return gc;
}

int
parse_fi_file(git_fi_data *fi_data, std::ifstream &infile)
{
    std::map<std::string, gitcmd_t> cmdmap;
    cmdmap[std::string("alias")] = parse_alias;
    cmdmap[std::string("blob")] = parse_blob;
    cmdmap[std::string("cat-blob")] = parse_cat_blob;
    cmdmap[std::string("checkpoint")] = parse_checkpoint;
    cmdmap[std::string("commit ")] = parse_commit;
    cmdmap[std::string("done")] = parse_done;
    cmdmap[std::string("feature")] = parse_feature;
    cmdmap[std::string("get-mark")] = parse_get_mark;
    cmdmap[std::string("ls")] = parse_ls;
    cmdmap[std::string("option")] = parse_option;
    cmdmap[std::string("progress")] = parse_progress;
    cmdmap[std::string("reset")] = parse_reset;
    cmdmap[std::string("tag")] = parse_tag;

    size_t offset = infile.tellg();
    std::string line;
    std::map<std::string, gitcmd_t>::iterator c_it;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    offset = infile.tellg();
	    continue;
	}

	gitcmd_t gc = gitit_find_cmd(line, cmdmap);
	if (!gc) {
	    //std::cerr << "Unsupported command!\n";
	    offset = infile.tellg();
	    continue;
	}

	// If we found a command, process it
	//std::cout << "line: " << line << "\n";
	// some commands have data on the command line - reset seek so the
	// callback can process it
	infile.seekg(offset);
	(*gc)(fi_data, infile);
	offset = infile.tellg();
    }


    return 0;
}

int
parse_splice_fi_file(git_fi_data *fi_data, std::ifstream &infile)
{
    std::map<std::string, gitcmd_t> cmdmap;
    cmdmap[std::string("alias")] = parse_alias;
    cmdmap[std::string("blob")] = parse_blob;
    cmdmap[std::string("cat-blob")] = parse_cat_blob;
    cmdmap[std::string("checkpoint")] = parse_checkpoint;
    cmdmap[std::string("commit ")] = parse_splice_commit;
    cmdmap[std::string("done")] = parse_done;
    cmdmap[std::string("feature")] = parse_feature;
    cmdmap[std::string("get-mark")] = parse_get_mark;
    cmdmap[std::string("ls")] = parse_ls;
    cmdmap[std::string("option")] = parse_option;
    cmdmap[std::string("progress")] = parse_progress;
    cmdmap[std::string("reset")] = parse_reset;
    cmdmap[std::string("tag")] = parse_tag;

    size_t offset = infile.tellg();
    std::string line;
    std::map<std::string, gitcmd_t>::iterator c_it;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    offset = infile.tellg();
	    continue;
	}

	gitcmd_t gc = gitit_find_cmd(line, cmdmap);
	if (!gc) {
	    //std::cerr << "Unsupported command!\n";
	    offset = infile.tellg();
	    continue;
	}

	// If we found a command, process it
	//std::cout << "line: " << line << "\n";
	// some commands have data on the command line - reset seek so the
	// callback can process it
	infile.seekg(offset);
	(*gc)(fi_data, infile);
	offset = infile.tellg();
    }


    return 0;
}

int
parse_replace_fi_file(git_fi_data *fi_data, std::ifstream &infile)
{
    std::map<std::string, gitcmd_t> cmdmap;
    cmdmap[std::string("alias")] = parse_alias;
    cmdmap[std::string("blob")] = parse_blob;
    cmdmap[std::string("cat-blob")] = parse_cat_blob;
    cmdmap[std::string("checkpoint")] = parse_checkpoint;
    cmdmap[std::string("commit ")] = parse_replace_commit;
    cmdmap[std::string("done")] = parse_done;
    cmdmap[std::string("feature")] = parse_feature;
    cmdmap[std::string("get-mark")] = parse_get_mark;
    cmdmap[std::string("ls")] = parse_ls;
    cmdmap[std::string("option")] = parse_option;
    cmdmap[std::string("progress")] = parse_progress;
    cmdmap[std::string("reset")] = parse_reset;
    cmdmap[std::string("tag")] = parse_tag;

    size_t offset = infile.tellg();
    std::string line;
    std::map<std::string, gitcmd_t>::iterator c_it;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    offset = infile.tellg();
	    continue;
	}

	gitcmd_t gc = gitit_find_cmd(line, cmdmap);
	if (!gc) {
	    //std::cerr << "Unsupported command!\n";
	    offset = infile.tellg();
	    continue;
	}

	// If we found a command, process it
	//std::cout << "line: " << line << "\n";
	// some commands have data on the command line - reset seek so the
	// callback can process it
	infile.seekg(offset);
	(*gc)(fi_data, infile);
	offset = infile.tellg();
    }

    return 0;
}

int
parse_add_fi_file(git_fi_data *fi_data, std::ifstream &infile)
{
    std::map<std::string, gitcmd_t> cmdmap;
    cmdmap[std::string("alias")] = parse_alias;
    cmdmap[std::string("blob")] = parse_blob;
    cmdmap[std::string("cat-blob")] = parse_cat_blob;
    cmdmap[std::string("checkpoint")] = parse_checkpoint;
    cmdmap[std::string("commit ")] = parse_add_commit;
    cmdmap[std::string("done")] = parse_done;
    cmdmap[std::string("feature")] = parse_feature;
    cmdmap[std::string("get-mark")] = parse_get_mark;
    cmdmap[std::string("ls")] = parse_ls;
    cmdmap[std::string("option")] = parse_option;
    cmdmap[std::string("progress")] = parse_progress;
    cmdmap[std::string("reset")] = parse_reset;
    cmdmap[std::string("tag")] = parse_tag;

    size_t offset = infile.tellg();
    std::string line;
    std::map<std::string, gitcmd_t>::iterator c_it;
    while (std::getline(infile, line)) {
	// Skip empty lines
	if (!line.length()) {
	    offset = infile.tellg();
	    continue;
	}

	gitcmd_t gc = gitit_find_cmd(line, cmdmap);
	if (!gc) {
	    //std::cerr << "Unsupported command!\n";
	    offset = infile.tellg();
	    continue;
	}

	// If we found a command, process it
	//std::cout << "line: " << line << "\n";
	// some commands have data on the command line - reset seek so the
	// callback can process it
	infile.seekg(offset);
	(*gc)(fi_data, infile);
	offset = infile.tellg();
    }


    return 0;
}



int
main(int argc, char *argv[])
{
    git_fi_data fi_data;
    bool splice_commits = false;
    bool replace_commits = false;
    bool add_commits = false;
    bool no_blobs = false;
    bool collapse_notes = false;
    bool wrap_commit_lines = false;
    bool trim_whitespace = false;
    bool list_empty = false;
    std::string repo_path;
    std::string email_map;
    std::string svn_map;
    std::string svn_rev_map;
    std::string svn_branch_map;
    std::string svn_branches_to_tags;
    std::string remove_commits;
    std::string correct_branches;
    std::string cvs_auth_map;
    std::string cvs_branch_map;
    std::string keymap;
    std::string children_file;
    std::string id_file;
    int cwidth = 72;

    // TODO - might be good do have a "validate" option that does the fast import and then
    // checks every commit saved from the old repo in the new one...
    try
    {
	cxxopts::Options options(argv[0], " - process git fast-import files");

	options.add_options()
	    ("e,email-map", "Specify replacement username+email mappings (one map per line, format is commit-id-1;commit-id-2)", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("s,svn-map", "Specify svn rev -> committer map (one mapping per line, format is commit-rev name)", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("svn-revs", "Specify git sha1 -> svn rev map (one mapping per line, format is sha1;[commit-rev])", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("svn-branches", "Specify [git sha1|rev] -> svn branch (one mapping per line, format is key:[branch;branch])", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("svn-branches-to-tags", "Specify git sha1 list that was committed to tags, not branches", cxxopts::value<std::vector<std::string>>(), "sha1 list")
	    ("correct-branches", "Specify rev -> branch sets (key;[branch;branch].  Will override svn-branches assignments.)", cxxopts::value<std::vector<std::string>>(), "map")
	    ("remove-commits", "Specify sha1 list of commits to remove from history", cxxopts::value<std::vector<std::string>>(), "list_file")

	    ("cvs-auth-map", "msg&time -> cvs author map (needs sha1->key map)", cxxopts::value<std::vector<std::string>>(), "file")
	    ("cvs-branch-map", "msg&time -> cvs branch map (needs sha1->key map)", cxxopts::value<std::vector<std::string>>(), "file")
	    ("keymap", "sha1 -> msg&time map (needs original-oid tags)", cxxopts::value<std::vector<std::string>>(), "file")

	    ("t,trim-whitespace", "Trim extra spaces and end-of-line characters from the end of commit messages", cxxopts::value<bool>(trim_whitespace))
	    ("w,wrap-commit-lines", "Wrap long commit lines to 72 cols (won't wrap messages already having multiple non-empty lines)", cxxopts::value<bool>(wrap_commit_lines))
	    ("width", "Column wrapping width (if enabled)", cxxopts::value<int>(), "N")

	    ("r,repo", "Original git repository path (must support running git log)", cxxopts::value<std::vector<std::string>>(), "path")
	    ("n,collapse-notes", "Take any git-notes contents and append them to regular commit messages.", cxxopts::value<bool>(collapse_notes))
	    ("no-blobs", "Write only commits in output .fi file.", cxxopts::value<bool>(no_blobs))
	    ("list-empty", "Print out information about empty commits.", cxxopts::value<bool>(list_empty))

	    ("splice-commits", "Look for git fast-import files in a 'splices' directory and insert them into the history.", cxxopts::value<bool>(splice_commits))
	    ("replace-commits", "Look for git fast-import files in a 'replace' directory and overwrite.  File of fast import file should be sha1 of target commit to replace.", cxxopts::value<bool>(replace_commits))
	    ("add-commits", "Look for git fast-import files in an 'add' directory and add to history.  Unlike splice commits, these are not being inserted into existing commit streams.", cxxopts::value<bool>(add_commits))

	    ("rebuild-ids", "Specify commits (revision number or SHA1) to rebuild.  Requires git-repo be set as well.  Needs --show-original-ids information in fast import file", cxxopts::value<std::vector<std::string>>(), "file")
	    ("rebuild-ids-children", "File with output of \"git rev-list --children --all\" - needed for processing rebuild-ids", cxxopts::value<std::vector<std::string>>(), "file")

	    ("h,help", "Print help")
	    ;

	auto result = options.parse(argc, argv);

	if (result.count("help"))
	{
	    std::cout << options.help({""}) << std::endl;
	    return 0;
	}

	if (result.count("r"))
	{
	    auto& ff = result["r"].as<std::vector<std::string>>();
	    repo_path = ff[0];
	}

	if (result.count("e"))
	{
	    auto& ff = result["e"].as<std::vector<std::string>>();
	    email_map = ff[0];
	}

	if (result.count("s"))
	{
	    auto& ff = result["s"].as<std::vector<std::string>>();
	    svn_map = ff[0];
	}

	if (result.count("rebuild-ids"))
	{
	    auto& ff = result["rebuild-ids"].as<std::vector<std::string>>();
	    id_file = ff[0];
	}

	if (result.count("rebuild-ids-children"))
	{
	    auto& ff = result["rebuild-ids-children"].as<std::vector<std::string>>();
	    children_file = ff[0];
	}

	if (result.count("cvs-auth-map"))
	{
	    auto& ff = result["cvs-auth-map"].as<std::vector<std::string>>();
	    cvs_auth_map = ff[0];
	}

	if (result.count("cvs-branch-map"))
	{
	    auto& ff = result["cvs-branch-map"].as<std::vector<std::string>>();
	    cvs_branch_map = ff[0];
	}

	if (result.count("keymap"))
	{
	    auto& ff = result["keymap"].as<std::vector<std::string>>();
	    keymap = ff[0];
	}

	if (result.count("svn-revs"))
	{
	    auto& ff = result["svn-revs"].as<std::vector<std::string>>();
	    svn_rev_map = ff[0];
	}

	if (result.count("svn-branches"))
	{
	    auto& ff = result["svn-branches"].as<std::vector<std::string>>();
	    svn_branch_map = ff[0];
	}

	if (result.count("correct-branches"))
	{
	    auto& ff = result["correct-branches"].as<std::vector<std::string>>();
	    correct_branches = ff[0];
	}

	if (result.count("remove-commits"))
	{
	    auto& ff = result["remove-commits"].as<std::vector<std::string>>();
	    remove_commits = ff[0];
	}

	if (result.count("svn-branches-to-tags"))
	{
	    auto& ff = result["svn-branches-to-tags"].as<std::vector<std::string>>();
	    svn_branches_to_tags = ff[0];
	}

	if (result.count("width"))
	{
	    cwidth = result["width"].as<int>();
	}

    }
    catch (const cxxopts::OptionException& e)
    {
	std::cerr << "error parsing options: " << e.what() << std::endl;
	return -1;
    }

    if (collapse_notes && !repo_path.length()) {
	std::cerr << "Cannot collapse notes into commit messages without knowing the path\nto the repository - aborting.  (It is necessary to run git log to\ncapture the message information, and for that we need the original\nrepository in addition to the fast-import file.)\n\nTo specify a repo folder, use the -r option.  Currently the folder must be in the working directory.\n";
	return -1;
    }

    if (id_file.length() && !repo_path.length()) {
	std::cerr << "Need Git repository path for CVS id list processing!\n";
	return -1;
    }

    if (argc != 3) {
	std::cout << "repowork [opts] <input_file> <output_file>\n";
	return -1;
    }
    std::ifstream infile(argv[1], std::ifstream::binary);
    if (!infile.good()) {
	return -1;
    }

    int ret = parse_fi_file(&fi_data, infile);

    if ((replace_commits || splice_commits || add_commits) && !fi_data.have_sha1s) {
	std::cerr << "Fatal - sha1 SVN rev updating requested, but don't have original sha1 ids - redo fast-export with the --show-original-ids option.\n";
	exit(1);
    }


    if (collapse_notes) {
	// Let the output routines know not to write notes commits.
	// (blobs will have to be taken care of later by git gc).
	fi_data.write_notes = false;

	// Reset the input stream
	infile.clear();
	infile.seekg(0, std::ios::beg);

	// Handle the notes
	git_unpack_notes(&fi_data, infile, repo_path);
    }

    if (keymap.length()) {
	read_key_sha1_map(&fi_data, keymap);
    }

    if (cvs_auth_map.length()) {
	if (!keymap.length()) {
	    std::cerr << "CVS author map specified without key map\n";
	    return -1;
	}
	read_key_cvsauthor_map(&fi_data, cvs_auth_map);
    }

    if (cvs_branch_map.length()) {
	if (!keymap.length()) {
	    std::cerr << "CVS branch map specified without key map\n";
	    return -1;
	}
	read_key_cvsbranch_map(&fi_data, cvs_branch_map);
    }

    if (email_map.length()) {
	// Reset the input stream
	infile.clear();
	infile.seekg(0, std::ios::beg);

	// Handle the notes
	git_map_emails(&fi_data, email_map);
    }

    if (svn_map.length()) {
	// Handle the svn committers
	git_map_svn_committers(&fi_data, svn_map);
    }

    if (list_empty) {
	for (size_t i = 0; i < fi_data.commits.size(); i++) {
	    if (fi_data.commits[i].commit_msg.length() && !fi_data.commits[i].fileops.size()) {
		if (fi_data.commits[i].id.sha1.length()) {
		    std::cout << "Empty commit(" << fi_data.commits[i].id.sha1 << "): " << fi_data.commits[i].commit_msg << "\n";
		} else {
		    std::cout << "Empty commit: " << fi_data.commits[i].commit_msg << "\n";
		}
	    }
	}
    }

    if (id_file.length()) {
	// Handle rebuild info
	git_id_rebuild_commits(&fi_data, id_file, repo_path, children_file);
    }

    if (remove_commits.length()) {
	git_remove_commits(&fi_data, remove_commits);
    }

    ////////////////////////////////////////////////////
    // Various note correction routines
    if (svn_rev_map.length()) {
	git_update_svn_revs(&fi_data, svn_rev_map);
    }

    // The subsequent steps, if invoked, may need svn_id set.
    git_sync_svn_revs(&fi_data);

    if (svn_branch_map.length()) {
	git_assign_branch_labels(&fi_data, svn_branch_map, 0);
    }
    if (correct_branches.length()) {
	git_assign_branch_labels(&fi_data, correct_branches, 1);
    }
    if (svn_branches_to_tags.length()) {
	git_set_tag_labels(&fi_data, svn_branches_to_tags);
    }

    fi_data.wrap_width = cwidth;
    fi_data.wrap_commit_lines = wrap_commit_lines;
    fi_data.trim_whitespace = trim_whitespace;

    infile.close();


    // If we have any replace commits, parse and overwrite.
    if (replace_commits) {

	std::filesystem::path ip = std::string(argv[1]);
	std::filesystem::path aip = std::filesystem::absolute(ip);
	std::filesystem::path pip = aip.parent_path();
	pip /= "replace";
	if (!std::filesystem::exists(pip)) {
	    std::cerr << "Warning - splices enabled but " << pip << " is not present on the filesystem.\n";
	} else {
	    for (const auto& de : std::filesystem::recursive_directory_iterator(pip)) {
		std::cout << "Processing " << de.path().string() << "\n";
		std::ifstream sfile(de.path(), std::ifstream::binary);
		fi_data.replace_sha1 = de.path().filename().string();
		int ret = parse_replace_fi_file(&fi_data, sfile);
		sfile.close();
	    }
	}
    }

    // If we have any additional commits, parse and insert them.
    if (add_commits) {

	std::filesystem::path ip = std::string(argv[1]);
	std::filesystem::path aip = std::filesystem::absolute(ip);
	std::filesystem::path pip = aip.parent_path();
	pip /= "add";
	if (!std::filesystem::exists(pip)) {
	    std::cerr << "Warning - adds enabled but " << pip << " is not present on the filesystem.\n";
	} else {
	    for (const auto& de : std::filesystem::recursive_directory_iterator(pip)) {
		std::cout << "Processing " << de.path().string() << "\n";
		std::ifstream sfile(de.path(), std::ifstream::binary);
		int ret = parse_add_fi_file(&fi_data, sfile);
		sfile.close();
	    }
	}
    }

    // If we have any splice commits, parse and insert them.  (Note - this comes last, for
    // bookkeeping reasons.)
    if (splice_commits) {

	std::filesystem::path ip = std::string(argv[1]);
	std::filesystem::path aip = std::filesystem::absolute(ip);
	std::filesystem::path pip = aip.parent_path();
	pip /= "splices";
	if (!std::filesystem::exists(pip)) {
	    std::cerr << "Warning - splices enabled but " << pip << " is not present on the filesystem.\n";
	} else {
	    for (const auto& de : std::filesystem::recursive_directory_iterator(pip)) {
		std::cout << "Processing " << de.path().string() << "\n";
		std::ifstream sfile(de.path(), std::ifstream::binary);
		int ret = parse_splice_fi_file(&fi_data, sfile);
		sfile.close();
	    }
	}
    }

    std::ifstream ifile(argv[1], std::ifstream::binary);
    std::ofstream ofile(argv[2], std::ios::out | std::ios::binary);
    if (!no_blobs) {
	ofile << "progress Writing blobs...\n";
	for (size_t i = 0; i < fi_data.blobs.size(); i++) {
	    write_blob(ofile, &fi_data.blobs[i], ifile);
	    if ( !(i % 1000) ) {
		ofile << "progress blob " << i << " of " << fi_data.blobs.size() << "\n";
	    }
	}
    }
    ofile << "progress Writing commits...\n";
    for (size_t i = 0; i < fi_data.commits.size(); i++) {
	write_commit(ofile, &fi_data.commits[i], &fi_data, ifile);
	if ( !(i % 1000) ) {
	    ofile << "progress commit " << i << " of " << fi_data.commits.size() << "\n";
	}
    }
    ofile << "progress Writing tags...\n";
    for (size_t i = 0; i < fi_data.tags.size(); i++) {
	write_tag(ofile, &fi_data.tags[i], ifile);
    }
    ofile << "progress Done.\n";

    ifile.close();
    ofile.close();

    std::cout << "Git fast-import file is generated:  " << argv[2] << "\n\n";
    std::cout << "Note that when imported, compression and packing will be suboptimal by default.\n";
    std::cout << "Some possible steps to take:\n";
    std::cout << "  mkdir git_repo && cd git_repo && git init\n";
    std::cout << "  cat ../" << argv[2] << " | git fast-import\n";
    std::cout << "  git gc --aggressive\n";
    std::cout << "  git reflog expire --expire-unreachable=now --all\n";
    std::cout << "  git gc --prune=now\n";

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
