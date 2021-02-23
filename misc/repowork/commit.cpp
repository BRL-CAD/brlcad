/*                      C O M M I T . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file commit.cpp
 *
 * The majority of the work is in processing
 * Git commits.
 *
 */

#include "TextFlow.hpp"
#include "repowork.h"

typedef int (*commitcmd_t)(git_commit_data *, std::ifstream &);

commitcmd_t
commit_find_cmd(std::string &line, std::map<std::string, commitcmd_t> &cmdmap)
{
    commitcmd_t cc = NULL;
    std::map<std::string, commitcmd_t>::iterator c_it;
    for (c_it = cmdmap.begin(); c_it != cmdmap.end(); c_it++) {
	if (!ficmp(line, c_it->first)) {
	    cc = c_it->second;
	    break;
	}
    }
    return cc;
}

int
commit_parse_author(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 7); // Remove "author " prefix
    size_t spos = line.find_first_of(">");
    if (spos == std::string::npos) {
	std::cerr << "Invalid author entry! " << line << "\n";
	exit(EXIT_FAILURE);
    }
    cd->author = line.substr(0, spos+1);
    cd->author_timestamp = line.substr(spos+2, std::string::npos);
    return 0;
}

int
commit_parse_committer(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 10); // Remove "committer " prefix
    size_t spos = line.find_first_of(">");
    if (spos == std::string::npos) {
	std::cerr << "Invalid committer entry! " << line << "\n";
	exit(EXIT_FAILURE);
    }
    cd->committer = line.substr(0, spos+1);
    cd->committer_timestamp = line.substr(spos+2, std::string::npos);
    //std::cout << "Committer: " << cd->committer << "\n";
    //std::cout << "Committer timestamp: " << cd->committer_timestamp << "\n";
    return 0;
}

int
commit_parse_commit(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 7);  // Remove "commit " prefix
    if (!ficmp(line, std::string("refs/notes/"))) {
	// Notes commit - flag accordingly
	cd->notes_commit = 1;
	return 0;
    }
    size_t spos = line.find_last_of("/");
    line.erase(0, spos+1); // Remove "refs/..." prefix
    cd->branch = line;
    //std::cout << "Branch: " << cd->branch << "\n";
    return 0;
}

int
commit_parse_data(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 5); // Remove "data " prefix
    size_t data_len = std::stoi(line);
    // This is the commit message - read it in
    char *cbuffer = new char [data_len+1];
    cbuffer[data_len] = '\0';
    infile.read(cbuffer, data_len);
    cd->commit_msg = std::string(cbuffer);
    delete[] cbuffer;
    //std::cout << "Commit message:\n" << cd->commit_msg << "\n";
    return 0;
}

int
commit_parse_encoding(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    std::cerr << "TODO - support encoding\n";
    exit(EXIT_FAILURE);
    return 0;
}

int
commit_parse_from(git_commit_data *cd, std::ifstream &infile)
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
commit_parse_mark(git_commit_data *cd, std::ifstream &infile)
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
commit_parse_merge(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 6); // Remove "merge " prefix
    //std::cout << "merge line: " << line << "\n";
    git_commitish merge_id;
    int ret = git_parse_commitish(merge_id, cd->s, line);
    if (!ret) {
	cd->merges.push_back(merge_id);
	return 0;
    }
    std::cerr << "TODO - unsupported \"merge\" specifier: " << line << "\n";
    git_parse_commitish(merge_id, cd->s, line);
    exit(EXIT_FAILURE);
}

int
commit_parse_original_oid(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 13);  // Remove "original-oid " prefix
    cd->id.sha1 = line;
    cd->s->have_sha1s = true;
    if (cd->s->sha12key.find(cd->id.sha1) != cd->s->sha12key.end()) {
	std::cout << "Have CVS info for commit " << cd->id.sha1 << "\n";
    }
    return 0;
}

