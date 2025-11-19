#include "RegionManager.h"
#include <cstdio>
#include <cstring>
#include <utility>

RegionManager::RegionManager(ComGeomReader &reader, rt_wdb *outfp, const RegionManagerOptions &opts)
    : m_r(reader), m_out(outfp), m_opts(opts)
{
    // For dynamic v0 decks (regTotal == 0) preallocate provisional capacity
    size_t allocCount = static_cast<size_t>(m_opts.regTotal + 1);
    if (m_opts.version == 0 && m_opts.regTotal == 0) {
        allocCount = 4097; // provisional region cap
    }
    m_regionHeads.resize(allocCount);
    for (auto &wm : m_regionHeads) {
        WMEMBER_INIT(&wm);
    }
    groupInit();
}

RegionManager::~RegionManager() {
    for (auto &wm : m_regionHeads) {
        if (wm.wm_name) {
            bu_free(wm.wm_name, "region wm_name");
            wm.wm_name = nullptr;
        }
    }
    for (int i = 0; i < m_groupCount; ++i) {
        if (m_groups[i].head.wm_name) {
            bu_free(m_groups[i].head.wm_name, "group wm_name");
            m_groups[i].head.wm_name = nullptr;
        }
    }
}

std::string RegionManager::makeRegionName(int n) const {
    std::string suf = m_opts.suffix;
    if (m_opts.legacy_trunc && suf.size() > 13) suf.resize(13);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "r%d%s", n, suf.c_str());
    return std::string(buf);
}

void RegionManager::colPrint(const std::string &s) {
    if (!m_opts.verbose) return;
    std::fputs(s.c_str(), stdout);
    m_colCur += s.size();
    while (m_colCur < 78 && (m_colCur % 10)) {
        putchar(' ');
        ++m_colCur;
    }
    if (m_colCur >= 78) {
        std::fputc('\n', stdout);
        m_colCur = 0;
    }
}

