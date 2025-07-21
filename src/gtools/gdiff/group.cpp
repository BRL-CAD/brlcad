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

/*
 * ============================================================================
 * GROUPING AND CLUSTERING ALGORITHMS IN THIS FILE
 * ============================================================================
 *
 * This file implements multi-level grouping (binning) and reporting for BRL-CAD
 * database files according to user-selectable similarity criteria.
 *
 * ----------------------------------------------------------------------------
 * 1. BK-tree Similarity Grouping (for filenames)
 * ----------------------------------------------------------------------------
 * - BK-tree (Burkhard-Keller tree) is a tree data structure optimized for searching
 *   and clustering items in a metric space where a distance function is defined.
 *   Each node contains a value and children at specific distances (e.g., all strings
 *   with edit distance 2 are in one subtree, etc.).
 * - Insertion and query operate by edit distance: for a new string, compare to the
 *   current node; if the distance is 0, group it; otherwise, descend to the child
 *   at the corresponding distance, creating one if needed.
 * - This allows efficient retrieval of all items within a given edit distance.
 * - In this code, BK-tree is used to group files whose base filenames are similar
 *   (by Damerau-Levenshtein edit distance), making it easy to find groups of files
 *   with minor typographical, version, or naming differences.
 *
 * ----------------------------------------------------------------------------
 * 2. Transitive Closure Clustering (for file content similarity via TLSH hash)
 * ----------------------------------------------------------------------------
 * - Transitive closure clustering forms clusters not only by direct similarity,
 *   but also by chains of indirect similarity. Two items are clustered together
 *   if there exists a path of pairwise-similar items connecting them.
 * - Here, each file is hashed (using TLSH) over selected content (object names,
 *   geometry, or both). The TLSH "distance" is computed for every file pair.
 * - If the distance is within a threshold, an edge is created between those files.
 *   Clusters are the connected components of this similarity graph.
 * - This captures families of similar files where changes may accumulate in a sequence
 *   (e.g., A~B~C), even if the endpoints are not directly similar.
 * - This method is used instead of BK-tree for TLSH because TLSH distance does not
 *   satisfy the triangle inequality (it's not a "metric"), so similarity may "chain"
 *   through multiple steps.
 *
 * ----------------------------------------------------------------------------
 * Usage
 * ----------------------------------------------------------------------------
 * - The user can enable filename grouping, content grouping, or both.
 * - Content grouping always uses a single hash and threshold.
 * - Reports show clusters, representative files, difference scores, and timestamps.
 *
 * ----------------------------------------------------------------------------
 * Hash Construction Details
 * ----------------------------------------------------------------------------
 * - A single TLSH hash incorporates object names, geometry, or both, as controlled by flags.
 * - Geometry hashing may be "deep" (similarity-sensitive) or "fast" (robust).
 * - A seed string is included for object names to avoid degenerate hashes for small datasets.
 * ============================================================================
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
#include <tuple>
#include <atomic>
#include <sstream>
#include "tlsh.hpp"
#include "levenshtein_ull.hpp"

extern "C" {
#include "bu/app.h"
#include "bu/hash.h"
#include "bu/file.h"
#include "bu/path.h"
#include "rt/db_io.h"
#include "./gdiff.h"
}

#define TLSH_DEFAULT_THRESHOLD 30

// ============================================================================
// FILE AND PATH UTILITIES
// ============================================================================

struct FileInfo {
    std::string path;
    std::filesystem::file_time_type mtime;
};

/*
 * Get the last modified time for a file.
 */
std::filesystem::file_time_type file_mtime(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return fs::file_time_type::min();
    return ftime;
}

/*
 * Return the last modified date of a file as "YYYY-MM-DD".
 */
