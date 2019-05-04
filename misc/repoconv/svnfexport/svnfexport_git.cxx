std::vector<std::string> all_git_branches;

void
load_branches_list()
{
    struct stat buffer;
    if (stat("branches.txt", &buffer)) {
	all_git_branches.push_back(std::string("AUTOCONF"));
	all_git_branches.push_back(std::string("CMD"));
	all_git_branches.push_back(std::string("Original"));
	all_git_branches.push_back(std::string("STABLE"));
	all_git_branches.push_back(std::string("ansi-20040316-freeze"));
	all_git_branches.push_back(std::string("ansi-6-0-branch"));
	all_git_branches.push_back(std::string("ansi-branch"));
	all_git_branches.push_back(std::string("autoconf-20031202"));
	all_git_branches.push_back(std::string("autoconf-20031203"));
	all_git_branches.push_back(std::string("autoconf-branch"));
	all_git_branches.push_back(std::string("bobWinPort"));
	all_git_branches.push_back(std::string("bobWinPort-20051223-freeze"));
	all_git_branches.push_back(std::string("brlcad_5_1_alpha_patch"));
	all_git_branches.push_back(std::string("ctj-4-5-post"));
	all_git_branches.push_back(std::string("ctj-4-5-pre"));
	all_git_branches.push_back(std::string("hartley-6-0-post"));
	all_git_branches.push_back(std::string("help"));
	all_git_branches.push_back(std::string("import-1.1.1"));
	all_git_branches.push_back(std::string("itcl3-2"));
	all_git_branches.push_back(std::string("libpng_1_0_2"));
	all_git_branches.push_back(std::string("master"));
	all_git_branches.push_back(std::string("master-UNNAMED-BRANCH"));
	all_git_branches.push_back(std::string("merge-to-head-20051223"));
	all_git_branches.push_back(std::string("offsite-5-3-pre"));
	all_git_branches.push_back(std::string("opensource-pre"));
	all_git_branches.push_back(std::string("phong-branch"));
	all_git_branches.push_back(std::string("photonmap-branch"));
	all_git_branches.push_back(std::string("postmerge-autoconf"));
	all_git_branches.push_back(std::string("premerge-20040315-windows"));
	all_git_branches.push_back(std::string("premerge-20040404-ansi"));
	all_git_branches.push_back(std::string("premerge-20051223-bobWinPort"));
	all_git_branches.push_back(std::string("premerge-autoconf"));
	all_git_branches.push_back(std::string("rel-2-0"));
	all_git_branches.push_back(std::string("rel-5-0"));
	all_git_branches.push_back(std::string("rel-5-0-beta"));
	all_git_branches.push_back(std::string("rel-5-0beta"));
	all_git_branches.push_back(std::string("rel-5-1"));
	all_git_branches.push_back(std::string("rel-5-1-branch"));
	all_git_branches.push_back(std::string("rel-5-1-patches"));
	all_git_branches.push_back(std::string("rel-5-2"));
	all_git_branches.push_back(std::string("rel-5-3"));
	all_git_branches.push_back(std::string("rel-5-4"));
	all_git_branches.push_back(std::string("rel-6-0"));
	all_git_branches.push_back(std::string("rel-6-0-1"));
	all_git_branches.push_back(std::string("rel-6-0-1-branch"));
	all_git_branches.push_back(std::string("rel-6-1-DP"));
	all_git_branches.push_back(std::string("rel-7-0"));
	all_git_branches.push_back(std::string("rel-7-0-1"));
	all_git_branches.push_back(std::string("rel-7-0-2"));
	all_git_branches.push_back(std::string("rel-7-0-4"));
	all_git_branches.push_back(std::string("rel-7-0-branch"));
	all_git_branches.push_back(std::string("rel-7-10-0"));
	all_git_branches.push_back(std::string("rel-7-10-2"));
	all_git_branches.push_back(std::string("rel-7-2-0"));
	all_git_branches.push_back(std::string("rel-7-2-2"));
	all_git_branches.push_back(std::string("rel-7-2-4"));
	all_git_branches.push_back(std::string("rel-7-2-6"));
	all_git_branches.push_back(std::string("rel-7-4-0"));
	all_git_branches.push_back(std::string("rel-7-4-branch"));
	all_git_branches.push_back(std::string("rel-7-6-0"));
	all_git_branches.push_back(std::string("rel-7-6-4"));
	all_git_branches.push_back(std::string("rel-7-6-6"));
	all_git_branches.push_back(std::string("rel-7-6-branch"));
	all_git_branches.push_back(std::string("rel-7-8-0"));
	all_git_branches.push_back(std::string("rel-7-8-2"));
	all_git_branches.push_back(std::string("rel-7-8-4"));
	all_git_branches.push_back(std::string("release-7-0"));
	all_git_branches.push_back(std::string("stable-branch"));
	all_git_branches.push_back(std::string("tcl8-3"));
	all_git_branches.push_back(std::string("temp_tag"));
	all_git_branches.push_back(std::string("tk8-3"));
	all_git_branches.push_back(std::string("trimnurbs-branch"));
	all_git_branches.push_back(std::string("windows-20040315-freeze"));
	all_git_branches.push_back(std::string("windows-6-0-branch"));
	all_git_branches.push_back(std::string("windows-branch"));
	all_git_branches.push_back(std::string("zlib_1_0_4"));
	return;
    }

    std::ifstream infile("branches.txt");
    if (!infile.good()) exit(1);
    std::string line;
    while (std::getline(infile, line)) {
	if (line.length() > 1) {
	    all_git_branches.push_back(line);
	}
    }
}

