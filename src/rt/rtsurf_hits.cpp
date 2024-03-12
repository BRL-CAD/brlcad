/*                 R T S U R F _ H I T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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

#include <map>
#include <string>
#include <mutex>
#include <functional>


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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
