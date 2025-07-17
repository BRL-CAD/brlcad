/*                         G R O U P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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

#include "common.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "tlsh.hpp"
#include "levenshtein_ull.hpp"

extern "C" {
#include "bu/app.h"
#include "bu/hash.h"
#include "bu/file.h"
#include "bu/path.h"
#include "rt/db_io.h" // for db_read
#include "./gdiff.h"
}

#define TLSH_DEFAULT_THRESHOLD 30

std::string
objnames_hash(const std::string &path)
{
    // open db
    struct db_i *dbip = db_open(path.c_str(), DB_OPEN_READONLY);
    if (!dbip)
	return std::string();
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return std::string();
    }
    db_update_nref(dbip, &rt_uniresource);

    //  build an ordered set of directory object names
    std::set<std::string> onames;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
	    continue;
	onames.insert(std::string(dp->d_namep));
    } FOR_ALL_DIRECTORY_END;

    tlsh::Tlsh hasher;
    std::set<std::string>::iterator o_it;
    std::string data;
    for (o_it = onames.begin(); o_it != onames.end(); ++o_it)
	data.append(*o_it);
    hasher.update((const unsigned char *)data.c_str(), data.length());
    hasher.final();
    std::string hstr = hasher.getHash();
    std::cout << path << ": " <<  hstr << "\n";

    db_close(dbip);
    return hstr;
}

std::string
geometry_hash(const std::string &path)
{
    // open db
    struct db_i *dbip = db_open(path.c_str(), DB_OPEN_READONLY);
    if (!dbip)
	return std::string();
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return std::string();
    }
    db_update_nref(dbip, &rt_uniresource);

    //  build an ordered set of directory object names
    std::map<std::string, struct directory *> objs;
    struct directory *dp;
    unsigned int max_bufsize = 0;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
	    continue;
	objs[std::string(dp->d_namep)] = dp;
	if (dp->d_len > max_bufsize)
	    max_bufsize = dp->d_len;
    } FOR_ALL_DIRECTORY_END;

    // Asemble hashes of the db internal serialized data into a vector. We'll
    // either Tlsh hash it or try the unsigned long long editdist trick.
    // Because we don't need to fully unpack, reuse an external like the
    // db5_size code does to avoid mallocs.
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    BU_EXTERNAL_INIT(&ext);
    /* Make sure the bu_external buffer has as much memory as it will ever need
     * to handle any one object. */
    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

    // Process the geometry in sorted order
    std::vector<unsigned long long> ghashes;
    std::map<std::string, struct directory *>::iterator o_it;
    for (o_it = objs.begin(); o_it != objs.end(); ++o_it) {
	ext.ext_nbytes = dp->d_len;
	if (db_read(dbip, (char *)ext.ext_buf, ext.ext_nbytes, o_it->second->d_addr) < 0) {
	    memset(ext.ext_buf, 0, dp->d_len);
	    ext.ext_nbytes = 0;
	    continue;
	}
	unsigned long long hash = bu_data_hash(ext.ext_buf, ext.ext_nbytes);
	ghashes.push_back(hash);
    }

    if (ext.ext_buf)
       	bu_free(ext.ext_buf, "final free of ext_buf");

    tlsh::Tlsh hasher;
    hasher.update((const unsigned char *)ghashes.data(), ghashes.size() * sizeof(unsigned long long));
    hasher.final();
    std::string hstr = hasher.getHash();
    std::cout << path << ": " <<  hstr << "\n";

    db_close(dbip);
    return hstr;
}

