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
	all_git_branches.push_back(std::string("cjohnson-mac-hack"));
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


void read_cached_rev_sha1s(int startrev)
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
	if (rev <= startrev)
	    rev_to_gsha1[std::pair<std::string,long int>(branch, rev)] = gsha1;
	//std::cout << branch << "," << rev << " -> " << gsha1 << "\n";
    }
}

void write_cached_rev_sha1s()
{
    std::map<std::pair<std::string,long int>, std::string>::iterator r_it;
    std::ofstream outfile("rev_gsha1s_tmp.txt", std::ios::out | std::ios::binary);
    if (!outfile.good()) exit(-1);
    for (r_it = rev_to_gsha1.begin(); r_it != rev_to_gsha1.end(); r_it++) {
	std::string s1 = r_it->first.first;
	std::string s2 = std::to_string(r_it->first.second);
	std::string s3 = r_it->second;
	if (!s1.length() || !s2.length() || !s3.length()) {
	    std::cerr << "Invalid rev sha1 entry: " << r_it->first.first << "," << r_it->first.second << "," << r_it->second << "\n";
	    exit(1);
	}
	outfile << r_it->first.first << "," << r_it->first.second << "," << r_it->second << "\n";
    }
    outfile.close();
    remove("rev_gsha1s.txt");
    rename("rev_gsha1s_tmp.txt", "rev_gsha1s.txt");
}

void generate_svn_tree(const char *svn_repo, const char *branch, long int rev)
{
    std::string tree_cmd;
    std::string prep_cmd;

    if (std::string(branch) == std::string("master")) {
	prep_cmd = std::string("rm -f custom/") + std::to_string(rev) + std::string("-tree.fi && rm -rf trunk-") + std::to_string(rev);
    } else {
	prep_cmd = std::string("rm -f custom/") + std::to_string(rev) + std::string("-tree.fi && rm -rf ") + std::string(branch) + std::string("-") + std::to_string(rev);
    }
    if (std::system(prep_cmd.c_str())) {
    	std::cerr << "Error running tree generation prep command: " << prep_cmd << "\n";
	exit(1);
    }
    if (std::string(branch) == std::string("master")) {
	tree_cmd = std::string("./sync_commit_trunk.sh ") + std::string(svn_repo) + std::string(" ") + std::to_string(rev);
    } else {
	tree_cmd = std::string("./sync_commit.sh ") + std::string(svn_repo) + std::string(" ") + std::string(branch) + std::string(" ") + std::to_string(rev);
    }
    if (std::system(tree_cmd.c_str())) {
	std::cerr << "Error running tree command: " << tree_cmd << "\n";
	exit(1);
    }
}


int apply_fi_file_working(std::string &fi_file, std::string &rbranch, long int rev, int is_tag, int do_verify, int try_recovery) {
    std::string git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    if (std::system(git_fi.c_str())) {
	std::string failed_file = std::string("failed-") + fi_file;
	std::cout << "Fatal - could not apply fi file to working repo " << fi_file << "\n";
	rename(fi_file.c_str(), failed_file.c_str());
	if (!try_recovery) {
	    exit(1);
	}
	return 1;
    }
    // TODO - should check tags as well...
    if (do_verify && !is_tag)
	return verify_repos(rev, rbranch);
    return 0;
}

void apply_fi_file(std::string &fi_file) {
    std::string git_fi = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    if (std::system(git_fi.c_str())) {
	std::cout << "Fatal - could not apply fi file to working repo " << fi_file << "\n";
	exit(1);
    }
}

// Update the sha1 for the branch from the cvs_git_working repo - used when we
// haven't done the final cvs_git commit but still need a current sha1 for
// reference
std::string get_rev_sha1(std::string branch, long int rev)
{
    std::string line;
    std::string git_sha1_cmd = std::string("cd cvs_git_working && git show-ref ") + branch + std::string(" > ../sha1.txt && cd ..");
    if (std::system(git_sha1_cmd.c_str())) {
	std::cout << "git_sha1_cmd failed: " << branch << "\n";
	exit(1);
    }
    std::ifstream hfile("sha1.txt");
    if (!hfile.good()) return std::string("");
    std::getline(hfile, line);
    size_t spos = line.find_first_of(" ");
    std::string hsha1 = line.substr(0, spos);
    if (hsha1.length()) {
	rev_to_gsha1[std::pair<std::string,long int>(branch, rev)] = hsha1;
    } else {
	std::cerr << "get_rev_sha1: failed to get sha1 for branch " << branch << ", revision " << rev << "\n";
	exit(1);
    }
    hfile.close();
    remove("sha1.txt");
    write_cached_rev_sha1s();
    return hsha1;
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
	} else {
	    std::cerr << "get_rev_sha1s: failed to get sha1 for branch " << branch << ", revision " << rev << "\n";
	    exit(1);
	}
	hfile.close();
	remove("sha1.txt");
    }
    write_cached_rev_sha1s();
}

std::string rgsha1(std::string &rbranch, long int rev)
{
    std::pair<std::string,long int> key = std::pair<std::string,long int>(rbranch, rev);
    std::string gsha1 = rev_to_gsha1[key];
    return gsha1;
}

