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
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <map>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>
#include <functional>
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

struct FileInfo {
    std::string path;
    std::filesystem::file_time_type mtime;
};

std::filesystem::file_time_type get_file_mtime(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return fs::file_time_type::min();
    return ftime;
}

// Returns YYYY-MM-DD string for a file path
std::string get_file_timestamp(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return "(timestamp unavailable)";
    using namespace std::chrono;
    auto sctp = std::chrono::system_clock::now() +
	std::chrono::duration_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    char buf[16];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&cftime))) {
	return std::string(buf);
    }
    return "(timestamp unavailable)";
}

// Helper: sort vector of paths by mtime, newest first
std::vector<std::string> sort_by_mtime(const std::vector<std::string>& files) {
    std::vector<FileInfo> finfos;
    for (const auto& path : files) {
	finfos.push_back({path, get_file_mtime(path)});
    }
    std::sort(finfos.begin(), finfos.end(),
	    [](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });
    std::vector<std::string> sorted;
    for (const auto& fi : finfos) sorted.push_back(fi.path);
    return sorted;
}

// BK-tree node and clustering
struct BKTreeNode {
    std::string value;
    std::vector<std::string> files;
    std::map<size_t, BKTreeNode*> children;
    BKTreeNode(const std::string& v) : value(v) {}
    ~BKTreeNode() {
	for (auto& kv : children) delete kv.second;
    }
};

class BKTree {
    public:
	BKTree(std::function<size_t(const std::string&, const std::string&)> dist)
	    : root(nullptr), distance(dist) {}

	void insert(const std::string& s, const std::string& file) {
	    if (!root) {
		root = new BKTreeNode(s);
		root->files.push_back(file);
		return;
	    }
	    BKTreeNode* node = root;
	    while (true) {
		size_t d = distance(node->value, s);
		if (d == 0) {
		    node->files.push_back(file);
		    break;
		}
		auto it = node->children.find(d);
		if (it == node->children.end()) {
		    node->children[d] = new BKTreeNode(s);
		    node->children[d]->files.push_back(file);
		    break;
		} else {
		    node = it->second;
		}
	    }
	}

	std::vector<std::vector<std::string>> clusters(size_t threshold) const {
	    std::vector<std::vector<std::string>> result;
	    std::unordered_set<const BKTreeNode*> node_seen;
	    std::function<void(BKTreeNode*)> traverse = [&](BKTreeNode* node) {
		if (!node) return;
		if (!node_seen.count(node)) {
		    std::vector<BKTreeNode*> cluster;
		    query(node->value, threshold, cluster);
		    std::vector<std::string> files;
		    for (BKTreeNode* n : cluster) {
			for (const auto& f : n->files) files.push_back(f);
			node_seen.insert(n);
		    }
		    if (!files.empty())
			result.push_back(files);
		}
		for (auto& kv : node->children) traverse(kv.second);
	    };
	    traverse(root);
	    return result;
	}

	void query(const std::string& s, size_t threshold, std::vector<BKTreeNode*>& out) const {
	    query_rec(root, s, threshold, out);
	}

	~BKTree() { delete root; }

    private:
	BKTreeNode* root;
	std::function<size_t(const std::string&, const std::string&)> distance;

	void query_rec(BKTreeNode* node, const std::string& s, size_t threshold, std::vector<BKTreeNode*>& out) const {
	    if (!node) return;
	    size_t d = distance(node->value, s);
	    if (d <= threshold) out.push_back(node);
	    for (size_t i = (d >= threshold ? d - threshold : 0); i <= d + threshold; ++i) {
		auto it = node->children.find(i);
		if (it != node->children.end()) query_rec(it->second, s, threshold, out);
	    }
	}
};

// ------------------------------------
// Hashing routines, split for parallelism
struct DBWithPath {
    std::string path;
    struct db_i *dbip;
};

