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

    // TODO - this isn't adequate.  pre-7-12-6 isn't getting updated after restart,
    // because it's not in the original branch list.  Need to maintain this list as
    // the rev_gsha1 list is maintained, so we always have a current branch list.
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

    int i = 0;
    if (argc < 3) {
	std::cerr << "svnfexport dumpfile author_map\n";
	return 1;
    }

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
    }

    std::cout << "Starting by verifying revision " << starting_rev << "\n";

    //starting_rev = 36000;

    // Make sure our starting point is sound
    if (std::system(git_setup.c_str())) {
	std::cout << "git setup failed!\n";
	exit(1);
    }
    verify_repos(starting_rev, std::string("trunk"), std::string("master"), 1);
    verify_repos(starting_rev, std::string("STABLE"), std::string("STABLE"), 1);
    verify_repos(starting_rev, std::string("rel-5-1-branch"), std::string("rel-5-1-branch"), 1);
    if (std::system(cleanup_cmd.c_str())) {
	std::cout << "git cleanup failed!\n";
    }


    /* Populate valid_projects */
    valid_projects.insert(std::string("brlcad"));

    /* SVN has some empty files - tell our setup how to handle them */
    svn_sha1_to_git_sha1[std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709")] = std::string("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");

    /* Branch/tag name mappings */
    branch_mappings[std::string("framebuffer-experiment")] = std::string("framebuffer-experiment");

    /* Read in pre-existing branch sha1 heads from git */
    load_author_map(argv[2]);

    /* If we're starting from scratch, read in pre-existing, pre-svn branch sha1 heads from git. */
    if (starting_rev == 29886) {
	load_branch_head_sha1s();
	remove("rev_gsha1s.txt");
    }

    /* Read in previously defined rev -> sha1 map, if any */
    read_cached_rev_sha1s();

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

    std::ifstream infile(argv[1]);

    for (i = starting_rev+1; i < 40000; i++) {
	rev_fast_export(infile, i);
	get_rev_sha1s(i);
	update_starting_rev(i);
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
