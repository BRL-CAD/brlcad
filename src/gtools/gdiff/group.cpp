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

#include <cstring>
#include <ctime>
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

extern "C" {
#include "bu/app.h"
#include "bu/hash.h"
#include "bu/file.h"
#include "bu/path.h"
#include "rt/db_io.h"
#include "./gdiff.h"
}

#define TLSH_DEFAULT_THRESHOLD 30
#define GDIFF_CACHE_FORMAT 1

// ============================================================================
// TIME, PATH, FILE UTILITIES
// ============================================================================

/*
 * Safe, allocation-free whitespace trim. Clears the string if it is all
 * whitespace.
 */
static inline void wtrim(std::string& s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) { s.clear(); return; }
    auto e = s.find_last_not_of(ws);
    // erase everything after last non-ws
    s.erase(e + 1);
    s.erase(0, b);
}

/*
 * Helper: convert std::filesystem::file_time_type to std::time_t portably.
 * This is needed because file_time_type may use an implementation-defined clock.
 */
inline std::time_t fs_time_to_time_t(std::filesystem::file_time_type ft) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
	    ft - std::filesystem::file_time_type::clock::now()
	    + system_clock::now()
	    );
    return system_clock::to_time_t(sctp);
}

/* Helper: Format a filesystem timestamp as "YYYY-MM-DD" using
 * fs_time_to_time_t. */
static inline std::string format_ymd(std::filesystem::file_time_type ft) {
    if (ft == std::filesystem::file_time_type::min())
	return "(timestamp unavailable)";
    static std::mutex timefmt_mutex;
    std::time_t tt = fs_time_to_time_t(ft);
    std::tm tm_copy;
    {
	std::lock_guard<std::mutex> lk(timefmt_mutex);
	std::tm *tm_ptr = std::localtime(&tt);
	if (!tm_ptr) return "(timestamp unavailable)";
	tm_copy = *tm_ptr; // copy out while protected
    }
    char buf[16];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_copy))
	return std::string(buf);
    return "(timestamp unavailable)";
}

/*
 * Helper: Get a display path string, relative if requested, otherwise canonical/absolute.
 */
static std::string get_display_path(const std::string& canon, bool show_relative) {
    if (show_relative) {
	std::error_code ec;
	auto rel = std::filesystem::relative(canon, std::filesystem::current_path(), ec);
	if (!ec)
	    return rel.string();
    }
    return canon;
}

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
 * Get POSIX mtime and file size for cache validation.
 */
