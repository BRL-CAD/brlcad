#pragma once

#include "common.h"
#include <array>
#include <string>
#include <vector>

extern "C" {
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
}

#include "ComGeomReader.h"

struct RegionManagerOptions {
    int version = 5;
    long verbose = 0;
    std::string suffix;
    int regTotal = 0;
};

/*
 * Modernized RegionManager:
 * - Uses WMEMBER_INIT for region and group heads (matches wdb.h)
 * - Avoids container reallocation for group heads (std::array)
 * - Relies on mk_addmember for node allocation and mk_comb/mk_lfcomb
 *   to free membership lists (per wdb.h contract)
 * - No manual traversal freeing of member nodes (use mk_freemembers only
 *   for temporary aggregation lists we explicitly build, e.g. "all.g" head)
 * - Stable addresses: region heads stored in std::vector sized once,
 *   group heads in fixed array
 */
class RegionManager {
public:
    RegionManager(ComGeomReader &reader, rt_wdb *outfp, const RegionManagerOptions &opts);
    ~RegionManager();

    bool parseRegions();      // read region membership cards
    bool writeGroups();       // emit group combinations + all.g

private:
    std::string makeRegionName(int n) const;
    void colPrint(const std::string &s);

    bool parseRegionTable();
    bool parseIdentTable();
    void registerRegion(int regNum, int id, int air, int mat, int los);

    void groupInit();
    void groupRegister(const char *name, int lo, int hi);
    void groupAdd(int identVal, const char *regionName);

private:
    ComGeomReader &m_r;
    rt_wdb *m_out = nullptr;
    RegionManagerOptions m_opts;

    std::vector<wmember> m_regionHeads;  // index by region number (1..regTotal)
    size_t m_colCur = 0;

    struct Group {
        wmember head;   // combination head (membership list)
        int lo = 0;
        int hi = 0;
        bool used = false; // whether at least one region fell into range
    };

    static constexpr int kMaxGroups = 21;
    std::array<Group, kMaxGroups> m_groups;
    int m_groupCount = 0;
};