std::string
populate_template(std::string ifile, std::string &rbranch)
{
    std::string obuf;
    std::ifstream infile(ifile);
    if (!infile.good()) {
	std::cerr << "Could not open template file " << ifile << "\n";
	exit(1);
    }

    // Replace branch,rev patterns with sha1 lookups in from and merge lines
    std::regex fromline("^from ([0-9]+)$");
    std::regex bfromline("^from ([A-Za-z-_0-9-]+),([0-9]+)$");
    std::regex mergeline("^merge ([A-Za-z-_0-9-]+),([0-9]+)$");
    std::string tline;
    while (std::getline(infile, tline)) {

	std::cout << "Processing: " << tline << "\n";
	if (std::regex_match(tline, fromline)) {
	    std::smatch frevmatch;
	    if (!std::regex_search(tline, frevmatch, fromline)) {
		std::cerr << "Error, could not find revision number in from line:\n" << tline << "\n" ;
		exit(1);
	    } else {
		std::string newfline= std::string("from ") + rgsha1(rbranch, std::stol(frevmatch[1]));
		std::cout << newfline << "\n";
		obuf.append(newfline);
		obuf.append("\n");
	    }
	    continue;
	}
	if (std::regex_match(tline, bfromline)) {
	    std::smatch brevmatch;
	    if (!std::regex_search(tline, brevmatch, bfromline)) {
		std::cerr << "Error, could not find revision number in branch from line:\n" << tline << "\n" ;
		exit(1);
	    } else {
		std::string fbranch = std::string(brevmatch[1]);
		std::string newfline = std::string("from ") + rgsha1(fbranch, std::stol(brevmatch[2]));
		std::cout << newfline << "\n";
		obuf.append(newfline);
		obuf.append("\n");
	    }
	    continue;
	}
	if (std::regex_match(tline, mergeline)) {
	    std::smatch mrevmatch;
	    if (!std::regex_search(tline, mrevmatch, mergeline)) {
		std::cerr << "Error, could not find revision number in merge line:\n" << tline << "\n" ;
		exit(1);
	    } else {
		std::string mbranch = std::string(mrevmatch[1]);
		std::string newmline= std::string("merge ") + rgsha1(mbranch, std::stol(mrevmatch[2]));
		std::cout << newmline << "\n";
		obuf.append(newmline);
		obuf.append("\n");
	    }
	    continue;
	}
	obuf.append(tline);
	obuf.append("\n");
    }

    std::string ofilename = ifile + std::string("-expanded");
    std::ofstream ofile(ofilename);

    ofile << obuf;
    ofile.close();

    return ofilename;

}


std::string note_svn_rev(struct svn_revision &rev, std::string &rbranch)
{
    std::string fi_file = std::to_string(rev.revision_number) + std::string("-note.fi");
    std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);

    std::string svn_id_str = std::string("svn:revision:") + std::to_string(rev.revision_number) + std::string("\n") + std::string("svn:branch:") + rbranch;

    outfile << "blob" << "\n";
    outfile << "mark :1" << "\n";
    outfile << "data " << svn_id_str.length() << "\n";
    outfile << svn_id_str << "\n";
    outfile << "commit refs/notes/commits" << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";

    std::string svn_id_commit_msg = std::string("Note SVN revision and branch");
    outfile << "data " << svn_id_commit_msg.length() << "\n";
    outfile << svn_id_commit_msg << "\n";


    std::string git_sha1_cmd = std::string("cd cvs_git_working && git show-ref refs/notes/commits > ../nsha1.txt && cd ..");
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

    return fi_file;
}

void apply_working_branch_and_mvall(struct svn_revision &rev, std::string &rbranch) {
    struct stat buffer;
    long int rev_num = rev.revision_number;
    std::string branch_fi_template;
    branch_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-b.fi");
    if (!file_exists(branch_fi_template)) {
	branch_fi_template = std::to_string(rev_num) + std::string("-b.fi");
	if (!file_exists(branch_fi_template)) {
	    branch_fi_template = std::string("");
	}
    }
    std::string wbfi_file;
    if (branch_fi_template.length()) {
	wbfi_file = populate_template(branch_fi_template, rbranch);
	apply_fi_file_working(wbfi_file,rbranch, rev.revision_number, 0, 0, 0);
	get_rev_sha1(rbranch, rev.revision_number);
	all_git_branches.push_back(rbranch);
    } else {
	wbfi_file = std::string("");
    }


    // Next, do a move-only commit if we have one
    std::string move_fi_template;
    move_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-mvonly.fi");
    if (stat(move_fi_template.c_str(), &buffer)) {
	move_fi_template = std::to_string(rev_num) + std::string("-mvonly.fi");
	if (stat(move_fi_template.c_str(), &buffer)) {
	    move_fi_template = std::string("");
	}
    }
    std::string wmvfi_file;
    if (move_fi_template.length()) {
	wmvfi_file = populate_template(move_fi_template, rbranch);
	apply_fi_file_working(wmvfi_file, rbranch, rev.revision_number, 0, 0, 0);
	get_rev_sha1(rbranch, rev.revision_number);
    } else {
	wmvfi_file = std::string("");
    }
}