/* ---------------- v0 (MAGIC) region table with OR semantics ---------------- */
bool RegionManager::parseRegionTableV0() {
    auto first = m_r.next("v0 region card");
    if (!first) { std::fprintf(stderr,"v0 region: EOF\n"); return false; }
    std::string current = first->s;

    int lastRegNum = 0;
    while (true) {
        int reg_num = ComGeomReader::fieldInt(current, 0, 5);
        if (reg_num < 0) break;
        if (reg_num == 0) {
            auto nl = m_r.next("v0 region card");
            if (!nl) break;
            current = nl->s;
            continue;
        }
        // resize if needed (dynamic)
        if (reg_num >= static_cast<int>(m_regionHeads.size())) {
            std::fprintf(stderr,"v0 region number %d exceeds provisional allocation\n", reg_num);
            return false;
        }
        lastRegNum = reg_num;
        wmember &head = m_regionHeads[static_cast<size_t>(reg_num)];
        if (!head.wm_name) head.wm_name = bu_strdup(makeRegionName(reg_num).c_str());

        struct Term { int body; bool subtract; };
        std::vector<std::vector<Term>> clauses;
        clauses.emplace_back();

        auto parseLine = [&](const std::string &line) {
            for (int i=0;i<9;++i) {
                size_t slot = 6 + i*7;
                if (slot + 7 > line.size()) break;
                std::string field = line.substr(slot, 7);
                std::string op = field.substr(0,2);
                std::string numstr = field.substr(2);
                ComGeomReader::trimLeadingSpacesInplace(op);
                numstr = ComGeomReader::rstrip(numstr);
                ComGeomReader::trimLeadingSpacesInplace(numstr);
                if (numstr.empty()) continue;
                int bnum = std::atoi(numstr.c_str());
                if (bnum == 0) continue;
                if (op == "OR" || op == "or") {
                    if (!clauses.back().empty()) clauses.emplace_back();
                    continue;
                }
                bool subtract = (bnum < 0);
                if (subtract) bnum = -bnum;
                clauses.back().push_back({bnum, subtract});
            }
        };
        parseLine(current);

        while (true) {
            auto nextLine = m_r.next("v0 region card");
            if (!nextLine) break;
            int newRegTest = ComGeomReader::fieldInt(nextLine->s, 0, 5);
            std::string trimmed = ComGeomReader::rstrip(nextLine->s);
            std::string t2 = trimmed; ComGeomReader::trimLeadingSpacesInplace(t2);
            if (t2 == "end") {
                if (m_opts.regTotal == 0) m_opts.regTotal = reg_num;
                break;
            }
            if (newRegTest < 0) { // negative sentinel
                if (m_opts.regTotal == 0) m_opts.regTotal = reg_num;
                current = nextLine->s;
                break;
            }
            if (newRegTest > 0) { // new region starts
                current = nextLine->s;
                break;
            }
            parseLine(nextLine->s);
        }

        int clauseIdx = 0;
        for (auto &cl : clauses) {
            if (cl.empty()) continue;
            wmember clauseHead; WMEMBER_INIT(&clauseHead);
            char cname[64]; std::snprintf(cname, sizeof(cname), "r%d_c%d", reg_num, ++clauseIdx);
            clauseHead.wm_name = bu_strdup(cname);
            bool firstTerm = true;
            for (auto &t : cl) {
                std::string memberName = std::string("s") + std::to_string(t.body) + m_opts.suffix;
                int op;
                if (firstTerm) {
                    op = WMOP_UNION; // baseline
                    firstTerm = false;
                } else {
                    op = t.subtract ? WMOP_SUBTRACT : WMOP_INTERSECT;
                }
                if (!mk_addmember(memberName.c_str(), &clauseHead.l, nullptr, op)) {
                    std::fprintf(stderr,"v0 mk_addmember failed clause %s member %s\n", cname, memberName.c_str());
                }
            }
            mk_comb(m_out, clauseHead.wm_name, &clauseHead.l, 0,
                    "", "", nullptr, 0,0,0,0,0,0,0);
            if (!mk_addmember(clauseHead.wm_name, &head.l, nullptr, WMOP_UNION)) {
                std::fprintf(stderr,"v0 mk_addmember region %d clause %s\n", reg_num, clauseHead.wm_name);
            }
        }
        if (m_opts.verbose) colPrint(head.wm_name);

        int sentinelTest = ComGeomReader::fieldInt(current, 0, 5);
        if (sentinelTest < 0) break;
    }
    if (m_opts.regTotal == 0) m_opts.regTotal = lastRegNum;
    return true;
}

bool RegionManager::parseRegions() {
    if (m_opts.version == 0) {
        if (!parseRegionTableV0()) return false;
        for (int i=1;i<=m_opts.regTotal && i < static_cast<int>(m_regionHeads.size());++i) registerRegion(i,0,0,0,0);
        return true;
    }
    if (!parseRegionTable()) return false;
    if (m_opts.version == 1) {
        for (int i = 1; i <= m_opts.regTotal; ++i) {
            registerRegion(i, 0, 0, 0, 0);
        }
        return true;
    }
    return parseIdentTable();
}

