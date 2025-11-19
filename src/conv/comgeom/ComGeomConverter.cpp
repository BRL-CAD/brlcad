#include "ComGeomConverter.h"

#include <cstdio>
#include <cstring>
#include "ComGeomReader.h"
#include "SolidParser.h"
#include "RegionManager.h"

ComGeomConverter::ComGeomConverter(const std::string &in,
                                   const std::string &out,
                                   const ConverterOptions &opts)
    : m_inPath(in), m_outPath(out), m_opts(opts)
{}

ComGeomConverter::~ComGeomConverter() {
    if (m_in) fclose(m_in);
    if (m_out) wdb_close(m_out);
}

bool ComGeomConverter::openInput() {
    if (!bu_file_exists(m_inPath.c_str(), NULL)) {
        std::perror(m_inPath.c_str());
        return false;
    }
    m_in = fopen(m_inPath.c_str(), "rb");
    if (!m_in) {
        std::perror(m_inPath.c_str());
        return false;
    }
    return true;
}

bool ComGeomConverter::openOutput() {
    if (bu_file_exists(m_outPath.c_str(), NULL))
        bu_log("Warning: appending to existing database\n");
    m_out = wdb_fopen(m_outPath.c_str());
    if (!m_out) {
        std::perror(m_outPath.c_str());
        return false;
    }
    return true;
}

bool ComGeomConverter::writeDbHeader(const std::string &title, const std::string &units) {
    if (mk_conversion(units.c_str()) < 0) {
        std::printf("WARNING: unknown units '%s', using inches\n", units.c_str());
        (void)mk_conversion("in");
    }
    if (mk_id_units(m_out, title.c_str(), units.c_str()) < 0) {
        std::printf("Unable to write database ID, units='%s'\n", units.c_str());
        return false;
    }
    return true;
}

int ComGeomConverter::run() {
    if (m_opts.version != 0 && m_opts.version != 1 && m_opts.version != 4 && m_opts.version != 5) {
        std::fprintf(stderr,"version %d not supported\n", m_opts.version);
        return 1;
    }
    if (!openInput() || !openOutput()) return 1;

    std::printf("Reading version %d COMGEOM file\n", m_opts.version);

    /* Title & units */
    std::string title, units;
    {
        ComGeomReader r(m_in);
        auto l = r.next("title card");
        if (!l) { std::printf("Empty input file: no title record\n"); return 10; }
        std::string_view sv(l->s);
        if (m_opts.version == 0) {
            title = ComGeomReader::rstrip(sv);
            ComGeomReader::trimLeadingSpacesInplace(title);
            units = "in";
        } else if (m_opts.version == 1) {
            title = ComGeomReader::rstrip(sv);
            ComGeomReader::trimLeadingSpacesInplace(title);
            units = "in";
        } else {
            units.assign(sv.substr(0,2));
            title.assign(sv.substr(3));
            ComGeomReader::trimLeadingSpacesInplace(title);
            title = ComGeomReader::rstrip(title);
            units = ComGeomReader::rstrip(units);
            ComGeomReader::toLowerInplace(units);
        }
        std::printf("Title: %s\nUnits: %s\n", title.c_str(), units.c_str());
    }
    if (!writeDbHeader(title, units)) return 1;

    int solTotal=0, regTotal=0;
    {
        ComGeomReader r(m_in);
        if (m_opts.version == 0) {
            // Attempt single read first
            auto l = r.next("v0 constraints candidate");
            if (l) {
                std::string_view sv(l->s);
                solTotal = ComGeomReader::fieldInt(sv, 30, 10);
                regTotal = ComGeomReader::fieldInt(sv, 40, 10);
            }
            if (solTotal <= 0 || regTotal <= 0) {
                // Scan up to 50 lines to find plausible NBODY/NRMAX pair
                solTotal = regTotal = 0;
                for (int scan=0; scan<50; ++scan) {
                    auto cand = r.next("scan constraints");
                    if (!cand) break;
                    std::string_view sv2(cand->s);
                    int nb = ComGeomReader::fieldInt(sv2, 30, 10);
                    int nr = ComGeomReader::fieldInt(sv2, 40, 10);
                    if (nb > 0 && nr > 0) {
                        solTotal = nb;
                        regTotal = nr;
                        break;
                    }
                }
                if (solTotal == 0 || regTotal == 0) {
                    std::printf("Warning: v0 NBODY/NRMAX not found, dynamic sizing will be used\n");
                    solTotal = 0; // dynamic
                    regTotal = 0;
                }
            }
        } else if (m_opts.version == 1) {
            solTotal = regTotal = 9999;
        } else {
            auto l = r.next("control card");
            if (!l) { std::printf("No control card .... STOP\n"); return 10; }
            std::string_view sv(l->s);
            if (m_opts.version == 4) {
                solTotal = ComGeomReader::fieldInt(sv, 0, 20);
                regTotal = ComGeomReader::fieldInt(sv, 20, 10);
            } else {
                solTotal = ComGeomReader::fieldInt(sv, 0, 5);
                regTotal = ComGeomReader::fieldInt(sv, 5, 5);
            }
        }
        if (m_opts.verbose)
            std::printf("Expecting %d solids, %d regions\n", solTotal, regTotal);
    }

    /* Solids */
    {
        if (m_opts.verbose) std::puts("Primitive table");
        ComGeomReader r(m_in);
        SolidParserOptions sopts{m_opts.version,m_opts.verbose,m_opts.suffix};
        sopts.legacy_trunc = m_opts.legacy_truncate_names;
        SolidParser sp(r, m_out, sopts);
        sp.setSolidsExpected(solTotal);
        while (true) {
            auto res = sp.parseNext();
            if (res.status == SolidParseResult::Eof) {
                if (m_opts.verbose)
                    std::printf("\nprocessed %d of %d solids\n\n", sp.solidsProcessed(), solTotal);
                if (solTotal > 0 && sp.solidsProcessed() < solTotal && m_opts.version > 1) {
                    std::printf("some solids are missing, aborting\n");
                    return 1;
                }
                break;
            }
            if (res.status == SolidParseResult::Error) {
                std::fprintf(stderr,"error converting solid %d: %s\n",
                             sp.solidsProcessed(), res.message.c_str());
            }
        }
    }

    /* Regions */
    {
        if (m_opts.verbose) std::puts("\nRegion table");
        ComGeomReader r(m_in);
        RegionManagerOptions ropts;
        ropts.version = m_opts.version;
        ropts.verbose = m_opts.verbose;
        ropts.suffix = m_opts.suffix;
        ropts.regTotal = regTotal;
        ropts.legacy_trunc = m_opts.legacy_truncate_names;
        RegionManager rm(r, m_out, ropts);
        if (!rm.parseRegions()) return 10;
        if (m_opts.verbose) std::puts("\nGroups");
        if (!rm.writeGroups()) return 10;
        if (m_opts.verbose) std::puts("");
    }

    return 0;
}