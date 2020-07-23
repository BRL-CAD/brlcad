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

#include "cxxopts.hpp"
#include "repowork.h"

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

	// Write the message to the commit's string;
	s->commits[i].notes_string = note;

	// SPECIAL PURPOSE CODE - should go away eventually.
	// For BRL-CAD specifically, this information contains
	// the SVN id associated with the commit.  We want to
	// use this info, so parse it out and store it.
	std::regex svnid("svn:revision:([0-9]+).*");
	std::smatch svnidvar;
	if (std::regex_search(note, svnidvar, svnid)) {
	    s->commits[i].svn_id = std::string(svnidvar[1]);
	    std::cout << "Identified revision " << s->commits[i].svn_id << "\n";
	}

	n.close();
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
git_map_svn_authors(git_fi_data *s, std::string &svn_map)
{
    // read map
    std::ifstream infile(svn_map, std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open svn_map file: " << svn_map << "\n";
	exit(-1);
    }

    // Create mapping of ids to svn authors
    std::map<std::string, std::string> svn_author_map;
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
	std::string author = line.substr(spos+1, std::string::npos);

	svn_author_map[id] = author;
    }

    // Iterate over the commits and assign authors.
    for (size_t i = 0; i < s->commits.size(); i++) {
	git_commit_data *c = &(s->commits[i]);
	if (!c->svn_id.length()) {
	    continue;
	}
	if (svn_author_map.find(c->svn_id) != svn_author_map.end()) {
	    std::string svnauth = svn_author_map[c->svn_id];
	    //std::cerr << "Found SVN commit \"" << c->svn_id << "\" with author \"" << svnauth << "\"\n";
	    c->svn_author = svnauth;
	}
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
main(int argc, char *argv[])
{
    git_fi_data fi_data;
    bool collapse_notes = false;
    bool wrap_commit_lines = false;
    bool trim_whitespace = false;
    std::string repo_path;
    std::string email_map;
    std::string svn_map;
    int cwidth = 72;

    // TODO - might be good do have a "validate" option that does the fast import and then
    // checks every commit saved from the old repo in the new one...
    try
    {
	cxxopts::Options options(argv[0], " - process git fast-import files");

	options.add_options()
	    ("e,email-map", "Specify replacement username+email mappings (one map per line, format is commit-id-1;commit-id-2)", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("n,collapse-notes", "Take any git-notes contents and append them to regular commit messages.", cxxopts::value<bool>(collapse_notes))
	    ("r,repo", "Original git repository path (must support running git log)", cxxopts::value<std::vector<std::string>>(), "path to repo")
	    ("s,svn-map", "Specify svn rev -> author map (one mapping per line, format is commit-rev authorname)", cxxopts::value<std::vector<std::string>>(), "map file")
	    ("t,trim-whitespace", "Trim extra spaces and end-of-line characters from the end of commit messages", cxxopts::value<bool>(trim_whitespace))
	    ("w,wrap-commit-lines", "Wrap long commit lines to 72 cols (won't wrap messages already having multiple non-empty lines)", cxxopts::value<bool>(wrap_commit_lines))
	    ("width", "Column wrapping width (if enabled)", cxxopts::value<int>(), "N")
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

    if (argc != 3) {
	std::cout << "repowork [opts] <input_file> <output_file>\n";
	return -1;
    }
    std::ifstream infile(argv[1], std::ifstream::binary);
    if (!infile.good()) {
	return -1;
    }

    int ret = parse_fi_file(&fi_data, infile);

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

    if (email_map.length()) {
	// Reset the input stream
	infile.clear();
	infile.seekg(0, std::ios::beg);

	// Handle the notes
	git_map_emails(&fi_data, email_map);
    }

    if (svn_map.length()) {
	// Handle the svn authors
	git_map_svn_authors(&fi_data, svn_map);
    }

    fi_data.wrap_width = cwidth;
    fi_data.wrap_commit_lines = wrap_commit_lines;
    fi_data.trim_whitespace = trim_whitespace;

    infile.close();

    std::ifstream ifile(argv[1], std::ifstream::binary);
    std::ofstream ofile(argv[2], std::ios::out | std::ios::binary);
    ofile << "progress Writing blobs...\n";
    for (size_t i = 0; i < fi_data.blobs.size(); i++) {
	write_blob(ofile, &fi_data.blobs[i], ifile);
	if ( !(i % 1000) ) {
	    ofile << "progress blob " << i << " of " << fi_data.blobs.size() << "\n";
	}
    }
    ofile << "progress Writing commits...\n";
    for (size_t i = 0; i < fi_data.commits.size(); i++) {
	write_commit(ofile, &fi_data.commits[i], ifile);
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
