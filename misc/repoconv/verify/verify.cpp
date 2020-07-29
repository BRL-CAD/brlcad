/*                      V E R I F Y . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file verify.cpp
 *
 * Testing logic to characterize the accuracy of the CVS+SVN -> Git conversion
 *
 */

#include <climits>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include "cxxopts.hpp"
#include "./sha1.hpp"

class cmp_info {
    public:
	std::string sha1;
	std::string msg;
	std::string timestamp_str;
	long timestamp = 0;
	std::string rev;
	long svn_rev = 0;

	std::string branch_svn = "trunk";
	std::set<std::string> branches;
	std::string cvs_date;

	bool branch_delete = false;

	std::string cvs_check_cmds;
	std::string git_check_cmds;
	std::string svn_check_cmds;
};

void
read_key_sha1_map(std::map<std::string, std::string> &key2sha1, std::string &keysha1file)
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
	if (key2sha1.find(key) != key2sha1.end()) {
	    //std::cout << "non-unique key: " << line << "\n";
	} else {
	    key2sha1[key] = sha1;
	}
    }
    infile.close();
}

void
read_branch_sha1_map(
	std::map<std::string, std::string> &sha12branch,
	std::map<std::string, std::string> &key2sha1,
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
		std::cout << "WARNING: non-unique key maps to both " << oldbranch << " and "  << branch << "\n";
	    }
	} else {
	    key2branch[key] = branch;
	}
    }
    infile.close();

    std::map<std::string, std::string>::iterator k2s_it;
    for (k2s_it = key2sha1.begin(); k2s_it != key2sha1.end(); k2s_it++) {
	std::string key = k2s_it->first;
	std::string sha1 = k2s_it->second;
	if (key2branch.find(key) == key2branch.end()) {
	    continue;
	}
	sha12branch[sha1] = key2branch[key];
	std::cout << sha1 << " -> " << key2branch[key] << "\n";
    }
}

/* Assuming a tree checked out, build a tree based on the contents */

class filemodify {
    public:
	std::string mode;
	std::string hash;
	std::string path;
};

void run_cmd(std::string &cmd)
{
    if (std::system(cmd.c_str())) {
	std::cerr << "cmd \"" << cmd << "\" failed!\n";
	exit(1);
    }
}

void
get_done_sha1s(std::set<std::string> &done, std::string &done_file)
{
    std::ifstream infile(done_file, std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open file: " << done_file << "\n";
	exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
	done.insert(line);
    }
    infile.close();
}


void
get_exec_paths(std::vector<filemodify> &m)
{
    std::string exec_cmd = std::string("cd brlcad && find . -type f ! -name .cvsignore ! -path \\*/CVS/\\* -executable | sed -e 's/.\\///' > ../exec.txt && cd ..");
    run_cmd(exec_cmd);
    std::ifstream infile("exec.txt", std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open file: exec.txt\n";
	exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
	filemodify nm;
	nm.mode = std::string("100755");
	nm.path = line;
	m.push_back(nm);
    }
    infile.close();
}

void
get_noexec_paths(std::vector<filemodify> &m)
{
    std::string noexec_cmd = std::string("cd brlcad && find . -type f ! -name .cvsignore ! -path \\*/CVS/\\* ! -executable | sed -e 's/.\\///' > ../noexec.txt && cd ..");
    run_cmd(noexec_cmd);
    std::ifstream infile("noexec.txt", std::ifstream::binary);
    if (!infile.good()) {
	std::cerr << "Could not open file: noexec.txt\n";
	exit(-1);
    }
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
	filemodify nm;
	nm.mode = std::string("100644");
	nm.path = line;
	m.push_back(nm);
    }
    infile.close();
}

std::string
git_sha1(const char *b, size_t size)
{
    std::string go_buff;
    go_buff.append("blob ");
    go_buff.append(std::to_string(size));
    go_buff.append(1, '\0');
    go_buff.append(b, size);
    std::string git_sha1 = sha1_hash_hex(go_buff.c_str(), go_buff.length());
    return git_sha1;
}

