/*        S V N _ M O V E D _ A N D _ E D I T E D . C X X
 *
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file svn_moved_and_edited.cxx
 *
 * Searches an SVN dump file for revisions with Node-paths that have both a
 * Note-copyfrom-path property and a non-zero Content-length - the idea being
 * that these are changes were a file was both moved and changed.
 *
 * These are potentially problematic commits when converting to git, since
 * they could break git log --follow
 */

#include "pstream.h"
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <set>
#include <map>
#include <sys/stat.h>

void cvs_init()
{
    std::system("rm -rf brlcad_cvs");
    std::system("tar -xf brlcad_cvs.tar.gz");
    std::system("rm brlcad_cvs/brlcad/src/librt/Attic/parse.c,v");
    std::system("rm brlcad_cvs/brlcad/pix/sphflake.pix,v");
    std::system("cp repaired/sphflake.pix,v brlcad_cvs/brlcad/pix/");
    // RCS headers introduce unnecessary file differences, and file differences
    // can be poison pills for git log --follow
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Date:[^$]*/$Date:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Header:[^$]*/$Header:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Id:[^$]*/$Id:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Log:[^$]*/$Log:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Revision:[^$]*/$Revision:/' {} \\;");
    std::system("find brlcad_cvs/brlcad/ -type f -exec sed -i 's/$Source:[^$]*/$Source:/' {} \\;");
    std::system("sed -i 's/$Author:[^$]*/$Author:/' brlcad_cvs/brlcad/misc/Attic/cvs2cl.pl,v");
    std::system("sed -i 's/$Author:[^$]*/$Author:/' brlcad_cvs/brlcad/sh/Attic/cvs2cl.pl,v");
    std::system("sed -i 's/$Locker:[^$]*/$Locker:/' brlcad_cvs/brlcad/src/other/URToolkit/tools/mallocNd.c,v");
}

void cvs_to_git()
{
    std::system("find brlcad_cvs/brlcad/ | cvs-fast-export -A authormap > brlcad_git.fi");
    std::system("rm -rf brlcad_git");
    std::system("mkdir brlcad_git");
    chdir("brlcad_git");
    std::system("git init");
    std::system("cat ../brlcad_git.fi | git fast-import");
    std::system("git checkout master");
    // This repository has as its newest commit a "test acl" commit that doesn't
    // appear in subversion - the newest master commit matching subversion corresponds
    // to r29886.  To clear this commit and label the new head with its corresponding
    // subversion revision, we do:
    std::system("git reset --hard HEAD~1");
    // Done
    chdir("..");
}

std::string
svn_to_git(std::string &path)
{
    std::string npath = path;
    if (!npath.compare(0, 1, "/")) npath.replace(0, 1, "");
    if (!npath.compare(0, 7, "brlcad/")) npath.replace(0, 7, "");;
    if (!npath.compare("trunk")) return std::string("master");
    if (!npath.compare(0, 6, "trunk/")) {
   	npath.replace(0, 6, "");
	return npath;
    }
    if (!npath.compare(0, 9, "branches/")) {
	npath.replace(0, 9, "");
	return npath;
    }
    if (!npath.compare(0, 5, "tags/")) {
	npath.replace(0, 5, "");
	return npath;
    }
}


std::string
git_to_svn(std::string &gin, std::string &branch, int is_tag)
{
    if (!branch.compare("master")) {
	if (gin.length()) {
	    std::string npath = "brlcad/trunk/" + gin;
	    return npath;
	} else {
	    return std::string("brlcad/trunk/");
	}
    }
    if (!is_tag) {
	std::string npath = "brlcad/branches/" + branch;
	if (gin.length()) {
	    npath.append("/");
	    npath.append(gin);
	}
	return npath;
    } else {
	std::string npath = "brlcad/tags/" + branch;
	if (gin.length()) {
	    npath.append("/");
	    npath.append(gin);
	} 
	return npath;
    }
}


