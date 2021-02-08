/*                        B L O B . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file blob.cpp
 *
 * Logic for handling git blobs
 *
 */

#include "repowork.h"

typedef int (*blobcmd_t)(git_blob_data *, std::ifstream &);
blobcmd_t
blob_find_cmd(std::string &line, std::map<std::string, blobcmd_t> &cmdmap)
{
    blobcmd_t cc = NULL;
    std::map<std::string, blobcmd_t>::iterator c_it;
    for (c_it = cmdmap.begin(); c_it != cmdmap.end(); c_it++) {
	if (!ficmp(line, c_it->first)) {
	    cc = c_it->second;
	    break;
	}
    }
    return cc;
}

int
blob_parse_blob(git_blob_data *cd, std::ifstream &infile)
{
    /* No other information on line - just consume it and continue */
    std::string line;
    std::getline(infile, line);
    return 0;
}

int
blob_parse_data(git_blob_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 5);  // Remove "data " prefix
    cd->length = std::stoi(line);
    cd->offset = infile.tellg();

#if 0
    // Detect binary blobs (in case we ever want to try post-processing
    // of text blobs
    int cc = 0;
    char c;
    bool bfile = false;
    while (!infile.eof() && infile >> std::noskipws >> c && cc < 500) {
	if (c == '\0') {
	    bfile = true;
	    break;
	}
	cc++;
    }
    if (bfile) {
	std::cout << "Binary blob\n";
    }
    infile.clear();
#endif

    long offset = cd->offset + cd->length;
    infile.seekg(offset);
    return 0;
}

int
blob_parse_mark(git_blob_data *cd, std::ifstream &infile)
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
blob_parse_original_oid(git_blob_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 13);  // Remove "original-oid " prefix
    cd->id.sha1 = line;
    return 0;
}

int
parse_blob(git_fi_data *fi_data, std::ifstream &infile)
{
    //std::cout << "Found command: blob\n";

    git_blob_data gbd;
    gbd.s = fi_data;

    // Tell the blob where it will be in the vector.
    gbd.id.index = fi_data->blobs.size();

    std::map<std::string, blobcmd_t> cmdmap;
    cmdmap[std::string("blob")] = blob_parse_blob;
    cmdmap[std::string("data")] = blob_parse_data;
    cmdmap[std::string("mark")] = blob_parse_mark;
    cmdmap[std::string("original-oid")] = blob_parse_original_oid;

    std::string line;
    size_t offset = infile.tellg();
    int blob_done = 0;
    while (!blob_done && std::getline(infile, line)) {

	blobcmd_t cc = blob_find_cmd(line, cmdmap);

	// If we found a command, process it.  Otherwise, we are done
	// with the blob and need to clean up.
	if (cc) {
	    //std::cout << "blob line: " << line << "\n";
	    infile.seekg(offset);
	    (*cc)(&gbd, infile);
	    offset = infile.tellg();
	} else {
	    // Whatever was on that line, it's not a blob input.
	    // Reset input to allow the parent routine to deal with
	    // it, and return.
	    infile.seekg(offset);
	    blob_done = 1;
	}
    }

    gbd.id.mark = fi_data->next_mark(gbd.id.mark);
    fi_data->mark_to_index[gbd.id.mark] = gbd.id.index;

    // If we have an original-oid sha1, associate it with the mark
    if (gbd.id.sha1.length() == 40) {
	fi_data->sha1_to_mark[gbd.id.sha1] = gbd.id.mark;
    }

    // Add the blob to the data
    fi_data->blobs.push_back(gbd);

    return 0;
}

int
write_blob(std::ofstream &outfile, git_blob_data *b, std::ifstream &infile)
{
    if (!infile.good()) {
        return -1;
    }

    // Header
    outfile << "blob\n";
    outfile << "mark :" << b->id.mark << "\n";
    if (b->id.sha1.length()) {
	outfile << "original-oid " << b->id.sha1 << "\n";
    } 
    outfile << "data " << b->length << "\n";

    // Contents
    /* TODO - probably don't really need to read this into memory... */
    char *buffer = new char [b->length];
    infile.seekg(b->offset);
    infile.read(buffer, b->length);
    outfile.write(buffer, b->length);
    delete[] buffer;
    outfile << "\n";
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