std::string objnames_hash_db(struct db_i *dbip) {
    if (!dbip)
	return std::string();
    db_update_nref(dbip, &rt_uniresource);
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

    // Per the TLSH algorithm, we need a minimum length and varied data to ensure
    // a non-zero hash.  If we get a short set of names we might just end up
    // generating a zero instead of something more useful for comparison, so try
    // seeding the data with a bit of string entropy.  Since this will be consistent
    // for all inputs the intent is that any differences in the output hashes will
    // reflect differences in the actual user data (even if the has doesn't exactly
    // correspond to the algorithmic TLSH hash for that binary input.)
    static const std::string tls_seed = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=";
    std::string data = tls_seed;
    for (const auto& o : onames) data.append(o);
    hasher.update((const unsigned char *)data.c_str(), data.length());
    hasher.final();
    std::string hstr = hasher.getHash();
    return hstr;
}

std::string geometry_hash_db(struct db_i *dbip) {
    if (!dbip)
	return std::string();
    db_update_nref(dbip, &rt_uniresource);
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
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    BU_EXTERNAL_INIT(&ext);
    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");
    std::vector<unsigned long long> ghashes;
    for (auto& o_it : objs) {
	ext.ext_nbytes = o_it.second->d_len;
	if (db_read(dbip, (char *)ext.ext_buf, ext.ext_nbytes, o_it.second->d_addr) < 0) {
	    memset(ext.ext_buf, 0, o_it.second->d_len);
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
    return hstr;
}

// ------------------------------------
// Parallel hash calculation (true parallelism)
template<typename HashFunc>
std::unordered_map<std::string, std::string> parallel_hash_db(const std::vector<DBWithPath>& dbs, HashFunc func, size_t nthreads=4) {
    std::unordered_map<std::string, std::string> result;
    std::mutex res_mutex;
    size_t total = dbs.size();
    std::vector<std::thread> threads;
    std::atomic<size_t> idx{0};
    auto worker = [&]() {
	while (true) {
	    size_t i = idx.fetch_add(1);
	    if (i >= total) break;
	    std::string hash = func(dbs[i].dbip);
	    if (!hash.empty()) {
		std::lock_guard<std::mutex> lock(res_mutex);
		result[dbs[i].path] = hash;
	    }
	}
    };
    for (size_t t = 0; t < nthreads; ++t) {
	threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
    return result;
}

// ------------------------------------
// Hash-based clustering and reporting
struct FileHashInfo {
    std::string path;
    std::filesystem::file_time_type mtime;
    std::string hash;
};

std::vector<std::vector<FileHashInfo>> cluster_by_hash(const std::vector<FileHashInfo>& files, int threshold) {
    size_t N = files.size();
    std::vector<std::vector<size_t>> adj(N);
    for (size_t i = 0; i < N; ++i) {
	tlsh::Tlsh hi; hi.fromTlshStr(files[i].hash.c_str());
	for (size_t j = i+1; j < N; ++j) {
	    tlsh::Tlsh hj; hj.fromTlshStr(files[j].hash.c_str());
	    int diff = hi.diff(hj);
	    if (diff < threshold) {
		adj[i].push_back(j);
		adj[j].push_back(i);
	    }
	}
    }
    std::vector<std::vector<FileHashInfo>> result;
    std::vector<bool> visited(N, false);
    for (size_t i = 0; i < N; ++i) {
	if (visited[i]) continue;
	std::vector<size_t> cluster;
	std::vector<size_t> stack{ i };
	visited[i] = true;
	while (!stack.empty()) {
	    size_t cur = stack.back(); stack.pop_back();
	    cluster.push_back(cur);
	    for (size_t nb : adj[cur]) {
		if (!visited[nb]) {
		    visited[nb] = true;
		    stack.push_back(nb);
		}
	    }
	}
	std::vector<FileHashInfo> c;
	for (size_t idx : cluster) c.push_back(files[idx]);
	result.push_back(c);
    }
    return result;
}

std::map<std::string, std::vector<std::pair<std::string, int>>> bins_from_clusters(const std::vector<std::vector<FileHashInfo>>& clusters) {
    std::map<std::string, std::vector<std::pair<std::string, int>>> bins;
    for (const auto& cluster : clusters) {
	if (cluster.empty()) continue;
	auto keyit = std::max_element(cluster.begin(), cluster.end(),
		[](const FileHashInfo& a, const FileHashInfo& b) { return a.mtime < b.mtime; });
	std::string key = keyit->path;
	tlsh::Tlsh hkey; hkey.fromTlshStr(keyit->hash.c_str());
	std::vector<std::pair<std::string, int>> entries;
	entries.push_back({key, 0});
	for (const auto& f : cluster) {
	    if (f.path == key) continue;
	    tlsh::Tlsh hcur; hcur.fromTlshStr(f.hash.c_str());
	    int diff = hkey.diff(hcur);
	    entries.push_back({f.path, diff});
	}
	// Sort leaf entries (not key) by mtime newest first
	std::vector<std::pair<std::string, int>> sorted_leaves;
	std::vector<FileInfo> leaf_infos;
	for (const auto& e : entries) {
	    if (e.first == key) continue;
	    leaf_infos.push_back({e.first, get_file_mtime(e.first)});
	}
	std::sort(leaf_infos.begin(), leaf_infos.end(),
		[](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });
	for (const auto& lfi : leaf_infos) {
	    for (const auto& e : entries) {
		if (e.first == lfi.path) {
		    sorted_leaves.push_back(e);
		    break;
		}
	    }
	}
	std::vector<std::pair<std::string, int>> bin_entries;
	bin_entries.push_back({key, 0});
	bin_entries.insert(bin_entries.end(), sorted_leaves.begin(), sorted_leaves.end());
	bins[key] = bin_entries;
    }
    return bins;
}

std::map<std::string, std::vector<std::pair<std::string, int>>> group_by_hash_map_with_scores(const std::vector<std::string>& files, const std::unordered_map<std::string, std::string>& path2hash, int threshold) {
    std::vector<FileHashInfo> infos;
    for (const auto& path : files) {
	if (!path2hash.count(path) || path2hash.at(path).empty()) continue;
	infos.push_back({path, get_file_mtime(path), path2hash.at(path)});
    }
    auto clusters = cluster_by_hash(infos, threshold);
    return bins_from_clusters(clusters);
}

int find_similarity_score(const std::vector<std::pair<std::string, int>>& entries, const std::string& key) {
    for (const auto& [entry, score] : entries) {
	if (entry == key) return score;
    }
    return 0;
}

// ------------------------------------
// Reporting
void report_single_level(const std::map<std::string, std::vector<std::pair<std::string, int>>>& groups, std::ostream& out) {
    for (const auto& [category, entries] : groups) {
	out << category << " (" << get_file_timestamp(category) << "):\n";
	for (const auto& [entry, score] : entries) {
	    if (entry == category) continue;
	    out << "\t" << entry << " [score: " << score << "]"
		<< " (" << get_file_timestamp(entry) << ")\n";
	}
    }
}

void report_two_level(const std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>>& groups,
	const std::unordered_map<std::string, std::string>& hash2path,
	std::ostream& out) {
    for (const auto& [cat1, cat2map] : groups) {
	out << cat1 << " (" << get_file_timestamp(cat1) << "):\n";
	for (const auto& [cat2, entries] : cat2map) {
	    std::string cat2_label = hash2path.count(cat2) ? hash2path.at(cat2) : cat2;
	    int score = find_similarity_score(entries, cat2_label);
	    out << "\t" << cat2_label << " [score: " << score << "]"
		<< " (" << get_file_timestamp(cat2_label) << "):\n";
	    for (const auto& [entry, e_score] : entries) {
		if (entry == cat2_label) continue;
		out << "\t\t" << entry << " [score: " << e_score << "]"
		    << " (" << get_file_timestamp(entry) << ")\n";
	    }
	}
    }
}

void report_three_level(const std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>>>& groups,
	const std::unordered_map<std::string, std::string>& namehash2path,
	const std::unordered_map<std::string, std::string>& geohash2path,
	std::ostream& out) {
    for (const auto& [cat1, cat2map] : groups) {
	out << cat1 << " (" << get_file_timestamp(cat1) << "):\n";
	for (const auto& [cat2, cat3map] : cat2map) {
	    std::string cat2_label = namehash2path.count(cat2) ? namehash2path.at(cat2) : cat2;
	    out << "\t" << cat2_label << " (" << get_file_timestamp(cat2_label) << "):\n";
	    for (const auto& [cat3, entries] : cat3map) {
		std::string cat3_label = geohash2path.count(cat3) ? geohash2path.at(cat3) : cat3;
		int score = find_similarity_score(entries, cat3_label);
		out << "\t\t" << cat3_label << " [score: " << score << "]"
		    << " (" << get_file_timestamp(cat3_label) << "):\n";
		for (const auto& [entry, e_score] : entries) {
		    if (entry == cat3_label) continue;
		    out << "\t\t\t" << entry << " [score: " << e_score << "]"
			<< " (" << get_file_timestamp(entry) << ")\n";
		}
	    }
	}
    }
}

// ------------------------------------
// Hierarchical grouping, reporting
void do_hierarchical_grouping(
	const std::unordered_map<std::string, std::vector<std::string>> &file_grps,
	int geomname_threshold,
	int geometry_threshold,
	const std::unordered_map<std::string, std::string> &path2namehash,
	const std::unordered_map<std::string, std::string> &path2geohash,
	const std::unordered_map<std::string, std::string> &namehash2path,
	const std::unordered_map<std::string, std::string> &geohash2path,
	std::ostream& out)
{
    // Determine which thresholds are active
    bool filename_active = (file_grps.size() > 1);
    bool geomname_active = (geomname_threshold >= 0);
    bool geometry_active = (geometry_threshold >= 0);

    // --- Depth 1: Only filename OR only geomname OR only geometry ---
    if (filename_active && !geomname_active && !geometry_active) {
	// Filename binning only
	std::map<std::string, std::vector<std::pair<std::string, int>>> report_map;
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    if (!sorted.empty()) {
		std::string key = sorted.front();
		std::vector<std::pair<std::string, int>> entries;
		entries.push_back({key, 0});
		for (size_t i = 1; i < sorted.size(); ++i)
		    entries.push_back({sorted[i], 0});
		report_map[key] = entries;
	    }
	}
	report_single_level(report_map, out);
	return;
    }
    if (!filename_active && geomname_active && !geometry_active) {
	// Geomname binning only
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto name_bins = group_by_hash_map_with_scores(all_files, path2namehash, geomname_threshold);
	report_single_level(name_bins, out);
	return;
    }
    if (!filename_active && !geomname_active && geometry_active) {
	// Geometry binning only
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto geo_bins = group_by_hash_map_with_scores(all_files, path2geohash, geometry_threshold);
	report_single_level(geo_bins, out);
	return;
    }

    // Depth 2
    if (filename_active && geomname_active && !geometry_active) {
	// Filename binning, then geomname binning inside each filename bin
	std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>> report_map;
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto name_bins = group_by_hash_map_with_scores(sorted, path2namehash, geomname_threshold);
	    if (!sorted.empty())
		report_map[sorted.front()] = name_bins;
	}
	report_two_level(report_map, namehash2path, out);
	return;
    }
    if (filename_active && !geomname_active && geometry_active) {
	// Filename binning, then geometry binning inside each filename bin
	std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>> report_map;
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto geo_bins = group_by_hash_map_with_scores(sorted, path2geohash, geometry_threshold);
	    if (!sorted.empty())
		report_map[sorted.front()] = geo_bins;
	}
	report_two_level(report_map, geohash2path, out);
	return;
    }
    if (!filename_active && geomname_active && geometry_active) {
	// Geomname binning, then geometry binning inside each geomname bin
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto name_bins = group_by_hash_map_with_scores(all_files, path2namehash, geomname_threshold);
	std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>> report_map;
	for (const auto& [key, entries] : name_bins) {
	    std::vector<std::string> geom_files;
	    for (const auto& pair : entries) geom_files.push_back(pair.first);
	    auto geo_bins = group_by_hash_map_with_scores(geom_files, path2geohash, geometry_threshold);
	    report_map[key] = geo_bins;
	}
	report_two_level(report_map, geohash2path, out);
	return;
    }

    // --- Depth 3: filename+geomname+geometry ---
    if (filename_active && geomname_active && geometry_active) {
	std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>>>
	    report_map;
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto name_bins = group_by_hash_map_with_scores(sorted, path2namehash, geomname_threshold);
	    std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, int>>>> level2_map;
	    for (const auto& [key2, nb_entries] : name_bins) {
		std::vector<std::string> geom_files;
		for (const auto& pair : nb_entries) geom_files.push_back(pair.first);
		auto geo_bins = group_by_hash_map_with_scores(geom_files, path2geohash, geometry_threshold);
		level2_map[key2] = geo_bins;
	    }
	    if (!sorted.empty())
		report_map[sorted.front()] = level2_map;
	}
	report_three_level(report_map, namehash2path, geohash2path, out);
	return;
    }
}