/* Even if writing out the blobs is disabled, we still need to calculate the
 * sha1 hashes for the tree output. */
void
process_blobs(std::vector<filemodify> &mods, std::string &sha1)
{
    // The -blob.fi file is prepared in case the tree incorporates a blob that
    // was never preserved in the original conversion.  blob.fi files take a
    // significant amount of space and slow subsequent fast-imports, so they
    // should be enabled only if that situation is discovered.
//#define WRITE_BLOBS
#ifdef WRITE_BLOBS
    std::string sha1file = sha1 + std::string("-blob.fi");
    std::ofstream outfile(sha1file.c_str(), std::ifstream::binary);
    if (!outfile.good()) {
	std::cerr << "Could not open file: " << sha1file << "\n";
	exit(-1);
    }
#endif

    for (size_t i = 0; i < mods.size(); i++) {
	std::string path = std::string("brlcad/") + mods[i].path;
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.good()) {
	    std::cerr << "Could not open file: " << path << "\n";
	    exit(-1);
	}
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	if (file.read(buffer.data(), size))
	{

	    const char *b = reinterpret_cast<char*>(buffer.data());
	    mods[i].hash = git_sha1(b, size);

#ifdef WRITE_BLOBS
	    outfile << "blob\n";
	    outfile << "data " << size << "\n";
	    outfile.write(reinterpret_cast<char*>(buffer.data()), size);
#endif
	}
	file.close();
    }

#ifdef WRITE_BLOBS
    outfile.close();
#endif
}

int
build_cvs_tree(std::string sha1)
{
    std::vector<filemodify> mods;
    get_exec_paths(mods);
    get_noexec_paths(mods);
    process_blobs(mods, sha1);

    if (!mods.size()) {
	return -1;
    }

    std::string sha1file = sha1 + std::string("-tree.fi");
    std::ofstream outfile(sha1file.c_str(), std::ifstream::binary);
    if (!outfile.good()) {
	std::cerr << "Could not open file: " << sha1file << "\n";
	exit(-1);
    }

    for (size_t i = 0; i < mods.size(); i++) {
	outfile << "M " << mods[i].mode << " " << mods[i].hash << " \"" << mods[i].path << "\"\n";
    }

    std::string cleanup("rm exec.txt noexec.txt");
    run_cmd(cleanup);
    return 0;
}

int verify_repos_cvs(std::ofstream &cvs_problem_sha1s, cmp_info &info, std::string git_repo, std::string cvs_repo) {
    std::string cvs_cmd;

    std::regex tag_invalid(".*[$,.:;@].*");
    if (std::regex_match(info.branch_svn, tag_invalid)) {
	std::cout << "Branch name contains invalid char, cannot be checked out by CVS, skipping\n";
	return 0;
    }
    std::regex muregex(".*master-UNNAMED-BRANCH.*");
    if (std::regex_match(info.branch_svn, muregex)) {
	std::cout << "Branch is master-UNNAMED-BRANCH, cannot be checked out by CVS, skipping\n";
	return 0;
    }

    if (info.branch_svn == std::string("trunk") || info.branch_svn == std::string("master")) {
	cvs_cmd = std::string("cvs -d ") + cvs_repo + std::string(" -Q co -ko -D \"") + info.cvs_date + std::string("\" -P brlcad");
    } else {
	cvs_cmd = std::string("cvs -d ") + cvs_repo + std::string(" -Q co -ko -D \"") + info.cvs_date + std::string("\" -r ") + info.branch_svn + std::string(" -P brlcad");
    }

    info.cvs_check_cmds.append(cvs_cmd);
    info.cvs_check_cmds.append(std::string("\n"));

    std::string cleanup_cmd = std::string("rm -rf brlcad");
    if (std::system(cleanup_cmd.c_str())) {
	std::cerr << "verify cleanup failed!\n";
	exit(1);
    }
    if (std::system(cvs_cmd.c_str())) {
	std::cerr << "cvs checkout failed: " << cvs_cmd << "\n";
	std::cerr << "skipping " << info.sha1 << "\n";
	if (std::system(cleanup_cmd.c_str())) {
	    std::cerr << "verify cleanup failed!\n";
	}
	return 0;
    }

    // Have both, do diff
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'\\$Source' -I'\\$Date' -I'\\$Log' -I'\\$Locker' --exclude \"CVS\" --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" --exclude \"saxon65.jar\" --exclude \"xalan27.jar\" brlcad ") + git_repo;


    info.cvs_check_cmds.append(repo_diff);
    info.cvs_check_cmds.append(std::string("\n"));

    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cerr << "CVS vs Git: diff test failed, SHA1" << info.sha1 << ", branch " << info.branch_svn << "\n";
	if (build_cvs_tree(info.sha1)) {
	    std::cerr << "CVS tree empty - probably not what is intended, skipping\n";
	    return 0;

	}
	cvs_problem_sha1s << info.sha1 << "\n";
	cvs_problem_sha1s.flush();
	return 1;
    }

    return 0;
}