int
git_tree_changed()
{
    std::string mods;
    redi::ipstream proc("git status --porcelain", redi::pstreams::pstdout);
    std::getline(proc.out(), mods);
    return mods.length();
}

int
git_commit(const char *svn_repo, int svn_rev, std::string &svn_branch, std::string &logmsg, std::string &author, std::string &date)
{
    // Pull patch from SVN
    std::string diffcmd = "svn diff -c" + std::to_string(svn_rev) + " file://" + svn_repo + "/brlcad/" + svn_branch + " > ../svn.diff";
    std::cout << diffcmd << "\n";
    std::system(diffcmd.c_str());
    std::cout << std::endl;

    // TODO - squash RCSid strings out of patches - otherwise will not apply cleanly...  this only
    // needs to be done up until they're removed from SVN history

    // Make sure we're in the right working branch
    std::cout << "Switching to " << svn_to_git(svn_branch) << " (svn branch " << svn_branch << ")\n";
    std::string bco = "git checkout " + svn_to_git(svn_branch);
    std::system(bco.c_str());
    std::cout << std::endl;

    // Apply the patch
    std::cout << "Patch file contains: \n";
    std::cout << std::endl;
    std::system("cat ../svn.diff");
    std::cout << std::endl;
    std::system("patch -f --remove-empty-files -p0 < ../svn.diff");
    std::cout << std::endl;

    // See what happened
    if (!git_tree_changed()) {
	std::cout << "*******************************\n";
	std::cout << "Warning - commit " << svn_rev << " is a no op!\n";
	std::cout << date << "\n";
	std::cout << std::endl;
	std::system("cat ../msg.txt");
	std::cout << std::endl;
	std::cout << author << "\n";
	std::cout << "*******************************\n";
	std::cout << std::endl;
	return 0;
    }
    //std::string commit_author = (author.length()) ? amap[author] : "Unknown <unknown@unknown>";
    std::string commit_author = "Unknown <unknown@unknown>";
    std::system("git add -A");
    std::cout << std::endl;
    if (logmsg.length()) {
	std::string git_cmd = "git commit -F ../msg.txt --author=\"" + commit_author + "\" --date=" + date;
	std::system(git_cmd.c_str());
	std::cout << std::endl;
    } else {
	std::string git_cmd = "git commit -m \"empty commit message\" --author=\"" + commit_author + "\" --date=" + date;
	std::system(git_cmd.c_str());
	std::cout << std::endl;
    }
    std::string git_note_cmd = "git notes add -m \"svn:revision:" + std::to_string(svn_rev) + " svn:author:" + author;
    if (svn_branch.length()) {
	// append svn branch if we have it
	git_note_cmd.append(" svn:branch:");
	git_note_cmd.append(svn_branch);
    }
    git_note_cmd.append("\"");
    std::system(git_note_cmd.c_str());
    std::cout << std::endl;
}

int
git_tag(std::string &tag_msg, std::string &author, std::string &date, std::string &svn_branch)
{
    std::string tag_author = (author.length()) ? author : "Unknown <unknown@unknown>";

}

void
git_make_branch(std::string &bname, int rev, std::string &author)
{
    std::string bcheck = "git show-ref --verify --quiet refs/heads/" + bname;
    if (!std::system(bcheck.c_str())) {
	std::cerr << "Error - branch " << bname << " already exists in repository\n";
	return;
    }
    // TODO - should we check if a "deleted" version of this branch exists, and if
    // so restore it rather than making a new branch? There are several cases in SVN
    // where STABLE was deleted and restored to try and clear out merge messes, so
    // in those cases it might be more appropriate to "restore" the removed branch...
    std::string bmake = "git branch " + bname;
    std::system(bmake.c_str());
    std::string bco = "git checkout " + bname;
    std::system(bco.c_str());
    std::string git_note_cmd = "git notes append -m \"Branch " + bname + "added: svn:revision:" + std::to_string(rev) + " svn:author:" + author + "\nsvn:branchmsg:\n\"";
    std::system(git_note_cmd.c_str());
    std::system("git notes append -F ../msg.txt");
}