void apply_commit(struct svn_revision &rev, std::string &rbranch, int verify_repo)
{

    struct stat buffer;
    long int rev_num = rev.revision_number;

    // Order of operations.  If we have a -b branching file, do that first and update the
    // sha1s.
    std::string branch_fi_template;
    branch_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-b.fi");
    if (!file_exists(branch_fi_template)) {
	branch_fi_template = std::to_string(rev_num) + std::string("-b.fi");
	if (!file_exists(branch_fi_template)) {
	    branch_fi_template = std::string("");
	}
    }
    std::string wbfi_file;
    if (branch_fi_template.length()) {
	wbfi_file = populate_template(branch_fi_template, rbranch);
	apply_fi_file_working(wbfi_file,rbranch, rev.revision_number, 0, 0, 0);
	get_rev_sha1(rbranch, rev.revision_number);
	all_git_branches.push_back(rbranch);
    } else {
	wbfi_file = std::string("");
    }


    // Next, do a move-only commit if we have one
    std::string move_fi_template;
    move_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-mvonly.fi");
    if (stat(move_fi_template.c_str(), &buffer)) {
	move_fi_template = std::to_string(rev_num) + std::string("-mvonly.fi");
	if (stat(move_fi_template.c_str(), &buffer)) {
	    move_fi_template = std::string("");
	}
    }
    std::string wmvfi_file;
    if (move_fi_template.length()) {
	wmvfi_file = populate_template(move_fi_template, rbranch);
	apply_fi_file_working(wmvfi_file, rbranch, rev.revision_number, 0, 0, 0);
	get_rev_sha1(rbranch, rev.revision_number);
    } else {
	wmvfi_file = std::string("");
    }

    // Next assemble the working commit and/or tag files
    std::string wfi_file = std::to_string(rev_num) + std::string("-main.fi");
    std::string blob_fi_file = std::string("");
    std::string commit_fi_file = std::string("");
    std::string tree_fi_file = std::string("");
    std::string wtfi_file = std::string("");
    std::string wtag_fi_file = std::string("");
    std::string nfi_file = std::string("");

    blob_fi_file = std::string("custom/") + std::to_string(rev_num) + std::string("-blob.fi");
    if (stat(blob_fi_file.c_str(), &buffer)) {
	blob_fi_file = std::to_string(rev_num) + std::string("-blob.fi");
	if (stat(blob_fi_file.c_str(), &buffer)) {
	    blob_fi_file = std::string("");
	}
    }

    std::string commit_fi_template;
    commit_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-commit.fi");
    if (stat(commit_fi_template.c_str(), &buffer)) {
	commit_fi_template = std::to_string(rev_num) + std::string("-commit.fi");
	if (stat(commit_fi_template.c_str(), &buffer)) {
	    commit_fi_template = std::string("");
	}
    }

    std::string tag_fi_template;
    tag_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-tag.fi");
    if (stat(tag_fi_template.c_str(), &buffer)) {
	tag_fi_template = std::to_string(rev_num) + std::string("-tag.fi");
	if (stat(tag_fi_template.c_str(), &buffer)) {
	    tag_fi_template = std::string("");
	}
    }


    // We need either a commit or a tag - without either, we're done
    if (commit_fi_template == std::string("") && tag_fi_template == std::string("")) {
	std::cerr << "Fatal - no commit or tag template for commit " << rev.revision_number << "\n";
	std::cerr << "Every SVN commit should have at least an empty commit with its commit message and date/author info, or a tag to another commit\n";
	exit(1);
    }


    // Doing the commit - commit wins if we have both -commit and -tag file around
    if (commit_fi_template.length()) {
	commit_fi_file = populate_template(commit_fi_template, rbranch);

	tree_fi_file = std::string("custom/") + std::to_string(rev_num) + std::string("-tree.fi");
	if (stat(tree_fi_file.c_str(), &buffer)) {
	    tree_fi_file = std::to_string(rev_num) + std::string("-tree.fi");
	    if (stat(tree_fi_file.c_str(), &buffer)) {
		tree_fi_file = std::string("");
	    }
	}


	// concat individual pieces of primary commit file and apply
	if (commit_fi_template.length()) {
	    std::string catstr = std::string("cat");
	    remove(wfi_file.c_str());

	    if (blob_fi_file != std::string("")) {
		catstr = catstr + std::string(" ") + blob_fi_file;
	    }

	    catstr = catstr + std::string(" ") + commit_fi_file;

	    if (tree_fi_file != std::string("")) {
		catstr = catstr + std::string(" ") + tree_fi_file;
	    }

	    catstr = catstr + std::string(" > ") + wfi_file;

	    if (std::system(catstr.c_str())) {
		std::cerr << "Failed to create " << wfi_file << "\n";
		exit(1);
	    }

	    // Apply the combined contents of the commit files
	    if (apply_fi_file_working(wfi_file, rbranch, rev.revision_number, 0, 1, 1)) {

		// If we're going to try this, we need to wipe cvs_git_working,
		// copy in cvs_git, and re-apply any branch or mvonly commits before
		// we proceed.
		std::string working_reset_cmd = std::string("rm -rf cvs_git_working && cp -r cvs_git cvs_git_working");
		if (std::system(working_reset_cmd.c_str())) {
		    std::cout << "working_reset_cmd failed.\n";
		    exit(1);
		}
		apply_working_branch_and_mvall(rev, rbranch);

		// If the apply failed, try generating the tree portion of the
		// commit from an actual svn checkout.  At least for the later
		// commits this is the most common remedy, so see if we can
		// automate it.
		generate_svn_tree(repo_checkout_path.c_str(), rbranch.c_str(), rev.revision_number);

		catstr = std::string("cat");
		remove(wfi_file.c_str());

		if (blob_fi_file != std::string("")) {
		    catstr = catstr + std::string(" ") + blob_fi_file;
		}

		catstr = catstr + std::string(" ") + commit_fi_file;

		tree_fi_file = std::string("custom/") + std::to_string(rev_num) + std::string("-tree.fi");
		if (!file_exists(tree_fi_file.c_str())) {
		    std::cerr << "Failed to generate custom tree file for commit " << rev.revision_number << "\n";
		    exit(1);
		}
		catstr = catstr + std::string(" ") + tree_fi_file + std::string(" > ") + wfi_file;

		if (std::system(catstr.c_str())) {
		    std::cerr << "Failed to create " << wfi_file << "\n";
		    exit(1);
		}

		if (apply_fi_file_working(wfi_file, rbranch, rev.revision_number, 0, 1, 0)) {
		    // If we fail again, we're done - need manual review
		    std::cerr << "Failed to apply commit with custom tree: " << rev.revision_number << "\n";
		    exit(1);
		}
	    }

	    get_rev_sha1(rbranch, rev.revision_number);




	    // Clean up intermediate commit file
	    remove(commit_fi_file.c_str());

	    // Generate the note file
	    nfi_file = note_svn_rev(rev, rbranch);
	    apply_fi_file_working(nfi_file, rbranch, rev.revision_number, 0, 0, 0);
	}

	std::string taftercommit_fi_template;
	taftercommit_fi_template = std::string("custom/") + std::to_string(rev_num) + std::string("-tagaftercommit.fi");
	if (stat(taftercommit_fi_template.c_str(), &buffer)) {
	    taftercommit_fi_template = std::to_string(rev_num) + std::string("-tagaftercommit.fi");
	    if (stat(taftercommit_fi_template.c_str(), &buffer)) {
		taftercommit_fi_template = std::string("");
	    }
	}
	if (taftercommit_fi_template.length()) {
	    wtfi_file = populate_template(taftercommit_fi_template, rbranch);
	    apply_fi_file_working(wtfi_file, rbranch, rev.revision_number, 1, 1, 0);
	}

    } else {
	// Doing a tag
	wtag_fi_file = populate_template(tag_fi_template, rbranch);

	// Apply the tag
	apply_fi_file_working(wtag_fi_file, rbranch, rev.revision_number, 1, 1, 0);
    }

    // If we got this far, we're good to go - apply all files to the primary repo
    if (wbfi_file.length()) {
	apply_fi_file(wbfi_file);
	remove(wbfi_file.c_str());
    }
    if (wmvfi_file.length()) {
	apply_fi_file(wmvfi_file);
	remove(wmvfi_file.c_str());
    }
    if (wfi_file.length()) {
	apply_fi_file(wfi_file);
	remove(wfi_file.c_str());
    }
    if (wtag_fi_file.length()) {
	apply_fi_file(wtag_fi_file);
	remove(wtag_fi_file.c_str());
    }
    if (nfi_file.length()) {
	apply_fi_file(nfi_file);
	remove(nfi_file.c_str());
    }
    if (wtfi_file.length()) {
	apply_fi_file(wtfi_file);
	remove(wtfi_file.c_str());
    }

    // Update all the sha1s
    get_rev_sha1s(rev.revision_number);

    std::string rmempty= std::string("find ./*.fi -empty -type f -exec rm \"{}\" \";\"");
    if (std::system(rmempty.c_str())) {
	std::cout << "Empty file removal failed\n";
    }
    std::string mvfifiles = std::string("mv ./*.fi ./done/");
    if (std::system(mvfifiles.c_str())) {
	std::cout << ".fi file move failed\n";
    }
}