void
write_branches_list()
{
    std::vector<std::string>::iterator b_it;
    remove("branches.txt");
    std::ofstream outfile("branches.txt", std::ios::out | std::ios::binary);
    if (!outfile.good()) exit(-1);
    for (b_it = all_git_branches.begin(); b_it != all_git_branches.end(); b_it++) {
	outfile << *b_it << "\n";
    }
    outfile.close();
}


void read_cached_rev_sha1s()
{
    std::ifstream infile("rev_gsha1s.txt");
    if (!infile.good()) return;
    std::string line;
    while (std::getline(infile, line)) {
	size_t spos = line.find_first_of(",");
	std::string branch = line.substr(0, spos);
	line = line.substr(spos + 1, std::string::npos);
	spos = line.find_first_of(",");
	std::string rev_str = line.substr(0, spos);
	std::string gsha1 = line.substr(spos+1, std::string::npos);
	long int rev = std::stol(rev_str);
	rev_to_gsha1[std::pair<std::string,long int>(branch, rev)] = gsha1;
	//std::cout << branch << "," << rev << " -> " << gsha1 << "\n";
    }
}

void write_cached_rev_sha1s()
{
    std::map<std::pair<std::string,long int>, std::string>::iterator r_it;
    remove("rev_gsha1s.txt");
    std::ofstream outfile("rev_gsha1s.txt", std::ios::out | std::ios::binary);
    if (!outfile.good()) exit(-1);
    for (r_it = rev_to_gsha1.begin(); r_it != rev_to_gsha1.end(); r_it++) {
	outfile << r_it->first.first << "," << r_it->first.second << "," << r_it->second << "\n";
    }
    outfile.close();
}

void get_rev_sha1s(long int rev)
{
    std::vector<std::string>::iterator b_it;

    for (b_it = all_git_branches.begin(); b_it != all_git_branches.end(); b_it++) {
	std::string line;
	std::string branch = *b_it;
	std::string git_sha1_cmd = std::string("cd cvs_git && git show-ref ") + branch + std::string(" > ../sha1.txt && cd ..");
	if (std::system(git_sha1_cmd.c_str())) {
	    std::cout << "git_sha1_cmd failed: " << branch << "\n";
	    exit(1);
	}
	std::ifstream hfile("sha1.txt");
	if (!hfile.good()) continue;
	std::getline(hfile, line);
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	if (hsha1.length()) {
	    rev_to_gsha1[std::pair<std::string,long int>(branch, rev)] = hsha1;
	    //std::cout << branch << "," << rev << " -> " << rev_to_gsha1[std::pair<std::string,long int>(branch, rev)] << "\n";
	}
	hfile.close();
	remove("sha1.txt");
    }
    write_cached_rev_sha1s();
}


std::string git_sha1(std::ifstream &infile, struct svn_node *n)
{
    std::string git_sha1;
    std::string go_buff;
    char *cbuffer = new char [n->text_content_length];;
    infile.read(cbuffer, n->text_content_length);
    if (n->text_content_sha1 == std::string("71fc76a4374348f844c480a375d594cd10835ab9")) {
	std::cout << "working " << n->path << ", rev " << n->revision_number << "\n";
    }
    go_buff.append("blob ");

    if (!n->crlf_content) {
	go_buff.append(std::to_string(n->text_content_length));
	go_buff.append(1, '\0');
	go_buff.append(cbuffer, n->text_content_length);
	git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
    } else {
	std::string crlf_buff;
	std::ostringstream cf(crlf_buff);
	for (int i = 0; i < n->text_content_length; i++) {
	    if (cbuffer[i] == '\n' && (i == 0 || cbuffer[i-1] != '\r')) {
		cf << '\r' << '\n';
	    } else {
		cf << cbuffer[i];
	    }
	}
	std::string crlf_file = cf.str();
	go_buff.append(std::to_string(crlf_file.length()));
	go_buff.append(1, '\0');
	go_buff.append(crlf_file.c_str(), crlf_file.length());
	git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
    }
    delete[] cbuffer;
    return git_sha1;
}