static std::pair<std::time_t, uintmax_t> stat_file_for_cache(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(path);
    std::time_t mtime = 0;
    uintmax_t size = 0;
    if (fs::exists(p, ec)) {
	mtime = fs_time_to_time_t(fs::last_write_time(p, ec));
	size = fs::file_size(p, ec);
    }
    return {mtime, size};
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
// CACHE SYSTEM SUPPORTING MULTIPLE CONFIGURATIONS + VERSIONED ON-DISK FORMAT
// ============================================================================
//
// Cache file format (text, line-oriented):
// - The file begins with a REQUIRED version header line in the form:
//       # gdiff-cache-version=<integer>
//   The <integer> must equal GDIFF_CACHE_FORMAT at read time, otherwise the
//   cache is rejected and ignored.
//
// - The second line is an informational comment describing the CSV columns:
//       # path,mtime,size,use_names,use_geometry,geom_fast,hash
//
// - Each subsequent non-empty, non-comment line is one cache record with 7
//   comma-separated fields:
//     1) path         : file path as written (no quoting/escaping performed)
//     2) mtime        : time_t value (seconds since epoch) captured at hash time
//     3) size         : uintmax_t file size in bytes captured at hash time
//     4) use_names    : integer flag controlling name-based hashing (e.g., 0/1)
//     5) use_geometry : integer flag controlling geometry-based hashing (0/1)
//     6) geom_fast    : integer flag for fast geometry mode (0/1)
//     7) hash         : computed digest string for the (file, flags) tuple
//
// Parsing rules:
// - Lines beginning with '#' are comments and are skipped (except the required
//   version header which is validated).
// - Blank lines are ignored.
// - Whitespace around individual CSV fields is trimmed.
// - Records are only accepted if:
//     * the version header is present and matches GDIFF_CACHE_FORMAT, and
//     * the file exists at 'path' at read time, and
//     * the current (mtime, size) from stat_file_for_cache(path) exactly match
//       the stored mtime and size. Otherwise, the record is skipped silently.
// - Malformed lines (wrong field count, parse failures, empty critical fields)
//   are skipped silently.
//
// Writing rules:
// - The output file is truncated and rewritten in full.
// - The exact first two lines are:
//       # gdiff-cache-version=<GDIFF_CACHE_FORMAT>
//       # path,mtime,size,use_names,use_geometry,geom_fast,hash
// - Each cache entry is written as a single CSV line with the 7 fields listed
//   above, in order, with no quoting or escaping.
//   NOTE: If 'path' contains a comma, reading will treat it as a field
//   separator and parsing will break. Current format assumes no commas in paths.
//
// Versioning behavior:
// - If the version line is missing or does not match GDIFF_CACHE_FORMAT, the
//   entire cache is ignored and an empty map is returned. This prevents mixing
//   cache files from incompatible formats.
//
// Key semantics:
// - The effective cache key is (path, mtime, size, use_names, use_geometry,
//   geom_fast). Two entries are considered identical only if all components
//   match, ensuring configuration-sensitive caching.
//
// Error handling:
// - I/O failures on open simply return an empty cache without throwing.
// - Version mismatches and missing version headers emit a warning to stderr and
//   return an empty cache.
// - Individual bad lines are skipped without warning to keep the cache robust.
//
// ============================================================================

struct HashCacheKey {
    std::string path;      // File path as used when hashing
    std::time_t mtime;     // File modification time recorded at hash time
    uintmax_t size;        // File size recorded at hash time
    int use_names;         // Hashing config: include name-based inputs (0/1)
    int use_geometry;      // Hashing config: include geometry inputs (0/1)
    int geom_fast;         // Hashing config: fast-geometry mode (0/1)

    // Keys are equal only if all components match. This ties cached hashes to
    // both the file's identity (path, mtime, size) and the hashing config.
    bool operator==(const HashCacheKey& other) const {
	return path == other.path &&
	    mtime == other.mtime &&
	    size == other.size &&
	    use_names == other.use_names &&
	    use_geometry == other.use_geometry &&
	    geom_fast == other.geom_fast;
    }
};

namespace std {
// Hash combiner for HashCacheKey.
// Uses XOR with shifting to mix individual field hashes into one value.
// Note: This is a simple combiner; for large maps consider a stronger mixer.
template<>
    struct hash<HashCacheKey> {
	std::size_t operator()(const HashCacheKey& k) const {
	    std::size_t h1 = std::hash<std::string>()(k.path);
	    std::size_t h2 = std::hash<std::time_t>()(k.mtime);
	    std::size_t h3 = std::hash<uintmax_t>()(k.size);
	    std::size_t h4 = std::hash<int>()(k.use_names);
	    std::size_t h5 = std::hash<int>()(k.use_geometry);
	    std::size_t h6 = std::hash<int>()(k.geom_fast);
	    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
	}
    };
}

// Read the entire cache file into memory, validating the format version and
// filtering out entries whose on-disk file state no longer matches.
    static std::unordered_map<HashCacheKey, std::string>
read_full_hash_cache(const std::string &filename)
{
    static const std::string kHdr = "# gdiff-cache-version=";
    std::unordered_map<HashCacheKey, std::string> cache;

    // If the file can't be opened, treat as empty cache.
    std::ifstream infile(filename);
    if (!infile) return cache;

    std::string line;
    bool version_ok = false; // Set true only after a valid version header
    bool header_seen = false; // Set after encountering the first data line

    while (std::getline(infile, line)) {
	if (line.empty()) continue; // Skip blank lines

	// Parse and validate version header
	if (line.rfind(kHdr, 0) == 0) {
	    std::string vstr = line.substr(kHdr.size());
	    // Trim trailing whitespace
	    wtrim(vstr);
	    int version = 0;
	    try {
		version = std::stoi(vstr);
	    } catch (...) {
		std::cerr << "Warning: invalid cache version header '" << vstr << "'. Skipping cache.\n";
		return {};
	    }
	    if (version == GDIFF_CACHE_FORMAT) {
		version_ok = true;
	    } else {
		std::cerr << "Warning: cache file format version " << version
		    << " does not match expected version " << GDIFF_CACHE_FORMAT << ". Skipping cache.\n";
		return {};
	    }
	    continue;
	}

	// Skip comment lines other than the version line above
	if (line[0] == '#') continue;

	header_seen = true;

	// Enforce presence of a correct version header before any data lines
	if (!version_ok) {
	    std::cerr << "Warning: cache file missing or has wrong version header. Skipping cache.\n";
	    return std::unordered_map<HashCacheKey, std::string>();
	}

	// Parse CSV fields: path, mtime, size, use_names, use_geometry, geom_fast, hash
	std::istringstream iss(line);
	std::string path, mtime_str, size_str, use_names_str, use_geometry_str, geom_fast_str, hash;
	if (std::getline(iss, path, ',') &&
		std::getline(iss, mtime_str, ',') &&
		std::getline(iss, size_str, ',') &&
		std::getline(iss, use_names_str, ',') &&
		std::getline(iss, use_geometry_str, ',') &&
		std::getline(iss, geom_fast_str, ',') &&
		std::getline(iss, hash)) {

	    // Trim whitespace around each field
	    wtrim(path);
	    wtrim(mtime_str);
	    wtrim(size_str);
	    wtrim(use_names_str);
	    wtrim(use_geometry_str);
	    wtrim(geom_fast_str);
	    wtrim(hash);

	    // Require all critical fields to be present after trimming
	    if (path.empty() || hash.empty() || mtime_str.empty() || size_str.empty()
		    || use_names_str.empty() || use_geometry_str.empty() || geom_fast_str.empty()) {
		continue; // Skip malformed line
	    }

	    // Convert to typed values
	    std::time_t mtime = 0;
	    uintmax_t fsize = 0;
	    int use_names = 0;
	    int use_geometry = 0;
	    int geom_fast = 0;
	    try {
		mtime = static_cast<std::time_t>(std::stoll(mtime_str));
		fsize = static_cast<uintmax_t>(std::stoull(size_str));
		use_names = std::stoi(use_names_str);
		use_geometry = std::stoi(use_geometry_str);
		geom_fast = std::stoi(geom_fast_str);
	    } catch (const std::exception&) {
		// Any parse failure indicates a malformed record; skip it.
		continue;
	    } catch (...) {
		continue;
	    }

	    // Accept record only if the file still exists and matches stored metadata
	    std::error_code ec;
	    if (!std::filesystem::exists(path, ec)) continue;

	    // stat_file_for_cache(path) is expected to return {mtime, size}
	    auto stat = stat_file_for_cache(path);
	    if (stat.first != mtime || stat.second != fsize) continue;

	    // Insert into in-memory cache keyed by file + config flags
	    HashCacheKey key { path, mtime, fsize, use_names, use_geometry, geom_fast };
	    cache[key] = hash;
	}
	// Malformed lines fall through and are ignored
    }

    // If we saw data but never a valid version header, reject the cache
    if (!version_ok && header_seen) {
	std::cerr << "Warning: cache file missing version header. Skipping cache.\n";
	return std::unordered_map<HashCacheKey, std::string>();
    }

    return cache;
}

// Write the entire cache to disk in the versioned CSV format described above.
// Existing file contents are truncated.
static void write_full_hash_cache(
	const std::string &filename,
	const std::unordered_map<HashCacheKey, std::string> &cache)
{
    std::ofstream outfile(filename, std::ios::trunc);
    if (!outfile) return; // Silently do nothing on open failure

    // Required version header; readers will reject if this does not match
    outfile << "# gdiff-cache-version=" << GDIFF_CACHE_FORMAT << "\n";

    // Informational column description line (comment)
    outfile << "# path,mtime,size,use_names,use_geometry,geom_fast,hash\n";

    // Emit one CSV record per cache entry. No escaping is performed.
    for (const auto &kv : cache) {
	outfile << kv.first.path << ","
	    << kv.first.mtime << ","
	    << kv.first.size << ","
	    << kv.first.use_names << ","
	    << kv.first.use_geometry << ","
	    << kv.first.geom_fast << ","
	    << kv.second << "\n";
    }
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
// TLSH content hash, parallel hashing, clustering, reporting
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
		return bu_strcmp(a->d_namep, b->d_namep) < 0;
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

	    size_t max_bufsize = 0;
	    for (const directory* dp_geom : directories) {
		if (dp_geom->d_len > max_bufsize)
		    max_bufsize = dp_geom->d_len;
	    }
	    ext.ext_buf = (uint8_t *)bu_realloc(ext.ext_buf, max_bufsize, "resize ext_buf");

	    for (const directory* dp_geom : directories) {
		if (dp_geom->d_len == 0)
		    continue;
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
    result.reserve(dbs.size());
    std::mutex res_mutex; // guards result map and logging
    size_t total = dbs.size();
    std::vector<std::thread> threads;
    std::atomic<size_t> idx{0};
    auto worker = [&]() {
	while (true) {
	    size_t i = idx.fetch_add(1);
	    if (i >= total) break;
	    auto t0 = std::chrono::steady_clock::now();
	    std::string hash = content_hash(
		    dbs[i].dbip,
		    gopts.use_names,
		    gopts.use_geometry,
		    gopts.geom_fast
		    );
	    auto t1 = std::chrono::steady_clock::now();
	    if (gopts.verbosity > 0) {
		double secs = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
		std::lock_guard<std::mutex> lock(res_mutex);
		std::cout << "[hash] " << dbs[i].path << " " << std::fixed << std::setprecision(3)
		    << secs << "s" << std::endl;
	    }
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
// Clustering, binning, reporting
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

    // Pre-decode TLSH strings once and reuse to avoid repeated parsing
    std::vector<tlsh::Tlsh> decoded(N);
    for (size_t i = 0; i < N; ++i) {
	decoded[i].fromTlshStr(files[i].hash.c_str());
    }
    for (size_t i = 0; i < N; ++i) {
	for (size_t j = i + 1; j < N; ++j) {
	    int diff = decoded[i].diff(decoded[j]);
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
	int N = static_cast<int>(cluster.size());
	auto newest_it = std::max_element(cluster.begin(), cluster.end(),
		[](const HashInfo& a, const HashInfo& b) { return a.mtime < b.mtime; });

	const auto& newest = *newest_it;
	int newest_idx = static_cast<int>(std::distance(cluster.begin(), newest_it));

	// Pre-decode TLSH strings for the cluster and reuse
	std::vector<tlsh::Tlsh> decoded(static_cast<size_t>(N));
	for (int i = 0; i < N; ++i) {
	    decoded[static_cast<size_t>(i)].fromTlshStr(cluster[static_cast<size_t>(i)].hash.c_str());
	}

	std::vector<int> total_diffs(N, 0);
	for (int i = 0; i < N; ++i) {
	    for (int j = 0; j < N; ++j) {
		if (i == j) continue;
		total_diffs[i] += decoded[static_cast<size_t>(i)].diff(decoded[static_cast<size_t>(j)]);
	    }
	}
	int center_idx = std::min_element(total_diffs.begin(), total_diffs.end()) - total_diffs.begin();

	std::vector<std::tuple<std::string, int, int, std::filesystem::file_time_type>> entries;
	for (int k = 0; k < N; ++k) {
	    const auto& f = cluster[static_cast<size_t>(k)];
	    int diff_newest = decoded[static_cast<size_t>(newest_idx)].diff(decoded[static_cast<size_t>(k)]);
	    int diff_center = decoded[static_cast<size_t>(center_idx)].diff(decoded[static_cast<size_t>(k)]);
	    entries.emplace_back(f.path, diff_newest, diff_center, f.mtime);
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
	int threshold,
	const std::unordered_map<std::string, std::filesystem::file_time_type>* path2mtime = nullptr)
{
    std::vector<HashInfo> infos;
    for (const auto& path : files) {
	if (!path2hash.count(path) || path2hash.at(path).empty()) continue;
	std::filesystem::file_time_type mft = std::filesystem::file_time_type::min();
	if (path2mtime && path2mtime->count(path)) {
	    mft = path2mtime->at(path);
	} else {
	    mft = file_mtime(path);
	}
	infos.push_back({path, mft, path2hash.at(path)});
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
	bool indent_group = false,
	bool show_relative = false
	)
{
    // Compute a global column width for top-level (newest) lines so they align
    // across all groups printed by this call. Sub-entry alignment remains per-group.
    size_t top_longest_path = 0;
    for (const auto& [newest, entries] : groups) {
        auto disp = get_display_path(newest, show_relative);
        if (disp.size() > top_longest_path) top_longest_path = disp.size();
    }
    const size_t top_score_col = top_longest_path + 2;

    for (const auto& [newest, entries] : groups) {
	auto disp_newest = get_display_path(newest, show_relative);
	// Per-group width used for entry lines in this group
	size_t group_longest_path = disp_newest.size();
	for (const auto& entry_tuple : entries) {
	    group_longest_path = std::max(group_longest_path,
		    get_display_path(std::get<0>(entry_tuple), show_relative).size());
	}
	size_t entry_score_col = group_longest_path + 2;

	// Compute center-distance for newest (for its top-level line badge)
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

	// entries are sorted by mtime descending (cluster_bins), so front is newest.
	std::filesystem::file_time_type newest_mtime = entries.empty() ? std::filesystem::file_time_type::min()
	    : std::get<3>(entries.front());

	out << group_indent << std::left << std::setw(top_score_col) << disp_newest
	    << parent_score_ss.str() << " " << format_ymd(newest_mtime) << "\n";

	for (const auto& entry_tuple : entries) {
	    const std::string& entry = std::get<0>(entry_tuple);
	    auto disp_entry = get_display_path(entry, show_relative);
	    int dn = std::get<1>(entry_tuple);
	    int dc = std::get<2>(entry_tuple);
	    if (entry == newest) continue;
	    std::ostringstream score_ss;
	    score_ss << "[Δ" << std::right << std::setw(4) << dn
		<< ", Δx̄" << std::right << std::setw(4) << dc << "]";
	    out << group_indent << "  " << std::left << std::setw(entry_score_col-2) << disp_entry
		<< score_ss.str() << " " << format_ymd(std::get<3>(entry_tuple)) << "\n";
	}
    }
}

/*
 * print_grouping_overview - One-time header explaining the grouping output.
 *
 * Printed before any per-bin or aggregated grouping reports.
 *
 * Explanation of columns:
 *   Seed/Newest line: the first line for a group (or each filename bin)
 *     [Δx̄####] -> TLSH distance from the newest file to the group's center (medoid).
 *
 *   Member lines:
 *     [Δ####, Δx̄####]
 *       Δ####   : TLSH distance from this file to the newest file in its group.
 *       Δx̄#### : TLSH distance from this file to the group's center (medoid).
 *
 * Lower distances mean higher similarity. Distances are computed on the chosen
 * hash configuration (object names, geometry, or both). The reported dates are
 * the filesystem last-modified timestamps (YYYY-MM-DD).
 *
 * Notes:
 *   - Groups are formed by transitive similarity: files may be related via chains.
 *   - The "center" is the file minimizing total TLSH distance to all others in its group.
 *   - Filename binning (if enabled) limits similarity clustering to files whose
 *     base names fall within the specified edit threshold.
 */
static void print_grouping_overview(const gdiff_group_opts &gopts,
                                    std::ostream &out,
                                    bool filename_binning_active)
{
    out << "====================== gdiff -G Grouping Summary ======================\n";
    out << "Hash inputs: " << (gopts.use_names ? "names" : "") << (gopts.use_names && gopts.use_geometry ? "+" : "")
        << (gopts.use_geometry ? "geometry" : "") << ( (!gopts.use_names && !gopts.use_geometry) ? "none" : "" ) << "\n";
    if (filename_binning_active) {
        out << "Filename binning active: edit distance <= " << gopts.filename_threshold << " groups top-level bins.\n";
    } else {
        out << "Filename binning inactive: all files clustered together by content similarity.\n";
    }
    out << "Distance thresholds (lower value = greater similarity): TLSH < " << gopts.threshold
        << (gopts.geom_fast ? " (fast geometry mode)" : "") << "\n";
    out << "Columns:\n";
    out << "  Seed/Newest line: [Δx̄####] distance from newest file to group center.\n";
    out << "  Member lines: [Δ####, Δx̄####] distances to newest and to group center.\n";
    out << "=======================================================================\n\n";
}

/*
 * print_filename_bin_header - Prints a report header for a filename bin.
 */
void print_filename_bin_header(const std::string& seed_path, std::filesystem::file_time_type seed_mtime, int threshold, std::ostream& out, bool show_relative = false)
{
    auto disp_seed = get_display_path(seed_path, show_relative);
    out << "====================================================================\n";
    out << "Filename Bin Report\n";
    out << "Seed (center file):   " << disp_seed << "\n";
    out << "Date of Seed:         " << format_ymd(seed_mtime) << "\n";
    out << "Max Damerau-Levenshtein edit distance of binned paths from seed:  " << threshold << "\n";
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
	std::ostream& out,
	bool show_relative = false,
	const std::unordered_map<std::string, std::filesystem::file_time_type> *path2mtime = nullptr)
{
    bool filename_active = (file_grps.size() > 1);

    if (filename_active) {
	for (const auto& fg : file_grps) {
	    std::vector<std::string> sorted = sort_files_by_mtime(fg.second);
	    auto hash_bins = hash_bins_for_files(sorted, path2hash, threshold, path2mtime);
	    if (!sorted.empty()) {
		auto disp = get_display_path(sorted.front(), show_relative);
		std::filesystem::file_time_type mtime = std::filesystem::file_time_type::min();
		if (path2mtime && path2mtime->count(sorted.front()))
		    mtime = path2mtime->at(sorted.front());
		out << disp << " (" << format_ymd(mtime) << "):\n";
		print_cluster_report(hash_bins, out, true, show_relative);
	    }
	}
    } else {
	std::vector<std::string> all_files;
	for (const auto& fg : file_grps) {
	    all_files.insert(all_files.end(), fg.second.begin(), fg.second.end());
	}
	auto hash_bins = hash_bins_for_files(all_files, path2hash, threshold, path2mtime);
	print_cluster_report(hash_bins, out, false, show_relative);
    }
}

// ============================================================================
// MAIN ENTRYPOINT: gdiff_group (cache, version)
// ============================================================================

int gdiff_group(int argc, const char **argv, struct gdiff_group_opts *g_opts)
{
    struct gdiff_group_opts gopts = GDIFF_GROUP_OPTS_DEFAULT;
    if (g_opts) {
	gopts.filename_threshold = g_opts->filename_threshold;
	gopts.threshold = g_opts->threshold;
	gopts.use_names = g_opts->use_names;
	gopts.use_geometry = g_opts->use_geometry;
	gopts.geom_fast = g_opts->geom_fast;
	gopts.thread_cnt = g_opts->thread_cnt;
	bu_vls_sprintf(&gopts.fpattern, "%s", bu_vls_cstr(&g_opts->fpattern));
	bu_vls_sprintf(&gopts.ofile, "%s", bu_vls_cstr(&g_opts->ofile));
	bu_vls_sprintf(&gopts.hash_infile, "%s", bu_vls_cstr(&g_opts->hash_infile));
	bu_vls_sprintf(&gopts.hash_outfile, "%s", bu_vls_cstr(&g_opts->hash_outfile));
    }

    if (!gopts.use_names && !gopts.use_geometry) {
	gopts.use_names = 1;
	gopts.use_geometry = 1;
    }
    if (gopts.filename_threshold == -1 && gopts.threshold == -1)
	gopts.threshold = TLSH_DEFAULT_THRESHOLD;
    if (!bu_vls_strlen(&gopts.fpattern))
	bu_vls_sprintf(&gopts.fpattern, "*.g");

    // Path display mode logic (auto/relative/absolute)
    bool all_argv_relative = true;
    for (int i = 0; i < argc; ++i) {
	if (std::filesystem::path(argv[i]).is_absolute()) {
	    all_argv_relative = false;
	    break;
	}
    }
    bool show_relative = false;
    if (gopts.path_display_mode == GDIFF_PATH_DISPLAY_RELATIVE) show_relative = true;
    else if (gopts.path_display_mode == GDIFF_PATH_DISPLAY_ABSOLUTE) show_relative = false;
    else show_relative = all_argv_relative;

    // File discovery (canonicalize all paths for internal use)
    std::vector<FileInfo> file_infos;
    std::unordered_map<std::string, std::pair<std::time_t, uintmax_t>> file_stats;
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, MAXPATHLEN, BU_DIR_CURR, NULL);

    if (argc && argv) {
	for (int i = 0; i < argc; i++) {
	    std::string user_arg = argv[i];
	    if (bu_file_directory(argv[i]))  {
		std::string rstr(user_arg);
		for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
		    if (!entry.is_regular_file())
			continue;
		    if (bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
			continue;
		    std::string canon;
		    try {
			canon = std::filesystem::weakly_canonical(entry.path()).string();
		    } catch (...) {
			canon = entry.path().lexically_normal().string();
		    }
		    auto [mtime, fsize] = stat_file_for_cache(canon);
		    file_infos.push_back({canon, file_mtime(canon)});
		    file_stats[canon] = {mtime, fsize};
		}
		continue;
	    }
	    if (bu_file_exists(argv[i], NULL)) {
		std::string canon;
		try {
		    canon = std::filesystem::weakly_canonical(user_arg).string();
		} catch (...) {
		    canon = std::filesystem::path(user_arg).lexically_normal().string();
		}
		auto [mtime, fsize] = stat_file_for_cache(canon);
		file_infos.push_back({canon, file_mtime(canon)});
		file_stats[canon] = {mtime, fsize};
	    }
	}

	if (file_infos.empty()) {
	    return BRLCAD_OK;
	}

    }
    if (file_infos.empty()) {
	std::string rstr(cwd);
	for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
	    if (!entry.is_regular_file())
		continue;
	    if (bu_path_match(bu_vls_cstr(&gopts.fpattern), entry.path().string().c_str(), 0))
		continue;
	    std::string canon;
	    try {
		canon = std::filesystem::weakly_canonical(entry.path()).string();
	    } catch (...) {
		canon = entry.path().lexically_normal().string();
	    }
	    auto [mtime, fsize] = stat_file_for_cache(canon);
	    file_infos.push_back({canon, file_mtime(canon)});
	    file_stats[canon] = {mtime, fsize};
	}
    }

    std::sort(file_infos.begin(), file_infos.end(),
	    [](const FileInfo& a, const FileInfo& b) { return a.mtime > b.mtime; });

    std::vector<std::string> files;
    for (const auto& fi : file_infos) files.push_back(fi.path);

    std::unordered_map<std::string, std::filesystem::file_time_type> path2mtime;
    for (const auto &fi : file_infos)
	path2mtime[fi.path] = fi.mtime;

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

    // ==== CACHE SYSTEM SUPPORTING MULTIPLE CONFIGURATIONS + VERSION HEADER ====
    // 1. Read full cache (all configs, all valid files)
    std::unordered_map<HashCacheKey, std::string> full_hash_cache;
    if (bu_vls_strlen(&gopts.hash_infile)) {
	full_hash_cache = read_full_hash_cache(bu_vls_cstr(&gopts.hash_infile));
    }

    // 2. For this run, look up hashes for current config only
    std::unordered_map<std::string, std::string> path2hash;
    std::vector<std::string> files_to_hash;
    for (const auto &f : files) {
	auto stat = file_stats[f];
	HashCacheKey k { f, stat.first, stat.second, gopts.use_names, gopts.use_geometry, gopts.geom_fast };
	auto it = full_hash_cache.find(k);
	if (it != full_hash_cache.end() && !it->second.empty()) {
	    path2hash[f] = it->second;
	} else {
	    files_to_hash.push_back(f);
	}
    }

    // Verbose summary before hashing begins
    if (gopts.verbosity > 0) {
	const size_t total_files = files.size();
	const size_t to_hash = files_to_hash.size();
	const size_t from_cache = (total_files >= to_hash) ? (total_files - to_hash) : 0;
	std::cout << "[group] Discovered " << total_files << " .g file(s). "
	    << to_hash << " to hash, " << from_cache << " from cache."
	    << std::endl;
    }

    // 3. Hash computation (only for files that need hashing)
    std::unordered_map<std::string, std::string> newly_computed_hashes;
    size_t parallel_threads = gopts.thread_cnt;
    if (!parallel_threads)
	parallel_threads = bu_avail_cpus();

    if (!files_to_hash.empty()) {
	if (parallel_threads > 1) {
	    std::vector<DBFile> dbs;
	    for (const auto& path : files_to_hash) {
		struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
		if (dbip) {
		    if (db_dirbuild(dbip) == 0) {
			dbs.push_back({path, dbip});
		    } else {
			db_close(dbip); // close on failure
		    }
		}
	    }
	    newly_computed_hashes = hash_files_parallel(dbs, gopts, parallel_threads);
	    for (auto& db : dbs) {
		db_close(db.dbip);
		db.dbip = nullptr;
	    }
	} else {
	    for (const auto& path : files_to_hash) {
		struct db_i* dbip = db_open(path.c_str(), DB_OPEN_READONLY);
		if (dbip && db_dirbuild(dbip) == 0) {
		    auto t0 = std::chrono::steady_clock::now();
		    std::string hash = content_hash(dbip, gopts.use_names, gopts.use_geometry, gopts.geom_fast);
		    auto t1 = std::chrono::steady_clock::now();
		    newly_computed_hashes[path] = hash;
		    if (gopts.verbosity > 0) {
			double secs = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
			std::cout << "[hash] " << path << " " << std::fixed << std::setprecision(3)
			    << secs << "s" << std::endl;
		    }
		}
		if (dbip) db_close(dbip);
	    }
	}
    }

    // 4. Merge new hashes for current config into full cache, and update path2hash
    for (const auto &f : files) {
	auto stat = file_stats[f];
	HashCacheKey k { f, stat.first, stat.second, gopts.use_names, gopts.use_geometry, gopts.geom_fast };
	if (newly_computed_hashes.count(f)) {
	    full_hash_cache[k] = newly_computed_hashes[f];
	    path2hash[f] = newly_computed_hashes[f];
	}
    }

    // 5. Write full cache (all valid configs/files, in canonical form)
    if (bu_vls_strlen(&gopts.hash_outfile)) {
	write_full_hash_cache(bu_vls_cstr(&gopts.hash_outfile), full_hash_cache);
    }

    // Output file support
    std::ostream* out = &std::cout;
    std::ofstream fout;
    if (bu_vls_strlen(&gopts.ofile)) {
	fout.open(bu_vls_cstr(&gopts.ofile));
	if (fout) out = &fout;
    }

    // One-time explanatory header before detailed grouping output
    bool filename_binning_active = (filename_threshold >= 0);
    print_grouping_overview(gopts, *out, filename_binning_active);

    if (filename_threshold >= 0) {
	for (const auto& [key, bin_files] : file_grps) {
	    std::filesystem::file_time_type seed_mtime = path2mtime.count(key) ? path2mtime[key] : std::filesystem::file_time_type::min();
	    print_filename_bin_header(key, seed_mtime, filename_threshold, *out, show_relative);
	    std::unordered_map<std::string, std::vector<std::string>> single_bin;
	    single_bin[key] = bin_files;
	    group_and_report(
		    single_bin,
		    gopts.threshold,
		    path2hash,
		    *out,
		    show_relative,
		    &path2mtime
		    );
	    *out << "\n";
	}
    } else {
	group_and_report(
		file_grps,
		gopts.threshold,
		path2hash,
		*out,
		show_relative,
		&path2mtime
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
