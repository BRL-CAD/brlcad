/**
 * Snapshot BRL-CAD object name normalization and collision handling.
 */

#include "common.h"

#include "conversion/name_allocator.h"

#include <stdint.h>
#include <string.h>

#include "raytrace.h"


static const char CREO_BRL_CORE_REGION_SUFFIX[] = ".r";
static const char CREO_BRL_CORE_UNKNOWN_NAME[] = "unknown";
static const char CREO_BRL_CORE_NAME_KEEP_CHARACTERS[] = "+-.=_";


std::string
creo_brl_snapshot_name_root(const std::string& source_name)
{
    std::string name;

    name.reserve(source_name.size());
    for (size_t index = 0; index < source_name.size(); ++index) {
        const unsigned char character = (unsigned char)source_name[index];
        const int is_uppercase = character >= 'A' && character <= 'Z';
        const int is_lowercase = character >= 'a' && character <= 'z';
        const int is_digit = character >= '0' && character <= '9';

        if (is_uppercase)
            name.push_back((char)(character - 'A' + 'a'));
        else if (is_lowercase || is_digit ||
                 strchr(CREO_BRL_CORE_NAME_KEEP_CHARACTERS, character))
            name.push_back((char)character);
        else if (name.empty() || name[name.size() - 1] != '_')
            name.push_back('_');
    }

    if (name.empty())
        name.assign(CREO_BRL_CORE_UNKNOWN_NAME);

    return name;
}


int
creo_brl_snapshot_unique_name(struct db_i *database,
                              const std::string& name_root,
                              const std::vector<std::string>& reserved_names,
                              std::string *name)
{
    uint32_t suffix = 0;

    if (!database || !name)
        return 0;

    for (;;) {
        std::string candidate = name_root;
        size_t index = 0;

        if (suffix > 0)
            candidate += "_" + std::to_string(suffix);

        if (db_lookup(database, candidate.c_str(), LOOKUP_QUIET) == RT_DIR_NULL) {
            for (index = 0; index < reserved_names.size(); ++index) {
                if (candidate == reserved_names[index])
                    break;
            }
            if (index == reserved_names.size()) {
                *name = candidate;
                return 1;
            }
        }

        if (suffix == UINT32_MAX)
            return 0;
        ++suffix;
    }
}


int
creo_brl_snapshot_object_names(struct db_i *database,
                               const std::string& name_root,
                               const char *solid_suffix,
                               std::string *solid_name,
                               std::string *region_name)
{
    uint32_t suffix = 0;

    if (!database || !solid_suffix || !solid_name || !region_name)
        return 0;

    for (;;) {
        std::string object_root = name_root;

        if (suffix > 0)
            object_root += "_" + std::to_string(suffix);

        *solid_name = object_root + solid_suffix;
        *region_name = object_root + CREO_BRL_CORE_REGION_SUFFIX;
        if (db_lookup(database, solid_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL &&
            db_lookup(database, region_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
            return 1;

        if (suffix == UINT32_MAX)
            return 0;
        ++suffix;
    }
}