bool
build_group(
	std::unordered_map<std::string, std::unordered_set<std::string>> *groups,
	std::unordered_map<std::string, std::set<std::string>> &hash2path,
	int threshold
	)
{
    std::unordered_map<std::string, std::unordered_set<std::string>>::iterator g_it;
    std::unordered_map<std::string, std::set<std::string>>::iterator h_it;
    for (h_it = hash2path.begin(); h_it != hash2path.end(); ++h_it) {
	std::string h = h_it->first;
	tlsh::Tlsh h1;
	h1.fromTlshStr(h_it->first.c_str());

	//std::cout << "Processing " << h << "\n";
	bool grouped = false;
	std::string gkey;
	int lscore = INT_MAX;
	for (g_it = groups->begin(); g_it != groups->end(); ++g_it) {
	    tlsh::Tlsh h2;
	    h2.fromTlshStr(g_it->first.c_str());
	    int score = h1.diff(h2);
	    //std::cout << *hash2path[h].begin() << "->"  << *hash2path[g_it->first].begin() << ": " << score << "\n";
	    if (score < threshold && score < lscore) {
		std::string c = g_it->first;
		gkey = c;
		grouped = true;
		lscore = score;
	    }
	}
	if (grouped) {
	    (*groups)[gkey].insert(h);
	} else {
	    (*groups)[h].insert(h);
	}
    }
    // See if we have something to report
    bool do_report = false;
    std::unordered_set<std::string>::iterator us_it;
    for (g_it = groups->begin(); g_it != groups->end(); ++g_it) {
	// Don't print out trivial groups
	bool trivial = true;
	for (us_it = g_it->second.begin(); us_it != g_it->second.end(); ++us_it) {
	    if (g_it->first != *us_it || hash2path[g_it->first].size() > 1) {
		trivial = false;
		break;
	    }
	}
	if (!trivial) {
	    do_report = true;
	    break;
	}
    }

    return do_report;
}

void
do_report(
	const std::string &fname,
	std::unordered_map<std::string, std::unordered_set<std::string>> &groups,
	std::unordered_map<std::string, std::set<std::string>> &hash2paths
	)
{
    if (fname.length())
	std::cout << "Groupings within the " << fname << " subgroup\n";

    std::unordered_map<std::string, std::unordered_set<std::string>>::iterator g_it;
    for (g_it = groups.begin(); g_it != groups.end(); ++g_it) {
	// Don't print out trivial groups
	bool trivial = true;
	std::unordered_set<std::string>::iterator us_it;
	for (us_it = g_it->second.begin(); us_it != g_it->second.end(); ++us_it) {
	    if (g_it->first != *us_it || hash2paths[g_it->first].size() > 1) {
		trivial = false;
		break;
	    }
	}
	if (trivial)
	    continue;
	std::string ganchor = *hash2paths[g_it->first].begin();
	tlsh::Tlsh h1;
	h1.fromTlshStr(g_it->first.c_str());
	std::cout << ganchor << ":\n";
	for (us_it = g_it->second.begin(); us_it != g_it->second.end(); ++us_it) {
	    tlsh::Tlsh h2;
	    h2.fromTlshStr(us_it->c_str());
	    std::set<std::string>::iterator hp_it;
	    for (hp_it = hash2paths[*us_it].begin(); hp_it != hash2paths[*us_it].end(); ++hp_it) {
		if (*hp_it == ganchor)
		    continue;
		int score = h1.diff(h2);
		std::cout << "\t" << *hp_it << "(" << score << ")" << "\n";
	    }
	}
	std::cout << "\n";
    }
}

void
do_grouping(
	const std::string &fname,
	struct gdiff_group_opts *g_opts,
	std::unordered_map<std::string, std::set<std::string>> *names,
	std::unordered_map<std::string, std::set<std::string>> *geom
	)
{
    if (names && g_opts->geomname_threshold >= 0) {
	std::unordered_map<std::string, std::unordered_set<std::string>> name_groups;
	if (build_group(&name_groups, *names, g_opts->geomname_threshold))
	    do_report(fname, name_groups, *names);
    }

    if (geom && g_opts->geometry_threshold >= 0) {
	std::unordered_map<std::string, std::unordered_set<std::string>> geom_groups;
	if (build_group(&geom_groups, *geom, g_opts->geometry_threshold))
	    do_report(fname, geom_groups, *names);
    }
}