void
git_delete_branch(std::string &bname, int rev, std::string &rev_date)
{
    std::string tname = bname + "-" + rev_date;
    std::string bcheck = "git show-ref --verify --quiet refs/heads/" + bname;
    if (std::system(bcheck.c_str())) {
	std::cerr << "Error - branch " << bname << " does not exist in the repository\n";
	return;
    }
    // Because we want to preserve our history, deleted branches are first
    // tagged with an "archived/" prefix so they can be recovered later.
    // ("Deleting" a branch for BRL-CAD is simply getting it out of the
    // "active" branch list reported by git branch, not completely removing it
    // from the repository history.)
    //
    // TODO - the below will preserve all deletes as separate archived tags, even
    // if the branch is deleted and re-created multiple times.  Should we instead
    // restore the branch from the tag if it is recreated, and simply delete the old
    // archived tag and add a new one if it is re-deleted?
    std::string tcheck = "git show-ref --verify --quiet refs/tags/archived/" + tname;
    if (!std::system(bcheck.c_str())) {
	std::cerr << "Error - " << tname << " already exists\n";
	return;
    }
    std::string tcmd = "git tag archived/" + tname + " " + bname;
    std::system(tcmd.c_str());
    std::string dcmd = "git branch -D " + bname;
    std::system(dcmd.c_str());
}

void
svn_patch(const char *repo, std::map<int, std::string> &bmap, int rev)
{
    std::string master = "trunk";
    if (bmap.find(rev) == bmap.end()) {
	std::cout << "Error - no known branch for commit " << rev << "\n";
	return;
    }
    std::string branch = bmap[rev];

    if (!master.compare(branch)) {
	std::system("git checkout master");
    } else {
	std::string git_branch = branch.substr(10);
	std::string git_bcmd = "git checkout " + git_branch;
	std::system(git_bcmd.c_str());
    }

    std::string patchcmd = "svn diff -c" + std::to_string(rev) + " file://" + std::string(repo) + "/brlcad/" + branch + " | patch -f --remove-empty-files -p 0";
}

int
path_is_branch(std::string path)
{
    if (path.length() <= 15 || path.compare(0, 15, "brlcad/branches")) return 0;
    std::string spath = path.substr(16);
    size_t slashpos = spath.find_first_of('/');
    return (slashpos == std::string::npos) ? 1 : 0;
}

std::string
path_get_branch(std::string path)
{
    std::string empty = "";
    if (path.length() <= 12) return empty;
    if (!path.compare(0, 12, "brlcad/trunk")) {
	return std::string("trunk");
    }
    if (path.length() <= 15) return empty;
    if (!path.compare(0, 15, "brlcad/branches")) {
	std::string spath = path.substr(16);
	std::string bpath = "branches/";
	size_t slashpos = spath.find_first_of('/');
	if (slashpos == std::string::npos) {
	    if (spath.length() > 0) {
		bpath.append(spath);
		return bpath;
	    } else {
		return empty;
	    }
	} else {
	    std::string bsubstr = spath.substr(0, slashpos);
	    bpath.append(bsubstr);
	    return bpath;
	}
    }
    return empty;
}

int
path_is_tag(std::string path)
{
    if (path.length() <= 11 || path.compare(0, 11, "brlcad/tags")) return 0;
    std::string spath = path.substr(12);
    size_t slashpos = spath.find_first_of('/');
    return (slashpos == std::string::npos) ? 1 : 0;
}