std::string rgsha1(std::string &rbranch, long int rev)
{
    std::pair<std::string,long int> key = std::pair<std::string,long int>(rbranch, rev);
    std::string gsha1 = rev_to_gsha1[key];
    return gsha1;
}


//Need to write blobs with CRLF if that's the mode we're in...
void
write_blob(std::ifstream &infile, std::ofstream &outfile, struct svn_node &node)
{
    char *buffer = new char [node.text_content_length];
    infile.seekg(node.content_start);
    infile.read(buffer, node.text_content_length);
    outfile << "blob\n";
    //std::cout << "	Adding node object for " << node.local_path << "\n";
    if (!node.crlf_content) {
	outfile << "data " << node.text_content_length << "\n";
	outfile.write(buffer, node.text_content_length);
    } else {
	std::string crlf_buff;
	std::ostringstream cf(crlf_buff);
	for (int i = 0; i < node.text_content_length; i++) {
	    if (buffer[i] == '\n' && (i == 0 || buffer[i-1] != '\r')) {
		cf << '\r' << '\n';
	    } else {
		cf << buffer[i];
	    }
	}
	std::string crlf_file = cf.str();
	outfile << "data " << crlf_file.length() << "\n";
	outfile.write(crlf_file.c_str(), crlf_file.length());
    }
    delete[] buffer;
}

void full_sync_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &bsrc, std::string &bdest)
{
    outfile << "commit " << bdest << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " <<  rev_to_gsha1[std::pair<std::string,long int>(bsrc, rev.revision_number-1)] << "\n";
    outfile << "merge " << "\n";
    outfile << "deleteall\n";

    std::string line;
    std::string ifile = std::to_string(rev.revision_number) + std::string(".txt");
    std::ifstream infile(ifile);
    if (!infile.good()) {
	std::cerr << "Fatal error: could not open file " << ifile << " for special commit handling, exiting\n";
	exit(1);
    }
    while (std::getline(infile, line)) {
	outfile << line << "\n";
    }
    outfile << "\n";
}

void move_only_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    // first, check if we truly have path changes between the copyfrom and dest local paths
    int have_real_rename = 0;
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if ((mlocal_path != node.local_path) && (cbranch == node.branch)) {
		have_real_rename = 1;
	    }
	}
    }

    if (!have_real_rename) {
	return;
    }

    std::string ncmsg = rev.commit_msg + std::string("(preliminiary file move commit)");

    std::string mvmark = std::string("1111") + std::to_string(rev.revision_number);

    outfile << "commit refs/heads/" << rbranch << "\n";
    outfile << "mark :" << mvmark << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << ncmsg.length() << "\n";
    outfile << ncmsg << "\n";
    outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(rbranch, rev.revision_number-1)] << "\n";

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << rev_to_gsha1[std::pair<std::string,long int>(rev.merged_from, rev.merged_rev)] << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge " << rgsha1(rev.merged_from, rev.merged_rev) << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	/* Don't add directory nodes themselves - git works on files */
	if (node.kind == ndir) continue;
	if (node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if (mlocal_path != node.local_path) {
		std::cout << "(r" << rev.revision_number << ") - renaming " << mlocal_path << " to " << node.local_path << "\n";
		outfile << "R " << mlocal_path << " " << node.local_path << "\n";
	    }
	}
    }

}