int
commit_parse_filecopy(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 2); // Remove "C " prefix
    size_t spos = line.find_first_of(" ");
    if (spos == std::string::npos) {
    	std::cerr << "Invalid copy specifier: " << line << "\n";
	return -1;
    }
    size_t qpos = line.find_first_of("\"");
    if (qpos != std::string::npos) {
    	std::cerr << "quoted path specifiers currently unsupported:" << line << "\n";
	exit(EXIT_FAILURE);
    }
    git_op op;
    op.type = filecopy;
    op.path = line.substr(0, spos);
    op.dest_path = line.substr(spos+1, std::string::npos);
    //std::cout << "filecopy: " << op.path << " -> " << op.dest_path << "\n";
    cd->fileops.push_back(op);
    return 0;
}

int
commit_parse_filedelete(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 2); // Remove "D " prefix
    git_op op;
    op.type = filedelete;
    op.path = line;
    cd->fileops.push_back(op);
    //std::cout << "filedelete: " << line << "\n";
    return 0;
}

int
commit_parse_filemodify(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 2); // Remove "M " prefix
    std::regex fmod("([0-9]+) ([:A-Za-z0-9]+) (.*)");
    std::smatch fmodvar;
    if (!std::regex_search(line, fmodvar, fmod)) {
	std::cerr << "Invalid modification specifier: " << line << "\n";
	return -1;
    }
    git_op op;
    op.type = filemodify;
    op.mode = std::string(fmodvar[1]);
    std::string dataref = std::string(fmodvar[2]);
    if (dataref == std::string("inline")) {
	std::cerr << "inline data unsupported\n";
	exit(EXIT_FAILURE);
    }
    int ret = git_parse_commitish(op.dataref, cd->s, dataref);
    if (ret || (op.dataref.mark == -1 && !op.dataref.sha1.length())) {
	std::cerr << "Invalid data ref!: " << dataref << "\n";
    }
    op.path = std::string(fmodvar[3]);

    //std::cout << "filemodify: " << op.mode << "," << op.dataref.index << "," << op.path << "\n";

    cd->fileops.push_back(op);

    return 0;
}

int
commit_parse_notemodify(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    std::cerr << "notemodify currently unsupported:" << line << "\n";
    exit(EXIT_FAILURE);
}

int
commit_parse_filerename(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 2); // Remove "R " prefix
    size_t spos = line.find_first_of(" ");
    if (spos == std::string::npos) {
    	std::cerr << "Invalid copy specifier: " << line << "\n";
	return -1;
    }
    size_t qpos = line.find_first_of("\"");
    if (spos != std::string::npos) {
    	std::cerr << "quoted path specifiers currently unsupported:" << line << "\n";
	exit(EXIT_FAILURE);
    }
    git_op op;
    op.type = filerename;
    op.path = line.substr(0, spos);
    op.dest_path = line.substr(spos+1, std::string::npos);
    //std::cout << "filerename: " << op.path << " -> " << op.dest_path << "\n"; 
    cd->fileops.push_back(op);
    return 0;
}

int
commit_parse_deleteall(git_commit_data *cd, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    if (line != std::string("deleteall")) {
    	std::cerr << "warning - invalid deleteall specifier:" << line << "\n";
    }
    git_op op;
    op.type = filedeleteall;
    cd->fileops.push_back(op);
    return 0;
}

