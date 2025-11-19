#include "SolidParser.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

/* Helper: build min/max from 6 RPP values */
static inline void vset_minmax(fastf_t *min, fastf_t *max, const fastf_t *dd) {
    VSET(min, dd[0], dd[2], dd[4]);
    VSET(max, dd[1], dd[3], dd[5]);
}

SolidParser::SolidParser(ComGeomReader &reader, rt_wdb *outfp, const SolidParserOptions &opts)
    : m_r(reader), m_out(outfp), m_opts(opts)
{}

std::string SolidParser::makeSolidName(int n) const {
    std::string suf = m_opts.suffix;
    if (m_opts.legacy_trunc && suf.size() > 13) suf.resize(13);
    char buf[64] = {0};
    std::snprintf(buf, sizeof(buf), "s%d%s", n, suf.c_str());
    return std::string(buf);
}

void SolidParser::colPrint(const std::string &s) {
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

bool SolidParser::parseSolidHeader(const std::string &line, int &solidNum,
                                   std::string &typeLower, std::string &lineOutForData) {
    std::string_view sv(line);
    std::string solidType;
    switch (m_opts.version) {
        case 5:
            solidNum  = ComGeomReader::fieldInt(sv, 0, 5);
            solidType = ComGeomReader::rstrip(sv.substr(5, 5));
            break;
        case 4:
            solidNum  = ComGeomReader::fieldInt(sv, 0, 3);
            solidType = ComGeomReader::rstrip(sv.substr(3, 4));
            break;
        case 1:
            solidNum  = ComGeomReader::fieldInt(sv, 5, 4);
            solidType = ComGeomReader::rstrip(sv.substr(2, 3));
            break;
        default:
            return false;
    }
    ComGeomReader::toLowerInplace(solidType);
    typeLower = solidType;
    lineOutForData = line;
    return true;
}

bool SolidParser::getSolData(std::vector<fastf_t> &data, int num, int solidNum,
                             bool /*firstCardAlreadyRead*/, std::string lineBuf) {
    data.clear();
    data.reserve(num);
    int cd = 1;
    std::string line = std::move(lineBuf);
    while (num > 0) {
        std::string_view sv(line);
        if (cd != 1) {
            if ((m_opts.version == 5 && sv.size() > 5 && sv[5] != ' ') ||
                (m_opts.version == 4 && sv.size() > 3 && sv[3] != ' ')) {
                std::fprintf(stderr, "solid %d (continuation) card %d non-blank\n", solidNum, cd);
                return false;
            }
        }
        int j = std::min(num, 6);
        for (int i = 0; i < j; ++i) {
            data.push_back(static_cast<fastf_t>(ComGeomReader::fieldDouble(sv, 10 + i*10, 10)));
        }
        num -= j;
        if (num <= 0) break;
        auto l = m_r.next("solid continuation card");
        if (!l) {
            std::fprintf(stderr, "too few cards for solid %d\n", solidNum);
            return false;
        }
        line = l->s;
        ++cd;
    }
    return true;
}

bool SolidParser::getXSolData(std::vector<fastf_t> &data, int num, int solidNum) {
    data.clear();
    data.reserve(num);
    int cd = 1;
    while (num > 0) {
        auto l = m_r.next("x solid card");
        if (!l) {
            std::fprintf(stderr, "too few cards for solid %d\n", solidNum);
            return false;
        }
        std::string_view sv(l->s);
        if (cd != 1) {
            if ((m_opts.version == 5 && sv.size() > 5 && sv[5] != ' ') ||
                (m_opts.version == 4 && sv.size() > 3 && sv[3] != ' ')) {
                std::fprintf(stderr, "solid %d (continuation) card %d non-blank\n", solidNum, cd);
                return false;
            }
        }
        int j = std::min(num, 6);
        for (int i = 0; i < j; ++i) {
            data.push_back(static_cast<fastf_t>(ComGeomReader::fieldDouble(sv, 10 + i*10, 10)));
        }
        num -= j;
        ++cd;
    }
    return true;
}

/* ---------------- Primitive Handlers ---------------- */

SolidParseResult SolidParser::handle_ars(const std::string &name, const std::string &firstLine) {
    std::string_view sv(firstLine);
    int ncurves = ComGeomReader::fieldInt(sv, 10, 10);
    int pts_per_curve = ComGeomReader::fieldInt(sv, 20, 10);
    if (ncurves <= 0 || pts_per_curve <= 0)
        return {SolidParseResult::Error, "ars invalid counts"};

    std::vector<std::vector<fastf_t>> curves(ncurves);
    for (int i = 0; i < ncurves; ++i) {
        curves[i].resize(static_cast<size_t>(pts_per_curve) * 3);
        if (!getXSolData(curves[i], pts_per_curve * 3, m_solWork))
            return {SolidParseResult::Error, "ars curve data read failure"};
    }
    std::vector<fastf_t*> ptrs(ncurves);
    for (int i = 0; i < ncurves; ++i) ptrs[i] = curves[i].data();

    int ret = mk_ars(m_out, name.c_str(), ncurves, pts_per_curve, ptrs.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_ars failed" : ""};
}

SolidParseResult SolidParser::handle_rpp(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 6) return {SolidParseResult::Error, "rpp needs 6 values"};
    fastf_t min[3], max[3];
    vset_minmax(min, max, dd.data());
    int ret = mk_rpp(m_out, name.c_str(), min, max);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_rpp failed" : ""};
}

SolidParseResult SolidParser::handle_box(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "box needs 12 values"};
    point_t v[8];
    const fastf_t *D0 = &dd[0], *D1=&dd[3], *D2=&dd[6], *D3=&dd[9];
    VMOVE(v[0], D0);
    VADD2(v[1], D0, D2);
    VADD3(v[2], D0, D2, D1);
    VADD2(v[3], D0, D1);
    VADD2(v[4], D0, D3);
    VADD3(v[5], D0, D3, D2);
    VADD4(v[6], D0, D3, D2, D1);
    VADD3(v[7], D0, D3, D1);
    int ret = mk_arb8(m_out, name.c_str(), &v[0][0]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(box) failed" : ""};
}

SolidParseResult SolidParser::handle_raw_wed(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "raw/wed needs 12 values"};
    point_t v[8];
    const fastf_t *D0=&dd[0], *D1=&dd[3], *D2=&dd[6], *D3=&dd[9];
    VMOVE(v[0], D0);
    VADD2(v[1], D0, D2);
    VMOVE(v[2], v[1]);
    VADD2(v[3], D0, D1);
    VADD2(v[4], D0, D3);
    VADD3(v[5], D0, D3, D2);
    VMOVE(v[6], v[5]);
    VADD3(v[7], D0, D3, D1);
    int ret = mk_arb8(m_out, name.c_str(), &v[0][0]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(raw/wed) failed" : ""};
}

SolidParseResult SolidParser::handle_rvw(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 7) return {SolidParseResult::Error, "rvw needs 7 values"};
    vect_t V; VMOVE(V, &dd[0]);
    double a2=dd[3], theta=dd[4], phi=dd[5], h2=dd[6];
    double angle1 = (phi + theta - 90.0) * DEG2RAD;
    double angle2 = (phi + theta) * DEG2RAD;
    double a2theta = a2 * std::tan(theta * DEG2RAD);
    vect_t a,b,c;
    VSET(a, a2theta * std::cos(angle1), a2theta * std::sin(angle1), 0.0);
    VSET(b, -a2 * std::cos(angle2), -a2 * std::sin(angle2), 0.0);
    VSET(c, 0.0, 0.0, h2);
    point_t vert[8];
    VSUB2(vert[0], V, b);
    VMOVE(vert[1], V);
    VMOVE(vert[2], vert[1]);
    VADD2(vert[3], vert[0], a);
    VADD2(vert[4], vert[0], c);
    VADD2(vert[5], vert[1], c);
    VMOVE(vert[6], vert[5]);
    VADD2(vert[7], vert[3], c);
    int ret = mk_arb8(m_out, name.c_str(), &vert[0][0]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(rvw) failed" : ""};
}

SolidParseResult SolidParser::handle_arw(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "arw needs 12 values"};
    point_t v[8];
    const fastf_t *D0=&dd[0], *D1=&dd[3], *D2=&dd[6], *D3=&dd[9];
    VMOVE(v[0], D0);
    VADD2(v[1], D0, D2);
    VADD3(v[2], D0, D2, D3);
    VADD2(v[3], D0, D3);
    VADD2(v[4], D0, D1);
    VMOVE(v[5], v[4]);
    VADD3(v[6], D0, D1, D3);
    VMOVE(v[7], v[6]);
    int ret = mk_arb8(m_out, name.c_str(), &v[0][0]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(arw) failed" : ""};
}

SolidParseResult SolidParser::handle_arb4(const std::string &name, const std::vector<fastf_t> &dd4) {
    if (dd4.size() < 12) return {SolidParseResult::Error, "arb4 needs 12 values"};
    int ret = mk_arb4(m_out, name.c_str(), dd4.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb4 failed" : ""};
}

SolidParseResult SolidParser::handle_arb5(const std::string &name, const std::vector<fastf_t> &dd5) {
    if (dd5.size() < 15) return {SolidParseResult::Error, "arb5 needs 15 values"};
    std::vector<fastf_t> dd_full(24);
    std::copy(dd5.begin(), dd5.begin()+15, dd_full.begin());
    const fastf_t *v4 = &dd5[12];
    for (int rep=5; rep<=7; ++rep) std::copy(v4, v4+3, dd_full.begin()+rep*3);
    int ret = mk_arb8(m_out, name.c_str(), dd_full.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(arb5) failed" : ""};
}

SolidParseResult SolidParser::handle_arb6(const std::string &name, const std::vector<fastf_t> &dd6) {
    if (dd6.size() < 18) return {SolidParseResult::Error, "arb6 needs 18 values"};
    std::vector<fastf_t> dd_full(24);
    std::copy(dd6.begin(), dd6.begin()+18, dd_full.begin());
    const fastf_t *D4=&dd6[12];
    const fastf_t *D5=&dd6[15];
    std::copy(D4, D4+3, dd_full.begin()+5*3); // D(5)=D(4)
    std::copy(D5, D5+3, dd_full.begin()+6*3); // D(6)=original D(5)
    std::copy(D5, D5+3, dd_full.begin()+7*3); // D(7)=original D(5)
    int ret = mk_arb8(m_out, name.c_str(), dd_full.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(arb6) failed" : ""};
}

SolidParseResult SolidParser::handle_arb7(const std::string &name, const std::vector<fastf_t> &dd7) {
    if (dd7.size() < 21) return {SolidParseResult::Error, "arb7 needs 21 values"};
    std::vector<fastf_t> dd_full(24);
    std::copy(dd7.begin(), dd7.begin()+21, dd_full.begin());
    const fastf_t *D4=&dd7[12];
    std::copy(D4, D4+3, dd_full.begin()+7*3); // D(7)=D(4)
    int ret = mk_arb8(m_out, name.c_str(), dd_full.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8(arb7) failed" : ""};
}

SolidParseResult SolidParser::handle_arb8(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 24) return {SolidParseResult::Error, "arb8 needs 24 values"};
    int ret = mk_arb8(m_out, name.c_str(), dd.data());
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_arb8 failed" : ""};
}

SolidParseResult SolidParser::handle_rcc(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 7) return {SolidParseResult::Error, "rcc needs 7 values"};
    int ret = mk_rcc(m_out, name.c_str(), dd.data(), dd.data()+3, dd[6]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_rcc failed" : ""};
}

SolidParseResult SolidParser::handle_rec(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "rec needs 12 values"};
    int ret = mk_tgc(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd.data()+9, dd.data()+6, dd.data()+9);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_tgc(rec) failed" : ""};
}

SolidParseResult SolidParser::handle_trc(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 8) return {SolidParseResult::Error, "trc needs 8 values"};
    int ret = mk_trc_h(m_out, name.c_str(), dd.data(), dd.data()+3, dd[6], dd[7]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_trc_h failed" : ""};
}

SolidParseResult SolidParser::handle_tec(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 13) return {SolidParseResult::Error, "tec needs 13 values"};
    double p = dd[12];
    if (NEAR_ZERO(p, SMALL)) return {SolidParseResult::Error, "tec p cannot be zero"};
    vect_t topA, topB;
    double invp = 1.0/p;
    VSCALE(topA, dd.data()+6, invp);
    VSCALE(topB, dd.data()+9, invp);
    int ret = mk_tgc(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd.data()+9, topA, topB);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_tgc(tec) failed" : ""};
}

SolidParseResult SolidParser::handle_tgc(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 14) return {SolidParseResult::Error, "tgc needs 14 values"};
    vect_t topA, topB;
    double magBaseA = MAGNITUDE(dd.data()+6);
    double magBaseB = MAGNITUDE(dd.data()+9);
    if (magBaseA < SMALL || magBaseB < SMALL) return {SolidParseResult::Error,"tgc zero-length A or B"};
    VSCALE(topA, dd.data()+6, dd[12]/magBaseA);
    VSCALE(topB, dd.data()+9, dd[13]/magBaseB);
    int ret = mk_tgc(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd.data()+9, topA, topB);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_tgc failed" : ""};
}

SolidParseResult SolidParser::handle_sph(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 4) return {SolidParseResult::Error, "sph needs 4 values"};
    int ret = mk_sph(m_out, name.c_str(), dd.data(), dd[3]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_sph failed" : ""};
}

SolidParseResult SolidParser::handle_wir(const std::string &name, const std::string &firstLine) {
    int numpts = ComGeomReader::fieldInt(firstLine, 8, 2);
    if (numpts <= 0) return {SolidParseResult::Error, "wir invalid point count"};
    int total = numpts*3 + 1;
    std::vector<fastf_t> raw;
    if (!getSolData(raw, total, m_solWork, true, firstLine))
        return {SolidParseResult::Error, "wir data read failure"};
    if ((int)raw.size() < total) return {SolidParseResult::Error, "wir insufficient data"};
    double radius = raw.back();
    if (radius <= 0.0) return {SolidParseResult::Error, "wir invalid radius"};
    double diameter = radius * 2.0;
    struct bu_list head;
    BU_LIST_INIT(&head);
    for (int i=0;i<numpts;++i) {
        struct wdb_pipe_pnt *ps;
        BU_ALLOC(ps, struct wdb_pipe_pnt);
        ps->l.magic = WDB_PIPESEG_MAGIC;
        VMOVE(ps->pp_coord, &raw[i*3]);
        ps->pp_id = 0;
        ps->pp_od = diameter;
        ps->pp_bendradius = diameter;
        BU_LIST_INSERT(&head, &ps->l);
    }
    int ret = mk_pipe(m_out, name.c_str(), &head);
    mk_pipe_free(&head);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_pipe failed" : ""};
}

SolidParseResult SolidParser::handle_rpc(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 10) return {SolidParseResult::Error, "rpc needs 10 values"};
    int ret = mk_rpc(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd[9]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_rpc failed" : ""};
}

SolidParseResult SolidParser::handle_rhc(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 11) return {SolidParseResult::Error, "rhc needs 11 values"};
    int ret = mk_rhc(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd[9], dd[10]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_rhc failed" : ""};
}

SolidParseResult SolidParser::handle_epa(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 11) return {SolidParseResult::Error, "epa needs 11 values"};
    int ret = mk_epa(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd[9], dd[10]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_epa failed" : ""};
}

SolidParseResult SolidParser::handle_ehy(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "ehy needs 12 values"};
    int ret = mk_ehy(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd[9], dd[10], dd[11]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_ehy failed" : ""};
}

SolidParseResult SolidParser::handle_eto(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 11) return {SolidParseResult::Error, "eto needs 11 values"};
    int ret = mk_eto(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd[9], dd[10]);
    return {ret < 0 ? SolidParseResult::Error : SolidParseResult::Ok, ret < 0 ? "mk_eto failed" : ""};
}