int
gdiff_group(int argc, const char **argv, struct gdiff_group_opts *g_opts)
{
    // If we have no patterns, there's nothing to group
    if (!argc || !argv)
	return BRLCAD_OK;

    struct gdiff_group_opts gopts = GDIFF_GROUP_OPTS_DEFAULT;
    if (g_opts) {
	gopts.filename_threshold = g_opts->filename_threshold;
	gopts.geomname_threshold = g_opts->geomname_threshold;
	gopts.geometry_threshold = g_opts->geometry_threshold;
	bu_vls_sprintf(&gopts.fpattern, "%s", bu_vls_cstr(&g_opts->fpattern));
    }

    // Establish sane defaults
    if (gopts.filename_threshold == -1 && gopts.geomname_threshold == -1 &&
	    gopts.geometry_threshold == -1) {
	// If no criteria are enabled, at least turn on the geometry
	// object name check.
	gopts.geomname_threshold = TLSH_DEFAULT_THRESHOLD;
    }
    if (!bu_vls_strlen(&gopts.fpattern))
	bu_vls_sprintf(&gopts.fpattern, "*.g");

    // Build a set of file paths from the argv
    std::set<std::string> files;
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, MAXPATHLEN, BU_DIR_CURR, NULL);
    for (int i = 0; i < argc; i++) {
	// 1.  If we've been given a directory, do a recursive hunt for a
	// pattern match.
	if (bu_file_directory(argv[i]))  {
	    std::string rstr(argv[i]);
	    for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
		if (!entry.is_regular_file())
		    continue;
		if (!bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
		    //std::cout << "matched " <<  entry.path().string() << "\n";
		    files.insert(entry.path().string());
	    }
	}
	continue;

	// 2.  If we've been given a fully qualified path, just store that
	if (bu_file_exists(argv[i], NULL)) {
	    //std::cout << "adding " << argv[i] << "\n";
	    files.insert(std::string(argv[i]));
	}
    }

    // If we have been instructed to, group files by filename
    std::unordered_map<std::string, std::set<std::string>> file_grps;
    std::unordered_map<std::string, std::set<std::string>>::iterator fg_it;
    std::set<std::string>::iterator s_it;
    // NOTE - we make this unique binning for now and only put it in the
    // closest match, but maybe we want to do more?  If an incoming name is
    // close to more than one key where those keys were individually distinct,
    // we may want it in both bins for testing purposes...
    if (gopts.filename_threshold >= 0) {
	for (s_it = files.begin(); s_it != files.end(); ++s_it) {
	    const char *s = s_it->c_str();
	    bool assigned = false;
	    size_t ledit = LONG_MAX;
	    std::string key;
	    for (fg_it = file_grps.begin(); fg_it != file_grps.end(); ++fg_it) {
		const char *k = fg_it->first.c_str();
		size_t edist = bu_editdist(s, k);
		if (edist < (size_t)gopts.filename_threshold && edist < ledit) {
		    key = fg_it->first;
		    assigned = true;
		    ledit = edist;
		}
	    }
	    if (!assigned) {
		file_grps[key].insert(*s_it);
	    } else {
		file_grps[*s_it].insert(*s_it);
	    }
	}
    } else {
	// No filename grouping - just add everything with an empty string key;
	file_grps[std::string("")] = files;
    }


    // Hash each file
    std::unordered_map<std::string, std::string> path2namehash;
    std::unordered_map<std::string, std::string> path2geohash;
    std::unordered_map<std::string, std::set<std::string>> namehash2path;
    std::unordered_map<std::string, std::set<std::string>> geohash2path;
    for (s_it = files.begin(); s_it != files.end(); ++s_it) {
	//std::cout << "hashing " << *s_it << "\n";
	if (gopts.geomname_threshold >= 0) {
	    std::string dhash = objnames_hash(*s_it);
	    std::string fpath(*s_it);
	    path2namehash[fpath] = dhash;
	    namehash2path[dhash].insert(fpath);
	}
	if (gopts.geometry_threshold >= 0) {
	    std::string dhash = geometry_hash(*s_it);
	    std::string fpath(*s_it);
	    path2geohash[fpath] = dhash;
	    geohash2path[dhash].insert(fpath);
	}

    }

    // Compare hashes and find similarity groupings
    for (fg_it = file_grps.begin(); fg_it != file_grps.end(); ++fg_it) {
	do_grouping(fg_it->first, &gopts, &namehash2path, &geohash2path);
    }

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
