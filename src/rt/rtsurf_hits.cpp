/*                 R T S U R F _ H I T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
/**
 *
 * Guts for keeping track of rtsurf hits using C++ containers.
 *
 */

#include "./rtsurf_hits.h"

#include <algorithm>
#include <map>
#include <string>
#include <sstream>
#include <mutex>
#include <functional>
#include <vector>


class HitCounterContext
{
public:
    std::map<std::string, size_t> regionHitCounters;
    std::map<int, size_t> materialHitCounters;
    size_t lineCount;
    std::mutex hitCounterMutex;
};


void*
rtsurf_context_create()
{
    return new HitCounterContext();
}


void
rtsurf_context_destroy(void* context)
{
    delete static_cast<HitCounterContext*>(context);
}


void
rtsurf_context_reset(void* context)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    ctx->regionHitCounters.clear();
    ctx->materialHitCounters.clear();
    ctx->lineCount = 0;
}


void
rtsurf_register_hit(void* context, const char *region, int materialId)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    ctx->regionHitCounters[region]++;
    ctx->materialHitCounters[materialId]++;
}


void
rtsurf_register_line(void* context) {
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    ctx->lineCount++;
}


void
rtsurf_get_hits(void* context, const char *region, int materialId, size_t *regionHits, size_t *materialHits, size_t *lines)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    if (regionHits != nullptr) {
        auto regionIt = ctx->regionHitCounters.find(region);
        *regionHits = (regionIt != ctx->regionHitCounters.end()) ? regionIt->second : 0;
    }
    if (materialHits != nullptr) {
        auto materialIt = ctx->materialHitCounters.find(materialId);
        *materialHits = (materialIt != ctx->materialHitCounters.end()) ? materialIt->second : 0;
    }
    if (lines != nullptr) {
	*lines = ctx->lineCount;
    }
}


void
rtsurf_iterate_regions(void* context, void (*callback)(const char *region, size_t hits, size_t lines, void* data), void* data)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    for (const auto& pair : ctx->regionHitCounters) {
        callback(pair.first.c_str(), pair.second, ctx->lineCount, data);
    }
}


void
rtsurf_iterate_materials(void* context, void (*callback)(int materialId, size_t hits, size_t lines, void* data), void* data)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    for (const auto& pair : ctx->materialHitCounters) {
        callback(pair.first, pair.second, ctx->lineCount, data);
    }
}


static std::vector<std::string>
split_string(const std::string& str, char delimiter)
{
    std::vector<std::string> components;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            components.push_back(item);
        }
    }
    return components;
}


static int
count_depth(const std::string& path)
{
    return std::count(path.begin(), path.end(), '/');
}


static bool
depth_comparator(const std::pair<std::string, size_t>& a, const std::pair<std::string, size_t>& b)
{
    int depthA = count_depth(a.first);
    int depthB = count_depth(b.first);
    // alpha order for same depth
    if (depthA == depthB)
	return a.first < b.first;
    // deeper paths first
    return depthA > depthB;
}


void
rtsurf_iterate_groups(void* context, void (*callback)(const char *assembly, size_t hits, size_t lines, void* data), void* data)
{
    auto* ctx = static_cast<HitCounterContext*>(context);

    // not necessary, but why not
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);

    std::map<std::string, size_t> aggregatedCounts;
    for (const auto& entry : ctx->regionHitCounters) {
        std::vector<std::string> pathComponents = split_string(entry.first, '/');
        std::string currentPath;

	// skip the leaf nodes (i.e., regions)
	for (size_t i = 0; i < pathComponents.size() - 1; ++i) {
	    if (!currentPath.empty())
		currentPath += "/";
            currentPath += pathComponents[i];
            aggregatedCounts[currentPath] += entry.second;
        }
    }

    // convert our map to a sorted list
    std::vector<std::pair<std::string, size_t>> paths(aggregatedCounts.begin(), aggregatedCounts.end());

    // sort paths by depth
    std::sort(paths.begin(), paths.end(), depth_comparator);

    // iterate over our aggregation
    for (const auto& pair : paths) {
	callback(pair.first.c_str(), pair.second, ctx->lineCount, data);
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