int
parse_commit(git_fi_data *fi_data, std::ifstream &infile)
{
    //std::cout << "Found command: commit\n";

    git_commit_data gcd;
    gcd.s = fi_data;

    // Tell the commit where it will be in the vector - this
    // uniquely identifies this specific commit, regardless of
    // its sha1.
    gcd.id.index = fi_data->commits.size();

    std::map<std::string, commitcmd_t> cmdmap;
    // Commit info modification commands
    cmdmap[std::string("author")] = commit_parse_author;
    cmdmap[std::string("commit ")] = commit_parse_commit; // Note - need space after commit to avoid matching committer!
    cmdmap[std::string("committer")] = commit_parse_committer;
    cmdmap[std::string("data")] = commit_parse_data;
    cmdmap[std::string("encoding")] = commit_parse_encoding;
    cmdmap[std::string("from")] = commit_parse_from;
    cmdmap[std::string("mark")] = commit_parse_mark;
    cmdmap[std::string("merge")] = commit_parse_merge;
    cmdmap[std::string("original-oid")] = commit_parse_original_oid;

    // tree modification commands
    cmdmap[std::string("C ")] = commit_parse_filecopy;
    cmdmap[std::string("D ")] = commit_parse_filedelete;
    cmdmap[std::string("M ")] = commit_parse_filemodify;
    cmdmap[std::string("N ")] = commit_parse_notemodify;
    cmdmap[std::string("R ")] = commit_parse_filerename;
    cmdmap[std::string("deleteall")] = commit_parse_deleteall;

    std::string line;
    size_t offset = infile.tellg();
    int commit_done = 0;
    while (!commit_done && std::getline(infile, line)) {

	commitcmd_t cc = commit_find_cmd(line, cmdmap);

	// If we found a command, process it.  Otherwise, we are done
	// with the commit and need to clean up.
	if (cc) {
	    //std::cout << "commit line: " << line << "\n";
	    infile.seekg(offset);
	    (*cc)(&gcd, infile);
	    offset = infile.tellg();
	} else {
	    // Whatever was on that line, it's not a commit input.
	    // Reset input to allow the parent routine to deal with
	    // it, and return.
	    infile.seekg(offset);
	    commit_done = 1;
	}
    }

    gcd.id.mark = fi_data->next_mark(gcd.id.mark);
    fi_data->mark_to_index[gcd.id.mark] = gcd.id.index;

    // If we have a sha1 and this is not a notes commit, we need to map it to
    // this commit's mark
    if (!gcd.notes_commit && gcd.id.sha1.length()) {
	fi_data->sha1_to_mark[gcd.id.sha1] = gcd.id.mark;
    }

    //std::cout << "commit new mark: " << gcd.id.mark << "\n";

    // Add the commit to the data
    fi_data->commits.push_back(gcd);

    return 0;
}

int
parse_splice_commit(git_fi_data *fi_data, std::ifstream &infile)
{
    //std::cout << "Found command: commit\n";

    git_commit_data gcd;
    gcd.s = fi_data;

    // Tell the commit where it will be in the vector - this
    // uniquely identifies this specific commit, regardless of
    // its sha1.
    gcd.id.index = fi_data->commits.size() + fi_data->splice_commits.size();

    std::map<std::string, commitcmd_t> cmdmap;
    // Commit info modification commands
    cmdmap[std::string("author")] = commit_parse_author;
    cmdmap[std::string("commit ")] = commit_parse_commit; // Note - need space after commit to avoid matching committer!
    cmdmap[std::string("committer")] = commit_parse_committer;
    cmdmap[std::string("data")] = commit_parse_data;
    cmdmap[std::string("encoding")] = commit_parse_encoding;
    cmdmap[std::string("from")] = commit_parse_from;
    cmdmap[std::string("mark")] = commit_parse_mark;
    cmdmap[std::string("merge")] = commit_parse_merge;
    cmdmap[std::string("original-oid")] = commit_parse_original_oid;

    // tree modification commands
    cmdmap[std::string("C ")] = commit_parse_filecopy;
    cmdmap[std::string("D ")] = commit_parse_filedelete;
    cmdmap[std::string("M ")] = commit_parse_filemodify;
    cmdmap[std::string("N ")] = commit_parse_notemodify;
    cmdmap[std::string("R ")] = commit_parse_filerename;
    cmdmap[std::string("deleteall")] = commit_parse_deleteall;

    std::string line;
    size_t offset = infile.tellg();
    int commit_done = 0;
    while (!commit_done && std::getline(infile, line)) {

	commitcmd_t cc = commit_find_cmd(line, cmdmap);

	// If we found a command, process it.  Otherwise, we are done
	// with the commit and need to clean up.
	if (cc) {
	    //std::cout << "commit line: " << line << "\n";
	    infile.seekg(offset);
	    (*cc)(&gcd, infile);
	    offset = infile.tellg();
	} else {
	    // Whatever was on that line, it's not a commit input.
	    // Reset input to allow the parent routine to deal with
	    // it, and return.
	    infile.seekg(offset);
	    commit_done = 1;
	}
    }

    gcd.id.mark = fi_data->next_mark(gcd.id.mark);
    fi_data->mark_to_index[gcd.id.mark] = gcd.id.index;

    //std::cout << "commit new mark: " << gcd.id.mark << "\n";

    // Add the commit to the data
    fi_data->splice_commits.push_back(gcd);

    // Mark the original commit as having a splice, so we will
    // be able to write this out in the correct order for the
    // fi file.
    if (gcd.from.index < (long)fi_data->commits.size()) {
	git_commit_data &oc = fi_data->commits[gcd.from.index];
	fi_data->splice_map[oc.id.mark] = gcd.id.mark;
    } else {
	std::cerr << "TODO: Multi-commit splices\n";
	exit(1);
    }

    // For any commits that listed oc as their from commit, they
    // now need to be updated to reference the new one instead.
    for (size_t i = 0; i < fi_data->commits.size(); i++) {
	git_commit_data &c = fi_data->commits[i];
	if (c.from.index == gcd.from.index) {
	    std::cout << "Updating from id of " << c.id.sha1 << "\n";
	    c.from = gcd.id;
	}
    }

    return 0;
}




