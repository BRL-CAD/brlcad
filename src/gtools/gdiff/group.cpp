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
 * ---------------------------------------------------------------------------
 * Clustering Algorithms Overview
 *
 * 1. BK-tree (Burkhard-Keller tree) Clustering
 * --------------------------------------------
 * BK-tree clustering is a method for organizing objects that can be compared
 * using a distance metric (such as string edit distance). It enables efficient
 * grouping of items by similarity, particularly when the "distance" between
 * items is small.
 *
 * In this code, BK-tree clustering is used only for filename grouping:
 *  - Database files whose base filenames are similar (by edit distance) are
 *    clustered together. For example, "model_v1.g" and "model_v2.g" would be
 *    grouped, even if they differ by a few characters.
 *  - This is valuable for identifying versioned or typo-related clusters,
 *    and for reducing redundant comparisons.
 *  - BK-tree allows efficient queries for all items within a given distance
 *    threshold, which is beneficial for large sets of filenames.
 *
 * 2. Transitive Closure Clustering for Hash Groups
 * -----------------------------------------------
 * For hash-based grouping (using TLSH for object name and geometry hashes),
 * the code uses a transitive closure algorithm instead of BK-tree clustering.
 * The process is:
 *   - Compute pairwise hash differences between all files.
 *   - Connect files whose hash difference is less than a given threshold.
 *   - Clusters are defined as sets of files connected (directly or indirectly)
 *     through such pairwise relationships.
 *
 * Why transitive closure for hashes instead of BK-tree?
 *   - BK-tree excels for metrics like string edit distance, but hash distances
 *     may not behave like typical string metrics. The TLSH hash difference may
 *     not satisfy properties (e.g., triangle inequality) that BK-tree relies on.
 *   - With transitive closure, clusters can form even if some file pairs are
 *     "far apart," as long as there is a chain of close-enough links.
 *   - This more accurately reflects the real world: two files may be similar
 *     through intermediary revisions, but not directly.
 *   - Transitive closure ensures that all files mutually reachable by hash
 *     similarity are grouped together, avoiding the fragmentation that
 *     could occur with BK-tree's stricter partitioning.
 *
 * In summary: BK-tree is used for filename clustering due to its efficiency
 * with edit distance, whereas transitive closure is used for hash clustering
 * to capture connected regions of similarity, even when direct pairwise
 * similarity is absent.
 * ---------------------------------------------------------------------------
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

/*
 * BK-tree node and clustering
 *
 * BK-tree is a data structure that efficiently clusters items (such as strings)
 * based on a distance metric (like edit distance or Levenshtein distance).
 * Each node represents a value, and children are organized by their distance
 * from the parent. This allows efficient queries for all items within a given
 * threshold of similarity.
 *
 * In this file, BK-tree is used to cluster filenames by their base name edit
 * distance. This makes it possible to group files that are likely versions or
 * variants of the same thing, even if they don't match exactly.
 */
struct BKTreeNode {
    std::string value;
    std::vector<std::string> files;
    std::map<size_t, BKTreeNode*> children;
    BKTreeNode(const std::string& v) : value(v) {}
    ~BKTreeNode() {
	for (auto& kv : children) delete kv.second;
    }
};

/*
 * BKTree class provides insertion and clustering logic for BK-tree.
 * It is used to group similar filenames (by edit distance) before
 * any further grouping or scoring is performed.
 */
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

	/*
	 * clusters(threshold): Returns clusters of files whose base names
	 * are within the specified edit distance threshold.
	 * Each file is included in exactly one cluster.
	 */
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