std::string
path_get_tag(std::string path)
{
    std::string empty = "";
    if (path.length() <= 11) return empty;
    if (!path.compare(0, 11, "brlcad/tags")) {
	std::string spath = path.substr(12);
	size_t slashpos = spath.find_first_of('/');
	if (slashpos == std::string::npos) {
	    if (spath.length() > 0) {
		return spath;
	    } else {
		return empty;
	    }
	} else {
	    std::string bsubstr = spath.substr(0, slashpos);
	    return bsubstr;
	}
    }
    return empty;
}

struct node_info {
    int in_branch = 0;
    int in_tag = 0;
    int have_node_path = 0;
    int is_add = 0;
    int is_delete = 0;
    int is_edit = 0;
    int is_file = 0;
    int is_move = 0;
    int merge_commit = 0;
    std::string from_path;
    int node_path_filtered = 0;
    int move_edit_rev = 0;
    int rev = -1;
    std::string node_path;
    std::string node_copyfrom_path;
};

struct commit_info {
    std::map<int, std::string> commit_branch;
    std::set<int> multi_branch_commits;
    std::map<int, std::set<std::string> > multi_branch_branches;
    std::set<int> merge_commits;
    std::map<int, std::string > merge_commit_from;
    std::set<int> branch_deletes;
    std::multimap<int, std::string > branch_delete_paths;
    std::set<int> branch_adds;
    std::multimap<int, std::string > branch_add_paths;
    std::set<int> tag_adds;
    std::multimap<int, std::string > tag_add_paths;
    std::set<int> tag_deletes;
    std::multimap<int, std::string > tag_delete_paths;
    std::set<int> tag_edits;
    std::set<std::string> edited_tags;
    std::map<std::string, int> max_rev_tag_edit;
    std::multimap<int, std::string > tag_edit_paths;
    std::set<int> move_edit_revs;
    std::multimap<int, std::pair<std::string, std::string> > revs_move_only_map;
    std::multimap<int, std::pair<std::string, std::string> > revs_move_edit_map;
    std::multimap<int, std::string > revs_edit_only_map;
};

void node_info_reset(struct node_info *i)
{
    i->in_branch = 0;
    i->in_tag = 0;
    i->have_node_path = 0;
    i->is_add = 0;
    i->is_delete = 0;
    i->is_edit = 0;
    i->is_file = 0;
    i->is_move = 0;
    i->merge_commit = 0;
    i->node_path_filtered = 0;
    i->move_edit_rev = 0;
    i->rev = -1;
    i->from_path.clear();
    i->node_path.clear();
    i->node_copyfrom_path.clear();
}