bool RegionManager::parseRegionTable() {
    auto first = m_r.next("region card");
    if (!first) {
        std::fprintf(stderr, "getregion: EOF\n");
        return false;
    }
    if (ComGeomReader::fieldInt(first->s, 0, 5) != 1) {
        std::fprintf(stderr, "First region card not #1\n");
        return false;
    }
    std::string current = first->s;

    while (true) {
        int reg_num = ComGeomReader::fieldInt(current, 0, 5);
        if (reg_num < 0) {
            return true;
        }
        if (reg_num > m_opts.regTotal) {
            std::fprintf(stderr, "%d regions > claimed %d\n", reg_num, m_opts.regTotal);
            return false;
        }
        wmember &head = m_regionHeads[static_cast<size_t>(reg_num)];
        if (!head.wm_name) {
            head.wm_name = bu_strdup(makeRegionName(reg_num).c_str());
        }

        while (true) {
            size_t base = (m_opts.version == 1) ? 10 : 6;
            for (int i = 0; i < 9; ++i) {
                size_t off = base + i * 7;
                if (off + 7 > current.size()) break;

                char token[8] = {0};
                int k = 0;
                for (int j = 2; j < 7; ++j) {
                    if (off + j >= current.size()) break;
                    char c = current[off + j];
                    if (!std::isspace(static_cast<unsigned char>(c))) {
                        token[k++] = c;
                    }
                }
                if (k == 0) continue;
                int inst_num = std::atoi(token);
                if (inst_num == 0) continue;

                int op = WMOP_UNION;
                bool region_ref = false;

                if (m_opts.version == 5) {
                    char flag = (off + 1 < current.size()) ? current[off + 1] : ' ';
                    if (flag == 'g' || flag == 'G') {
                        region_ref = true;
                    } else if (flag == 'R' || flag == 'r') {
                        op = WMOP_UNION;
                    } else {
                        if (inst_num < 0) {
                            op = WMOP_SUBTRACT;
                            inst_num = -inst_num;
                        } else {
                            op = WMOP_INTERSECT;
                        }
                    }
                } else {
                    char flag = (off + 1 < current.size()) ? current[off + 1] : ' ';
                    if (flag == ' ') {
                        if (inst_num < 0) {
                            op = WMOP_SUBTRACT;
                            inst_num = -inst_num;
                        } else {
                            op = WMOP_INTERSECT;
                        }
                    } else {
                        op = WMOP_UNION;
                    }
                }

                std::string suf = m_opts.suffix;
                if (m_opts.legacy_trunc && suf.size() > 13) suf.resize(13);

                std::string memberName = region_ref
                    ? makeRegionName(inst_num)
                    : (std::string("s") + std::to_string(inst_num) + suf);

                if (!mk_addmember(memberName.c_str(), &head.l, nullptr, op)) {
                    std::fprintf(stderr, "mk_addmember failed for region %d member '%s'\n",
                                 reg_num, memberName.c_str());
                }
            }

            auto nextLine = m_r.next("region card");
            if (!nextLine) {
                std::fprintf(stderr, "getregion: EOF\n");
                return false;
            }
            std::string trimmed = ComGeomReader::rstrip(nextLine->s);
            ComGeomReader::trimLeadingSpacesInplace(trimmed);
            if (trimmed == "end") {
                m_opts.regTotal = reg_num;
                return true;
            }
            int possible = ComGeomReader::fieldInt(nextLine->s, 0, 5);
            if (possible != 0) {
                current = nextLine->s;
                break;
            }
            current = nextLine->s;
        }

        if (m_opts.verbose) colPrint(head.wm_name);
    }
}

bool RegionManager::parseIdentTable() {
    if (m_opts.version == 4) {
        (void)m_r.next("blank card");
    }
    if (m_opts.verbose) std::puts("\nRegion ident table");

    while (true) {
        auto line = m_r.next("ident card");
        if (!line) return true;
        if (line->s.empty()) return true;

        int reg_num, id, air, mat, los;
        if (m_opts.version == 5) {
            reg_num = ComGeomReader::fieldInt(line->s, 0, 5);
            id      = ComGeomReader::fieldInt(line->s, 5, 5);
            air     = ComGeomReader::fieldInt(line->s, 10, 5);
            mat     = ComGeomReader::fieldInt(line->s, 15, 5);
            los     = ComGeomReader::fieldInt(line->s, 20, 5);
        } else {
            reg_num = ComGeomReader::fieldInt(line->s, 0, 10);
            id      = ComGeomReader::fieldInt(line->s, 10, 10);
            air     = ComGeomReader::fieldInt(line->s, 20, 10);
            mat     = ComGeomReader::fieldInt(line->s, 74, 3);
            los     = ComGeomReader::fieldInt(line->s, 77, 3);
        }

        if (reg_num <= 0) return true;
        registerRegion(reg_num, id, air, mat, los);
    }
}

