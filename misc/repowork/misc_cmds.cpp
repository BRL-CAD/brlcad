/*                   M I S C _ C M D S . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file misc_cmds.cpp
 *
 * Various commands not truly handled/implemented
 * in repowork but mentioned in the git fast-import
 * documentation.
 *
 */

#include "repowork.h"

int
parse_alias(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 6);  // Remove "alias " prefix

    // For the moment, we don't support aliass so this never works...
    std::cerr << "Unsupported command \"alias\" (specified: : " << line << ")\n";

    return -1;
}

int
parse_cat_blob(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support cat_blobs so this never works...
    std::cerr << "Unsupported command \"cat_blob\" - ignored\n";

    return 0;
}

int
parse_checkpoint(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support checkpoints so this never works...
    std::cerr << "Unsupported command \"checkpoint\" - ignored\n";

    return 0;
}

int
parse_done(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support dones so this never works...
    std::cerr << "Unsupported command \"done\"- ignored\n";

    return 0;
}

int
parse_feature(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);
    line.erase(0, 8);  // Remove "feature " prefix

    // For the moment, we don't support any features so this never works...
    std::cerr << "Unsupported command \"feature\" (specified: " << line << ")\n";

    return -1;
}

int
parse_get_mark(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support get_marks so this never works...
    std::cerr << "Unsupported command \"get_mark\" - ignored\n";

    return -1;
}

int
parse_ls(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support lss so this never works...
    std::cerr << "Unsupported command \"ls\" - ignored\n";

    return 0;
}

int
parse_option(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    // For the moment, we don't support options so this never works...
    std::cout << "Unsupported command \"option\" - ignored\n";

    return -1;
}

int
parse_progress(git_fi_data *fi_data, std::ifstream &infile)
{
    std::string line;
    std::getline(infile, line);

    std::cerr << line << "\n";

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