void process_node(struct node_info *i, struct commit_info *c)
{
    int branch_delete = 0;
    if (i->is_file) {
	if (i->is_move && i->is_edit) {
	    std::pair<std::string, std::string> move(i->node_copyfrom_path, i->node_path);
	    std::pair<int, std::pair<std::string, std::string> > mvmap(i->rev, move);
	    c->revs_move_edit_map.insert(mvmap);
	    c->move_edit_revs.insert(i->rev);
	} else {
	    if (i->is_move) {
		std::pair<std::string, std::string> move(i->node_copyfrom_path, i->node_path);
		std::pair<int, std::pair<std::string, std::string> > mvmap(i->rev, move);
		c->revs_move_only_map.insert(mvmap);
	    }
	    if (i->is_edit) {
		std::pair<int, std::string > editmap(i->rev, i->node_path);
		c->revs_edit_only_map.insert(editmap);
	    }
	}
    }
    if (i->merge_commit && i->in_branch) {
	if (c->merge_commits.find(i->rev) == c->merge_commits.end()) {
	    c->merge_commits.insert(i->rev);
	    c->merge_commit_from[i->rev] = i->from_path;
	    //std::cerr << "Assigning merged-from branch " << i->from_path << " for commit " << i->rev << "\n";
	}
    }
    if (path_is_branch(i->node_path) && i->is_delete) {
	//std::cout << "Branch delete " << i->rev << ": " << i->node_path << "\n";
	c->branch_deletes.insert(i->rev);
	std::pair<int, std::string > nmap(i->rev, i->node_path);
	c->branch_delete_paths.insert(nmap);
	branch_delete = 1;
    }
    if (path_is_branch(i->node_path) && i->is_add) {
	//std::cout << "Branch add    " << i->rev << ": " << i->node_path << "\n";
	c->branch_adds.insert(i->rev);
	std::pair<int, std::string > nmap(i->rev, i->node_path);
	c->branch_add_paths.insert(nmap);
    }
    if (path_is_tag(i->node_path) && i->is_add) {
	//std::cout << "Tag add       " << i->rev << ": " << i->node_path << "\n";
	c->tag_adds.insert(i->rev);
	std::pair<int, std::string > nmap(i->rev, i->node_path);
	c->tag_add_paths.insert(nmap);
    }
    if (path_is_tag(i->node_path) && i->is_delete) {
	//std::cout << "Tag delete    " << i->rev << ": " << i->node_path << "\n";
	c->tag_deletes.insert(i->rev);
	std::pair<int, std::string > nmap(i->rev, i->node_path);
	c->tag_delete_paths.insert(nmap);
    }
    if (c->tag_adds.find(i->rev) == c->tag_adds.end() && i->in_tag && !(path_is_tag(i->node_path) && i->is_delete)) {
	//std::cout << "Tag edit      " << i->rev << ": " << i->node_path << "\n";
	std::string curr_tag = path_get_tag(i->node_path);
	if (curr_tag.length()) {
	    c->tag_edits.insert(i->rev);
	    std::cerr << "adding tag " << curr_tag << "\n";
	    c->edited_tags.insert(curr_tag);
	    c->max_rev_tag_edit[curr_tag] = i->rev;
	    std::pair<int, std::string > nmap(i->rev, i->node_path);
	    c->tag_edit_paths.insert(nmap);
	} else {
	    //std::cerr << "Tag edit detected on " << i->rev << "," << i->node_path << " but could not determine current tag\n";
	}
    }

    if (!branch_delete) {
	std::string branch = path_get_branch(i->node_path);
	if (branch.length() > 0) {
	    if (c->commit_branch.find(i->rev) != c->commit_branch.end()) {
		std::string existing_branch = c->commit_branch[i->rev];
		if (existing_branch.compare(branch)) {
		    c->multi_branch_commits.insert(i->rev);
		    c->multi_branch_branches[i->rev].insert(existing_branch);
		    c->multi_branch_branches[i->rev].insert(branch);
		}
	    } else {
		c->commit_branch[i->rev] = branch;
	    }
	}
    }

}