void
write_op(std::ofstream &outfile, git_op *o)
{
    switch (o->type) {
	case filemodify:
	    if (o->dataref.mark > -1) {
		outfile << "M " << o->mode << " :" << o->dataref.mark << " " << o->path << "\n";
	    } else {
		if (o->dataref.sha1.length()) {
		outfile << "M " << o->mode << " " << o->dataref.sha1 << " " << o->path << "\n";
		} else {
		    std::cerr << "Invalid filemodify dataref: " << o->dataref.mark << "," << o->dataref.sha1 << "\n";
		    exit(1);
		}
	    }
	    break;
	case filedelete:
	    outfile << "D " << o->path << "\n";
	    break;
	case filecopy:
	    outfile << "C " << o->path << " " << o->dest_path << "\n";
	    break;
	case filerename:
	    outfile << "R " << o->path << " " << o->dest_path << "\n";
	    break;
	case filedeleteall:
	    outfile << "deleteall\n";
	    break;
	case notemodify:
	    std::cerr << "TODO - write notemodify\n";
	    break;
    }
}

// trim from end (in place) - https://stackoverflow.com/a/217605
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !(std::isspace(ch) || ch == '\n' || ch == '\r');
    }).base(), s.end());
}

std::string
commit_msg(git_commit_data *c)
{
    int cwidth = c->s->wrap_width;
    std::string nmsg;

    // Any whitespace at the end of the message, just trim it
    if (c->s->trim_whitespace) {
	rtrim(c->commit_msg);
    }

    if (c->s->wrap_commit_lines) {
	// Wrap the commit messages - gitk doesn't like long one liners by
	// default.  Don't know why the line wrap ISN'T on by default, but we
	// might as well deal with it while we're here...
	size_t pdelim = c->commit_msg.find("\n\n");
	if (pdelim == std::string::npos) {
	    size_t spos = c->commit_msg.find_first_of('\n');
	    if (spos == std::string::npos) {
		std::string wmsg = TextFlow::Column(c->commit_msg).width(cwidth).toString();
		c->commit_msg = wmsg;
	    }
	} else {
	    // Multiple paragraphs - separate them for individual consideration.
	    std::vector<std::string> paragraphs;
	    std::string paragraphs_str = c->commit_msg;
	    while (paragraphs_str.length()) {
		std::string para = paragraphs_str.substr(0, pdelim);
		std::string remainder = paragraphs_str.substr(pdelim+2, std::string::npos);
		paragraphs_str = remainder;
		paragraphs.push_back(para);
		pdelim = paragraphs_str.find("\n\n");
		if (pdelim == std::string::npos) {
		    // That's it - last line
		    paragraphs.push_back(paragraphs_str);
		    paragraphs_str = std::string("");
		}
	    }
	    bool can_wrap = true;
	    // If any of the paragraphs delimited by two returns has returns inside of it, we can't
	    // do anything - the message already has some sort of formatting.
	    for (size_t i = 0; i < paragraphs.size(); i++) {
		size_t spos = paragraphs[i].find_first_of('\n');
		if (spos != std::string::npos) {
		    can_wrap = false;
		    break;
		}
	    }
	    if (can_wrap) {
		// Wrap each paragraph individually
		for (size_t i = 0; i < paragraphs.size(); i++) {
		    std::string newpara = TextFlow::Column(paragraphs[i]).width(cwidth).toString();
		    paragraphs[i] = newpara;
		}
		std::string newcommitmsg;
		// Reassemble
		for (size_t i = 0; i < paragraphs.size(); i++) {
		    newcommitmsg.append(paragraphs[i]);
		    newcommitmsg.append("\n\n");
		}
		rtrim(newcommitmsg);
		c->commit_msg = newcommitmsg;
	    }
	}
    }

    if (c->notes_string.length()) {
	std::string nstr = c->notes_string;
	if (c->s->trim_whitespace) rtrim(nstr);
	if (c->s->wrap_commit_lines) {
	    size_t spos = nstr.find_first_of('\n');
	    if (spos == std::string::npos) {
		std::string wmsg = TextFlow::Column(nstr).width(cwidth).toString();
		nstr = wmsg;
	    }
	}
	if (c->svn_committer.length()) {
	    std::string committerstr = std::string("svn:account:") + c->svn_committer;
	    nmsg = c->commit_msg + std::string("\n\n") + nstr + std::string("\n") + committerstr + std::string("\n");
	} else {
	    nmsg = c->commit_msg + std::string("\n\n") + nstr + std::string("\n");
	}
    } else {
	if (c->s->trim_whitespace) {
	    nmsg = c->commit_msg + std::string("\n");
	} else {
	    nmsg = c->commit_msg;
	}
    }

    // Check for CVS information to add
    if (c->s->sha12key.find(c->id.sha1) != c->s->sha12key.end()) {
	std::string cvsmsg = nmsg;
	std::string key = c->s->sha12key[c->id.sha1];
	int have_ret = (c->svn_id.length()) ? 1 : 0;
	if (c->s->key2cvsbranch.find(key) != c->s->key2cvsbranch.end()) {
	    //std::cout << "Found branch: " << c->s->key2cvsbranch[key] << "\n";
	    if (!have_ret) {
		cvsmsg.append("\n");
		have_ret = 1;
	    }
	    std::string cb = c->s->key2cvsbranch[key];
	    cvsmsg.append("cvs:branch:");
	    if (cb == std::string("master")) {
		cvsmsg.append("trunk");
	    } else {
		cvsmsg.append(cb);
	    }
	    cvsmsg.append("\n");
	}
	if (c->s->key2cvsauthor.find(key) != c->s->key2cvsauthor.end()) {
	    //std::cout << "Found author: " << c->s->key2cvsauthor[key] << "\n";
	    if (!have_ret) {
		cvsmsg.append("\n");
	    }
	    std::string svnname = std::string("svn:account:") + c->s->key2cvsauthor[key];
	    std::string cvsaccount = std::string("cvs:account:") + c->s->key2cvsauthor[key];
	    size_t index = cvsmsg.find(svnname);
	    if (index != std::string::npos) {
		std::cout << "Replacing svn:account\n";
		cvsmsg.replace(index, cvsaccount.length(), cvsaccount);
	    } else {
		cvsmsg.append(cvsaccount);
		cvsmsg.append("\n");
	    }
	}
	nmsg = cvsmsg;
    }

    return nmsg;
}

