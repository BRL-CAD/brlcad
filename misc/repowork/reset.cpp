/*                       R E S E T . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file reset.cpp
 *
 * Handle reset command - stored as a commit for ordering,
 * but it is treated a bit differently.
 *
 */

#include "repowork.h"

typedef int (*resetcmd_t)(git_commit_data *, std::ifstream &);
resetcmd_t
reset_find_cmd(std::string &line, std::map<std::string, resetcmd_t> &cmdmap)
{
    resetcmd_t cc = NULL;
    std::map<std::string, resetcmd_t>::iterator c_it;
    for (c_it = cmdmap.begin(); c_it != cmdmap.end(); c_it++) {
	if (!ficmp(line, c_it->first)) {
	    cc = c_it->second;
	    break;
	}
    }
    return cc;
}

int
reset_parse_reset(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 6);  // Remove "reset " prefix
    // Stash the reference in the branch string.  It may be
    // a tag rather than a branch, so in this case we save
    // the full reference path
    cd->branch = line;
    return 0;
}

int
reset_parse_from(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 5); // Remove "from " prefix
    //std::cout << "from line: " << line << "\n";
    int ret = git_parse_commitish(cd->from, cd->s, line);
    if (!ret) {
	return 0;
    }
    std::cerr << "TODO - unsupported \"from\" specifier: " << line << "\n";
    exit(EXIT_FAILURE);
}

int
parse_reset(git_fi_data *fi_data, std::ifstream &infile)
{
    //std::cout << "Found command: reset\n";

    git_commit_data gcd;
    gcd.s = fi_data;
    gcd.reset_commit = 1;

    // Tell the reset where it will be in the vector.
    gcd.id.index = fi_data->commits.size();

    std::map<std::string, resetcmd_t> cmdmap;
    cmdmap[std::string("reset")] = reset_parse_reset;
    cmdmap[std::string("from")] = reset_parse_from;

    std::string line;
    size_t offset = infile.tellg();
    int reset_done = 0;
    while (!reset_done && std::getline(infile, line)) {

	resetcmd_t cc = reset_find_cmd(line, cmdmap);

	// If we found a command, process it.  Otherwise, we are done
	// with the reset and need to clean up.
	if (cc) {
	    //std::cout << "reset line: " << line << "\n";
	    infile.seekg(offset);
	    (*cc)(&gcd, infile);
	    offset = infile.tellg();
	} else {
	    // Whatever was on that line, it's not a reset input.
	    // Reset input to allow the parent routine to deal with
	    // it, and return.
	    infile.seekg(offset);
	    reset_done = 1;
	}
    }

    // resets don't get a mark - they are written out in the stream
    // in commit order
    gcd.id.mark = 0;

    // Add the reset to the commit data
    fi_data->commits.push_back(gcd);

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