std::string git_sha1(std::ifstream &infile, struct svn_node *n)
{
    std::string git_sha1;
    std::string go_buff;
    char *cbuffer = new char [n->text_content_length];;
    infile.read(cbuffer, n->text_content_length);
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


int
write_gitignore_blob(std::ofstream &outfile, long int rev)
{
    struct stat sbuffer;
    std::string gi_file = std::string("gitignore/") + std::to_string(rev) + std::string(".gitignore");
    if (stat(gi_file.c_str(), &sbuffer)) {
	return 0;
    }
    char *buffer = new char [sbuffer.st_size];
    std::ifstream ifile(gi_file, std::ifstream::binary);
    if (!ifile.good()) {
	std::cerr << "error reading .gitignore file for revision " << rev << "\n";
	exit(-1);
    }
    ifile.read(buffer, sbuffer.st_size);
    outfile << "blob\n";
    outfile << "data " << sbuffer.st_size << "\n";
    outfile.write(buffer, sbuffer.st_size);

    return 1;
}

void
write_gitignore_tree(std::ofstream &outfile, long int rev)
{
    if (rev == 29895) {
	std::cout << "got something\n";
    }
 
    std::string gi_file = std::string("gitignore/") + std::to_string(rev) + std::string(".gitignore");
    struct stat sbuffer;
    if (stat(gi_file.c_str(), &sbuffer)) {
	return;
    }
    char *buffer = new char [sbuffer.st_size];
    std::ifstream ifile(gi_file, std::ifstream::binary);
    if (!ifile.good()) {
	std::cerr << "error reading .gitignore file for revision " << rev << "\n";
	exit(-1);
    }
    ifile.read(buffer, sbuffer.st_size);
    std::string go_buff;
    go_buff.append("blob ");
    go_buff.append(std::to_string(sbuffer.st_size));
    go_buff.append(1, '\0');
    go_buff.append(buffer, sbuffer.st_size); 
    std::string git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());

    outfile << "M 100644 ";
    outfile << git_sha1;
    outfile << " \".gitignore\"";
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

void write_commit_core(std::ofstream &outfile, std::string &rbranch, struct svn_revision &rev, const char *alt_cmsg, int nomerge, int mvedcommit, int branch_first)
{
    outfile << "commit refs/heads/" << rbranch << "\n";
    outfile << "mark :" << rev.revision_number << "\n";
    outfile << "committer " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
    if (!alt_cmsg) {
	outfile << "data " << rev.commit_msg.length() << "\n";
	outfile << rev.commit_msg << "\n";
    } else {
	std::string cmsg(alt_cmsg);
	outfile << "data " << cmsg.length() << "\n";
	outfile << cmsg << "\n";
    }
    if (!mvedcommit && !branch_first) {
	outfile << "from " << rev.revision_number-1 << "\n";
    } else {
	outfile << "from " << rev.revision_number << "\n";
    }

    if (!nomerge && rev.merged_from.length()) {
	std::cout << "Revision " << rev.revision_number << " merged from: " << rev.merged_from << "(" << rev.merged_rev << "), id " << rev_to_gsha1[std::pair<std::string,long int>(rev.merged_from, rev.merged_rev)] << "\n";
	std::cout << "Revision " << rev.revision_number << "        from: " << rbranch << "\n";
	outfile << "merge " << rev.merged_from << "," << rev.merged_rev << "\n";
    }
}

int move_only_commit(struct svn_revision &rev, std::string &rbranch)
{
    struct stat buffer;
    std::string cfi_file = std::string("custom/") + std::to_string(rev.revision_number) + std::string("-mvonly.fi");

    // Skip if we already have a custom file
    if (file_exists(cfi_file.c_str())) {
	return 1;
    }

    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    // first, check if we truly have path changes between the copyfrom and dest local paths
    int have_real_rename = 0;
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (node.move_edit && node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if ((mlocal_path != node.local_path) && (cbranch == node.branch)) {
		have_real_rename = 1;
	    }
	}
    }

    if (!have_real_rename) {
	return 0;
    }

    std::string fi_file = std::to_string(rev.revision_number) + std::string("-mvonly.fi");
    std::ofstream outfile(fi_file.c_str(), std::ios::out | std::ios::binary);
    std::string ncmsg = rev.commit_msg + std::string(" (preliminary file move commit)");

    write_commit_core(outfile, rbranch, rev, ncmsg.c_str(), 0, 0, 0);

    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	/* Don't add directory nodes themselves - git works on files */
	if (node.kind == ndir) continue;
	if (!node.move_edit) continue;
	if (node.copyfrom_path.length() > 0) {
	    int is_tag;
	    std::string mproject, cbranch, mlocal_path, ctag;
	    node_path_split(node.copyfrom_path, mproject, cbranch, ctag, mlocal_path, &is_tag);
	    if (mlocal_path != node.local_path && mlocal_path.length()) {
		std::cout << "(r" << rev.revision_number << ") - renaming " << mlocal_path << " to " << node.local_path << "\n";
		outfile << "D " << mlocal_path << "\n";
		outfile << "M ";
		if (node.exec_path) {
		    outfile << "100755 ";
		} else {
		    outfile << "100644 ";
		}
		outfile << svn_sha1_to_git_sha1[node.text_copy_source_sha1] << " " << node.local_path << "\n";
	    }
	}
    }

    outfile.close();
    return 1;
}