void write_git_node(std::ofstream &outfile, struct svn_revision &rev, struct svn_node &node)
{
    /* Don't add directory nodes themselves - git works on files.
     *
     * However, there is an important exception here for directory copies and deletes!
     * */
    if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length()) {
	int is_tag;
	std::string mproject, cbranch, mlocal_path, ctag;
	node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	if (mlocal_path != node.local_path) {
	    std::cout << "(r" << rev.revision_number << ") - renaming " << mlocal_path << " to " << node.local_path << "\n";
	    outfile << "C " << mlocal_path << " " << node.local_path << "\n";
	}
	return;
    }
    if (node.kind == ndir && node.action == nadd) return;

    // If it's a straight-up path delete, do it and return
    if ((node.kind == nkerr || node.kind == ndir) && node.action == ndelete) {
	std::cerr << "Revision r" << rev.revision_number << " delete: " << node.local_path << "\n";
	outfile << "D \"" << node.local_path << "\"\n";
	return;
    }

    //std::cout << "	Processing node " << print_node_action(node.action) << " " << node.local_path << " into branch " << node.branch << "\n";
    std::string gsha1;
    if (node.action == nchange || node.action == nadd || node.action == nreplace) {
	if (node.exec_change || node.copyfrom_path.length() || node.text_content_length || node.text_content_sha1.length()) {
	    // TODO - if we have both of these, shouldn't the node be a move+edit?  confirm this, doesn't look like logic is doing that for r30495
	    if (node.text_copy_source_sha1.length() && !node.text_content_sha1.length()) {
		gsha1 = svn_sha1_to_git_sha1[node.text_copy_source_sha1];
	    } else {
		gsha1 = svn_sha1_to_git_sha1[current_sha1[node.path]];
	    }
	    if (gsha1.length() < 40) {
		std::string tpath;
		if (node.copyfrom_path.length()) {
		    tpath = node.copyfrom_path;
		} else {
		    tpath = std::string("brlcad/trunk/") + node.local_path;
		}
		gsha1 = svn_sha1_to_git_sha1[current_sha1[tpath]];
		if (gsha1.length() < 40) {
		    std::cout << "Fatal - could not find git sha1 - r" << rev.revision_number << ", node: " << node.path << "\n";
		    std::cout << "current sha1: " << current_sha1[node.path] << "\n";
		    std::cout << "trunk sha1: " << current_sha1[tpath] << "\n";
		    std::cout << "Revision merged from: " << rev.merged_from << "\n";
		    print_node(node);
		    exit(1);
		} else {
		    std::cout << "Warning(r" << rev.revision_number << ") - couldn't find SHA1 for " << node.path << ", using node from " << tpath << "\n";
		}
	    }
	} else {
	    std::cout << "Warning(r" << rev.revision_number << ") - skipping " << node.path << " - no git applicable change found.\n";
	    return;
	}
    }

    switch (node.action) {
	case nchange:
	    outfile << "M ";
	    break;
	case nadd:
	    outfile << "M ";
	    break;
	case nreplace:
	    outfile << "M ";
	    break;
	case ndelete:
	    outfile << "D ";
	    break;
	default:
	    std::cerr << "Unhandled node action type " << print_node_action(node.action) << "\n";
	    outfile << "? ";
	    outfile << "\"" << node.local_path << "\"\n";
	    std::cout << "Fatal - unhandled node action at r" << rev.revision_number << ", node: " << node.path << "\n";
	    exit(1);
    }

    if (node.action != ndelete) {
	if (node.exec_path) {
	    outfile << "100755 ";
	} else {
	    outfile << "100644 ";
	}
    }

    if (node.action == nchange || node.action == nadd || node.action == nreplace) {
	outfile << gsha1 << " \"" << node.local_path << "\"\n";
	return;
    }

    if (node.action == ndelete) {
	outfile << "\"" << node.local_path << "\"\n";
	return;
    }

    std::cout << "Error(r" << rev.revision_number << ") - unhandled node action: " << print_node_action(node.action) << "\n";
    print_node(node);
    exit(1);
}

void old_ref_process_dir(std::ofstream &outfile, struct svn_revision &rev, struct svn_node *node)
{
    std::cout << "Non-standard DIR handling needed: " << node->path << "\n";
    std::string ppath, bbpath, llpath, tpath;
    int is_tag;
    node_path_split(node->copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);
    std::set<struct svn_node *>::iterator n_it;
    std::set<struct svn_node *> *node_set = path_states[node->copyfrom_path][node->copyfrom_rev];
    // TODO - if this is messed up, write out a shell .fi file for manual tweaking and exit
    if (node_set) {
    for (n_it = node_set->begin(); n_it != node_set->end(); n_it++) {
	if ((*n_it)->kind == ndir) {
	    std::cout << "Stashed dir node: " << (*n_it)->path << "\n";
	    old_ref_process_dir(outfile, rev, *n_it);
	}
	std::cout << "Stashed file node: " << (*n_it)->path << "\n";
	std::string rp = path_subpath(llpath, (*n_it)->local_path);
	std::string gsha1 = svn_sha1_to_git_sha1[(*n_it)->text_content_sha1];
	std::string exestr = ((*n_it)->exec_path) ? std::string("100755 ") : std::string("100644 ");
	outfile << "M " << exestr << gsha1 << " \"" << node->local_path << rp << "\"\n";
    }
    }
}

void old_references_commit(std::ifstream &infile, std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    // If we need to write any blobs, take care of it now - old_references_commit short-circuits the "main" logic that handles this
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.text_content_length > 0) {
	    write_blob(infile, outfile, node);
	}
    }

    outfile << "commit refs/heads/" << rbranch << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(rbranch, rev.revision_number-1)] << "\n";

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << rev_to_gsha1[std::pair<std::string,long int>(rev.merged_from, rev.merged_rev)] << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge " << rgsha1(rev.merged_from, rev.merged_rev) << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }

    brlcad_revs.insert(rev.revision_number);

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];

	if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length() && node.copyfrom_rev < rev.revision_number - 1) {
	    old_ref_process_dir(outfile, rev, &node);
	    continue;
	}

	// Not one of the special cases, handle normally
	write_git_node(outfile, rev, node);

    }
}

