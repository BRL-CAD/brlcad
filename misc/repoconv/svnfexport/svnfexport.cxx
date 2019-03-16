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

int main(int argc, const char **argv)
{
    struct stat buffer;
    int i = 0;
    if (argc < 3) {
	std::cerr << "svnfexport dumpfile author_map\n";
	return 1;
    }

    starting_rev = 29886;
    std::string srfile = std::to_string(starting_rev+1) + std::string(".fi");
    while (!stat(srfile.c_str(), &buffer)) {
	starting_rev = starting_rev+1;
	srfile = std::to_string(starting_rev+1) + std::string(".fi");
    }

    std::cout << "Starting by verifying revision " << starting_rev << "\n";

    //starting_rev = 36000;

    // Make sure our starting point is sound
    verify_repos(starting_rev, std::string("trunk"), std::string("master"));
    verify_repos(starting_rev, std::string("branches/STABLE"), std::string("STABLE"));

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