int characterize_commits(const char *dfile, struct commit_info *c)
{
    int rev = -1;
    struct node_info info;
    std::ifstream infile(dfile);
    std::string line;
    while (std::getline(infile, line))
    {
	int high_merge_rev = -1;
	std::istringstream ss(line);
	std::string s = ss.str();
	if (!s.compare(0, 16, "Revision-number:")) {
	    if (rev >= 0) {
		// Process last node of previous revision
		process_node(&info, c);
	    }
	    high_merge_rev = -1;
	    node_info_reset(&info);
	    // Grab new revision number
	    rev = std::stoi(s.substr(17));
	    std::cerr << "Analyzing revision " << rev << "...\n";
	}
	if (rev >= 0) {
	    // OK , now we have a revision - start looking for content
	    if (!s.compare(0, 10, "Node-path:")) {
		if (info.have_node_path) {
		    // Process previous node
		    process_node(&info, c);
		}
		// Have a node - initialize
		node_info_reset(&info);
		info.rev = rev;
		info.have_node_path = 1;
		info.node_path = s.substr(11);
		if (info.node_path.compare(0, 6, "brlcad") != 0) {
		    info.node_path_filtered = 1;
		} else {
		    info.node_path_filtered = 0;
		    if (!info.node_path.compare(0, 15, "brlcad/branches")) {
			info.in_branch = 1;
		    }
		    if (!info.node_path.compare(0, 11, "brlcad/tags")) {
			info.in_tag = 1;
		    }
		}
		//std::cout <<  "Node path: " << node_path << "\n";
	    } else {
		if (info.have_node_path && !info.node_path_filtered) {
		    if (!s.compare(0, 19, "Node-copyfrom-path:")) {
			info.is_move = 1;
			info.node_copyfrom_path = s.substr(19);
			//std::cout <<  "Node copyfrom path: " << node_copyfrom_path << "\n";
		    }
		    if (!s.compare(0, 15, "Content-length:")) {
			info.is_edit = 1;
		    }
		    if (!s.compare(0, 15, "Node-kind: file")) {
			info.is_file = 1;
		    }
		    if (!s.compare(0, 19, "Node-action: delete")) {
			info.is_delete = 1;
		    }
		    if (!s.compare(0, 19, "Node-action: add")) {
			info.is_add = 1;
		    }

		}
		if (!info.merge_commit) {
		    if (!s.compare(0, 13, "svn:mergeinfo")) {
			info.merge_commit = 1;
			std::getline(infile, line);
			std::istringstream mss(line);
			std::string ms = mss.str();
			if (ms.compare(0, 3, "V 0")) {
			    // If we have a non-empty mergeinfo property, read it to get the
			    // "from" branch for the merge.
			    std::string fs = "";
			    std::string from_path = "";
			    while (fs.compare(0, 9, "PROPS-END")) {
				std::getline(infile, line);
				std::istringstream fss(line);
				fs = fss.str();
				if (!fs.compare(0, 8, "/brlcad/")) {
				    size_t spos = fs.find_last_of(":-");
				    std::string rev = fs.substr(spos+1);
				    int mrev = std::atoi(rev.c_str());
				    if (mrev > high_merge_rev) {
					fs.replace(0, 8, "");
					if (!fs.compare(0, 9, "branches/")) {
					    fs.replace(0, 9, "");
					} else if (!fs.compare(0, 5, "tags/")) {
					    fs.replace(0, 5, "");
					}
					size_t epos = fs.find_first_of("/:");
					std::string branch = path_get_branch(info.node_path);
					if (!branch.compare(0, 9, "branches/")) {
					    branch.replace(0, 9, "");
					}
					std::string candidate = fs.substr(0, epos);
					if (branch.compare(candidate)) {
					    from_path = fs.substr(0, epos);
					    high_merge_rev = mrev;
					}
				    }
				}
			    }
			    if (from_path.length()) {
				info.from_path = from_path;
			    }
			}
		    }
		}
	    }
	}
    }

    return rev;
}

std::string
revision_date(const char *repo, int rev)
{
    std::string datestr;
    std::string datecmd = "svn propget --revprop -r " + std::to_string(rev) + " svn:date file://" + std::string(repo);
    redi::ipstream proc(datecmd.c_str(), redi::pstreams::pstdout);
    std::getline(proc.out(), datestr);
    return datestr;
}

std::string
revision_author(const char *repo, int rev)
{
    std::string authorstr;
    std::string authorcmd = "svn propget --revprop -r " + std::to_string(rev) + " svn:author file://" + std::string(repo);
    redi::ipstream proc(authorcmd.c_str(), redi::pstreams::pstdout);
    std::getline(proc.out(), authorstr);
    return authorstr;
}

// Provides both in-memory and in-msg.txt-file versions of log msg, for both
// in memory processing and git add -F msg.txt support.
std::string
revision_log_msg(const char *repo, int rev)
{
    std::string logmsg;
    std::string logcmd = "svn log -r" + std::to_string(rev) + " --xml file://" + std::string(repo) + " > ../msg.xml";
    std::system(logcmd.c_str());
    std::system("xsltproc ../svn_logmsg.xsl ../msg.xml > ../msg.txt");
    std::ifstream mstream("../msg.txt");
    std::stringstream mbuffer;
    mbuffer << mstream.rdbuf();
    logmsg = mbuffer.str();
    return logmsg;
}