void write_git_node(std::ofstream &toutfile, struct svn_revision &rev, struct svn_node &node)
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
	    toutfile << "C " << mlocal_path << " " << node.local_path << "\n";
	}
	return;
    }
    if (node.kind == ndir && node.action == nadd) return;

    // If it's a straight-up path delete, do it and return
    if ((node.kind == nkerr || node.kind == ndir) && node.action == ndelete) {
	std::cerr << "Revision r" << rev.revision_number << " delete: " << node.local_path << "\n";
	if (node.local_path.length()) {
	    toutfile << "D \"" << node.local_path << "\"\n";
	}
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

    if (node.local_path.length()) {
	switch (node.action) {
	    case nchange:
		toutfile << "M ";
		break;
	    case nadd:
		toutfile << "M ";
		break;
	    case nreplace:
		toutfile << "M ";
		break;
	    case ndelete:
		toutfile << "D ";
		break;
	    default:
		std::cerr << "Unhandled node action type " << print_node_action(node.action) << "\n";
		toutfile << "? ";
		toutfile << "\"" << node.local_path << "\"\n";
		std::cout << "Fatal - unhandled node action at r" << rev.revision_number << ", node: " << node.path << "\n";
		exit(1);
	}

	if (node.action != ndelete) {
	    if (node.exec_path) {
		toutfile << "100755 ";
	    } else {
		toutfile << "100644 ";
	    }
	}

	if (node.action == nchange || node.action == nadd || node.action == nreplace) {
	    toutfile << gsha1 << " \"" << node.local_path << "\"\n";
	    return;
	}

	if (node.action == ndelete) {
	    toutfile << "\"" << node.local_path << "\"\n";
	    return;
	}
    } else {
	std::cout << "Note (r" << rev.revision_number << ") - skipping node with no local path.\n";
	return;
    }

    std::cout << "Error(r" << rev.revision_number << ") - unhandled node action: " << print_node_action(node.action) << "\n";
    print_node(node);
    exit(1);
}

void old_ref_process_dir(std::ofstream &toutfile, struct svn_revision &rev, struct svn_node *node)
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
		old_ref_process_dir(toutfile, rev, *n_it);
	    }
	    std::cout << "Stashed file node: " << (*n_it)->path << "\n";
	    std::string rp = path_subpath(llpath, (*n_it)->local_path);
	    std::string gsha1 = svn_sha1_to_git_sha1[(*n_it)->text_content_sha1];
	    std::string exestr = ((*n_it)->exec_path) ? std::string("100755 ") : std::string("100644 ");
	    toutfile << "M " << exestr << gsha1 << " \"" << node->local_path << rp << "\"\n";
	}
    }
}