int verify_repos_svn(cmp_info &info, std::string git_repo, std::string svn_repo)
{
    int ret = 0;
    std::string git_fi;
    std::string branch_git;

    std::map<std::string, std::string> branch_mappings;
    branch_mappings[std::string("trunk")] = std::string("master");
    branch_mappings[std::string("dmtogl-branch")] = std::string("dmtogl");

    if (branch_mappings.find(info.branch_svn) != branch_mappings.end()) {
	branch_git = branch_mappings[info.branch_svn];
    }

    std::cout << "Verifying r" << info.rev << ", branch " << info.branch_svn << "\n";

    // First, check out the correct SVN tree
    std::string svn_cmd;
    if (info.branch_svn == std::string("trunk")) {
	svn_cmd = std::string("svn co -q -r") + info.rev + std::string(" file://") + svn_repo + std::string("/brlcad/trunk brlcad_svn_checkout");
    } else {
	svn_cmd = std::string("svn co -q file://") + svn_repo + std::string("/brlcad/branches/") + info.branch_svn + std::string("@") + info.rev + std::string(" brlcad_svn_checkout");
    }

    info.svn_check_cmds = svn_cmd;
    info.svn_check_cmds.append(std::string("\n"));

    std::string cleanup_cmd = std::string("rm -rf brlcad_svn_checkout");
    if (std::system(cleanup_cmd.c_str())) {
	std::cerr << "verify cleanup failed!\n";
	exit(1);
    }
    if (std::system(svn_cmd.c_str())) {
	std::cerr << "svn checkout failed!\n";
	if (std::system(cleanup_cmd.c_str())) {
	    std::cerr << "verify cleanup failed!\n";
	}
	exit(1);
    }

    // Git doesn't do empty directories, so strip any that SVN creates
    std::string svn_emptydir_rm = std::string("find brlcad_svn_checkout -type d -empty -print0 |xargs -0 rmdir 2>/dev/null");
    ret = std::system(svn_emptydir_rm.c_str());
    while (!ret) {
	ret = std::system(svn_emptydir_rm.c_str());
    }

    // Have both, do diff
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'\\$Source' -I'\\$Date' -I'\\$Log' -I'\\$Locker' --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" --exclude \"saxon65.jar\" --exclude \"xalan27.jar\" brlcad_svn_checkout ") + git_repo;

    info.svn_check_cmds.append(repo_diff);
    info.svn_check_cmds.append(std::string("\n"));

    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cerr << "SVN diff test failed, r" << info.rev << ", branch " << info.branch_svn << "\n";
        return 1;
    }

    return 0;
}

