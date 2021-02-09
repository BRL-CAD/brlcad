#include "common_structs.h"

std::string git_sha1(std::ifstream &infile, struct svn_node *n);

#include "svnfexport_svn.cxx"
#include "svnfexport_git.cxx"


void
load_author_map(const char *f)
{

    std::ifstream afile(f);
    std::string line;
    while (std::getline(afile, line)) {
	size_t spos = line.find_first_of(" ");
	std::string svnauthor = line.substr(0, spos);
	std::string gitauthor = line.substr(spos+1, std::string::npos);
	author_map[svnauthor] = gitauthor;
    }
}

int get_starting_rev()
{
    struct stat buffer;
    if (stat("current_rev.txt", &buffer)) {
	return 29886;
    }
    std::ifstream afile("current_rev.txt");
    std::string line;
    std::getline(afile, line);
    return std::stoi(line);
}
void update_starting_rev(int rev)
{
    std::ofstream ofile("current_rev.txt");
    ofile << rev;
    ofile.close();
}

int main(int argc, const char **argv)
{
    std::string git_setup = std::string("rm -rf cvs_git_working && cp -r cvs_git cvs_git_working");
    std::string cleanup_cmd = std::string("rm -rf cvs_git_working");

    int i = 0;
    if (argc < 4) {
	std::cerr << "svnfexport dumpfile author_map repo_checkout_path \n";
	return 1;
    }

    repo_checkout_path = std::string(argv[3]);

    load_branches_list();

    starting_rev = get_starting_rev();

    if (starting_rev == 29886) {
	std::string git_sync_1 = std::string("cd cvs_git && cat ../custom/r29886_cvs_svn_trunk_sync.fi | git fast-import && cd ..");
	/* Apply sync fast imports */
	if (std::system(git_sync_1.c_str())) {
	    std::cerr << "Initial trunk sync failed!\n";
	    exit(1);
	}
	std::string git_sync_2 = std::string("cd cvs_git && cat ../custom/r29886_cvs_svn_rel-5-1-branch_sync.fi | git fast-import && cd ..");
	if (std::system(git_sync_2.c_str())) {
	    std::cerr << "Initial rel-5-1-branch sync failed!\n";
	    exit(1);
	}
	std::string git_sync_3 = std::string("cd cvs_git && cat ../custom/r29886_cjohnson_mac_hack.fi | git fast-import && cd ..");
	if (std::system(git_sync_3.c_str())) {
	    std::cerr << "Initial mac-hack sync failed!\n";
	    exit(1);
	}
	std::string git_sync_4 = std::string("cd cvs_git && cat ../custom/r29886_tags.fi | git fast-import && cd ..");
	if (std::system(git_sync_4.c_str())) {
	    std::cerr << "Initial tag sync failed!\n";
	    exit(1);
	}
	std::string git_sync_5 = std::string("cd cvs_git && cat ../custom/r29886_branches.fi | git fast-import && cd ..");
	if (std::system(git_sync_5.c_str())) {
	    std::cerr << "Initial branch sync failed!\n";
	    exit(1);
	}
    }


    /* Branch/tag name mappings */
    branch_mappings[std::string("framebuffer-experiment")] = std::string("framebuffer-experiment");
    branch_mappings[std::string("master")] = std::string("trunk");
    branch_mappings[std::string("dmtogl")] = std::string("dmtogl-branch");


    std::cout << "Starting by verifying revision " << starting_rev << "\n";

    //starting_rev = 36000;

    // Make sure our starting point is sound
    if (std::system(git_setup.c_str())) {
	std::cout << "git setup failed!\n";
	exit(1);
    }
    verify_repos(starting_rev, std::string("master"));

    /* Populate valid_projects */
    valid_projects.insert(std::string("brlcad"));

    /* SVN has some empty files - tell our setup how to handle them */
    svn_sha1_to_git_sha1[std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709")] = std::string("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");

    /* Read in pre-existing branch sha1 heads from git */
    load_author_map(argv[2]);

    /* If we're starting from scratch, read in pre-existing, pre-svn branch sha1 heads from git. */
    if (starting_rev == 29886) {
	load_branch_head_sha1s();
	remove("rev_gsha1s.txt");
    }

    /* Read in previously defined rev -> sha1 map, if any */
    read_cached_rev_sha1s(starting_rev);

    /* Read in pre-existing blob sha1s from git */
    load_blob_sha1s();

    int rev_cnt = load_dump_file(argv[1]);
    if (rev_cnt > 0) {
	std::cout << "Dump file " << argv[1] << " loaded - found " << rev_cnt << " revisions\n";
	analyze_dump();
    } else {
	std::cerr << "No revision found - quitting\n";
	return -1;
    }

    std::string smaxrev_cmd = std::string("svn info --show-item revision file://$PWD/repo_dercs > smaxrev.txt");
    if (std::system(smaxrev_cmd.c_str())) {
	std::cout << "smaxrev_cmd failed\n";
	exit(1);
    }
    std::string maxline;
    std::ifstream maxfile("smaxrev.txt");
    if (!maxfile.good()) exit(1);
    std::getline(maxfile, maxline);
    long mrev = std::stol(maxline);

    // This process will create a lot of fi files - stash them
    if (!file_exists(std::string("done"))) {
	std::string stashdir_cmd = std::string("mkdir done");
	if (std::system(stashdir_cmd.c_str())) {
	    std::cout << "failed to create 'done' directory!\n";
	    exit(1);
	}
    }

    std::ifstream infile(argv[1]);

    for (i = starting_rev+1; i <= mrev; i++) {
	rev_fast_export(infile, i);
	get_rev_sha1s(i);
	update_starting_rev(i);
	write_branches_list();
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