int
write_commit(std::ofstream &outfile, git_commit_data *c, git_fi_data *d, std::ifstream &infile)
{
    if (!infile.good()) {
        return -1;
    }

    if (c->skip_commit)
	return 0;

    // If this is a reset commit, it's handled quite differently
    if (c->reset_commit) {
	outfile << "reset " << c->branch << "\n";
	if (c->from.mark != -1) {
	    outfile << "from :" << c->from.mark << "\n";
	}
	outfile << "\n";
	return 0;
    }

#if 0
    // If this is a rebuild, write the blobs first
    if (c->id.sha1.length()) {
	if (c->s->rebuild_commits.find(c->id.sha1) != c->s->rebuild_commits.end()) {
	    std::cout << "rebuild commit!\n";
	    std::string sha1blobs = c->id.sha1 + std::string("-blob.fi");
	    std::ifstream s1b(sha1blobs, std::ifstream::binary | std::ios::ate);
	    std::streamsize size = s1b.tellg();
	    s1b.seekg(0, std::ios::beg);
	    std::vector<char> buffer(size);
	    if (s1b.read(buffer.data(), size)) {
		outfile.write(reinterpret_cast<char*>(buffer.data()), size);
	    } else {
		std::cerr << "Failed to open rebuild file " << sha1blobs << "\n";
		exit(1);
	    }
	    s1b.close();
	}
    }
#endif

    // Header
    if (c->notes_commit) {
	// Don't output notes commits - we're handling things differently.
 	return 0;
    } else {
	outfile << "commit refs/heads/" << c->branch << "\n";
    }
    outfile << "mark :" << c->id.mark << "\n";
#if 0
    if (c->id.sha1.length()) {
	outfile << "original-oid " << c->id.sha1 << "\n";
    }
#endif
    if (c->author.length()) {
	outfile << "author " << c->author << " " << c->author_timestamp << "\n";
    } else {
	outfile << "author " << c->committer << " " << c->committer_timestamp << "\n";
    }
    outfile << "committer " << c->committer << " " << c->committer_timestamp << "\n";

    std::string nmsg = commit_msg(c);
    outfile << "data " << nmsg.length() << "\n";
    outfile << nmsg;

    if (c->from.mark != -1) {
	// Check to see if a commit was spliced in between this commit and its from commit.
	if (d->splice_map.find(c->from.mark) != d->splice_map.end()) {
	    git_commit_data &cd = d->splice_commits[c->from.mark];
	    outfile << "from :" << cd.id.mark << "\n";
	} else {
	    outfile << "from :" << c->from.mark << "\n";
	}
    }
    for (size_t i = 0; i < c->merges.size(); i++) {
	outfile << "merge :" << c->merges[i].mark << "\n";
    }

    bool write_ops = true;
    if (c->id.sha1.length()) {
	if ((c->s->rebuild_commits.find(c->id.sha1) != c->s->rebuild_commits.end()) ||
		(c->s->reset_commits.find(c->id.sha1) != c->s->reset_commits.end())) {
	    write_ops = false;
	    std::string sha1tree = std::string("trees/") + c->id.sha1 + std::string("-tree.fi");
	    std::ifstream s1t(sha1tree, std::ifstream::binary | std::ios::ate);
	    std::streamsize size = s1t.tellg();
	    s1t.seekg(0, std::ios::beg);
	    std::vector<char> buffer(size);
	    if (s1t.read(buffer.data(), size)) {
		outfile.write(reinterpret_cast<char*>(buffer.data()), size);
	    } else {
		std::cerr << "Failed to open rebuild file " << sha1tree << "\n";
		exit(1);
	    }
	    s1t.close();
	}
    }
    if (write_ops) {
	for (size_t i = 0; i < c->fileops.size(); i++) {
	    write_op(outfile, &c->fileops[i]);
	}
    }
    outfile << "\n";

    // If there is a splice commit that follows this one, write it out now.
    if (d->splice_map.find(c->id.mark) != d->splice_map.end()) {
	std::cout << "Found splice commit to follow " << c->id.sha1 << "\n";
	long s1 = d->mark_to_index[d->splice_map[c->id.mark]];
	long scind = s1 - d->commits.size();
	if (scind < 0) {
	    std::cerr << "Couldn't find splice commit\n";
	    exit(1);
	}
	git_commit_data *sc = &d->splice_commits[scind];
	write_commit(outfile, sc, d, infile);
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