SolidParseResult SolidParser::handle_v4_ell_transform(const std::string &name, const std::vector<fastf_t> &foci) {
    if (foci.size() < 7) return {SolidParseResult::Error, "v4 ell needs 7 values"};
    vect_t F1,F2; VMOVE(F1,&foci[0]); VMOVE(F2,&foci[3]);
    double L = foci[6];
    vect_t V; VADD2SCALE(V,F1,F2,0.5);
    vect_t work; VSUB2(work,F2,F1);
    double m1 = MAGNITUDE(work);
    if (m1 < SMALL) return {SolidParseResult::Error,"degenerate ell"};
    double a_scale = (L*0.5)/m1;
    vect_t baseA; VSCALE(baseA, work, a_scale);
    double r = std::sqrt(MAGSQ(baseA) - (m1*0.5)*(m1*0.5));
    if (!(r>0.0)) return {SolidParseResult::Error,"invalid ell radius"};
    vect_t Vp{V[0]+M_PI, V[1]+M_PI, V[2]+M_PI};
    vect_t B; VCROSS(B,Vp,baseA); double mb=MAGNITUDE(B); if (mb<SMALL) return {SolidParseResult::Error,"degenerate B"};
    VSCALE(B,B,r/mb);
    vect_t C; VCROSS(C,baseA,B); double mc=MAGNITUDE(C); if (mc<SMALL) return {SolidParseResult::Error,"degenerate C"};
    VSCALE(C,C,r/mc);
    int ret = mk_ell(m_out, name.c_str(), V, baseA, B, C);
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_ell failed":""};
}

