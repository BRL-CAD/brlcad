/*                         T A G . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file tag.cpp
 *
 * Process Git tags
 *
 */

#include "repowork.h"

typedef int (*tagcmd_t)(git_tag_data *, std::ifstream &);

tagcmd_t
tag_find_cmd(std::string &line, std::map<std::string, tagcmd_t> &cmdmap)
{
    tagcmd_t cc = NULL;
    std::map<std::string, tagcmd_t>::iterator c_it;
    for (c_it = cmdmap.begin(); c_it != cmdmap.end(); c_it++) {
	if (!ficmp(line, c_it->first)) {
	    cc = c_it->second;
	    break;
	}
    }
    return cc;
}


int
tag_parse_data(git_tag_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 5); // Remove "data " prefix
    size_t data_len = std::stoi(line);
    // This is the commit message - read it in
    char *cbuffer = new char [data_len+1];
    cbuffer[data_len] = '\0';
    infile.read(cbuffer, data_len);
    cd->tag_msg = std::string(cbuffer);
    delete[] cbuffer;
    //std::cout << "Tagging message:\n" << cd->tag_msg << "\n";
    return 0;
}

int
tag_parse_from(git_tag_data *cd, std::ifstream &infile)
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
tag_parse_mark(git_tag_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    //std::cout << "mark line: " << line << "\n";
    line.erase(0, 5); // Remove "mark " prefix
    //std::cout << "mark line: " << line << "\n";
    if (line.c_str()[0] != ':') {
	std::cerr << "Mark without \":\" character??: " <<  line << "\n";
	return -1;
    }
    line.erase(0, 1); // Remove ":" prefix
    cd->id.mark = cd->s->next_mark(std::stol(line));
    //std::cout << "Mark id :" << line << " -> " << cd->id.mark << "\n";
    return 0;
}

int
tag_parse_original_oid(git_tag_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 13);  // Remove "original-oid " prefix
    cd->id.sha1 = line;
    return 0;
}

int
tag_parse_tag(git_tag_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 4);  // Remove "tag " prefix
    size_t spos = line.find_last_of("/");
    line.erase(0, spos+1); // Remove "refs/..." prefix
    cd->tag = line;
    //std::cout << "Tag: " << cd->tag << "\n";
    return 0;
}

int
tag_parse_tagger(git_tag_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 7); // Remove "tagger " prefix
    size_t spos = line.find_first_of(">");
    if (spos == std::string::npos) {
	std::cerr << "Invalid tagger entry! " << line << "\n";
	exit(EXIT_FAILURE);
    }
    cd->tagger = line.substr(0, spos+1);
    cd->tagger_timestamp = line.substr(spos+2, std::string::npos);
    //std::cout << "Tagger: " << cd->tagger << "\n";
    //std::cout << "Tagger timestamp: " << cd->tagger_timestamp << "\n";
    return 0;
}

int
parse_tag(git_fi_data *fi_data, std::ifstream &infile)
{
    //std::cout << "Found command: tag\n";

    git_tag_data gcd;
    gcd.s = fi_data;

    // Tell the tag where it will be in the vector - this
    // uniquely identifies this specific tag, regardless of
    // its sha1.
    gcd.id.index = fi_data->tags.size();

    std::map<std::string, tagcmd_t> cmdmap;
    cmdmap[std::string("data")] = tag_parse_data;
    cmdmap[std::string("from")] = tag_parse_from;
    cmdmap[std::string("mark")] = tag_parse_mark;
    cmdmap[std::string("original-oid")] = tag_parse_original_oid;
    cmdmap[std::string("tag ")] = tag_parse_tag;
    cmdmap[std::string("tagger")] = tag_parse_tagger;

    std::string line;
    size_t offset = infile.tellg();
    int tag_done = 0;
    while (!tag_done && std::getline(infile, line)) {

	tagcmd_t cc = tag_find_cmd(line, cmdmap);

	// If we found a command, process it.  Otherwise, we are done
	// with the tag and need to clean up.
	if (cc) {
	    //std::cout << "tag line: " << line << "\n";
	    infile.seekg(offset);
	    (*cc)(&gcd, infile);
	    offset = infile.tellg();
	} else {
	    // Whatever was on that line, it's not a tag input.
	    // Reset input to allow the parent routine to deal with
	    // it, and return.
	    infile.seekg(offset);
	    tag_done = 1;
	}
    }

    // If we had a mark supplied by the input, map it to the
    // commit id
    gcd.id.mark = fi_data->next_mark(gcd.id.mark);
    fi_data->mark_to_index[gcd.id.mark] = gcd.id.index;

    // Add the tag to the data
    fi_data->tags.push_back(gcd);

    return 0;
}

int
write_tag(std::ofstream &outfile, git_tag_data *t, std::ifstream &infile)
{
    // Header
    outfile << "tag " << t->tag << "\n";
    outfile << "mark :" << t->id.mark << "\n";
    outfile << "from :" << t->from.mark << "\n";
    if (t->id.sha1.length()) {
	outfile << "original-oid " << t->id.sha1 << "\n";
    }
    outfile << "tagger :" << t->tagger << " " << t->tagger_timestamp << "\n";
    outfile << "data " << t->tag_msg.length() << "\n";
    outfile << t->tag_msg << "\n";
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