int main(int argc, const char **argv)
{
    int start_svn_rev = 29887;
    //int start_svn_rev = 1;
    int max_svn_rev = -1;
    struct commit_info c;

    // To obtain a repository copy for processing, do:
    //
    // rsync -av svn.code.sf.net::p/brlcad/code .
    //
    // To produce an efficient dump file for processing, use the
    // incremental and delta options for the subversion dump (we
    // are after the flow of commits and metadata rather than the
    // specific content of the commits, and this produces a much
    // smaller file to parse):
    //
    // svnadmin dump --incremental --deltas /home/user/code > brlcad.dump

    if (argc != 3) {
	std::cerr << "svn_moved_and_edited <dumpfile> <repository_clone>\n";
	return -1;
    }

    // We begin by processing the CVS repository for commits through r29886
    //cvs_init();
    //cvs_to_git();

    // There are several categories of commits in SVN that need particular
    // handling, including:
    //
    // 1.  Branch adds
    // 2.  Branch deletes
    // 3.  Tag adds
    // 4.  Tag deletes
    // 5.  Tags containing subsequent edits
    // 6.  Merge commits (from branch to branch)
    // 7.  Commits that both move and edit a file simultaneously
    //     (these can break git log --follow)
    //
    // Analyze the dump file to spot these cases and build up information
    // about them.
    max_svn_rev = characterize_commits(argv[1], &c);

    // Replay subversion-only commits in git repository, according to the specific
    // commit type.
    chdir("brlcad_git");
    std::multimap<int, std::string>::iterator mmit;
    std::pair<std::multimap<int, std::string>::iterator, std::multimap<int, std::string>::iterator> rr;
    for (int i = start_svn_rev; i <= max_svn_rev; i++) {

	// Get the SVN commit message and prepare msg.txt
	std::string logmsg = revision_log_msg(argv[2], i);

	struct stat buffer;
	std::string rev_script = "../rev_scripts/" + std::to_string(i) + ".sh";
	if (stat(rev_script.c_str(), &buffer) == 0) {
	    // If we have a specific script defined for a particular revision,
	    // run it and continue.  Before running, send endl to cout to make
	    // sure any output from the script ends up at the correct place in
	    // the log
	    std::cout << std::endl;
	    if (std::system(rev_script.c_str())) {
		std::cerr << "Error executing script " << rev_script.c_str() << "\n";
		return -1;
	    }
	    continue;
	}

	// If we're not running a custom script for this revision, get the SVN
	// commit author and date as well
	std::string rauthor = revision_author(argv[2], i);
	std::string rdate = revision_date(argv[2], i);

	if (c.branch_adds.find(i) != c.branch_adds.end()) {
	    std::cout << "Branch add " << i << ":\n";
	    rr = c.branch_add_paths.equal_range(i);
	    for (mmit = rr.first; mmit != rr.second; mmit++) {
		std::cout << "               " << mmit->second << "\n";
	    }
	    continue;
	}
	if (c.branch_deletes.find(i) != c.branch_deletes.end()) {
	    std::cout << "Branch delete " << i << ":\n";
	    rr = c.branch_delete_paths.equal_range(i);
	    for (mmit = rr.first; mmit != rr.second; mmit++) {
		std::cout << "               " << mmit->second << "\n";
	    }
	    continue;
	}
	if (c.tag_adds.find(i) != c.tag_adds.end()) {
	    std::cout << "Tag add " << i << ":\n";
	    rr = c.tag_add_paths.equal_range(i);
	    for (mmit = rr.first; mmit != rr.second; mmit++) {
		std::cout << "               " << mmit->second << "\n";
		if (c.edited_tags.find(svn_to_git(mmit->second)) != c.edited_tags.end()) {
		    std::cout << "    Tag " << mmit->second << " contains edits - adding as branch\n";
		} else {
		    std::cout << "    Valid tag - tagging\n";
		}
	    }
	    continue;
	}
	if (c.tag_deletes.find(i) != c.tag_deletes.end()) {
	    std::cout << "Tag delete " << i << ":\n";
	    rr = c.tag_delete_paths.equal_range(i);
	    for (mmit = rr.first; mmit != rr.second; mmit++) {
		std::cout << "               " << mmit->second << "\n";
	    }
	    continue;
	}
	if (c.tag_edits.find(i) != c.tag_edits.end()) {
	    std::cout << "Tag edit " << i << ":\n";
	    rr = c.tag_edit_paths.equal_range(i);
	    for (mmit = rr.first; mmit != rr.second; mmit++) {
		std::cout << "               " << mmit->second << "\n";
	    }
	    // TODO - if this is the last commit that edits this
	    // particular tag branch, tag it and archive/remove the branch
	    continue;
	}
	if (c.merge_commits.find(i) != c.merge_commits.end()) {
	    //std::cout << "Merge commit (" << c.merge_commit_from[i] << " -> " << c.commit_branch[i]  << ") " << i << ": " << logmsg << "\n";
	    std::cout << "Merge commit (" << c.merge_commit_from[i] << " -> " << c.commit_branch[i]  << ") " << i << "\n";
	    if (c.move_edit_revs.find(i) != c.move_edit_revs.end()) {
		std::cout << "Warning - merge commit " << i << " also contains move + edit operations!!\n";
	    } else {
		continue;
	    }
	}
	if (c.multi_branch_commits.find(i) != c.multi_branch_commits.end()) {
	    std::set<std::string>::iterator mbit;
	    std::cout << "Commit edits multiple branches " << i << ":\n";
	    for (mbit = c.multi_branch_branches[i].begin(); mbit != c.multi_branch_branches[i].end(); mbit++) {
		std::cout << "               " << *mbit << "\n";
	    }
	    continue;
	}
	if (c.move_edit_revs.find(i) != c.move_edit_revs.end()) {
	    std::cout << "Commit contains move + edits (" << c.commit_branch[i]  << ") " << i << ":\n";
	    std::multimap<int, std::pair<std::string, std::string> >::iterator rmit;
	    std::pair<std::multimap<int, std::pair<std::string, std::string> >::iterator, std::multimap<int, std::pair<std::string, std::string> >::iterator> revrange;
	    std::cout << "Move + edit:\n";
	    revrange = c.revs_move_edit_map.equal_range(i);
	    for (rmit = revrange.first; rmit != revrange.second; rmit++) {
		std::cout << "   " << rmit->second.first << " -> " << rmit->second.second << "\n";
	    }
	    revrange = c.revs_move_only_map.equal_range(i);
	    if (revrange.first != revrange.second) {
		std::cout << "Moves:\n";
		for (rmit = revrange.first; rmit != revrange.second; rmit++) {
		    std::cout << "   " << rmit->second.first << " -> " << rmit->second.second << "\n";
		}
	    }
	    std::multimap<int, std::string >::iterator ermit;
	    std::pair<std::multimap<int, std::string >::iterator, std::multimap<int, std::string >::iterator> erevrange;
	    erevrange = c.revs_edit_only_map.equal_range(i);
	    if (revrange.first != revrange.second) {
		std::cout << "Edits:\n";
		for (ermit = erevrange.first; ermit != erevrange.second; ermit++) {
		    std::cout << "   " << ermit->second << "\n";
		}
	    }

	    continue;
	}
	std::string branch = c.commit_branch[i];
	if (branch.length()) {
	    std::cout << "Standard commit (" << c.commit_branch[i] << ") " << i << "\n";
	    if (!c.commit_branch[i].compare("trunk")) {
		git_commit(argv[2], i, c.commit_branch[i], logmsg, rauthor, rdate);
	    }
	}
    }
    chdir("..");

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