/*
 * Transitive Closure Hash Clustering
 *
 * For hash-based grouping, we use a transitive closure algorithm:
 * - Each file is a node.
 * - An edge is formed between any two files whose hash difference is less than
 *   the specified threshold.
 * - Clusters are defined as connected components in this graph, found by
 *   traversing all reachable nodes.
 *
 * Why use transitive closure for hashes instead of BK-tree?
 * - Hash differences (e.g., TLSH) may not behave like edit distances and may
 *   not satisfy triangle inequality, making BK-tree clustering less suitable.
 * - Transitive closure allows clusters to form through chains of similarity,
 *   reflecting cases where files are linked by intermediate revisions but not
 *   directly similar.
 * - This captures broader regions of similarity and avoids fragmentation that
 *   could occur with BK-tree, which only clusters strictly within threshold of
 *   the reference value.
 *
 * In practice, this means all files mutually reachable by a chain of "close"
 * hash relationships are grouped together, providing more robust clusters for
 * geometry and object name hashes.
 */

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

    struct directory *dp;
    unsigned int max_bufsize = 0;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
	    continue;
	if (dp->d_len > max_bufsize)
	    max_bufsize = dp->d_len;
    } FOR_ALL_DIRECTORY_END;

    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    BU_EXTERNAL_INIT(&ext);
    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

    std::vector<unsigned long long> ghashes;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	if (UNLIKELY(!dp->d_namep || !strlen(dp->d_namep)))
	    continue;
	ext.ext_nbytes = dp->d_len;
	if (db_read(dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp->d_addr) < 0) {
	    memset(ext.ext_buf, 0, dp->d_len);
	    ext.ext_nbytes = 0;
	    continue;
	}
	unsigned long long hash = bu_data_hash(ext.ext_buf, ext.ext_nbytes);
	ghashes.push_back(hash);
    } FOR_ALL_DIRECTORY_END;

    std::sort(ghashes.begin(), ghashes.end());

    tlsh::Tlsh hasher;
    hasher.update((const unsigned char *)ghashes.data(), ghashes.size() * sizeof(unsigned long long));
    hasher.final();
    std::string hstr = hasher.getHash();

    if (ext.ext_buf)
	bu_free(ext.ext_buf, "final free of ext_buf");
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

/*
 * cluster_by_hash: Given a vector of FileHashInfo and a TLSH threshold,
 * construct clusters using transitive closure: any files reachable via
 * a chain of pairwise TLSH-difference-below-threshold edges are grouped.
 */
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

// For each cluster, use the newest file as the key, calculate the center, and report both TLSH differences
std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> bins_from_clusters_newest_and_center(
	const std::vector<std::vector<FileHashInfo>>& clusters)
{
    std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> bins;
    for (const auto& cluster : clusters) {
	if (cluster.empty()) continue;
	int N = cluster.size();
	// Find the newest file by mtime
	auto newest_it = std::max_element(cluster.begin(), cluster.end(),
		[](const FileHashInfo& a, const FileHashInfo& b) { return a.mtime < b.mtime; });
	const auto& newest = *newest_it;

	// Calculate total diffs for each file to find center
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

	// For each file, report both TLSH diff to newest and to center
	std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>> entries;
	for (const auto& f : cluster) {
	    tlsh::Tlsh hcur; hcur.fromTlshStr(f.hash.c_str());
	    int diff_newest = hnewest.diff(hcur);
	    int diff_center = hcenter.diff(hcur);
	    entries.push_back({f.path, diff_newest, diff_center, f.mtime});
	}
	// Sort by mtime, newest first
	std::sort(entries.begin(), entries.end(),
		[](const auto& a, const auto& b){ return std::get<3>(a) > std::get<3>(b); });

	bins[newest.path] = entries;
    }
    return bins;
}

std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>> group_by_hash_map_newest_and_center(
	const std::vector<std::string>& files,
	const std::unordered_map<std::string, std::string>& path2hash,
	int threshold)
{
    std::vector<FileHashInfo> infos;
    for (const auto& path : files) {
	if (!path2hash.count(path) || path2hash.at(path).empty()) continue;
	infos.push_back({path, get_file_mtime(path), path2hash.at(path)});
    }
    auto clusters = cluster_by_hash(infos, threshold);
    return bins_from_clusters_newest_and_center(clusters);
}

