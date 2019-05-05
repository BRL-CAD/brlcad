#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include <iomanip>
#include <regex>
#include <sys/stat.h>
#include "sha1.hpp"
#include "svn_date.h"

long int starting_rev;

enum svn_node_kind_t { nkerr, nfile, ndir };
enum svn_node_action_t { naerr, nchange, nadd, ndelete, nreplace };

struct svn_node {
    /* Dump file contents */
    long int revision_number;
    std::string path;
    svn_node_kind_t kind;
    svn_node_action_t action;
    long int copyfrom_rev;
    std::string copyfrom_path;
    std::string text_copy_source_md5;
    std::string text_copy_source_sha1;
    std::string text_content_md5;
    std::string text_content_sha1;
    long int text_content_length;
    long int prop_content_length;
    long int content_length;
    std::map<std::string, std::string> node_props;
    size_t content_start; // offset position in dump file
    /* Information from analysis */
    int exec_path;
    int exec_change;
    int crlf_content;
    int move_edit;
    int tag_edit;
    int branch_add;
    int branch_delete;
    int tag_add;
    int tag_delete;
    int tag_path;
    std::string project;
    std::string branch;
    std::string tag;
    std::string local_path;
};

struct svn_revision {
    /* Dump file contents */
    long int revision_number;
    std::string project;
    std::string author;
    std::string timestamp;
    std::string commit_msg;
    std::vector<struct svn_node> nodes;
    /* Information from analysis */
    std::set<std::string> merges; // TODO - can this info live at the node level?
    int move_edit;  // Has revision level implications - set if any child nodes are set
    int tag_edit; // Has revision level implications - set if any child nodes are set
    int merge;
    std::string merged_from;
    long int merged_rev;
};


std::map<std::string,std::pair<size_t, long int>> sha1_blobs;
std::set<std::string> exec_paths;
std::map<std::string,std::string> current_sha1;
std::map<std::string,std::string> tag_ids;
std::map<std::string,std::string> svn_sha1_to_git_sha1;

std::set<std::string> cvs_blob_sha1;
std::map<std::string,std::string> author_map;

std::map<std::string, std::map<long int, std::set<struct svn_node *> *>> path_states;
std::map<std::string, std::map<long int, std::set<struct svn_node *> *>>::iterator ps_it;
std::set<long int> rebuild_revs;


/* Branches */
std::set<std::string> branches;
/* Tags */
std::set<std::string> tags;

/* Subversion allows as a technical matter the editing of tags, since
 * they are just more paths copied into specific directories.  Build
 * up some sets with relevant information */
std::set<std::string> edited_tags;
/* The last subversion revision that makes a change to the tag */
std::map<std::string, long int> edited_tag_max_rev;
std::map<std::string, long int> edited_tag_first_rev;

std::map<long int, struct svn_revision> revs;


/* Holds strings identifying all known projects in the svn repo */
std::set<std::string> valid_projects;

std::set<long int> brlcad_revs;

std::map<std::string, std::string> branch_mappings;
std::map<std::string, std::string> tag_mappings;

std::map<std::pair<std::string,long int>, std::string> rev_to_gsha1;

int verify_repos(long int rev, std::string branch_svn, std::string branch_git, int already_applied)
{
    int ret = 0;
    std::string git_fi;

    std::string cleanup_cmd = std::string("rm -rf brlcad_svn_checkout brlcad_git_checkout");
    std::string svn_cmd;
    if (branch_svn == std::string("trunk") || branch_svn == std::string("master")) {
	svn_cmd = std::string("svn co -q -r") + std::to_string(rev) + std::string(" file:///home/cyapp/brlcad_repo/repo_dercs/brlcad/trunk brlcad_svn_checkout");
    } else {
	svn_cmd = std::string("svn co -q file:///home/cyapp/brlcad_repo/repo_dercs/brlcad/branches/") + branch_svn + std::string("@") + std::to_string(rev) + std::string(" brlcad_svn_checkout");
    }
    std::string svn_emptydir_rm = std::string("find brlcad_svn_checkout -type d -empty -print0 |xargs -0 rmdir");
    std::string git_emptydir_rm = std::string("find brlcad_git_checkout -type d -empty -print0 |xargs -0 rmdir");
    std::string git_clone;
    if (branch_git == std::string("trunk") || branch_git == std::string("master")) {
	git_clone = std::string("git clone --single-branch --branch master ./cvs_git_working/.git brlcad_git_checkout");
    } else {
	git_clone = std::string("git clone --single-branch --branch ") + branch_git + std::string(" ./cvs_git_working/.git brlcad_git_checkout");
    }
    std::string repo_diff = std::string("diff --no-dereference -qrw -I '\\$Id' -I '\\$Revision' -I'\\$Header' -I'$Source' -I'$Date' -I'$Log' -I'$Locker' --exclude \".cvsignore\" --exclude \".gitignore\" --exclude \"terra.dsp\" --exclude \".git\" --exclude \".svn\" brlcad_svn_checkout brlcad_git_checkout");
    std::cout << "Verifying r" << rev << ", branch " << branch_svn << "\n";
    if (std::system(cleanup_cmd.c_str())) {
	std::cerr << "verify cleanup failed!\n";
	exit(1);
    }
    if (std::system(svn_cmd.c_str())) {
	std::cerr << "svn checkout failed!\n";
	if (std::system(cleanup_cmd.c_str())) {
	    std::cerr << "verify cleanup failed!\n";
	}
	return 0;
    }
    ret = std::system(svn_emptydir_rm.c_str());
    while (!ret) {
	ret = std::system(svn_emptydir_rm.c_str());
    }
    ret = std::system(git_emptydir_rm.c_str());
    while (!ret) {
	ret = std::system(git_emptydir_rm.c_str());
    }
    if (rev > starting_rev) {
	struct stat buffer;
	std::string fi_file = std::to_string(rev) + std::string(".fi");
	if (stat(fi_file.c_str(), &buffer) == 0) {
	    if (!already_applied) {
		git_fi = std::string("cd cvs_git_working && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
	    } else {
		git_fi = std::string("cd cvs_git_working && git reset --hard HEAD && cd ..");
	    }
	    if (std::system(git_fi.c_str())) {
		std::string failed_file = std::string("failed-") + fi_file;
		std::cout << "Fatal - could not apply fi file for revision " << rev << "\n";
		rename(fi_file.c_str(), failed_file.c_str());
		exit(1);
	    }
	}
    }
    if (std::system(git_clone.c_str())) {
	std::cerr << "git clone failed!\n";
	exit(1);
    }
    int diff_ret = std::system(repo_diff.c_str());
    if (diff_ret) {
        std::cout << "diff test failed, r" << rev << ", branch " << branch_svn << "\n";
        exit(1);
    }

    std::string fi_file = std::to_string(rev) + std::string(".fi");
    if (!already_applied) {
	git_fi = std::string("cd cvs_git && cat ../") + fi_file + std::string(" | git fast-import && git reset --hard HEAD && cd ..");
    } else {
	git_fi = std::string("cd cvs_git && git reset --hard HEAD && cd ..");
    }
    if (std::system(git_fi.c_str())) {
	std::string failed_file = std::string("failed-") + fi_file;
	std::cout << "Fatal - could not apply fi file for revision " << rev << "\n";
	rename(fi_file.c_str(), failed_file.c_str());
	exit(1);
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