void standard_commit(std::ofstream &outfile, struct svn_revision &rev, std::string &rbranch)
{
    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    brlcad_revs.insert(rev.revision_number);

    outfile << "commit refs/heads/" << rbranch << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    outfile << "data " << rev.commit_msg.length() << "\n";
    outfile << rev.commit_msg << "\n";
    outfile << "from " << rgsha1(rbranch, rev.revision_number - 1) << "\n";

    if (rev.merged_from.length()) {
	if (brlcad_revs.find(rev.merged_rev) != brlcad_revs.end()) {
	    std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << rev_to_gsha1[std::pair<std::string,long int>(rev.merged_from, rev.merged_rev)] << "\n";
	    std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	    outfile << "merge " << rgsha1(rev.merged_from, rev.merged_rev) << "\n";
	} else {
	    std::cout << "Warning: r" << rev.revision_number << " is referencing a commit id (" << rev.merged_rev << ") that is not a BRL-CAD commit\n";
	}
    }


    // Check for directory deletes and copyfrom in the same node set.  If this happens,
    // we need to defer the delete of the old dir until after the copy is made.  This is
    // related to the problem of pulling old dir contents, but if we're in this function
    // the assumption is that the working dir's contents are what we need here.
    std::set<struct svn_node *> deferred_deletes;
    std::set<struct svn_node *> deletes;
    std::set<struct svn_node *>::iterator d_it;
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.action == ndelete) {
	    deletes.insert(&node);
	}
    }
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.kind != nfile && node.copyfrom_path.length()) {
	    for (d_it = deletes.begin(); d_it != deletes.end(); d_it++) {
		if (node.copyfrom_path == (*d_it)->path) {
		    std::cout << "Deferring delete of " << node.copyfrom_path << "\n";
		    deferred_deletes.insert(*d_it);
		}
	    }
	}
    }

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.action != ndelete || deferred_deletes.find(&node) == deferred_deletes.end()) {
	    write_git_node(outfile, rev, node);
	}
    }

    for (d_it = deferred_deletes.begin(); d_it != deferred_deletes.end(); d_it++) {
	write_git_node(outfile, rev, **d_it);
    }

}

void note_svn_rev(struct svn_revision &rev, std::string &rbranch)
{
    std::string fi_file = std::to_string(rev.revision_number) + std::string("-note.fi");
    std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);

    std::string svn_id_str = std::string("svn:revision:") + std::to_string(rev.revision_number);

    outfile << "blob" << "\n";
    outfile << "mark :1" << "\n";
    outfile << "data " << svn_id_str.length() << "\n";
    outfile << svn_id_str << "\n";
    outfile << "\n";
    outfile << "commit refs/notes/commits" << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";

    std::string svn_id_commit_msg = std::string("Note SVN revision ") + std::to_string(rev.revision_number);
    outfile << "data " << svn_id_commit_msg.length() << "\n";
    outfile << svn_id_commit_msg << "\n";


    std::string git_sha1_cmd = std::string("cd cvs_git && git show-ref refs/notes/commits > ../nsha1.txt && cd ..");
    if (std::system(git_sha1_cmd.c_str())) {
	std::cout << "git_sha1_cmd failed: refs/notes/commits\n";
	exit(1);
    }
    std::ifstream hfile("nsha1.txt");
    if (!hfile.good()) {
	std::cout << "couldn't open nsha1.txt\n";
	exit(1);
    }
    std::string line;
    std::getline(hfile, line);
    size_t spos = line.find_first_of(" ");
    std::string nsha1 = line.substr(0, spos);

    outfile << "from " << nsha1 << "\n";
    outfile << "N :1 " << rgsha1(rbranch, rev.revision_number) << "\n";
    outfile.close();

    std::string git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    std::string git_fi2 = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    if (std::system(git_fi.c_str())) {
	std::string failed_file = std::string("failed-") + fi_file;
	std::cout << "Fatal - could not apply note fi file for revision " << rev.revision_number << "\n";
	rename(fi_file.c_str(), failed_file.c_str());
	exit(1);
    }
    if (std::system(git_fi2.c_str())) {
	std::string failed_file = std::string("failed-") + fi_file;
	std::cout << "Fatal - could not apply note fi file for revision " << rev.revision_number << "\n";
	rename(fi_file.c_str(), failed_file.c_str());
	exit(1);
    }

}


