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
#include <sstream>
#include <string>
#include "cxxopts.hpp"

class cmp_info {
    public:
	std::string rev;
	std::string branch_svn;
	std::string sha1;
	std::string cvs_date;

	std::string cvs_check_cmds;
	std::string git_check_cmds;
	std::string svn_check_cmds;
};

int verify_repos_cvs(cmp_info &info, std::string git_repo, std::string cvs_repo) {
    std::string cvs_cmd;
    if (info.branch_svn == std::string("trunk")) {
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
	std::cerr << "cvs checkout failed!\n";
	if (std::system(cleanup_cmd.c_str())) {
	    std::cerr << "verify cleanup failed!\n";
	}
	exit(1);
    }

    // Have both, do diff
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'\\$Source' -I'\\$Date' -I'\\$Log' -I'\\$Locker' --exclude \"CVS\" --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" --exclude \"saxon65.jar\" --exclude \"xalan27.jar\" brlcad ") + git_repo;


    info.cvs_check_cmds.append(repo_diff);
    info.cvs_check_cmds.append(std::string("\n"));

    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cerr << "CVS vs Git: diff test failed, r" << info.rev << ", branch " << info.branch_svn << "\n";
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

int main(int argc, char *argv[])
{
    int ret;
    int start_rev = INT_MAX;
    std::string cvs_repo = std::string();

    try
    {
	cxxopts::Options options(argv[0], " - verify svn->git conversion");

	options.add_options()
	    ("cvs-repo", "Use the specified CVS repository for checks", cxxopts::value<std::vector<std::string>>(), "path to repo")
	    ("s,start-rev", "Skip any revision higher than this number", cxxopts::value<int>(), "#")
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

	if (result.count("s"))
	{
	    start_rev = result["s"].as<int>();
	}

    }
    catch (const cxxopts::OptionException& e)
    {
	std::cerr << "error parsing options: " << e.what() << std::endl;
	return -1;
    }


    if (argc != 3) {
	std::cerr << "Usage: verify [options] <git_repo_full_path> <svn_repo_full_path>\n";
	return -1;
    }

    std::string svn_repo(argv[2]);
    std::string git_repo(argv[1]);
    std::string list_sha1 = std::string("cd ") + git_repo + std::string(" && git log --all --pretty=format:\"%H\" > ../commits.txt && cd ..");
    ret = std::system(list_sha1.c_str());
    if (ret) {
	std::cerr << "sha1 listing failed!\n";
	return -1;
    }

    // Set up working git repo
    std::string git_working("git_working");
    std::string git_init = std::string("git clone ") + git_repo + std::string(" ") + git_working;

    // Build up a map of SVN revs to SHA1 ids.  We'll work in SVN order for a more intuitive result
    std::ifstream infile("commits.txt", std::ifstream::binary);
    if (!infile.good()) {
        std::cerr << "Could not open sha1 file: commits.txt\n";
        exit(-1);
    }


    std::map<int, cmp_info> rev_to_cmp;

    std::string sha1;
    std::cout << "Building test pairing information...\n";
    while (std::getline(infile, sha1)) {
        // Skip empty lines
        if (!sha1.length()) {
            continue;
        }

	// Get commit msg
	std::string get_msg = std::string("cd ") + git_repo + std::string(" && git log -1 " + sha1 + " --pretty=format:\"%B\" > ../msg.txt && cd ..");
	ret = std::system(get_msg.c_str());
	if (ret) {
	    std::cerr << "getting git commit message failed!\n";
	    return -1;
	}

	std::ifstream msg_infile("msg.txt");
	if (!msg_infile.good()) {
	    std::cerr << "Could not open msg.txt file\n";
	    exit(-1);
	}

	std::string msg((std::istreambuf_iterator<char>(msg_infile)), std::istreambuf_iterator<char>());
	msg_infile.close();


	std::regex revnum_regex(".*svn:revision:([0-9]+).*");
	std::smatch rmatch;
	if (!std::regex_search(msg, rmatch, revnum_regex)) {
	    std::cerr << "No svn id found for " << sha1 << ", skipping verification\n";
	    continue;
	}
	std::string rev = std::string(rmatch[1]);

	if (std::stol(rev) > start_rev) {
	    continue;
	}

	// svn branch deletes can't be verified by checkout, skip those
	std::regex bdelete_regex(".*svn branch delete.*");
	std::smatch bd_match;
	if (std::regex_search(msg, bd_match, bdelete_regex)) {
	    std::cerr << rev << " is a branch delete commit, skipping verification\n";
	    continue;
	}

	std::string branch("trunk");
	std::regex branch_regex(".*svn:branch:([a-zA-Z0-9_-]+).*");
	std::smatch bmatch;
	if (std::regex_search(msg, bmatch, branch_regex)) {
	    branch = std::string(bmatch[1]);
	}

	cmp_info info;
	info.rev = rev;
	info.branch_svn = branch;
	info.sha1 = sha1;

	// If old enough and we have a CVS repo to check against, get CVS compatible date
	if (std::stol(rev) < 29866 && cvs_repo.length()) {
	    std::string get_date = std::string("cd ") + git_repo + std::string(" && git log -1 " + sha1 + " --pretty=format:\"%ci\" > ../date.txt && cd ..");
	    ret = std::system(get_date.c_str());
	    if (ret) {
		std::cerr << "getting git commit date failed!\n";
		return -1;
	    }

	    std::ifstream date_infile("date.txt");
	    if (!date_infile.good()) {
		std::cerr << "Could not open date.txt file\n";
		exit(-1);
	    }

	    std::string date((std::istreambuf_iterator<char>(date_infile)), std::istreambuf_iterator<char>());
	    date_infile.close();

	    info.cvs_date = date;
	    //std::cout << "Date(" << rev << "): " << info.cvs_date << "\n";
	} else {
	    info.cvs_date = std::string();
	}

	rev_to_cmp[std::stol(rev)] = info;
    }

    std::cerr << "Starting verifications...\n";

    std::map<int, cmp_info>::reverse_iterator r_it;
    for(r_it = rev_to_cmp.rbegin(); r_it != rev_to_cmp.rend(); r_it++) {
	int cvs_err = 0;
	int svn_err = 1;
	cmp_info &info = r_it->second;

	std::cout << "Check SVN revision " << info.rev << "\n";

	// Git checkout
	std::string git_checkout = std::string("cd ") + git_repo + std::string(" && git checkout --quiet ") + info.sha1 + std::string(" && cd ..");
	info.git_check_cmds.append(git_checkout);
	info.git_check_cmds.append(std::string("\n"));
	if (std::system(git_checkout.c_str())) {
	    std::cerr << "git checkout failed!:\n" << git_checkout << "\n";
	    exit(1);
	}

	// If we're old enough and have the cvs repository, check it
	if (info.cvs_date.length() && std::stol(info.rev) < 29866) {
	    cvs_err = verify_repos_cvs(info, git_repo, cvs_repo);
	}

	// Always check the SVN repository
	svn_err = verify_repos_svn(info, git_repo, svn_repo);

	// If we saw any errors, report the commands that prompted them:
	if (cvs_err || svn_err) {
	    std::cerr << "Differences found:\n";
	    std::cerr << "Git checkout command:\n\t" << info.git_check_cmds << "\n";
	    if (cvs_err) {
		std::cerr << "CVS check cmds:\n\t" << info.cvs_check_cmds << "\n";
	    } else {
		if (info.cvs_date.length() && std::stol(info.rev) < 29866) {
		    std::cerr << "CVS check: OK\n";
		}
	    }
	    if (svn_err) {
		std::cerr << "SVN check cmds:\n\t" << info.svn_check_cmds << "\n";
	    } else {
		std::cerr << "SVN check: OK\n";
	    }
	}

    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