std::string file_timestamp(const std::string& path) {
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

/*
 * Sort file paths by mtime (newest first).
 */
std::vector<std::string> sort_files_by_mtime(const std::vector<std::string>& files) {
    std::vector<FileInfo> finfos;
    for (const auto& path : files) {
	finfos.push_back({path, file_mtime(path)});
    }
    std::sort(finfos.begin(), finfos.end(),
	    [](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });
    std::vector<std::string> sorted;
    for (const auto& fi : finfos) sorted.push_back(fi.path);
    return sorted;
}

// ============================================================================
// FILENAME GROUPING (BK-TREE)
// ============================================================================

struct BKNode {
    std::string value;
    std::vector<std::string> files;
    std::map<size_t, BKNode*> children;
    BKNode(const std::string& v) : value(v) {}
    ~BKNode() {
	for (auto& kv : children) delete kv.second;
    }
};

/*
 * BKTree: for fast filename similarity grouping by edit distance.
 */
class BKTree {
    public:
	BKTree(std::function<size_t(const std::string&, const std::string&)> dist)
	    : root(nullptr), distance(dist) {}

	void insert(const std::string& s, const std::string& file) {
	    if (!root) {
		root = new BKNode(s);
		root->files.push_back(file);
		return;
	    }
	    BKNode* node = root;
	    while (true) {
		size_t d = distance(node->value, s);
		if (d == 0) {
		    node->files.push_back(file);
		    break;
		}
		auto it = node->children.find(d);
		if (it == node->children.end()) {
		    node->children[d] = new BKNode(s);
		    node->children[d]->files.push_back(file);
		    break;
		} else {
		    node = it->second;
		}
	    }
	}

	/*
	 * Return clusters of files whose base names are within the given edit distance.
	 */
	std::vector<std::vector<std::string>> clusters(size_t threshold) const {
	    std::vector<std::vector<std::string>> result;
	    std::unordered_set<const BKNode*> node_seen;
	    std::function<void(BKNode*)> traverse = [&](BKNode* node) {
		if (!node) return;
		if (!node_seen.count(node)) {
		    std::vector<BKNode*> cluster;
		    query(node->value, threshold, cluster);
		    std::vector<std::string> files;
		    for (BKNode* n : cluster) {
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

	/*
	 * Find all BKNodes within edit distance threshold of string s.
	 */
	void query(const std::string& s, size_t threshold, std::vector<BKNode*>& out) const {
	    query_rec(root, s, threshold, out);
	}

	~BKTree() { delete root; }

    private:
	BKNode* root;
	std::function<size_t(const std::string&, const std::string&)> distance;

	void query_rec(BKNode* node, const std::string& s, size_t threshold, std::vector<BKNode*>& out) const {
	    if (!node) return;
	    size_t d = distance(node->value, s);
	    if (d <= threshold) out.push_back(node);
	    for (size_t i = (d >= threshold ? d - threshold : 0); i <= d + threshold; ++i) {
		auto it = node->children.find(i);
		if (it != node->children.end()) query_rec(it->second, s, threshold, out);
	    }
	}
};

// ============================================================================
// CONTENT CLUSTERING (TLSH + TRANSITIVE CLOSURE)
// ============================================================================

struct DBFile {
    std::string path;
    struct db_i *dbip;
};

/*
 * content_hash - Computes a single TLSH hash for a database, optionally including:
 *   - Object names (sorted, concatenated, with a seed)
 *   - Geometry contents (sorted, deep or fast)
 * Returns an empty string if neither is enabled.
 */
std::string content_hash(struct db_i *dbip, bool use_names, bool use_geometry, int geom_fast) {
    if (!dbip)
	return std::string();

    db_update_nref(dbip, &rt_uniresource);
    tlsh::Tlsh hasher;

    if (use_names) {
	std::set<std::string> onames;
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;
	    onames.insert(std::string(dp->d_namep));
	} FOR_ALL_DIRECTORY_END;
	static const std::string tls_seed = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=";
	std::string data = tls_seed;
	for (const auto& o : onames) data.append(o);
	hasher.update((const unsigned char *)data.c_str(), data.length());
    }

    if (use_geometry) {
	std::vector<const directory*> directories;
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    if (dp->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
		continue;
	    directories.push_back(dp);
	} FOR_ALL_DIRECTORY_END;
	std::sort(directories.begin(), directories.end(),
		[](const directory* a, const directory* b) {
		return strcmp(a->d_namep, b->d_namep) < 0;
		});

	if (!geom_fast) {
	    std::vector<uint8_t> buffer;
	    constexpr size_t CHUNK_SIZE = 1024 * 1024;

	    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
	    BU_EXTERNAL_INIT(&ext);

	    for (const directory* dp_geom : directories) {
		if (dp_geom->d_len == 0)
		    continue;
		ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, dp_geom->d_len, "resize ext_buf");
		ext.ext_nbytes = dp_geom->d_len;
		if (db_read(dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp_geom->d_addr) < 0) {
		    memset(ext.ext_buf, 0, dp_geom->d_len);
		    ext.ext_nbytes = 0;
		    continue;
		}
		buffer.insert(buffer.end(), ext.ext_buf, ext.ext_buf + ext.ext_nbytes);
		if (buffer.size() >= CHUNK_SIZE) {
		    hasher.update(buffer.data(), buffer.size());
		    buffer.clear();
		}
	    }
	    if (!buffer.empty()) {
		hasher.update(buffer.data(), buffer.size());
		buffer.clear();
	    }
	    if (ext.ext_buf)
		bu_free(ext.ext_buf, "final free of ext_buf");
	} else {
	    std::vector<unsigned long long> ghashes;
	    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
	    BU_EXTERNAL_INIT(&ext);

	    unsigned int max_bufsize = 0;
	    for (const directory* dp_geom : directories) {
		if (dp_geom->d_len > max_bufsize)
		    max_bufsize = dp_geom->d_len;
	    }
	    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

	    for (const directory* dp_geom : directories) {
		ext.ext_nbytes = dp_geom->d_len;
		if (db_read(dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp_geom->d_addr) < 0) {
		    memset(ext.ext_buf, 0, dp_geom->d_len);
		    ext.ext_nbytes = 0;
		    continue;
		}
		unsigned long long hash = bu_data_hash(ext.ext_buf, ext.ext_nbytes);
		ghashes.push_back(hash);
	    }
	    std::sort(ghashes.begin(), ghashes.end());
	    hasher.update((const unsigned char *)ghashes.data(), ghashes.size() * sizeof(unsigned long long));
	    if (ext.ext_buf)
		bu_free(ext.ext_buf, "final free of ext_buf");
	}
    }

    hasher.final();
    return hasher.getHash();
}

/*
 * hash_files_parallel - Computes content hashes for files in parallel.
 * Returns a map path -> hash.
 */
std::unordered_map<std::string, std::string> hash_files_parallel(
	const std::vector<DBFile>& dbs,
	const struct gdiff_group_opts& gopts,
	size_t nthreads=4)
{
    std::unordered_map<std::string, std::string> result;
    std::mutex res_mutex;
    size_t total = dbs.size();
    std::vector<std::thread> threads;
    std::atomic<size_t> idx{0};
    auto worker = [&]() {
	while (true) {
	    size_t i = idx.fetch_add(1);
	    if (i >= total) break;
	    std::string hash = content_hash(
		    dbs[i].dbip,
		    gopts.use_names,
		    gopts.use_geometry,
		    gopts.geom_fast
		    );
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

// ============================================================================
// CLUSTERING, BINNING, AND REPORTING
// ============================================================================

struct HashInfo {
    std::string path;
    std::filesystem::file_time_type mtime;
    std::string hash;
};

/*
 * cluster_content - Cluster files by transitive closure on TLSH distance.
 */
std::vector<std::vector<HashInfo>> cluster_content(const std::vector<HashInfo>& files, int threshold) {
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
    std::vector<std::vector<HashInfo>> result;
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
	std::vector<HashInfo> c;
	for (size_t idx : cluster) c.push_back(files[idx]);
	result.push_back(c);
    }
    return result;
}

/*
 * cluster_bins - Build bins from clusters, reporting newest file and center.
 */
std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> cluster_bins(
	const std::vector<std::vector<HashInfo>>& clusters)
{
    std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> bins;
    for (const auto& cluster : clusters) {
	if (cluster.empty()) continue;
	int N = cluster.size();
	auto newest_it = std::max_element(cluster.begin(), cluster.end(),
		[](const HashInfo& a, const HashInfo& b) { return a.mtime < b.mtime; });
	const auto& newest = *newest_it;

	std::vector<int> total_diffs(N, 0);
	for (int i = 0; i < N; ++i) {
	    tlsh::Tlsh hi; hi.fromTlshStr(cluster[i].hash.c_str());
	    for (int j = 0; j < N; ++j) {
		if (i == j) continue;
		tlsh::Tlsh hj; hj.fromTlshStr(cluster[j].hash.c_str());
		total_diffs[i] += hi.diff(hj);
	    }
	}
	int center_idx = std::min_element(total_diffs.begin(), total_diffs.end()) - total_diffs.begin();
	const auto& center = cluster[center_idx];

	tlsh::Tlsh hnewest; hnewest.fromTlshStr(newest.hash.c_str());
	tlsh::Tlsh hcenter; hcenter.fromTlshStr(center.hash.c_str());

	std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>> entries;
	for (const auto& f : cluster) {
	    tlsh::Tlsh hcur; hcur.fromTlshStr(f.hash.c_str());
	    int diff_newest = hnewest.diff(hcur);
	    int diff_center = hcenter.diff(hcur);
	    entries.push_back({f.path, diff_newest, diff_center, f.mtime});
	}
	std::sort(entries.begin(), entries.end(),
		[](const auto& a, const auto& b){ return std::get<3>(a) > std::get<3>(b); });

	bins[newest.path] = entries;
    }
    return bins;
}

/*
 * hash_bins_for_files - Hash and cluster a set of files, returning bins for reporting.
 */
std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> hash_bins_for_files(
	const std::vector<std::string>& files,
	const std::unordered_map<std::string, std::string>& path2hash,
	int threshold)
{
    std::vector<HashInfo> infos;
    for (const auto& path : files) {
	if (!path2hash.count(path) || path2hash.at(path).empty()) continue;
	infos.push_back({path, file_mtime(path), path2hash.at(path)});
    }
    auto clusters = cluster_content(infos, threshold);
    return cluster_bins(clusters);
}

/*
 * print_cluster_report - Print a cluster/bin report to the output stream.
 */
void print_cluster_report(
	const std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>>& groups,
	std::ostream& out,
	bool indent_group = false
	)
{
    for (const auto& [newest, entries] : groups) {
	size_t longest_path = newest.size();
	for (const auto& entry_tuple : entries) {
	    longest_path = std::max(longest_path, std::get<0>(entry_tuple).size());
	}
	size_t score_col = longest_path + 2;

	int dc_newest = 0;
	for (const auto& entry_tuple : entries) {
	    if (std::get<0>(entry_tuple) == newest) {
		dc_newest = std::get<2>(entry_tuple);
		break;
	    }
	}
	std::ostringstream parent_score_ss;
	parent_score_ss << "[Δx̄" << std::right << std::setw(4) << dc_newest << "]";

	std::string group_indent = indent_group ? "  " : "";

	out << group_indent << std::left << std::setw(score_col) << newest
	    << parent_score_ss.str() << " " << file_timestamp(newest) << "\n";

	for (const auto& entry_tuple : entries) {
	    const std::string& entry = std::get<0>(entry_tuple);
	    int dn = std::get<1>(entry_tuple);
	    int dc = std::get<2>(entry_tuple);
	    if (entry == newest) continue;
	    std::ostringstream score_ss;
	    score_ss << "[Δ" << std::right << std::setw(4) << dn
		<< ", Δx̄" << std::right << std::setw(4) << dc << "]";
	    out << group_indent << "  " << std::left << std::setw(score_col-2) << entry
		<< score_ss.str() << " " << file_timestamp(entry) << "\n";
	}
    }
}

/*
 * print_filename_bin_header - Prints a report header for a filename bin.
 */
void print_filename_bin_header(const std::string& seed_path, int threshold, std::ostream& out)
{
    out << "====================================================================\n";
    out << "Filename Bin Report\n";
    out << "Seed (center file):   " << seed_path << "\n";
    out << "Date of Seed:         " << file_timestamp(seed_path) << "\n";
    out << "Max Damarau-Levenshtein edit distance of binned paths from seed:  " << threshold << "\n";
    out << "====================================================================\n";
}

/*
 * group_and_report - Top-level grouping and reporting logic.
 * - If filename grouping is active, clusters within each filename bin.
 * - Otherwise, clusters all files together.
 */
void group_and_report(
	const std::unordered_map<std::string, std::vector<std::string>> &file_grps,
	int threshold,
	const std::unordered_map<std::string, std::string> &path2hash,
	std::ostream& out)
{
    bool filename_active = (file_grps.size() > 1);

    if (filename_active) {
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_files_by_mtime(fg.second);
	    auto hash_bins = hash_bins_for_files(sorted, path2hash, threshold);
	    if (!sorted.empty()) {
		out << sorted.front() << " (" << file_timestamp(sorted.front()) << "):\n";
		print_cluster_report(hash_bins, out, true);
	    }
	}
    } else {
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto hash_bins = hash_bins_for_files(all_files, path2hash, threshold);
	print_cluster_report(hash_bins, out);
    }
}

// ============================================================================
// MAIN ENTRYPOINT: gdiff_group
// ============================================================================

/*
 * gdiff_group - Main function for grouped similarity reporting.
 * - Handles file discovery, grouping, hashing, clustering, and reporting.
 * - If neither use_names nor use_geometry are enabled, enables both by default.
 */
int gdiff_group(int argc, const char **argv, struct gdiff_group_opts *g_opts)
{
    struct gdiff_group_opts gopts = GDIFF_GROUP_OPTS_DEFAULT;
    if (g_opts) {
	gopts.filename_threshold = g_opts->filename_threshold;
	gopts.threshold = g_opts->threshold;
	gopts.use_names = g_opts->use_names;
	gopts.use_geometry = g_opts->use_geometry;
	gopts.geom_fast = g_opts->geom_fast;
	bu_vls_sprintf(&gopts.fpattern, "%s", bu_vls_cstr(&g_opts->fpattern));
	bu_vls_sprintf(&gopts.ofile, "%s", bu_vls_cstr(&g_opts->ofile));
    }

    if (!gopts.use_names && !gopts.use_geometry) {
	gopts.use_names = 1;
	gopts.use_geometry = 1;
    }
    if (gopts.filename_threshold == -1 && gopts.threshold == -1)
	gopts.threshold = TLSH_DEFAULT_THRESHOLD;
    if (!bu_vls_strlen(&gopts.fpattern))
	bu_vls_sprintf(&gopts.fpattern, "*.g");

    // File discovery
    std::vector<FileInfo> file_infos;
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, MAXPATHLEN, BU_DIR_CURR, NULL);

    if (argc && argv) {
	for (int i = 0; i < argc; i++) {
	    if (bu_file_directory(argv[i]))  {
		std::string rstr(argv[i]);
		for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
		    if (!entry.is_regular_file())
			continue;
		    if (!bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
			file_infos.push_back({entry.path().string(), file_mtime(entry.path().string())});
		}
		continue;
	    }
	    if (bu_file_exists(argv[i], NULL)) {
		file_infos.push_back({std::string(argv[i]), file_mtime(argv[i])});
	    }
	}
    }
    if (!file_infos.size()) {
	std::string rstr(cwd);
	for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
	    if (!entry.is_regular_file())
		continue;
	    if (!bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
		file_infos.push_back({entry.path().string(), file_mtime(entry.path().string())});
	}
    }

    std::sort(file_infos.begin(), file_infos.end(),
	    [](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });

    std::vector<std::string> files;
    for (const auto& fi : file_infos) files.push_back(fi.path);

    // Filename grouping
    std::unordered_map<std::string, std::vector<std::string>> file_grps;
    int filename_threshold = gopts.filename_threshold;
    if (filename_threshold >= 0) {
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
	auto clusters = bktree.clusters(filename_threshold);
	for (const auto& cluster : clusters) {
	    std::vector<std::string> sorted = sort_files_by_mtime(cluster);
	    if (!sorted.empty())
		file_grps[sorted.front()] = cluster;
	}
    } else {
	std::vector<std::string> sorted = sort_files_by_mtime(files);
	if (!sorted.empty())
	    file_grps[sorted.front()] = files;
    }

    // Hash computation
    std::unordered_map<std::string, std::string> path2hash;
    size_t parallel_threads = gopts.thread_cnt;
    if (!parallel_threads)
	parallel_threads = bu_avail_cpus();

    if (parallel_threads > 1) {
	std::vector<DBFile> dbs;
	std::mutex db_mutex;
	for (const auto& path : files) {
	    std::lock_guard<std::mutex> lock(db_mutex);
	    struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
	    if (dbip && db_dirbuild(dbip) == 0) {
		dbs.push_back({path, dbip});
	    }
	}
	path2hash = hash_files_parallel(dbs, gopts, parallel_threads);
	for (auto& db : dbs) {
	    std::lock_guard<std::mutex> lock(db_mutex);
	    db_close(db.dbip);
	    db.dbip = nullptr;
	}
    } else {
	for (const auto& path : files) {
	    struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
	    if (dbip && db_dirbuild(dbip) == 0) {
		std::string hash = content_hash(dbip, gopts.use_names, gopts.use_geometry, gopts.geom_fast);
		path2hash[path] = hash;
	    }
	    if (dbip) db_close(dbip);
	}
    }

    std::ostream* out = &std::cout;
    std::ofstream fout;
    if (bu_vls_strlen(&gopts.ofile)) {
	fout.open(bu_vls_cstr(&gopts.ofile));
	if (fout) out = &fout;
    }

    if (filename_threshold >= 0) {
	for (const auto& [key, bin_files] : file_grps) {
	    print_filename_bin_header(key, filename_threshold, *out);
	    std::unordered_map<std::string, std::vector<std::string>> single_bin;
	    single_bin[key] = bin_files;
	    group_and_report(
		    single_bin,
		    gopts.threshold,
		    path2hash,
		    *out
		    );
	    *out << "\n";
	}
    } else {
	group_and_report(
		file_grps,
		gopts.threshold,
		path2hash,
		*out
		);
    }

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