void
parse_git_info(std::vector<cmp_info> &commits, const char *fname)
{
    // Build up a map of SVN revs to SHA1 ids.  We'll work in SVN order for a more intuitive result
    std::ifstream infile("commits.txt", std::ifstream::binary);
    if (!infile.good()) {
        std::cerr << "Could not open info file: commits.txt\n";
        exit(-1);
    }

    std::string bstr("GITCOMMIT");
    std::string estr("GITCOMMITEND");
    std::regex revnum_regex(".*svn:revision:([0-9]+).*");
    std::regex branch_regex(".*svn:branch:([a-zA-Z0-9_-]+).*");
    std::regex bdelete_regex(".*svn branch delete.*");
    std::regex note_regex(".*Note SVN revision and branch*");
    std::regex note_regex2(".*Note SVN revision [0-9]+.*");

    std::string line;
    while (std::getline(infile, line)) {
        if (!line.length()) continue;
	if (line == bstr) {
	    cmp_info ncommit;
	    std::getline(infile, ncommit.sha1);
	    std::getline(infile, ncommit.timestamp_str);
	    ncommit.timestamp = std::stol(ncommit.timestamp_str);
	    std::getline(infile, ncommit.cvs_date);
	    bool note_commit = false;

	    while (line != estr) {
		std::getline(infile, line);
		if (line == estr) {
		    break;
		}
		ncommit.msg.append(line);
		std::smatch rmatch;
		if (std::regex_search(line, rmatch, revnum_regex)) {
		    ncommit.rev = std::string(rmatch[1]);
		    ncommit.svn_rev = std::stol(ncommit.rev);
		}
		std::smatch bmatch;
		if (std::regex_search(line, bmatch, branch_regex)) {
		    ncommit.branch_svn = std::string(bmatch[1]);
		}
		std::smatch bd_match;
		if (std::regex_search(line, bd_match, bdelete_regex)) {
		    ncommit.branch_delete = true;
		}
		std::smatch note_match;
		if (std::regex_search(line, note_match, note_regex)) {
		    note_commit = true;
		}
		std::smatch note_match2;
		if (std::regex_search(line, note_match2, note_regex2)) {
		    note_commit = true;
		}
	    }

	    if (note_commit) {
		continue;
	    }

	    commits.push_back(ncommit);
	}
    }
    infile.close();
}

// trim whitespace - https://stackoverflow.com/a/49253841
static inline void wtrim(std::string &s) {
    if (s.empty()) return;
    while (s.find(" ") == 0) {s.erase(0, 1);}
    size_t len = s.size();
    while (s.rfind(" ") == --len) { s.erase(len, len + 1); }
}

