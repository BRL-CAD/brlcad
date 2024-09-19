/*               P E R M _ T E S T . C P P
 * BRL-CAD
 *
 * Published in 2024 by the United States Government.
 * This work is in the public domain.
 *
 */

#include "common.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void pprint(char lbl, std::filesystem::perms filep, std::filesystem::perms p)
{
    std::cout << (std::filesystem::perms::none == (filep & p) ? '-' : lbl);
}

void pout(std::filesystem::perms p)
{
    pprint('r', p, std::filesystem::perms::owner_read);
    pprint('w', p, std::filesystem::perms::owner_write);
    pprint('x', p, std::filesystem::perms::owner_exec);
    pprint('r', p, std::filesystem::perms::group_read);
    pprint('w', p, std::filesystem::perms::group_write);
    pprint('x', p, std::filesystem::perms::group_exec);
    pprint('r', p, std::filesystem::perms::others_read);
    pprint('w', p, std::filesystem::perms::others_write);
    pprint('x', p, std::filesystem::perms::others_exec);
    std::cout << '\n';
}

int
main(int argc, const char **argv)
{
    const char *f = "p.txt";
    int perms = 0;
    if (argc > 1)
	perms = std::stoi(std::string(argv[1]), 0, 8);
    std::filesystem::perms perms_mask = std::filesystem::perms::none;
    perms_mask = perms_mask | std::filesystem::perms::owner_all | std::filesystem::perms::group_all;
    if (perms)
	perms_mask = static_cast<std::filesystem::perms>(perms);

    std::ofstream fs(f); // create file
    fs.close();

    std::filesystem::perms initial_perms = std::filesystem::status(f).permissions();
    std::cout << "Initial(" << std::oct << static_cast<int>(initial_perms) << "): ";
    pout(initial_perms);

    std::filesystem::permissions(f, perms_mask, std::filesystem::perm_options::add);

    std::filesystem::perms out_perms = std::filesystem::status(f).permissions();
    std::cout << " Adding(" << std::oct << static_cast<int>(perms_mask) << "): ";
    pout(perms_mask);
    std::cout << "  Final(" << std::oct << static_cast<int>(out_perms) << "): ";
    pout(out_perms);

    std::filesystem::remove(f);
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
