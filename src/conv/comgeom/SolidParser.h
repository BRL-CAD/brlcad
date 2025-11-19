#ifndef BRLCAD_SOLID_PARSER_H
#define BRLCAD_SOLID_PARSER_H

#include "common.h"

#include <string>
#include <vector>
#include <optional>

extern "C" {
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "bg.h"
}

#include "ComGeomReader.h"

struct SolidParseResult {
    enum Status { Ok, Eof, Error } status = Error;
    std::string message;
};

struct SolidParserOptions {
    int version = 5;
    int verbose = 0;
    std::string suffix;
    bool legacy_trunc = false;
};

class SolidParser {
public:
    SolidParser(ComGeomReader &reader, rt_wdb *outfp, const SolidParserOptions &opts);
    SolidParseResult parseNext();
    int solidsExpected() const { return m_solTotal; }
    int solidsProcessed() const { return m_solWork; }
    void setSolidsExpected(int c) { m_solTotal = c; }

private:
    std::string makeSolidName(int n) const;
    void colPrint(const std::string &s);
    bool parseSolidHeader(const std::string &line, int &solidNum,
                          std::string &typeLower, std::string &lineOutForData);
    bool getSolData(std::vector<fastf_t> &data, int num, int solidNum,
                    bool firstCardAlreadyRead, std::string lineBuf);
    bool getXSolData(std::vector<fastf_t> &data, int num, int solidNum);

    /* Helpers */
    bool isIntegerField(std::string_view sv, size_t start, size_t len, int &value) const;
    bool isLikelyFaceCard(const std::string &line) const;

    /* Primitive handlers */
    SolidParseResult handle_ars(const std::string &name, const std::string &firstLine);
    SolidParseResult handle_rpp(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_box(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_raw_wed(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_rvw(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_arw(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_arb_magic(const std::string &name, const std::string &firstLine);
    SolidParseResult handle_arb4(const std::string &name, const std::vector<fastf_t> &dd4);
    SolidParseResult handle_arb5(const std::string &name, const std::vector<fastf_t> &dd5);
    SolidParseResult handle_arb6(const std::string &name, const std::vector<fastf_t> &dd6);
    SolidParseResult handle_arb7(const std::string &name, const std::vector<fastf_t> &dd7);
    SolidParseResult handle_arb8(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_rcc(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_rec(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_trc(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_tec(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_tgc(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_sph(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_wir(const std::string &name, const std::string &firstLine);
    SolidParseResult handle_rpc(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_rhc(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_epa(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_ehy(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_eto(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_v4_ell_transform(const std::string &name, const std::vector<fastf_t> &foci);
    SolidParseResult handle_ell(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_ellg(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_tor(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_haf(const std::string &name, const std::vector<fastf_t> &dd);
    SolidParseResult handle_arbn(const std::string &name, const std::string &firstLine);

private:
    ComGeomReader &m_r;
    rt_wdb *m_out = nullptr;
    SolidParserOptions m_opts;
    int m_solTotal = 0;
    int m_solWork = 0;
    size_t m_colCur = 0;
};

#endif