void old_references_commit(std::ifstream &infile, struct svn_revision &rev, std::string &rbranch)
{
    struct stat buffer;
    std::string fi_file;

    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    // If we need to write any blobs, take care of it now - old_references_commit short-circuits the "main" logic that handles this
    std::string bfi_file = std::to_string(rev.revision_number) + std::string("-blob.fi");
    fi_file = std::string("custom/") + bfi_file;
    // Only make one if we don't alredy have a custom file
    if (stat(fi_file.c_str(), &buffer)) {

	std::ofstream boutfile(fi_file, std::ios::out | std::ios::binary);

	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    if (node.text_content_length > 0) {
		write_blob(infile, boutfile, node);
	    }
	}

	write_gitignore_blob(boutfile, rev.revision_number);

	boutfile.close();
    }

    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");
    fi_file = std::string("custom/") + cfi_file;
    // Only make one if we don't alredy have a custom file
    if (stat(fi_file.c_str(), &buffer)) {
	std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);
	write_commit_core(coutfile, rbranch, rev, NULL, 0, 0, 0);
	coutfile.close();
    }

    std::string tfi_file = std::to_string(rev.revision_number) + std::string("-tree.fi");
    fi_file = std::string("custom/") + tfi_file;
    // Only make one if we don't alredy have a custom file
    if (stat(fi_file.c_str(), &buffer)) {

	std::ofstream toutfile(tfi_file, std::ios::out | std::ios::binary);

	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];

	    if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length() && node.copyfrom_rev < rev.revision_number - 1) {
		old_ref_process_dir(toutfile, rev, &node);
		continue;
	    }

	    // Not one of the special cases, handle normally
	    write_git_node(toutfile, rev, node);

	}
	write_gitignore_tree(toutfile, rev.revision_number);

	toutfile.close();
    }
}

void standard_commit(struct svn_revision &rev, std::string &rbranch, int mvedcommit)
{
    struct stat buffer;
    std::string fi_file;

    if (author_map.find(rev.author) == author_map.end()) {
	std::cout << "Error - couldn't find author map for author " << rev.author << " on revision " << rev.revision_number << "\n";
	exit(1);
    }

    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");
    fi_file = std::string("custom/") + cfi_file;
    // Only make one if we don't alredy have a custom file
    if (stat(fi_file.c_str(), &buffer)) {

	std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);

	write_commit_core(coutfile, rbranch, rev, NULL, 0, mvedcommit, 0);

	coutfile.close();
    }

    std::string tfi_file = std::to_string(rev.revision_number) + std::string("-tree.fi");
    fi_file = std::string("custom/") + tfi_file;
    // Only make one if we don't alredy have a custom file
    if (stat(fi_file.c_str(), &buffer)) {

	std::ofstream toutfile(tfi_file, std::ios::out | std::ios::binary);

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
		write_git_node(toutfile, rev, node);
	    }
	}

	for (d_it = deferred_deletes.begin(); d_it != deferred_deletes.end(); d_it++) {
	    write_git_node(toutfile, rev, **d_it);
	}

	write_gitignore_tree(toutfile, rev.revision_number);

	toutfile.close();
    }
}

// We don't delete branches during conversion, so make an empty commit that denotes
// the delete event
void branch_delete_commit(struct svn_revision &rev, std::string &rbranch)
{
    if (rbranch == std::string("scriptics")) {
	std::string itclstr = std::string("itcl3-2");
	std::string tclstr = std::string("tcl8-3");
	std::string tkstr = std::string("tk8-3");
	branch_delete_commit(rev, itclstr);
	branch_delete_commit(rev, tclstr);
	branch_delete_commit(rev, tkstr);
	return;
    }
    std::string wbranch = rbranch;
    if (rbranch == std::string("VendorARL")) {
	wbranch = std::string("Original");
    }
    if (rbranch == std::string("libpng")) {
	wbranch = std::string("libpng_1_0_2");
    }
    if (rbranch == std::string("zlib")) {
	wbranch = std::string("zlib_1_0_4");
    }
    if (rbranch == std::string("unlabeled-1.1.1")) return;
    if (rbranch == std::string("unlabeled-1.2.1")) return;
    if (rbranch == std::string("unlabeled-11.1.1")) return;
    if (rbranch == std::string("unlabeled-2.12.1")) return;
    if (rbranch == std::string("unlabeled-2.6.1")) return;
    if (rbranch == std::string("unlabeled-9.1.1")) return;
    if (rbranch == std::string("unlabeled-9.10.1")) return;
    if (rbranch == std::string("unlabeled-9.12.1")) return;
    if (rbranch == std::string("unlabeled-9.2.1")) return;
    if (rbranch == std::string("unlabeled-9.3.1")) return;
    if (rbranch == std::string("unlabeled-9.9.1")) return;
    if (rbranch == std::string("unlabeled-1.1.2")) {
	wbranch = std::string("master-UNNAMED-BRANCH");
    }
    if (rbranch == std::string("unlabeled-9.7.1")) {
	wbranch = std::string("cjohnson-mac-hack");
    }
    if (rbranch == std::string("unlabeled-2.5.1")) {
	wbranch = std::string("master-UNNAMED-BRANCH");
    }

    if (rbranch == std::string("ansi-20040405-merged")) return;
    if (rbranch == std::string("autoconf-freeze")) return;
    if (rbranch == std::string("hartley-6-0-pre")) return;
    if (rbranch == std::string("opensource-post")) return;
    if (rbranch == std::string("postmerge-20040315-windows")) return;
    if (rbranch == std::string("postmerge-20040405-ansi")) return;
    if (rbranch == std::string("postmerge-20051223-bobWinPort")) return;


    std::string bdelete_msg = rev.commit_msg + std::string(" (svn branch delete)");

    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-bdelete.fi");
    std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);
    write_commit_core(coutfile, wbranch, rev, bdelete_msg.c_str(), 0, 0, 0);
    coutfile.close();

    std::string commit_fi_file = populate_template(cfi_file, wbranch);

    apply_fi_file_working(commit_fi_file, wbranch, rev.revision_number, 0, 0, 0);
    get_rev_sha1(wbranch, rev.revision_number);

    std::string nfi_file = note_svn_rev(rev, wbranch);
    apply_fi_file_working(nfi_file, wbranch, rev.revision_number, 0, 0, 0);

    apply_fi_file(commit_fi_file);
    apply_fi_file(nfi_file);

    remove(commit_fi_file.c_str());
    remove(nfi_file.c_str());
}