// ------------------------------------
// Reporting (key: newest file, scores are [Δnewest_score, Δx̄center_score], aligned output)
/*
 * Output lines:
 * - For each group key (the newest file), prints the path, its Δx̄ score (distance from center), and its date, all aligned.
 * - For each leaf entry, prints path, [Δ(newest), Δx̄(center)] scores, and date, all aligned.
 * - Ensures score/date columns start at the same position for both key and leaves, always with at least one space after the longest path.
 * - When filename threshold is active, indent entries within filename group for clarity.
 */
void report_single_level_newest_and_center(
	const std::map<std::string, std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>>>& groups,
	std::ostream& out,
	bool indent_group = false // Set to true when printing within filename grouping
	)
{
    for (const auto& [newest, entries] : groups) {
	// Find longest file path for output alignment (includes all entries)
	size_t longest_path = newest.size();
	for (const auto& entry_tuple : entries) {
	    longest_path = std::max(longest_path, std::get<0>(entry_tuple).size());
	}
	size_t score_col = longest_path + 2; // At least one space after path

	// Find this bin's Δx̄ score for the newest file
	int dc_newest = 0;
	for (const auto& entry_tuple : entries) {
	    if (std::get<0>(entry_tuple) == newest) {
		dc_newest = std::get<2>(entry_tuple);
		break;
	    }
	}
	// Use a unique variable for the parent key's score string
	std::ostringstream parent_score_ss;
	parent_score_ss << "[Δx̄" << std::right << std::setw(4) << dc_newest << "]";

	// Indent group if requested (for filename threshold mode)
	std::string group_indent = indent_group ? "  " : "";

	out << group_indent << std::left << std::setw(score_col) << newest
	    << parent_score_ss.str() << " " << get_file_timestamp(newest) << "\n";

	// Print leaf entries
	for (const auto& entry_tuple : entries) {
	    const std::string& entry = std::get<0>(entry_tuple);
	    int dn = std::get<1>(entry_tuple);
	    int dc = std::get<2>(entry_tuple);
	    if (entry == newest) continue; // Already printed above
	    std::ostringstream score_ss;
	    score_ss << "[Δ" << std::right << std::setw(4) << dn
		<< ", Δx̄" << std::right << std::setw(4) << dc << "]";
	    out << group_indent << "  " << std::left << std::setw(score_col-2) << entry
		<< score_ss.str() << " " << get_file_timestamp(entry) << "\n";
	}
    }
}