void RegionManager::registerRegion(int regNum, int id, int air, int mat, int los) {
    if (regNum <= 0 || regNum >= static_cast<int>(m_regionHeads.size())) return;
    wmember &head = m_regionHeads[static_cast<size_t>(regNum)];

    if (BU_LIST_IS_EMPTY(&head.l)) {
        if (m_opts.verbose && head.wm_name) {
            colPrint(std::string("(") + head.wm_name + ")");
        }
        return;
    }

    int ret = mk_comb(m_out, head.wm_name, &head.l, 1,
                      "", "", nullptr,
                      id, air, mat, los, 0, 0, 1);
    if (ret < 0) {
        std::fprintf(stderr, "mk_comb failed for region %d (%s)\n", regNum, head.wm_name);
    } else {
        groupAdd(id, head.wm_name);
        if (m_opts.verbose) colPrint(head.wm_name);
    }
}

void RegionManager::groupInit() {
    struct Def { const char *n; int lo; int hi; } defs[] = {
        {"g00",0,0},{"g0",1,99},{"g1",100,199},{"g2",200,299},{"g3",300,399},
        {"g4",400,499},{"g5",500,599},{"g6",600,699},{"g7",700,799},{"g8",800,899},
        {"g9",900,999},{"g10",1000,1999},{"g11",2000,2999},{"g12",3000,3999},{"g13",4000,4999},
        {"g14",5000,5999},{"g15",6000,6999},{"g16",7000,7999},{"g17",8000,8999},{"g18",9000,9999},
        {"g19",10000,32767}
    };
    for (const auto &d : defs) {
        if (m_groupCount >= kMaxGroups) {
            std::fprintf(stderr, "Too many groups (limit %d)\n", kMaxGroups);
            break;
        }
        groupRegister(d.n, d.lo, d.hi);
    }
}

void RegionManager::groupRegister(const char *name, int lo, int hi) {
    Group &g = m_groups[m_groupCount];
    WMEMBER_INIT(&g.head);
    std::string suf = m_opts.suffix;
    if (m_opts.legacy_trunc && suf.size() > 13) suf.resize(13);
    std::string full = std::string(name) + suf;
    g.head.wm_name = bu_strdup(full.c_str());
    g.lo = lo;
    g.hi = hi;
    g.used = false;
    ++m_groupCount;
}

void RegionManager::groupAdd(int identVal, const char *regionName) {
    for (int i = m_groupCount - 1; i >= 0; --i) {
        if (identVal < m_groups[i].lo || identVal > m_groups[i].hi) continue;
        if (!mk_addmember(regionName, &m_groups[i].head.l, nullptr, WMOP_UNION)) {
            std::fprintf(stderr, "mk_addmember failed adding region '%s' to group '%s'\n",
                         regionName, m_groups[i].head.wm_name);
        } else {
            m_groups[i].used = true;
        }
        return;
    }
    std::fprintf(stderr, "Unable to find group for ident %d (region %s)\n", identVal, regionName);
}

bool RegionManager::writeGroups() {
    wmember allHead;
    WMEMBER_INIT(&allHead);
    allHead.wm_name = const_cast<char *>("all.g");

    for (int i = 0; i < m_groupCount; ++i) {
        Group &g = m_groups[i];
        if (BU_LIST_IS_EMPTY(&g.head.l)) continue;

        int ret = mk_comb(m_out, g.head.wm_name, &g.head.l, 0,
                          nullptr, nullptr, nullptr,
                          0, 0, 0, 0, 0, 0, 0);
        if (ret < 0) {
            std::fprintf(stderr, "mk_comb (group) failed for '%s'\n", g.head.wm_name);
            continue;
        }

        if (!mk_addmember(g.head.wm_name, &allHead.l, nullptr, WMOP_UNION)) {
            std::fprintf(stderr, "mk_addmember failed adding '%s' to all.g\n", g.head.wm_name);
        }
        if (m_opts.verbose) colPrint(g.head.wm_name);
    }

    int retAll = mk_comb(m_out, "all.g", &allHead.l, 0,
                         nullptr, nullptr, nullptr,
                         0, 0, 0, 0, 0, 0, 0);
    if (retAll < 0) {
        std::fprintf(stderr, "mk_comb failed for all.g\n");
        return false;
    }
    return true;
}