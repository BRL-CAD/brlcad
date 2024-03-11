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

#include <unordered_map>
#include <mutex>
#include <functional>


class HitCounterContext
{
public:
    std::unordered_map<int, size_t> regionHitCounters;
    std::unordered_map<int, size_t> materialHitCounters;
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
rtsurf_register_hit(void* context, int regionId, int materialId)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    ctx->regionHitCounters[regionId]++;
    ctx->materialHitCounters[materialId]++;
}


void
rtsurf_get_hits(void* context, int regionId, int materialId, size_t *regionHits, size_t *materialHits)
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    if (regionHits != nullptr) {
        auto regionIt = ctx->regionHitCounters.find(regionId);
        *regionHits = (regionIt != ctx->regionHitCounters.end()) ? regionIt->second : 0;
    }
    if (materialHits != nullptr) {
        auto materialIt = ctx->materialHitCounters.find(materialId);
        *materialHits = (materialIt != ctx->materialHitCounters.end()) ? materialIt->second : 0;
    }
}


void
rtsurf_iterate_regions(void* context, void (*callback)(int regionId, size_t hits))
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    for (const auto& pair : ctx->regionHitCounters) {
        callback(pair.first, pair.second);
    }
}


void
rtsurf_iterate_materials(void* context, void (*callback)(int materialId, size_t hits))
{
    auto* ctx = static_cast<HitCounterContext*>(context);
    std::lock_guard<std::mutex> lock(ctx->hitCounterMutex);
    for (const auto& pair : ctx->materialHitCounters) {
        callback(pair.first, pair.second);
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
