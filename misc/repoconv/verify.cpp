#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <regex>

int verify_repos(std::string rev, std::string branch_svn, std::string sha1, std::string git_repo, std::string svn_repo)
{
    int ret = 0;
    std::string git_fi;
    std::string branch_git;

    std::map<std::string, std::string> branch_mappings;
    branch_mappings[std::string("trunk")] = std::string("master");
    branch_mappings[std::string("dmtogl-branch")] = std::string("dmtogl");

    if (branch_mappings.find(branch_svn) != branch_mappings.end()) {
	branch_git = branch_mappings[branch_svn];
    }

    std::cout << "Verifying r" << rev << ", branch " << branch_svn << "\n";

    // First, check out the correct SVN tree
    std::string svn_cmd;
    if (branch_svn == std::string("trunk")) {
	svn_cmd = std::string("svn co -q -r") + rev + std::string(" file://") + svn_repo + std::string("/brlcad/trunk brlcad_svn_checkout");
    } else {
	svn_cmd = std::string("svn co -q file://") + svn_repo + std::string("/brlcad/branches/") + branch_svn + std::string("@") + rev + std::string(" brlcad_svn_checkout");
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
    std::string git_checkout = std::string("cd ") + git_repo + std::string(" && git checkout ") + sha1 + std::string(" && cd ..");
    if (std::system(git_checkout.c_str())) {
	std::cerr << "git checkout failed!\n";
	exit(1);
    }

    // Have both, do diff
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" --exclude \"saxon65.jar\" --exclude \"xalan27.jar\" brlcad_svn_checkout ") + git_repo;
    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cout << "diff test failed, r" << rev << ", branch " << branch_svn << "\n";
        exit(1);
    }

    return 0;
}

int main(int argc, const char *argv[])
{
    int ret;
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
    std::string sha1;
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

	std::string branch("trunk");
	std::regex branch_regex(".*svn:branch:([a-zA-Z0-9_-]+).*");
	std::smatch bmatch;
	if (std::regex_search(msg, bmatch, branch_regex)) {
	    branch = std::string(bmatch[1]);
	}

	std::cout << "Branch " << branch << ", rev: " << rev << "\n";
	if (std::stol(rev) < 29866) {
	    std::cout << "Revisions from the CVS era are problematic, skipping " << rev << "\n";
	    continue;
	}
	verify_repos(rev, branch, sha1, git_repo, svn_repo);
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