int main(int argc, char *argv[])
{
    int ret;
    std::string keymap = std::string();
    std::string branchmap = std::string();
    std::string cvs_repo = std::string();
    std::string svn_repo = std::string();
    std::string done_list = std::string();
    long cvs_maxtime = 1199132714;
    long min_timestamp = 0;
    long max_timestamp = LONG_MAX;
    long max_rev = LONG_MAX;
    long min_rev = 0;

    std::ofstream cvs_problem_sha1s("cvs_problem_sha1.txt", std::ifstream::binary);

    try
    {
	cxxopts::Options options(argv[0], " - verify svn->git conversion");

	options.add_options()
	    ("cvs-repo", "Use the specified CVS repository for checks", cxxopts::value<std::vector<std::string>>(), "repo")
	    ("keymap", "msgtim key to SHA1 lookup map", cxxopts::value<std::vector<std::string>>(), "file")
	    ("branchmap", "msgtim key to CVS branch lookup map", cxxopts::value<std::vector<std::string>>(), "file")
	    ("svn-repo", "Use the specified SVN repository for checks", cxxopts::value<std::vector<std::string>>(), "repo")
	    ("max-rev", "Skip any revision higher than this number", cxxopts::value<int>(), "#")
	    ("min-rev", "Skip any revision lower than this number", cxxopts::value<int>(), "#")
	    ("done", "File with SHA1 identifiers (1/line) which have already been checked", cxxopts::value<std::vector<std::string>>(), "repo")
	    ("h,help", "Print help")
	    ;

	auto result = options.parse(argc, argv);

	if (result.count("help"))
	{
	    std::cout << options.help({""}) << std::endl;
	    return 0;
	}

	if (result.count("cvs-repo"))
	{
	    auto& ff = result["cvs-repo"].as<std::vector<std::string>>();
	    cvs_repo = ff[0];
	}

	if (result.count("keymap"))
	{
	    auto& ff = result["keymap"].as<std::vector<std::string>>();
	    keymap = ff[0];
	}

	if (result.count("branchmap"))
	{
	    auto& ff = result["branchmap"].as<std::vector<std::string>>();
	    branchmap = ff[0];
	}

	if (result.count("svn-repo"))
	{
	    auto& ff = result["svn-repo"].as<std::vector<std::string>>();
	    svn_repo = ff[0];
	}

	if (result.count("done"))
	{
	    auto& ff = result["done"].as<std::vector<std::string>>();
	    done_list = ff[0];
	}

	if (result.count("max-rev"))
	{
	    max_rev = result["max-rev"].as<int>();
	}

	if (result.count("min-rev"))
	{
	    min_rev = result["min-rev"].as<int>();
	}

    }
    catch (const cxxopts::OptionException& e)
    {
	std::cerr << "error parsing options: " << e.what() << std::endl;
	return -1;
    }


    if (argc != 2) {
	std::cerr << "Usage: verify [options] <git_repo_full_path>\n";
	return -1;
    }


    std::set<std::string> done_sha1;
    if (done_list.length()) {
	get_done_sha1s(done_sha1, done_list);
    }

    std::map<std::string, std::string> sha12branch;
    if (keymap.length()) {
	std::map<std::string, std::string> key2sha1;
	read_key_sha1_map(key2sha1, keymap);
	if (branchmap.length()) {
	    read_branch_sha1_map(sha12branch, key2sha1, branchmap);
	}
    }


    // Set up working git repo
    std::string git_repo(argv[1]);
    std::string git_working("git_working");
    std::string git_init = std::string("git clone ") + git_repo + std::string(" ") + git_working;


    // Get the necessary information
    std::string get_git_info = std::string("cd ") + git_repo + std::string(" && git log --all --pretty=format:\"GITCOMMIT%n%H%n%ct%n%ci%n%B%n%N%nGITCOMMITEND%n\" > ../commits.txt && cd ..");
    ret = std::system(get_git_info.c_str());
    if (ret) {
	std::cerr << "git commit listing failed!\n";
	return -1;
    }

    std::vector<cmp_info> commits;
    parse_git_info(commits, "commits.txt");

    // If we're doing a CVS only check, there's no point in working
    // with newer commits
    if (!svn_repo.length() && cvs_repo.length()) {
	max_timestamp = cvs_maxtime;
    }

    // Figure out min/max timestamps from the min/max revs, if we have them
    std::map<long, long> rev_to_timestamp;
    for (size_t i = 0; i < commits.size(); i++) {
	if (commits[i].svn_rev) {
	    rev_to_timestamp[commits[i].svn_rev] = commits[i].timestamp;
	}
    }
    if (max_rev < LONG_MAX) {
	bool have_timestamp = false;
	int mrev = max_rev;
	while (!have_timestamp && mrev < commits.size()) {
	    if (rev_to_timestamp.find(mrev) != rev_to_timestamp.end()) {
		have_timestamp = true;
		max_timestamp = rev_to_timestamp[mrev];
	    }
	    mrev++;
	}
    }
    if (min_rev) {
	bool have_timestamp = false;
	int mrev = min_rev;
	while (!have_timestamp && mrev > 0) {
	    if (rev_to_timestamp.find(mrev) != rev_to_timestamp.end()) {
		have_timestamp = true;
		min_timestamp = rev_to_timestamp[mrev];
	    }
	    mrev--;
	}
    }

    std::set<std::pair<long, size_t>> timestamp_to_cmp;
    for (size_t i = 0; i < commits.size(); i++) {

	// Skip any commits we've already checked
	if (done_sha1.find(commits[i].sha1) != done_sha1.end()) {
	    continue;
	}

	// Skip any commits that don't meet the criteria
	if (min_timestamp && commits[i].timestamp < min_timestamp) {
	    continue;
	}
	if (max_timestamp != LONG_MAX && commits[i].timestamp > max_timestamp) {
	    continue;
	}


	timestamp_to_cmp.insert(std::make_pair(commits[i].timestamp, i));
	if (commits[i].svn_rev) {
	    std::cout << "Queueing revision " << commits[i].rev << "\n";
	} else {
	    std::cout << "Queueing " << commits[i].sha1 << ", timestamp " << commits[i].timestamp << "\n";
	}
    }

    std::cerr << "Starting verifications...\n";

    std::string dfout;
    if (done_list.length()) {
	dfout = done_list;
    } else {
	dfout = std::string("done_sha1.txt");
    }
    std::ofstream ofile(dfout, std::ios_base::app);
    if (!ofile.good()) {
	std::cerr << "Couldn't open " << dfout << " for writing.\n";
	exit(1);
    }

    std::set<std::pair<long, size_t>>::reverse_iterator r_it;
    for(r_it = timestamp_to_cmp.rbegin(); r_it != timestamp_to_cmp.rend(); r_it++) {
	int cvs_err = 0;
	int svn_err = 0;
	cmp_info &info = commits[r_it->second];

	if (info.rev.length()) {
	    std::cout << "Checking SVN revision " << info.rev << "\n";
	} else {
	    std::cout << "Checking non-SVN commit, timestamp " << r_it->first << "\n";
	}

	if (info.timestamp < cvs_maxtime) {
	    info.branch_svn = sha12branch[info.sha1];
	    if (!info.branch_svn.length()) {
		info.branch_svn = std::string("master");
	    }
	}

	// Git checkout
	std::string git_checkout = std::string("cd ") + git_repo + std::string(" && git checkout --quiet ") + info.sha1 + std::string(" && cd ..");
	info.git_check_cmds.append(git_checkout);
	info.git_check_cmds.append(std::string("\n"));
	if (std::system(git_checkout.c_str())) {
	    std::cerr << "git checkout failed!:\n" << git_checkout << "\n";
	    exit(1);
	}

	// If we're old enough and have the cvs repository, check it
	if (cvs_repo.length() && info.timestamp < cvs_maxtime) {
	    cvs_err = verify_repos_cvs(cvs_problem_sha1s, info, git_repo, cvs_repo);
	}

	// If we have the SVN repo and a revision, check SVN
	if (svn_repo.length() && info.rev.length()) {
	    svn_err = verify_repos_svn(info, git_repo, svn_repo);
	}

	// If we saw any errors, report the commands that prompted them:
	if (cvs_err || svn_err) {
	    std::cerr << "Differences found (" << info.sha1 << "):\n";
	    std::cerr << "Git checkout command:\n\t" << info.git_check_cmds << "\n";
	    if (cvs_err) {
		std::cerr << "CVS check cmds:\n\t" << info.cvs_check_cmds << "\n";
	    } else {
		if (cvs_repo.length() && info.timestamp < cvs_maxtime) {
		    std::cerr << "CVS check: OK\n";
		}
	    }
	    if (svn_err) {
		std::cerr << "SVN check cmds:\n\t" << info.svn_check_cmds << "\n";
	    } else {
		if (svn_repo.length() && info.rev.length()) {
		    std::cerr << "SVN check: OK\n";
		}
	    }
	}

	ofile << info.sha1 << "\n";
	ofile.flush();

    }
    ofile.close();

    cvs_problem_sha1s.close();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