// ------------------------------------
// Main grouping function, entrypoint
int gdiff_group(int argc, const char **argv, struct gdiff_group_opts *g_opts)
{
    if (!argc || !argv)
	return BRLCAD_OK;

    struct gdiff_group_opts gopts = GDIFF_GROUP_OPTS_DEFAULT;
    if (g_opts) {
	gopts.filename_threshold = g_opts->filename_threshold;
	gopts.geomname_threshold = g_opts->geomname_threshold;
	gopts.geometry_threshold = g_opts->geometry_threshold;
	bu_vls_sprintf(&gopts.fpattern, "%s", bu_vls_cstr(&g_opts->fpattern));
	bu_vls_sprintf(&gopts.ofile, "%s", bu_vls_cstr(&g_opts->ofile));
    }

    if (gopts.filename_threshold == -1 && gopts.geomname_threshold == -1 &&
	    gopts.geometry_threshold == -1) {
	gopts.geomname_threshold = TLSH_DEFAULT_THRESHOLD;
    }
    if (!bu_vls_strlen(&gopts.fpattern))
	bu_vls_sprintf(&gopts.fpattern, "*.g");

    // Build a vector of file paths, sorted by mtime
    std::vector<FileInfo> file_infos;
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, MAXPATHLEN, BU_DIR_CURR, NULL);
    for (int i = 0; i < argc; i++) {
	if (bu_file_directory(argv[i]))  {
	    std::string rstr(argv[i]);
	    for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
		if (!entry.is_regular_file())
		    continue;
		if (!bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
		    file_infos.push_back({entry.path().string(), get_file_mtime(entry.path().string())});
	    }
	    continue;
	}
	if (bu_file_exists(argv[i], NULL)) {
	    file_infos.push_back({std::string(argv[i]), get_file_mtime(argv[i])});
	}
    }
    std::sort(file_infos.begin(), file_infos.end(),
	    [](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });

    std::vector<std::string> files;
    for (const auto& fi : file_infos) files.push_back(fi.path);

    // Filename binning using base filename edit distance, or single group if not active
    std::unordered_map<std::string, std::vector<std::string>> file_grps;
    if (gopts.filename_threshold >= 0) {
	auto basename = [](const std::string& path) {
	    namespace fs = std::filesystem;
	    return fs::path(path).filename().string();
	};
	BKTree bktree([](const std::string& a, const std::string& b) {
		return bu_editdist(a.c_str(), b.c_str());
		});
	for (const auto& path : files) {
	    bktree.insert(basename(path), path);
	}
	auto clusters = bktree.clusters(gopts.filename_threshold);
	for (const auto& cluster : clusters) {
	    std::vector<std::string> sorted = sort_by_mtime(cluster);
	    if (!sorted.empty())
		file_grps[sorted.front()] = cluster;
	}
    } else {
	std::vector<std::string> sorted = sort_by_mtime(files);
	if (!sorted.empty())
	    file_grps[sorted.front()] = files;
    }

    // Precompute hashes for all files and build reverse maps
    std::unordered_map<std::string, std::string> path2namehash;
    std::unordered_map<std::string, std::string> path2geohash;
    std::unordered_map<std::string, std::string> namehash2path;
    std::unordered_map<std::string, std::string> geohash2path;

    // TODO - right now, the material_head global in librt means doing this
    // hashing in parallel is a mess.  We'll default to serial hashing, but
    // this logic is in place for when we can fix that and get a speedup.
    size_t parallel_threads = gopts.thread_cnt;
    if (!parallel_threads)
	parallel_threads = bu_avail_cpus();

    if (parallel_threads > 1) {
	// --- PHASE 1: Sequential setup ---
	std::vector<DBWithPath> dbs;
	std::mutex db_mutex;
	for (const auto& path : files) {
	    std::lock_guard<std::mutex> lock(db_mutex);
	    struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
	    if (dbip && db_dirbuild(dbip) == 0) {
		dbs.push_back({path, dbip});
	    }
	}

	// --- PHASE 2: Parallel hashing ---
	if (gopts.geomname_threshold >= 0) {
	    path2namehash = parallel_hash_db(dbs, objnames_hash_db, parallel_threads);
	    for (const auto& kv : path2namehash) namehash2path[kv.second] = kv.first;
	}
	if (gopts.geometry_threshold >= 0) {
	    path2geohash = parallel_hash_db(dbs, geometry_hash_db, parallel_threads);
	    for (const auto& kv : path2geohash) geohash2path[kv.second] = kv.first;
	}

	// --- PHASE 3: Sequential teardown ---
	for (auto& db : dbs) {
	    std::lock_guard<std::mutex> lock(db_mutex);
	    db_close(db.dbip);
	    db.dbip = nullptr;
	}
    } else {
	// --- SERIAL: Open, hash, close one at a time ---
	if (gopts.geomname_threshold >= 0) {
	    for (const auto& path : files) {
		struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
		if (dbip && db_dirbuild(dbip) == 0) {
		    std::string hash = objnames_hash_db(dbip);
		    path2namehash[path] = hash;
		    namehash2path[hash] = path;
		}
		if (dbip) db_close(dbip);
	    }
	}
	if (gopts.geometry_threshold >= 0) {
	    for (const auto& path : files) {
		struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
		if (dbip && db_dirbuild(dbip) == 0) {
		    std::string hash = geometry_hash_db(dbip);
		    path2geohash[path] = hash;
		    geohash2path[hash] = path;
		}
		if (dbip) db_close(dbip);
	    }
	}
    }

    std::ostream* out = &std::cout;
    std::ofstream fout;
    if (bu_vls_strlen(&gopts.ofile)) {
	fout.open(bu_vls_cstr(&gopts.ofile));
	if (fout) out = &fout;
    }

    do_hierarchical_grouping(file_grps,
	    gopts.geomname_threshold,
	    gopts.geometry_threshold,
	    path2namehash,
	    path2geohash,
	    namehash2path,
	    geohash2path,
	    *out);

    if (fout.is_open()) fout.close();

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