void rev_fast_export(std::ifstream &infile, long int rev_num)
{
    struct stat buffer;
    std::string fi_file = std::string("custom/") + std::to_string(rev_num) + std::string(".fi");


    if (rev_num == 29882) {
	std::cout << "at 29882\n";
    }

    struct svn_revision &rev = revs.find(rev_num)->second;


    if (rev.project != std::string("brlcad")) {
	//std::cout << "Revision " << rev.revision_number << " is not part of brlcad, skipping\n";
	return;
    }

    // If we have a text content sha1, update the map
    // to the current path state
    if (rev.nodes.size() > 0) {
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    if (node.text_content_sha1.length()) {
		current_sha1[node.path] = node.text_content_sha1;
	    }
	}
    }
    std::string rbranch = rev.nodes[0].branch;


    if (stat(fi_file.c_str(), &buffer) == 0) {
	// If we have a hand-crafted import file for this revision, use it
	std::string git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	if (std::system(git_fi.c_str())) {
	    std::string failed_file = std::string("failed-") + fi_file;
	    std::cout << "Fatal - could not apply fi file for revision " << rev_num << "\n";
	    rename(fi_file.c_str(), failed_file.c_str());
	    exit(1);
	}

	verify_repos(rev.revision_number, rbranch, rbranch, 1);

	if (rev_num == 36053) {
	    all_git_branches.push_back(std::string("rel8"));
	}


	if (rev_num == 64060) {
	    all_git_branches.push_back(std::string("eab"));
	}


	git_fi = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	if (std::system(git_fi.c_str())) {
	    std::string failed_file = std::string("failed-") + fi_file;
	    std::cout << "Fatal - could not apply fi file for revision " << rev_num << "\n";
	    rename(fi_file.c_str(), failed_file.c_str());
	    exit(1);
	}

	return;
    }

    fi_file = std::to_string(rev_num) + std::string(".fi");

    std::set<int> special_revs;
    std::string empty_sha1("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    std::map<long int, struct svn_revision>::iterator r_it;


    // particular revisions that will have to be special cased:
    special_revs.insert(32314);
    special_revs.insert(36633);
    special_revs.insert(36843);
    special_revs.insert(39465);


    if (rev.revision_number == 30760) {
	std::cout << "Skipping r30760 - premature tagging of rel-7-12-2.  Will be tagged by 30792\n";
	return;
    }

    if (rev.revision_number == 36472) {
	std::cout << "Skipping r36472 - branch rename, handled by mapping the original name in the original commit (dmtogl-branch) to the desired branch name (dmtogl)\n";
	return;
    }

#if 0
    std::cout << "Processing revision " << rev.revision_number << "\n";
    if (rev.merged_from.length()) {
	std::cout << "Note: merged from " << rev.merged_from << "\n";
    }
#endif
    int git_changes = 0;
    int have_commit = 0;
    int tag_after_commit = 0;
    int branch_delete = 0;
    std::string ctag, cfrom;

    if (reject_tag(rev.nodes[0].tag)) {
	std::cout << "Skipping " << rev.revision_number << " - edit to old tag:\n" << rev.commit_msg << "\n";
	return;
    }

    if (special_revs.find(rev.revision_number) != special_revs.end()) {
	std::string bsrc, bdest;
	brlcad_revs.insert(rev.revision_number);
	std::cout << "Revision " << rev.revision_number << " requires special handling\n";
	if (rev.revision_number == 36633 || rev.revision_number == 39465) {
	    bdest = std::string("refs/heads/dmtogl");
	    bsrc = rev_to_gsha1[std::pair<std::string,long int>(std::string("master"), rev.revision_number-1)];
	} else {
	    bdest = std::string("refs/heads/STABLE");
	}
	if (rev.revision_number == 31039) {
	    bsrc = tag_ids[std::string("rel-7-12-2")];
	}
	if (rev.revision_number == 32314 || rev.revision_number == 36843) {
	    bsrc = tag_ids[std::string("rel-7-12-6")];
	}

	std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);
	full_sync_commit(outfile, rev, bsrc, bdest);
	outfile.close();

	if (rev.revision_number == 36633 || rev.revision_number == 39465) {

	    verify_repos(rev.revision_number, std::string("dmtogl"), std::string("dmtogl"), 0);

	} else {

	    verify_repos(rev.revision_number, std::string("STABLE"), std::string("STABLE"), 0);
	}

	return;
    }

    if (rebuild_revs.find(rev.revision_number) != rebuild_revs.end()) {
	std::cout << "Revision " << rev.revision_number << " references non-current SVN info, needs special handling\n";
	std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);
	old_references_commit(infile, outfile, rev, rbranch);
	outfile.close();

	verify_repos(rev.revision_number, rbranch, rbranch, 0);

	return;
    }

    std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];

	if (node.tag_add || node.tag_edit) {
	    std::cout << "Processing revision " << rev.revision_number << "\n";
	    std::string ppath, bbpath, llpath, tpath;
	    int is_tag;
	    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);

	    int edited_tag_minr = -1;
	    int edited_tag_maxr = -1;

	    // For edited tags - first tag create branch, final tag is "real" tag,
	    // else just branch commits
	    if (edited_tags.find(node.tag) != edited_tags.end()) {
		edited_tag_minr = edited_tag_first_rev[node.tag];
		edited_tag_maxr = edited_tag_max_rev[node.tag];
		std::cout << node.tag << ": " << edited_tag_minr << " " << edited_tag_maxr << "\n";
		brlcad_revs.insert(rev.revision_number);

		if (rev.revision_number < edited_tag_minr) {
		    std::string nbranch = std::string("refs/heads/") + node.tag;
		    std::cout << "Adding tag branch " << nbranch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
		    std::cout << rev.commit_msg << "\n";
		    outfile << "reset " << nbranch << "\n";
		    outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(bbpath, rev.revision_number-1)] << "\n";
		    continue;
		}
		if (rev.revision_number == edited_tag_maxr) {
		    tag_after_commit = 1;
		    rbranch = node.branch;
		    ctag = node.tag;
		    cfrom = node.branch;
		}
		std::cout << "Non-final tag edit, processing normally: " << node.branch << ", r" << rev.revision_number<< "\n";
		rbranch = node.branch;
		//git_changes = 1;
	    } else {
		brlcad_revs.insert(rev.revision_number);
		std::cout << "Adding tag " << node.tag << " from " << bbpath << ", r" << rev.revision_number << "\n";
		have_commit = 1;
		outfile << "tag " << node.tag << "\n";
		outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(bbpath, rev.revision_number-1)] << "\n";
		outfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
		outfile << "data " << rev.commit_msg.length() << "\n";
		outfile << rev.commit_msg << "\n";
		tag_ids[node.tag] = rev_to_gsha1[std::pair<std::string,long int>(bbpath, rev.revision_number-1)];
		continue;
	    }

	}

	if (node.tag_delete) {
	    branch_delete = 1;
	    std::cout << "processing revision " << rev.revision_number << "\n";
	    std::cout << "TODO - delete tag: " << node.tag << "\n";
	    continue;
	}

	if (node.branch_add) {
	    brlcad_revs.insert(rev.revision_number);
	    std::cout << "Processing revision " << rev.revision_number << "\n";
	    std::string ppath, bbpath, llpath, tpath;
	    int is_tag;
	    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);
	    std::cout << "Adding branch " << node.branch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
	    std::cout << rev.commit_msg << "\n";
	    outfile << "reset refs/heads/" << node.branch << "\n";
	    outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(bbpath, rev.revision_number-1)] << "\n";
	    outfile.close();
	    have_commit = 1;
	    all_git_branches.push_back(node.branch);
	    std::string git_fi;
	    git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	    if (std::system(git_fi.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file.c_str(), failed_file.c_str());
		exit(1);
	    }
	    git_fi = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	   if (std::system(git_fi.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file.c_str(), failed_file.c_str());
		exit(1);
	    }
	    get_rev_sha1s(rev_num);

	    // Make an empty commit on the new branch with the commit message from SVN, but no changes
	    std::string fi_file2 = std::to_string(rev_num) + std::string("-b.fi");
	    std::ofstream outfile2(fi_file2.c_str(), std::ios::out | std::ios::binary);
	    outfile2 << "commit refs/heads/" << node.branch << "\n";
	    outfile2 << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
	    outfile2 << "data " << rev.commit_msg.length() << "\n";
	    outfile2 << rev.commit_msg << "\n";
	    outfile2 << "from " << rev_to_gsha1[std::pair<std::string,long int>(node.branch, rev.revision_number)] << "\n";
	    outfile2.close();
	    std::string git_fi2 = std::string("cd cvs_git_working && cat ../") + fi_file2 + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	    if (std::system(git_fi2.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file2;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file2.c_str(), failed_file.c_str());
		exit(1);
	    }

	    verify_repos(rev.revision_number, node.branch, node.branch, 1);

	    git_fi2 = std::string("cd cvs_git && cat ../") + fi_file2 + std::string(" | git fast-import && git reset --hard HEAD && cd ..");

	    if (std::system(git_fi2.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file2;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file2.c_str(), failed_file.c_str());
		exit(1);
	    }
	    continue;
	}

	if (node.branch_delete) {
	    //std::cout << "processing revision " << rev.revision_number << "\n";
	    std::cout << "Delete branch instruction: " << node.branch << " - deferring.\n";
	    branch_delete = 1;
	    continue;
	}


	if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length()) {
	    git_changes = 1;
	}

	if (node.text_content_length > 0) {
	    //std::cout << "	Adding node object for " << node.local_path << "\n";
	    write_blob(infile, outfile, node);
	    git_changes = 1;
	}
	if (node.text_content_sha1.length()) {
	    git_changes = 1;
	}
	if (node.text_copy_source_sha1.length()) {
	    git_changes = 1;
	}
	if (node.exec_change) {
	    git_changes = 1;
	}
	if (node.action == ndelete) {
	    git_changes = 1;
	}
    }

    if (git_changes) {
	if (have_commit) {
	    std::cout << "Error - more than one commit generated for revision " << rev.revision_number << "\n";
	}

	if (rev.move_edit) {
	    move_only_commit(outfile, rev, rbranch);
	}

	standard_commit(outfile, rev, rbranch);

	if (tag_after_commit) {
	    // Note - in this situation, we need to both build a commit and do a tag.  Will probably
	    // take some refactoring.  Merge information will also be a factor.
	    std::cout << "Adding tag after final tag branch commit: " << ctag << " from " << cfrom << ", r" << rev.revision_number << "\n";
	    outfile << "tag " << ctag << "\n";
	    outfile << "from " << rev_to_gsha1[std::pair<std::string,long int>(cfrom, rev.revision_number-1)] << "\n";
	    outfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
	    outfile << "data " << rev.commit_msg.length() << "\n";
	    outfile << rev.commit_msg << "\n";
	}


	outfile.close();
	verify_repos(rev.revision_number, rbranch, rbranch, 0);
#if 0
	if (rev.revision_number % 10 == 0 || rbranch != std::string("master")) {
	} else {
	    // Not verifying this one - just apply the fast import file and go
	    std::string git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	    std::string git_fi2 = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	    if (std::system(git_fi.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file.c_str(), failed_file.c_str());
		exit(1);
	    }
	    if (std::system(git_fi2.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file;
		std::cout << "Fatal - could not apply fi file for revision " << rev.revision_number << "\n";
		rename(fi_file.c_str(), failed_file.c_str());
		exit(1);
	    }
	}
#endif

    } else {
	outfile.close();
	if (!branch_delete && !have_commit) {
	    std::cout << "Warning(r" << rev.revision_number << ") - skipping SVN commit, no git applicable changes found\n";
	    std::cout << rev.commit_msg << "\n";
	}
	remove(fi_file.c_str());
    }

}