SolidParseResult SolidParser::handle_ell(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 7) return {SolidParseResult::Error, "ell needs 7 values"};
    vect_t V, A; VMOVE(V,&dd[0]); VMOVE(A,&dd[3]);
    double r = dd[6]; if (r <= 0.0) return {SolidParseResult::Error,"invalid ell radius"};
    vect_t Vp{V[0]+M_PI,V[1]+M_PI,V[2]+M_PI};
    vect_t B; VCROSS(B,Vp,A); double mb=MAGNITUDE(B); if (mb<SMALL) return {SolidParseResult::Error,"degenerate B"};
    VSCALE(B,B,r/mb);
    vect_t C; VCROSS(C,A,B); double mc=MAGNITUDE(C); if (mc<SMALL) return {SolidParseResult::Error,"degenerate C"};
    VSCALE(C,C,r/mc);
    int ret = mk_ell(m_out, name.c_str(), V, A, B, C);
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_ell failed":""};
}

SolidParseResult SolidParser::handle_ellg(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 12) return {SolidParseResult::Error, "ellg needs 12 values"};
    int ret = mk_ell(m_out, name.c_str(), dd.data(), dd.data()+3, dd.data()+6, dd.data()+9);
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_ell failed":""};
}

SolidParseResult SolidParser::handle_tor(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 8) return {SolidParseResult::Error, "tor needs 8 values"};
    int ret = mk_tor(m_out, name.c_str(), dd.data(), dd.data()+3, dd[6], dd[7]);
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_tor failed":""};
}

