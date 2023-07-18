/*                    G S U R V E Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2023 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gsurvey.cpp
 *
 * Analyze and summaries the contents of .g files in subdirectories of a target
 * directory.
 *
 */

#include "common.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <fstream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "md5.hpp"

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"
}

#include "bu.h"
#include "raytrace.h"

static unsigned long long
hash_string(const char *s)
{
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    static struct bu_vls vs = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vs, "%s", bu_path_normalize(s));
    // TODO - xxhash needed a minimum input size per Coverity - I believe that got fixed, but
    // I'm not sure if that was in a branch...  need to check
    if (bu_vls_strlen(&vs) < 10) {
	bu_vls_printf(&vs, "GGGGGGGGGGGGG");
    }
    XXH64_update(&h_state, bu_vls_cstr(&vs), bu_vls_strlen(&vs)*sizeof(char));
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    return hash;
}

class GData {
    public:

	// Decodes any hash into its associated string
	std::unordered_map<unsigned long long, std::string> string_map;

	// For file names (without path data), list the file content hashes -
	// multiple hashes for a file name indicates multiple .g files with the
	// same name but different contents in different directories
	std::unordered_map<unsigned long long, std::unordered_set<std::string>> gfile_hashes;

	// For unique file hashes, list the file paths - identifies duplicate .g files
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> dup_paths;

	// For each object name, identify the unique file hashes holding an object using that name
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> names_files;

	// For each unique file hash, identify the unique object hashes present
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> file_objects;

	// For each unique object hash, identify the unique file hashes containing it
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> object_files;

	// For each object name, identify the set of unique object hashes associated with it.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> name_objects;

	// For each object hash, identify the set of object names associated with it.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> object_names;
};

static void
scan_path(std::vector<std::filesystem::path> &g_files, const char *p)
{
    std::cerr << "Scanning " << p << "\n";
    std::filesystem::path g_path = std::filesystem::absolute(std::filesystem::path(p));
    if (!std::filesystem::is_directory(g_path)) {
	// If we have an individual file listed, process it
	if (db_filetype(g_path.string().c_str()) > 0)
	    g_files.push_back(g_path);
	return;
    }

    // If we have a directory, go .g file hunting
    for (auto de = std::filesystem::recursive_directory_iterator(g_path, std::filesystem::directory_options::skip_permission_denied); de != std::filesystem::recursive_directory_iterator(); ++de) {
	// Permissions check first
	std::error_code ec;
	auto perms = std::filesystem::status(*de, ec).permissions();
	if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none ||
		(perms & std::filesystem::perms::group_read) == std::filesystem::perms::none ||
		(perms & std::filesystem::perms::others_read) == std::filesystem::perms::none) {
	    continue;
	}

	// Stuff to skip
	if (de->is_symlink() || !de->exists() || !de->is_regular_file()) {
	    continue;
	}

	// Is this a .g file?
	std::string pstr = de->path().string();
	if (db_filetype(pstr.c_str()) > 0)
	    g_files.push_back(de->path());
    }
}

static void
analyze_g(GData &gdata, std::string &path, unsigned long long fhash)
{
    struct db_i *dbip = db_open(path.c_str(), DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	return;
    if (db_dirbuild(dbip) < 0)
	return;
    db_update_nref(dbip, &rt_uniresource);
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (struct directory *dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & RT_DIR_HIDDEN)
		continue;
	    unsigned long long objname_hash = hash_string(dp->d_namep);
	    gdata.string_map[objname_hash] = std::string(dp->d_namep);
	    gdata.names_files[objname_hash].insert(fhash);
	    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
	    if (db_get_external(&ext, dp, dbip))
		continue;
	    XXH64_state_t h_state;
	    XXH64_reset(&h_state, 0);
	    XXH64_update(&h_state, ext.ext_buf, ext.ext_nbytes*sizeof(uint8_t));
	    XXH64_hash_t hash_val = XXH64_digest(&h_state);
	    unsigned long long objdata_hash = (unsigned long long)hash_val;
	    gdata.string_map[objdata_hash] = std::string(dp->d_namep);
	    gdata.file_objects[fhash].insert(objdata_hash);
	    gdata.object_files[objdata_hash].insert(fhash);
	    gdata.name_objects[objname_hash].insert(objdata_hash);
	    gdata.object_names[objdata_hash].insert(objname_hash);
	}
    }
    db_close(dbip);
}

int main(int argc, const char **argv)
{
    std::vector<std::filesystem::path> g_files;
    std::chrono::time_point<std::chrono::steady_clock> stime;
    std::chrono::time_point<std::chrono::steady_clock> etime;

    // Stash the program name
    bu_setprogname(argv[0]);

    // Global initialization
    rt_init_resource(&rt_uniresource, 0, NULL);

    argc--;argv++;

    if (argc) {
	for (int i = 0; i < argc; i++)
	    scan_path(g_files, argv[i]);
    } else {
	const char *spath = bu_dir(NULL, 0, BU_DIR_CURR, NULL);
	scan_path(g_files, spath);
    }

    GData gdata;
    std::cerr << "\n\nFound .g files:\n";
    for (size_t i = 0; i < g_files.size(); i++) {
	std::string gname = g_files[i].string();
	unsigned long long path_hash = hash_string(gname.c_str());
	gdata.string_map[path_hash] = gname;
	unsigned long long filename_hash = hash_string(g_files[i].filename().string().c_str());
	gdata.string_map[filename_hash] = g_files[i].filename().string();
	gdata.dup_paths[filename_hash].insert(path_hash);

	std::ifstream gfilestream(gname);
	std::string gcontents((std::istreambuf_iterator<char>(gfilestream)), std::istreambuf_iterator<char>());
	std::string calc_md5 = md5_hash_hex(gcontents.data(), gcontents.length());
	std::cout << gname << ": " << calc_md5 << "\n";
	gdata.gfile_hashes[filename_hash].insert(calc_md5);
    }

    std::unordered_map<unsigned long long, std::unordered_set<std::string>>::iterator g_it;
    for (g_it = gdata.gfile_hashes.begin(); g_it != gdata.gfile_hashes.end(); g_it++) {
	if (g_it->second.size() < 2)
	    continue;
	std::cout << "Disjoint content for matching names: " << gdata.string_map[g_it->first] << "\n";
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator d_it;
	d_it = gdata.dup_paths.find(g_it->first);
	if (d_it == gdata.dup_paths.end())
	    continue;
	std::set<std::string> s_paths;
	std::unordered_set<unsigned long long>::iterator s_it;
	for (s_it = d_it->second.begin(); s_it != d_it->second.end(); s_it++) {
	    s_paths.insert(gdata.string_map[*s_it]);
	}
	std::set<std::string>::iterator sp_it;
	for (sp_it = s_paths.begin(); sp_it != s_paths.end(); sp_it++) {
	    std::cout << "	" << *sp_it << "\n";
	}
    }

    // Iterate over unique .g files to break out objects.
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator d_it;
    for (d_it = gdata.dup_paths.begin(); d_it != gdata.dup_paths.end(); d_it++) {
	// Only need to analyze one path for unique contents - .g contents are the same
	unsigned long long phash = *d_it->second.begin();
	std::string gpath = gdata.string_map[phash];
	std::cout << "Analyzing " << gpath << "\n";
	analyze_g(gdata, gpath, d_it->first);
    }

    for (d_it = gdata.names_files.begin(); d_it != gdata.names_files.end(); d_it++) {
	std::string oname = gdata.string_map[d_it->first];
	std::cout << oname << " count: " << d_it->second.size() << "\n";
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

