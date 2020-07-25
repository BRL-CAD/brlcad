#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <regex>
#include "cxxopts.hpp"

class cmp_info {
    public:
	std::string rev;
	std::string branch_svn;
	std::string sha1;
};

int verify_repos(cmp_info &info, std::string git_repo, std::string svn_repo)
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
    std::string svn_emptydir_rm = std::string("find brlcad_svn_checkout -type d -empty -print0 |xargs -0 rmdir");
    ret = std::system(svn_emptydir_rm.c_str());
    while (!ret) {
	ret = std::system(svn_emptydir_rm.c_str());
    }

    // Have SVN, get Git
    std::string git_checkout = std::string("cd ") + git_repo + std::string(" && git checkout ") + info.sha1 + std::string(" && cd ..");
    if (std::system(git_checkout.c_str())) {
	std::cerr << "git checkout failed!\n";
	exit(1);
    }

    // Have both, do diff
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" --exclude \"saxon65.jar\" --exclude \"xalan27.jar\" brlcad_svn_checkout ") + git_repo;
    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cout << "diff test failed, r" << info.rev << ", branch " << info.branch_svn << "\n";
        exit(1);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret;
    int start_rev = 100000;

    try
    {
	cxxopts::Options options(argv[0], " - verify svn->git conversion");

	options.add_options()
	    ("s,start-rev", "Skip any revision higher than this number", cxxopts::value<int>(), "#")
	    ("h,help", "Print help")
	    ;

	auto result = options.parse(argc, argv);

	if (result.count("help"))
	{
	    std::cout << options.help({""}) << std::endl;
	    return 0;
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
	std::cerr << "Usage: verify <git_repo_full_path> <svn_repo_full_path>\n";
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
	rev_to_cmp[std::stol(rev)] = info;

    }

    std::cerr << "Starting verifications...\n";

    std::map<int, cmp_info>::reverse_iterator r_it;
    for(r_it = rev_to_cmp.rbegin(); r_it != rev_to_cmp.rend(); r_it++) {

	if (std::stol(r_it->second.rev) < 29866) {
	    std::cout << "Revisions from the CVS era are problematic, stopping here.\n";
	    exit(0);
	}

	verify_repos(r_it->second, git_repo, svn_repo);
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