void rev_fast_export(std::ifstream &infile, long int rev_num)
{
    struct stat buffer;
    struct svn_revision &rev = revs.find(rev_num)->second;

    if (rev.project != std::string("brlcad")) {
	//std::cout << "Revision " << rev.revision_number << " is not part of brlcad, skipping\n";
	return;
    }

    // Periodically, run git gc
    if (rev_num % 100 == 0) {
	std::string gitgc = std::string("cd cvs_git && git gc && cd ..");
	if (std::system(gitgc.c_str())) {
	    std::cerr << "cvs_git git gc failed\n";
	    exit(1);
	}

	std::string gitwgc = std::string("cd cvs_git_working && git gc && cd ..");
	if (std::system(gitwgc.c_str())) {
	    std::cerr << "cvs_git_working git gc failed\n";
	    exit(1);
	}
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

    std::string rbranch = std::string("");
    for (size_t n = 0; n != rev.nodes.size(); n++) {
	struct svn_node &node = rev.nodes[n];
	if (rbranch.length()) break;
	rbranch = rev.nodes[0].branch;
    }
    if (!rbranch.length()) {
	// If we don't have a branch, may be editing a tag.  (See, for example, r34555)
	for (size_t n = 0; n != rev.nodes.size(); n++) {
	    struct svn_node &node = rev.nodes[n];
	    if (rbranch.length()) break;
	    rbranch = rev.nodes[0].tag;
	}
    }

    if (rev_num == 36053) {
	all_git_branches.push_back(std::string("rel8"));
    }


    if (rev_num == 64060) {
	all_git_branches.push_back(std::string("eab"));
    }

    if (reject_tag(rev.nodes[0].tag)) {
	std::cout << "Skipping " << rev.revision_number << " - edit to old tag:\n" << rev.commit_msg << "\n";
	return;
    }

    if (rev_num == 72430 || rev_num ==72431 ||  rev_num ==72432 || rev_num == 76359) {
	return;
    }


    std::cout << "Processing revision " << rev.revision_number << "\n";

    if (rev.move_edit) {
	std::cout << "Move edit " << rev.merged_from << "\n";
    }
#if 0
    if (rev.merged_from.length()) {
	std::cout << "Note: merged from " << rev.merged_from << "\n";
    }
#endif
    int git_changes = 0;
    int have_commit = 0;
    int tag_after_commit = 0;
    int branch_add = 0;
    int branch_delete = 0;
    int tag_only_commit = 0;
    std::string ctag, cfrom;

    if (rebuild_revs.find(rev.revision_number) != rebuild_revs.end()) {
	std::cout << "Revision " << rev.revision_number << " references non-current SVN info, needs special handling\n";
	old_references_commit(infile, rev, rbranch);
	apply_commit(rev, rbranch, 1);
	return;
    }


    // clear old commit file, if there is one
    std::string efi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");
    if (file_exists(efi_file.c_str())) {
	remove(efi_file.c_str());
    }

    std::string blob_fi_file = std::to_string(rev_num) + std::string("-blob.fi");
    std::ofstream bloboutfile(blob_fi_file.c_str(), std::ios::out | std::ios::binary);

    if (rev.revision_number == 29895) {
	std::cout << "got something\n";
    }

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

		if (rev.revision_number < edited_tag_minr) {
		    std::string nbranch = std::string("refs/heads/") + node.tag;
		    std::cout << "Adding tag branch " << nbranch << " from " << bbpath << ", r" << rev.revision_number <<"\n";
		    std::string bfi_file = std::to_string(rev_num) + std::string("-b.fi");
		    std::ofstream boutfile(bfi_file.c_str(), std::ios::out | std::ios::binary);
		    std::cout << rev.commit_msg << "\n";
		    boutfile << "reset " << nbranch << "\n";
		    boutfile << "from " << bbpath << "," << rev.revision_number-1 << "\n";
		    boutfile.close();
		    all_git_branches.push_back(node.tag);

		    branch_add = 1;

		    // Make an empty commit on the new branch with the commit message from SVN, but no changes

		    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");
		    std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);

		    write_commit_core(coutfile, node.tag, rev, NULL, 1, 0, 1);

		    coutfile.close();

		    // Make sure rbranch knows what branch this is. (Hi r33127...)
		    rbranch = node.tag;

		    continue;
		}
		if (rev.revision_number == edited_tag_maxr) {
		    tag_after_commit = 1;
		    rbranch = node.branch;
		    ctag = node.tag;
		    cfrom = node.branch;
		    std::string tfi_file = std::to_string(rev.revision_number) + std::string("-tagaftercommit.fi");
		    std::ofstream toutfile(tfi_file.c_str(), std::ios::out | std::ios::binary);

		    // Note - in this situation, we need to both build a commit and do a tag.  Will probably
		    // take some refactoring.  Merge information will also be a factor.
		    std::cout << "Adding tag after final tag branch commit: " << ctag << " from " << cfrom << ", r" << rev.revision_number << "\n";
		    toutfile << "tag " << ctag << "\n";
		    toutfile << "from " << cfrom << "," << rev.revision_number-1 << "\n";
		    toutfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
		    toutfile << "data " << rev.commit_msg.length() << "\n";
		    toutfile << rev.commit_msg << "\n";
		    toutfile.close();

		}
		std::cout << "Non-final tag edit, processing normally: " << node.branch << ", r" << rev.revision_number<< "\n";
		rbranch = node.branch;
		//git_changes = 1;
	    } else {
		std::cout << "Adding tag " << node.tag << " from " << bbpath << ", r" << rev.revision_number << "\n";
		std::string tfi_file = std::to_string(rev_num) + std::string("-tag.fi");
		std::ofstream toutfile(tfi_file.c_str(), std::ios::out | std::ios::binary);
		toutfile << "tag " << node.tag << "\n";
		toutfile << "from " << bbpath << "," << rev.revision_number-1 << "\n";
		toutfile << "tagger " << author_map[rev.author] << " " << svn_time_to_git_time(rev.timestamp.c_str()) << "\n";
		toutfile << "data " << rev.commit_msg.length() << "\n";
		toutfile << rev.commit_msg << "\n";
		tag_ids[node.tag] = rev_to_gsha1[std::pair<std::string,long int>(bbpath, rev.revision_number-1)];
		toutfile.close();
		tag_only_commit = 1;
		continue;
	    }

	}

	if (node.tag_delete) {
	    std::cout << "delete tag: " << rev.revision_number << "\n";
	    // Because we are applying and commiting immediately, these commits can be
	    // somewhat involved to unwind - back up first
	    std::string clear_old_cvsbackup = std::string("rm -rf cvs_git-r*");
	    if (std::system(clear_old_cvsbackup .c_str())) {
		std::cerr << "clearing old cvs_git backups didn't work\n";
	    }
	    std::string cvsbackup = std::string("cvs_git-r") + std::to_string(rev_num-1);
	    if (!file_exists(cvsbackup)) {
		std::cerr << "backing up cvs_git ahead of tag delete in commit " << rev_num << "\n";
		std::string cvsbackupcmd = std::string("cp -r cvs_git ") + cvsbackup;
		if (std::system(cvsbackupcmd.c_str())) {
		    std::cerr << "backing up cvs_git ahead of delete tag failed\n";
		    exit(1);
		}
	    }

	    branch_delete_commit(rev, node.tag);
	    continue;
	}

	if (node.branch_add) {
	    std::cout << "Processing revision " << rev.revision_number << "\n";
	    std::string ppath, bbpath, llpath, tpath;
	    int is_tag;
	    node_path_split(node.copyfrom_path, ppath, bbpath, tpath, llpath, &is_tag);
	    std::cout << "Adding branch " << node.branch << " from " << bbpath << ", r" << rev.revision_number <<"\n";

	    std::string bfi_file = std::to_string(rev_num) + std::string("-b.fi");
	    std::ofstream boutfile(bfi_file.c_str(), std::ios::out | std::ios::binary);

	    boutfile << "reset refs/heads/" << node.branch << "\n";
	    boutfile << "from " << bbpath << "," << rev.revision_number-1 << "\n";
	    boutfile.close();

	    all_git_branches.push_back(node.branch);

	    // Make an empty commit on the new branch with the commit message from SVN, but no changes

	    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");
	    std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);

	    write_commit_core(coutfile, node.branch, rev, NULL, 1, 0, 1);

	    coutfile.close();

	    // Make sure rbranch knows what branch this is.
	    rbranch = node.branch;

	    branch_add = 1;

	    continue;
	}

	if (node.branch_delete) {
	    // Branch deletes can hit multiple branches per commit - process fully and immediately
	    
	    // Because we are applying and commiting locally, these commits can be
	    // somewhat involved to unwind - back up first
	    std::string cvsbackup = std::string("cvs_git-r") + std::to_string(rev_num - 1);
	    if (!file_exists(cvsbackup)) {
		std::cerr << "backing up cvs_git ahead of branch delete in commit " << rev_num << "\n";
		std::string cvsbackupcmd = std::string("cp -r cvs_git ") + cvsbackup;
		if (std::system(cvsbackupcmd.c_str())) {
		    std::cerr << "backing up cvs_git ahead of branch delete failed\n";
		    exit(1);
		}
	    }

	    branch_delete_commit(rev, node.branch);
	    branch_delete = 1;
	    continue;
	}


	if (node.kind == ndir && node.action == nadd && node.copyfrom_path.length()) {
	    git_changes = 1;
	}

	if (node.text_content_length > 0) {
	    //std::cout << "	Adding node object for " << node.local_path << "\n";
	    write_blob(infile, bloboutfile, node);
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

    if (write_gitignore_blob(bloboutfile, rev.revision_number)) {
	git_changes = 1;
    }
    bloboutfile.close();

    if (tag_only_commit) {
	remove(efi_file.c_str());
    }

    int have_mvonly = 0;
    if (git_changes) {
	if (have_commit) {
	    std::cout << "Error - more than one commit generated for revision " << rev.revision_number << "\n";
	}

	if (rev.move_edit) {
	    have_mvonly = move_only_commit(rev, rbranch);
	    standard_commit(rev, rbranch, have_mvonly);
	} else {
	    standard_commit(rev, rbranch, 0);
	}
    } else {
	if (!branch_delete && !tag_only_commit) {
	    // If nothing else, make an empty commit
	    std::string cfi_file = std::to_string(rev.revision_number) + std::string("-commit.fi");

	    if (!file_exists(cfi_file)) {
		std::ofstream coutfile(cfi_file, std::ios::out | std::ios::binary);

		write_commit_core(coutfile, rbranch, rev, NULL, 1, 0, 0);

		coutfile.close();
	    }
	}
    }

    if (branch_add || git_changes || tag_only_commit) {
	apply_commit(rev, rbranch, 0);
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