void
load_branch_head_sha1s()
{
    std::string git_sha1_heads_cmd = std::string("rm -f brlcad_cvs_git_heads_sha1.txt && cd cvs_git && git show-ref --heads --tags > ../brlcad_cvs_git_heads_sha1.txt && cd ..");
    if (std::system(git_sha1_heads_cmd.c_str())) {
	std::cerr << "git_sha1_heads_cmd failed\n";
	exit(1);
    }
    std::ifstream hfile("brlcad_cvs_git_heads_sha1.txt");
    if (!hfile.good()) exit(-1);
    std::string line;
    while (std::getline(hfile, line)) {
	std::string prefix = std::string("refs/heads/");
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	std::string hbranch = line.substr(spos+1, std::string::npos);
	std::string branch = hbranch.replace(0, prefix.length(), "");
	rev_to_gsha1[std::pair<std::string,long int>(branch, 29886)] = hsha1;
    }
    hfile.close();
    remove("brlcad_cvs_git_heads_sha1.txt");
}

void
load_blob_sha1s()
{
    std::string git_sha1_heads_cmd = std::string("rm -f brlcad_cvs_git_all_blob_sha1.txt && cd cvs_git && git rev-list --objects --all | git cat-file --batch-check='%(objectname) %(objecttype) %(rest)' | grep '^[^ ]* blob' | cut -d\" \" -f1,3- > ../brlcad_cvs_git_all_blob_sha1.txt && echo \"e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 src/other/tkhtml3/AUTHORS\n\" >> ../brlcad_cvs_git_all_blob_sha1.txt && cd ..");
    if (std::system(git_sha1_heads_cmd.c_str())) {
	std::cout << "git load blobs failed\n";
	exit(1);
    }
    std::ifstream bfile("brlcad_cvs_git_all_blob_sha1.txt");
    if (!bfile.good()) exit(-1);
    std::string line;
    while (std::getline(bfile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string hsha1 = line.substr(0, spos);
	cvs_blob_sha1.insert(hsha1);
    }
    bfile.close();
    remove("brlcad_cvs_git_all_blob_sha1.txt");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