SolidParseResult SolidParser::handle_haf(const std::string &name, const std::vector<fastf_t> &dd) {
    if (dd.size() < 4) return {SolidParseResult::Error, "haf needs 4 values"};
    vect_t N; VMOVE(N,&dd[0]);
    double magN = MAGNITUDE(N);
    if (magN < SMALL) return {SolidParseResult::Error,"haf normal too small"};
    int ret = mk_half(m_out, name.c_str(), N, -dd[3]);
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_half failed":""};
}

SolidParseResult SolidParser::handle_arbn(const std::string &name, const std::string &firstLine) {
    std::string_view sv(firstLine);
    int npt  = ComGeomReader::fieldInt(sv, 10 + 0*10, 10);
    int npe  = ComGeomReader::fieldInt(sv, 10 + 1*10, 10);
    int neq  = ComGeomReader::fieldInt(sv, 10 + 2*10, 10);
    int nae  = ComGeomReader::fieldInt(sv, 10 + 3*10, 10);
    int nface = npe + neq + nae;
    if (npt < 1)   return {SolidParseResult::Error, "arbn requires >=1 point"};
    if (nface < 1) return {SolidParseResult::Error, "arbn requires >=1 face"};

    std::vector<plane_t> eqn(static_cast<size_t>(nface));
    std::vector<int> used(static_cast<size_t>(nface), 0);
    std::vector<fastf_t> input_points;
    input_points.reserve(npt*3);

    struct bn_tol tol;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    {   // vertex points
        std::vector<fastf_t> pts;
        if (!getXSolData(pts, npt*3, m_solWork))
            return {SolidParseResult::Error, "arbn vertex read failure"};
        input_points = std::move(pts);
    }

    int cur_eq = 0;
    int symm = 0;

    /* Planes from triples */
    for (int i=0;i<npe;i+=6) {
        auto l = m_r.next("arbn vertex point indices");
        if (!l) return {SolidParseResult::Error,"arbn EOF (point indices)"};
        std::string_view lv(l->s);
        for (int j=0;j<6 && (i+j)<npe;++j) {
            int q = ComGeomReader::fieldInt(lv, 10 + j*10 + 0, 4);
            int r = ComGeomReader::fieldInt(lv, 10 + j*10 + 4, 3);
            int s = ComGeomReader::fieldInt(lv, 10 + j*10 + 7, 3);
            if (q==0 || r==0 || s==0) continue;
            point_t a,b,c;
            auto fetch = [&](int idx, point_t &out){
                int abs_idx = std::abs(idx)-1;
                VMOVE(out, &input_points[abs_idx*3]);
                if (idx < 0) { out[Y] = -out[Y]; symm = 1; }
            };
            fetch(q,a); fetch(r,b); fetch(s,c);
            if (bg_make_plane_3pnts(eqn[cur_eq], a, b, c, &tol) < 0) {
                if (m_opts.verbose) std::fprintf(stderr,"arbn degenerate 3p plane ignored\n");
                continue;
            }
            ++cur_eq;
        }
    }

    /* Planes from equations */
    for (int i=0;i<neq;++i) {
        auto l = m_r.next("arbn plane equation card");
        if (!l) return {SolidParseResult::Error,"arbn EOF (equation)"};
        std::string_view lv(l->s);
        eqn[cur_eq][X] = ComGeomReader::fieldDouble(lv, 10, 10);
        eqn[cur_eq][Y] = ComGeomReader::fieldDouble(lv, 20, 10);
        eqn[cur_eq][Z] = ComGeomReader::fieldDouble(lv, 30, 10);
        eqn[cur_eq][W] = ComGeomReader::fieldDouble(lv, 40, 10);
        double scale = MAGNITUDE(eqn[cur_eq]);
        if (scale < SMALL) {
            if (m_opts.verbose) std::fprintf(stderr,"arbn plane normal too small (eqn)\n");
            continue;
        }
        scale = 1.0/scale;
        VSCALE(eqn[cur_eq], eqn[cur_eq], scale);
        eqn[cur_eq][W] *= scale;
        ++cur_eq;
    }

    /* Planes from az/el */
    for (int i=0;i<nae;i+=2) {
        auto l = m_r.next("arbn az/el card");
        if (!l) return {SolidParseResult::Error,"arbn EOF (az/el)"};
        std::string_view lv(l->s);
        for (int j=0;j<2 && (i+j)<nae;++j) {
            double az = ComGeomReader::fieldDouble(lv, 10 + j*30 + 0*10, 10) * DEG2RAD;
            double el = ComGeomReader::fieldDouble(lv, 10 + j*30 + 1*10, 10) * DEG2RAD;
            int vert_no = ComGeomReader::fieldInt(lv, 10 + j*30 + 2*10, 10);
            if (vert_no == 0) break;
            double cos_el = std::cos(el);
            eqn[cur_eq][X] = std::cos(az) * cos_el;
            eqn[cur_eq][Y] = std::sin(az) * cos_el;
            eqn[cur_eq][Z] = std::sin(el);
            point_t pt;
            int abs_idx = std::abs(vert_no)-1;
            VMOVE(pt, &input_points[abs_idx*3]);
            if (vert_no < 0) pt[Y] = -pt[Y];
            eqn[cur_eq][W] = VDOT(pt, eqn[cur_eq]);
            ++cur_eq;
        }
    }

    if (cur_eq != nface)
        return {SolidParseResult::Error, "arbn face count mismatch"};

    point_t cent; VSETALL(cent, 0);
    for (int i=0;i<npt;++i) VADD2(cent, cent, &input_points[i*3]);
    VSCALE(cent, cent, 1.0/static_cast<double>(npt));
    if (symm) cent[Y] = 0.0;

    constexpr double DIST_TOL = 1.0e-8;
    for (int i=0;i<nface;++i) {
        double dist = VDOT(eqn[i], cent) - eqn[i][W];
        if (dist < -DIST_TOL) continue;
        if (dist > DIST_TOL) {
            VREVERSE(eqn[i], eqn[i]);
            eqn[i][W] = -eqn[i][W];
        } else {
            return {SolidParseResult::Error,"arbn centroid lies on face"};
        }
    }

    std::vector<double> vertex;
    int last_vertex = 0;
    for (int i=0;i<nface-2;++i) {
        for (int j=i+1;j<nface-1;++j) {
            double dot = VDOT(eqn[i], eqn[j]);
            if (!NEAR_ZERO(dot, 0.999999)) continue;
            for (int k=j+1;k<nface;++k) {
                point_t pt;
                if (bg_make_pnt_3planes(pt, eqn[i], eqn[j], eqn[k]) < 0) continue;
                bool outside=false;
                for (int m=0;m<nface;++m) {
                    if (m==i||m==j||m==k) continue;
                    if (VDOT(pt, eqn[m]) - eqn[m][W] > DIST_TOL) { outside=true; break; }
                }
                if (outside) continue;
                bool duplicate=false;
                for (int m=0;m<last_vertex;++m) {
                    vect_t dv; VSUB2(dv, pt, &vertex[m*3]);
                    if (MAGSQ(dv) < 1.0e-10) { duplicate=true; break; }
                }
                if (duplicate) continue;
                vertex.resize((last_vertex+1)*3);
                VMOVE(&vertex[last_vertex*3], pt);
                ++last_vertex;
                used[i]++; used[j]++; used[k]++;
            }
        }
    }
    for (int i=0;i<nface;++i)
        if (used[i]==0)
            return {SolidParseResult::Error,"arbn face unused (not convex)"};

    int ret = mk_arbn(m_out, name.c_str(), nface, eqn.data());
    return {ret<0?SolidParseResult::Error:SolidParseResult::Ok, ret<0?"mk_arbn failed":""};
}