// ------------------------------------
// Hierarchical grouping, reporting (for hash-based levels, ordering by date, reporting both scores)
void do_hierarchical_grouping_newest_and_center(
	const std::unordered_map<std::string, std::vector<std::string>> &file_grps,
	int geomname_threshold,
	int geometry_threshold,
	const std::unordered_map<std::string, std::string> &path2namehash,
	const std::unordered_map<std::string, std::string> &path2geohash,
	std::ostream& out)
{
    bool filename_active = (file_grps.size() > 1);
    bool geomname_active = (geomname_threshold >= 0);
    bool geometry_active = (geometry_threshold >= 0);

    // --- Depth 1: Only filename OR only geomname OR only geometry ---
    if (filename_active && !geomname_active && !geometry_active) {
	std::map<std::string, std::vector<std::string>> report_map;
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    if (!sorted.empty()) {
		report_map[sorted.front()] = sorted;
	    }
	}
	for (const auto& [key, entries] : report_map) {
	    out << key << " (" << get_file_timestamp(key) << "):\n";
	    for (const auto& entry : entries) {
		if (entry == key) continue;
		out << "  " << entry << " (" << get_file_timestamp(entry) << ")\n";
	    }
	}
	return;
    }
    if (!filename_active && geomname_active && !geometry_active) {
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto name_bins = group_by_hash_map_newest_and_center(all_files, path2namehash, geomname_threshold);
	report_single_level_newest_and_center(name_bins, out);
	return;
    }
    if (!filename_active && !geomname_active && geometry_active) {
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto geo_bins = group_by_hash_map_newest_and_center(all_files, path2geohash, geometry_threshold);
	report_single_level_newest_and_center(geo_bins, out);
	return;
    }

    // Depth 2
    if (filename_active && geomname_active && !geometry_active) {
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto name_bins = group_by_hash_map_newest_and_center(sorted, path2namehash, geomname_threshold);
	    if (!sorted.empty()) {
		out << sorted.front() << " (" << get_file_timestamp(sorted.front()) << "):\n";
		report_single_level_newest_and_center(name_bins, out, true); // << INDENT HERE
	    }
	}
	return;
    }
    if (filename_active && !geomname_active && geometry_active) {
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto geo_bins = group_by_hash_map_newest_and_center(sorted, path2geohash, geometry_threshold);
	    if (!sorted.empty()) {
		out << sorted.front() << " (" << get_file_timestamp(sorted.front()) << "):\n";
		report_single_level_newest_and_center(geo_bins, out, true); // << INDENT HERE
	    }
	}
	return;
    }
    if (!filename_active && geomname_active && geometry_active) {
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto name_bins = group_by_hash_map_newest_and_center(all_files, path2namehash, geomname_threshold);
	report_single_level_newest_and_center(name_bins, out);
	for (const auto& [newest, entries] : name_bins) {
	    out << std::left << std::setw(newest.size() + 1) << newest
		<< get_file_timestamp(newest) << "\n";
	    std::vector<std::string> geom_files;
	    for (const auto& entry_tuple : entries) {
		geom_files.push_back(std::get<0>(entry_tuple));
	    }
	    auto geo_bins = group_by_hash_map_newest_and_center(geom_files, path2geohash, geometry_threshold);
	    report_single_level_newest_and_center(geo_bins, out, true); // << INDENT HERE
	}
	return;
    }

    // --- Depth 3: filename+geomname+geometry ---
    if (filename_active && geomname_active && geometry_active) {
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_by_mtime(fg.second);
	    auto name_bins = group_by_hash_map_newest_and_center(sorted, path2namehash, geomname_threshold);
	    if (!sorted.empty()) {
		out << sorted.front() << " (" << get_file_timestamp(sorted.front()) << "):\n";
		for (const auto& [newest, entries] : name_bins) {
		    out << "  " << std::left << std::setw(newest.size() + 1) << newest
			<< get_file_timestamp(newest) << "\n";
		    std::vector<std::string> geom_files;
		    for (const auto& entry_tuple : entries) {
			geom_files.push_back(std::get<0>(entry_tuple));
		    }
		    auto geo_bins = group_by_hash_map_newest_and_center(geom_files, path2geohash, geometry_threshold);
		    report_single_level_newest_and_center(geo_bins, out, true); // << INDENT HERE
		}
	    }
	}
	return;
    }
}

// ------------------------------------
// Main grouping function, entrypoint
int gdiff_group(int argc, const char **argv, struct gdiff_group_opts *g_opts)
{
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

    // If we got something via argv arguments, go with it.  If we have nothing, however,
    // iterate over the cwd with fpattern
    if (argc && argv) {
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
    }
    if (!file_infos.size()) {
	std::string rstr(cwd);
	for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
	    if (!entry.is_regular_file())
		continue;
	    if (!bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
		file_infos.push_back({entry.path().string(), get_file_mtime(entry.path().string())});
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
	}
	if (gopts.geometry_threshold >= 0) {
	    path2geohash = parallel_hash_db(dbs, geometry_hash_db, parallel_threads);
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

    // Use binning/reporting logic: key is newest file, report both Δnewest_score and Δx̄center_score
    do_hierarchical_grouping_newest_and_center(file_grps,
	    gopts.geomname_threshold,
	    gopts.geometry_threshold,
	    path2namehash,
	    path2geohash,
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