/* ---------------- Dispatcher  ---------------- */
SolidParseResult SolidParser::parseNext() {
    if (m_solWork >= m_solTotal && m_solTotal > 0) {
        return {SolidParseResult::Eof, ""};
    }

    auto l = m_r.next("solid card");
    if (!l) return {SolidParseResult::Eof, ""};

    int solidNum = 0;
    std::string type, firstLine = l->s;
    if (!parseSolidHeader(l->s, solidNum, type, firstLine)) {
        if (m_solWork >= m_solTotal) return {SolidParseResult::Eof, ""};
        return {SolidParseResult::Error, "invalid solid header"};
    }

    // v1 end sentinel handled in region code; in solids, "end" terminates
    if (type == "end") {
        return {SolidParseResult::Eof, ""};
    }

    if (m_opts.version == 5) {
        int expectedNext = m_solWork + 1;
        if (solidNum != expectedNext) {
            // proceed but warn
            std::fprintf(stderr,
                         "warning: solid numbering mismatch: expected %d, got %d - consuming this solid to maintain alignment\n",
                         expectedNext, solidNum);
        }
    }

    // Always advance processed count; names use sequential counter
    m_solWork++;

    std::string name = makeSolidName(m_solWork);
    colPrint(name);

    std::vector<fastf_t> dd;

    if (type == "ars")        return handle_ars(name, firstLine);
    if (type == "rpp")        { if(!getSolData(dd,6,solidNum,true,firstLine)) return {SolidParseResult::Error,"rpp data read"}; return handle_rpp(name,dd); }
    if (type == "box")        { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"box data read"}; return handle_box(name,dd); }
    if (type == "raw" || type == "wed") { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"raw/wed data read"}; return handle_raw_wed(name,dd); }
    if (type == "rvw")        { if(!getSolData(dd,7,solidNum,true,firstLine)) return {SolidParseResult::Error,"rvw data read"}; return handle_rvw(name,dd); }
    if (type == "arw")        { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"arw data read"}; return handle_arw(name,dd); }
    if (type == "arb4")       { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"arb4 data read"}; return handle_arb4(name,dd); }
    if (type == "arb5")       { if(!getSolData(dd,15,solidNum,true,firstLine)) return {SolidParseResult::Error,"arb5 data read"}; return handle_arb5(name,dd); }
    if (type == "arb6")       { if(!getSolData(dd,18,solidNum,true,firstLine)) return {SolidParseResult::Error,"arb6 data read"}; return handle_arb6(name,dd); }
    if (type == "arb7")       { if(!getSolData(dd,21,solidNum,true,firstLine)) return {SolidParseResult::Error,"arb7 data read"}; return handle_arb7(name,dd); }
    if (type == "arb8")       { if(!getSolData(dd,24,solidNum,true,firstLine)) return {SolidParseResult::Error,"arb8 data read"}; return handle_arb8(name,dd); }
    if (type == "rcc")        { if(!getSolData(dd,7,solidNum,true,firstLine)) return {SolidParseResult::Error,"rcc data read"}; return handle_rcc(name,dd); }
    if (type == "rec")        { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"rec data read"}; return handle_rec(name,dd); }
    if (type == "trc")        { if(!getSolData(dd,8,solidNum,true,firstLine)) return {SolidParseResult::Error,"trc data read"}; return handle_trc(name,dd); }
    if (type == "tec")        { if(!getSolData(dd,13,solidNum,true,firstLine)) return {SolidParseResult::Error,"tec data read"}; return handle_tec(name,dd); }
    if (type == "tgc")        { if(!getSolData(dd,14,solidNum,true,firstLine)) return {SolidParseResult::Error,"tgc data read"}; return handle_tgc(name,dd); }
    if (type == "sph")        { if(!getSolData(dd,4,solidNum,true,firstLine))  return {SolidParseResult::Error,"sph data read"}; return handle_sph(name,dd); }
    if (type.rfind("wir",0)==0) return handle_wir(name, firstLine);
    if (type == "rpc")        { if(!getSolData(dd,10,solidNum,true,firstLine)) return {SolidParseResult::Error,"rpc data read"}; return handle_rpc(name,dd); }
    if (type == "rhc")        { if(!getSolData(dd,11,solidNum,true,firstLine)) return {SolidParseResult::Error,"rhc data read"}; return handle_rhc(name,dd); }
    if (type == "epa")        { if(!getSolData(dd,11,solidNum,true,firstLine)) return {SolidParseResult::Error,"epa data read"}; return handle_epa(name,dd); }
    if (type == "ehy")        { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"ehy data read"}; return handle_ehy(name,dd); }
    if (type == "eto")        { if(!getSolData(dd,11,solidNum,true,firstLine)) return {SolidParseResult::Error,"eto data read"}; return handle_eto(name,dd); }
    if (type == "ellg")       { if(!getSolData(dd,12,solidNum,true,firstLine)) return {SolidParseResult::Error,"ellg data read"}; return handle_ellg(name,dd); }
    if (type == "ell" || type == "ell1") {
        if (m_opts.version <=4 && type=="ell") {
            if (!getSolData(dd,7,solidNum,true,firstLine)) return {SolidParseResult::Error,"v4 ell data read"};
            return handle_v4_ell_transform(name, dd);
        } else {
            if (!getSolData(dd,7,solidNum,true,firstLine)) return {SolidParseResult::Error,"ell data read"};
            return handle_ell(name, dd);
        }
    }
    if (type == "tor")        { if(!getSolData(dd,8,solidNum,true,firstLine)) return {SolidParseResult::Error,"tor data read"}; return handle_tor(name,dd); }
    if (type == "haf")        { if(!getSolData(dd,4,solidNum,true,firstLine)) return {SolidParseResult::Error,"haf data read"}; return handle_haf(name,dd); }
    if (type == "arbn")       return handle_arbn(name, firstLine);

    std::fprintf(stderr, "getsolid: unsupported type '%s'\n", type.c_str());
    return {SolidParseResult::Error, "unsupported solid"};
}
