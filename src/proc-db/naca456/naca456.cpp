/*                    N A C A 4 5 6 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/naca456/naca456.cpp
 *
 * Procedural wing generator using NACA airfoil sections.
 *
 * The NACA section formulas are implemented from the public-domain
 * NACA456 program by Ralph Carmichael, Public Domain Aeronautical
 * Software: https://www.pdas.com/naca456download.html
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "bio.h"
#include "brep/edit.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "rt/geom.h"
#include "wdb.h"


struct point2d {
    double x = 0.0;
    double z = 0.0;
};


struct point3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};


struct airfoil4 {
    double camber = 0.0;
    double camber_pos = 0.0;
    double thickness = 0.12;
};


enum class airfoil_series {
    four,
    five,
    six
};


struct airfoil_spec {
    airfoil_series series = airfoil_series::four;
    airfoil4 four;
    double cl = 0.0;
    double xmaxc = 0.0;
    bool reflex = false;
    int six_family = 0;
    bool six_a = false;
    double six_a_param = 1.0;
    double thickness = 0.12;
};


struct section4 {
    std::vector<point2d> upper;
    std::vector<point2d> lower;
};


struct brep_builder {
    ON_Brep *brep = nullptr;
    std::vector<ON_3dPoint> points;
    std::map<std::pair<int, int>, int> edges;
};


enum class output_mode {
    bot,
    brep
};


enum class brep_surface_mode {
    ruled,
    smooth
};


struct options {
    std::string outfile = "naca456-wing.g";
    std::string name = "naca456_wing";
    std::string airfoil = "2412";
    std::string tip_airfoil;
    std::string demo_file;
    std::string section_file;
    output_mode mode = output_mode::bot;
    brep_surface_mode brep_surface = brep_surface_mode::ruled;
    fastf_t span = 1000.0;
    fastf_t root_chord = 250.0;
    fastf_t tip_chord = 125.0;
    fastf_t sweep = 0.0;
    fastf_t dihedral_deg = 0.0;
    fastf_t tip_twist_deg = 0.0;
    fastf_t six_a_param = 1.0;
    fastf_t offset_x = 0.0;
    fastf_t offset_y = 0.0;
    fastf_t offset_z = 0.0;
    int stations = 9;
    int samples = 65;
    bool sharp_te = false;
    bool overwrite = false;
    bool append = false;
};


static void
usage(const char *argv0, struct bu_opt_desc *d)
{
    bu_log("Usage: %s [options]\n\n", argv0);
    char *option_help = bu_opt_describe(d, NULL);
    if (option_help) {
	bu_log("%s", option_help);
	bu_free(option_help, "option help");
    }
}


static std::string
mode_name(output_mode mode)
{
    return mode == output_mode::bot ? "bot" : "brep";
}


static std::string
brep_surface_name(brep_surface_mode mode)
{
    return mode == brep_surface_mode::smooth ? "smooth" : "ruled";
}


static std::string
bool_name(bool val)
{
    return val ? "1" : "0";
}


static std::string
num_string(double val)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    bu_vls_printf(&str, "%.17g", val);
    std::string ret = bu_vls_cstr(&str);
    bu_vls_free(&str);
    return ret;
}


static std::string
normalized_airfoil_code(const std::string &code)
{
    std::string out;
    out.reserve(code.size());

    for (char ch : code) {
	const unsigned char uch = static_cast<unsigned char>(ch);
	if (std::isspace(uch) || ch == '-')
	    continue;
	out.push_back(static_cast<char>(std::toupper(uch)));
    }

    const std::string prefix = "NACA";
    if (out.compare(0, prefix.size(), prefix) == 0)
	out.erase(0, prefix.size());

    return out;
}


static bool
all_digits(const std::string &str)
{
    if (str.empty())
	return false;

    for (char c : str) {
	if (c < '0' || c > '9')
	    return false;
    }

    return true;
}


static int
six_family_index(const std::string &family)
{
    if (family == "63")
	return 1;
    if (family == "64")
	return 2;
    if (family == "65")
	return 3;
    if (family == "66")
	return 4;
    if (family == "67")
	return 5;
    if (family == "63A")
	return 6;
    if (family == "64A")
	return 7;
    if (family == "65A")
	return 8;

    return 0;
}


static airfoil_spec
parse_airfoil(const std::string &code, double six_a_param)
{
    const std::string digits = normalized_airfoil_code(code);
    airfoil_spec spec;
    spec.six_a_param = six_a_param;

    if (digits.size() != 4) {
	if (digits.size() == 5 && all_digits(digits) && digits[0] != '6') {
	    spec.series = airfoil_series::five;
	    spec.cl = 0.15 * static_cast<double>(digits[0] - '0');
	    spec.xmaxc = static_cast<double>(digits[1] - '0') / 20.0;
	    spec.reflex = digits[2] == '1';
	    if (digits[2] != '0' && digits[2] != '1')
		throw std::runtime_error("5-digit NACA mean-line reflex digit must be 0 or 1");
	    spec.thickness = ((digits[3] - '0') * 10 + (digits[4] - '0')) / 100.0;
	    if (spec.cl <= 0.0 || spec.xmaxc <= 0.0)
		throw std::runtime_error("5-digit NACA airfoils require nonzero design lift and camber position");
	    if (spec.xmaxc > 0.25)
		throw std::runtime_error("5-digit NACA camber position must be 5, 10, 15, 20, or 25 percent chord");
	    if (spec.reflex && spec.xmaxc < 0.10)
		throw std::runtime_error("5-digit reflex NACA camber position must be 10, 15, 20, or 25 percent chord");
	    if (spec.thickness <= 0.0)
		throw std::runtime_error("NACA thickness must be positive");
	    return spec;
	}

	size_t family_len = 0;
	if (digits.size() >= 6 && digits[2] == 'A') {
	    family_len = 3;
	} else if (digits.size() >= 5) {
	    family_len = 2;
	}

	if (family_len > 0) {
	    const std::string family = digits.substr(0, family_len);
	    const int family_index = six_family_index(family);
	    const std::string rest = digits.substr(family_len);
	    if (family_index > 0 && rest.size() == 3 && all_digits(rest)) {
		spec.series = airfoil_series::six;
		spec.six_family = family_index;
		spec.six_a = family_index >= 6;
		spec.cl = static_cast<double>(rest[0] - '0') / 10.0;
		spec.thickness = ((rest[1] - '0') * 10 + (rest[2] - '0')) / 100.0;
		if (spec.thickness <= 0.0)
		    throw std::runtime_error("NACA thickness must be positive");
		if (spec.six_a_param < 0.0 || spec.six_a_param > 1.0)
		    throw std::runtime_error("6-series mean-line loading parameter must be in [0, 1]");
		return spec;
	    }
	}

	throw std::runtime_error("NACA code must be 4-digit, 5-digit, or 6-series such as 63-215/64A210");
    }
    for (char c : digits) {
	if (c < '0' || c > '9') {
	    throw std::runtime_error("NACA airfoil code must contain only digits");
	}
    }

    spec.series = airfoil_series::four;
    spec.four.camber = (digits[0] - '0') / 100.0;
    spec.four.camber_pos = (digits[1] - '0') / 10.0;
    spec.four.thickness = ((digits[2] - '0') * 10 + (digits[3] - '0')) / 100.0;
    spec.thickness = spec.four.thickness;

    if (spec.four.thickness <= 0.0) {
	throw std::runtime_error("NACA thickness must be positive");
    }
    if (spec.four.camber > 0.0 && spec.four.camber_pos <= 0.0) {
	throw std::runtime_error("cambered 4-digit NACA sections require nonzero camber position");
    }

    return spec;
}


static struct bu_opt_desc *
option_desc(options &opts, struct bu_vls &outfile, struct bu_vls &name, struct bu_vls &airfoil, struct bu_vls &tip_airfoil, struct bu_vls &mode, struct bu_vls &brep_surface, struct bu_vls &demo_file, struct bu_vls &section_file, bool &help)
{
    static struct bu_opt_desc d[26];

    BU_OPT(d[0], "h", "help", "", NULL, &help, "Print help and exit");
    BU_OPT(d[1], "?", "", "", NULL, &help, "");
    BU_OPT(d[2], "o", "output", "file.g", &bu_opt_vls, &outfile, "Output .g file for a single wing");
    BU_OPT(d[3], "n", "name", "name", &bu_opt_vls, &name, "Base object name");
    BU_OPT(d[4], "a", "airfoil", "code", &bu_opt_vls, &airfoil, "Root NACA airfoil code");
    BU_OPT(d[5], "A", "tip-airfoil", "code", &bu_opt_vls, &tip_airfoil, "Tip NACA airfoil code");
    BU_OPT(d[6], "m", "mode", "bot|brep", &bu_opt_vls, &mode, "Output geometry type");
    BU_OPT(d[7], "", "brep-surface", "ruled|smooth", &bu_opt_vls, &brep_surface, "BREP side-surface construction mode");
    BU_OPT(d[8], "s", "span", "length", &bu_opt_fastf_t, &opts.span, "Semi-span length");
    BU_OPT(d[9], "r", "root-chord", "length", &bu_opt_fastf_t, &opts.root_chord, "Root chord length");
    BU_OPT(d[10], "t", "tip-chord", "length", &bu_opt_fastf_t, &opts.tip_chord, "Tip chord length");
    BU_OPT(d[11], "w", "sweep", "offset", &bu_opt_fastf_t, &opts.sweep, "Tip leading-edge X offset");
    BU_OPT(d[12], "d", "dihedral", "degrees", &bu_opt_fastf_t, &opts.dihedral_deg, "Tip dihedral angle");
    BU_OPT(d[13], "T", "tip-twist", "degrees", &bu_opt_fastf_t, &opts.tip_twist_deg, "Tip twist angle");
    BU_OPT(d[14], "u", "six-a", "a", &bu_opt_fastf_t, &opts.six_a_param, "6-series mean-line loading parameter");
    BU_OPT(d[15], "S", "stations", "count", &bu_opt_int, &opts.stations, "Spanwise station count");
    BU_OPT(d[16], "c", "samples", "count", &bu_opt_int, &opts.samples, "Chordwise sample count");
    BU_OPT(d[17], "x", "x-offset", "offset", &bu_opt_fastf_t, &opts.offset_x, "Model X offset");
    BU_OPT(d[18], "y", "y-offset", "offset", &bu_opt_fastf_t, &opts.offset_y, "Model Y offset");
    BU_OPT(d[19], "z", "z-offset", "offset", &bu_opt_fastf_t, &opts.offset_z, "Model Z offset");
    BU_OPT(d[20], "p", "sharp-te", "", NULL, &opts.sharp_te, "Use sharp trailing-edge coefficient for 4/5-series thickness");
    BU_OPT(d[21], "f", "force", "", NULL, &opts.overwrite, "Overwrite output file");
    BU_OPT(d[22], "j", "append", "", NULL, &opts.append, "Append objects to an existing output .g file");
    BU_OPT(d[23], "", "demo-file", "file.g", &bu_opt_vls, &demo_file, "Write a 36-wing BoT/BREP sampler database");
    BU_OPT(d[24], "", "section-file", "file.tsv", &bu_opt_vls, &section_file, "Write normalized section coordinates for regression/reference checks");
    BU_OPT_NULL(d[25]);

    return d;
}


static options
parse_args(int argc, char **argv)
{
    options opts;
    struct bu_vls outfile = BU_VLS_INIT_ZERO;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls airfoil = BU_VLS_INIT_ZERO;
    struct bu_vls tip_airfoil = BU_VLS_INIT_ZERO;
    struct bu_vls mode = BU_VLS_INIT_ZERO;
    struct bu_vls brep_surface = BU_VLS_INIT_ZERO;
    struct bu_vls demo_file = BU_VLS_INIT_ZERO;
    struct bu_vls section_file = BU_VLS_INIT_ZERO;
    bool help = false;
    struct bu_opt_desc *d = option_desc(opts, outfile, name, airfoil, tip_airfoil, mode, brep_surface, demo_file, section_file, help);
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const int parsed = bu_opt_parse(&msg, argc - 1, (const char **)(argv + 1), d);

    if (parsed < 0) {
	const char *err = bu_vls_addr(&msg);
	std::string msg_str = err ? err : "";
	bu_vls_free(&msg);
	bu_vls_free(&outfile);
	bu_vls_free(&name);
	bu_vls_free(&airfoil);
	bu_vls_free(&tip_airfoil);
	bu_vls_free(&mode);
	bu_vls_free(&brep_surface);
	bu_vls_free(&demo_file);
	bu_vls_free(&section_file);
	throw std::runtime_error(msg_str.empty() ? "option parsing failed" : msg_str);
    }
    bu_vls_free(&msg);

    if (help) {
	usage(argv[0], d);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    if (bu_vls_strlen(&outfile) > 0)
	opts.outfile = bu_vls_cstr(&outfile);
    if (bu_vls_strlen(&name) > 0)
	opts.name = bu_vls_cstr(&name);
    if (bu_vls_strlen(&airfoil) > 0)
	opts.airfoil = bu_vls_cstr(&airfoil);
    if (bu_vls_strlen(&tip_airfoil) > 0)
	opts.tip_airfoil = bu_vls_cstr(&tip_airfoil);
    if (bu_vls_strlen(&demo_file) > 0)
	opts.demo_file = bu_vls_cstr(&demo_file);
    if (bu_vls_strlen(&section_file) > 0)
	opts.section_file = bu_vls_cstr(&section_file);
    std::string mode_str = bu_vls_strlen(&mode) > 0 ? bu_vls_cstr(&mode) : "bot";
    std::string brep_surface_str = bu_vls_strlen(&brep_surface) > 0 ? bu_vls_cstr(&brep_surface) : "ruled";

    bu_vls_free(&outfile);
    bu_vls_free(&name);
    bu_vls_free(&airfoil);
    bu_vls_free(&tip_airfoil);
    bu_vls_free(&mode);
    bu_vls_free(&brep_surface);
    bu_vls_free(&demo_file);
    bu_vls_free(&section_file);

    if (parsed != 0) {
	throw std::runtime_error("unexpected positional arguments");
    }
    if (mode_str == "bot") {
	opts.mode = output_mode::bot;
    } else if (mode_str == "brep") {
	opts.mode = output_mode::brep;
    } else {
	throw std::runtime_error("output mode must be bot or brep");
    }
    if (brep_surface_str == "ruled") {
	opts.brep_surface = brep_surface_mode::ruled;
    } else if (brep_surface_str == "smooth") {
	opts.brep_surface = brep_surface_mode::smooth;
    } else {
	throw std::runtime_error("BREP surface mode must be ruled or smooth");
    }
    if (opts.span <= 0.0 || opts.root_chord <= 0.0 || opts.tip_chord <= 0.0) {
	throw std::runtime_error("span and chord lengths must be positive");
    }
    if (opts.stations < 2) {
	throw std::runtime_error("station count must be at least 2");
    }
    if (opts.samples < 5) {
	throw std::runtime_error("chordwise sample count must be at least 5");
    }
    if (opts.name.empty()) {
	throw std::runtime_error("object name must not be empty");
    }
    if (opts.append && opts.overwrite) {
	throw std::runtime_error("-j append and -f overwrite are mutually exclusive");
    }

    if (!opts.demo_file.empty()) {
	if (opts.append)
	    throw std::runtime_error("--demo-file cannot be combined with append mode");
	if (!opts.section_file.empty())
	    throw std::runtime_error("--demo-file and --section-file are mutually exclusive");
	return opts;
    }

    parse_airfoil(opts.airfoil, opts.six_a_param);
    if (!opts.tip_airfoil.empty())
	parse_airfoil(opts.tip_airfoil, opts.six_a_param);
    if (!opts.section_file.empty()) {
	if (opts.append)
	    throw std::runtime_error("--section-file cannot be combined with append mode");
	if (!opts.tip_airfoil.empty())
	    throw std::runtime_error("--section-file writes one airfoil at a time");
    }
    return opts;
}


static double
thickness4(double x, const airfoil4 &af, bool sharp_te)
{
    const double a0 = 0.2969;
    const double a1 = -0.1260;
    const double a2 = -0.3516;
    const double a3 = 0.2843;
    const double a4 = sharp_te ? -0.1036 : -0.1015;

    return 5.0 * af.thickness * (a0 * std::sqrt(x) + x * (a1 + x * (a2 + x * (a3 + x * a4))));
}


static void
mean_line4(double x, const airfoil4 &af, double &yc, double &dyc_dx)
{
    yc = 0.0;
    dyc_dx = 0.0;

    if (af.camber <= 0.0 || af.camber_pos <= 0.0)
	return;

    if (x < af.camber_pos) {
	const double p2 = af.camber_pos * af.camber_pos;
	yc = af.camber / p2 * (2.0 * af.camber_pos * x - x * x);
	dyc_dx = 2.0 * af.camber / p2 * (af.camber_pos - x);
    } else {
	const double omp = 1.0 - af.camber_pos;
	const double omp2 = omp * omp;
	yc = af.camber / omp2 * ((1.0 - 2.0 * af.camber_pos) + 2.0 * af.camber_pos * x - x * x);
	dyc_dx = 2.0 * af.camber / omp2 * (af.camber_pos - x);
    }
}


static double
linear_table_lookup(const std::vector<double> &xs, const std::vector<double> &ys, double x)
{
    if (xs.size() != ys.size() || xs.empty())
	throw std::runtime_error("invalid interpolation table");

    if (x <= xs.front())
	return ys.front();
    if (x >= xs.back())
	return ys.back();

    auto it = std::upper_bound(xs.begin(), xs.end(), x);
    const size_t hi = static_cast<size_t>(it - xs.begin());
    const size_t lo = hi - 1;
    const double t = (x - xs[lo]) / (xs[hi] - xs[lo]);
    return ys[lo] + t * (ys[hi] - ys[lo]);
}


static void
mean_line5(double x, const airfoil_spec &af, double &yc, double &dyc_dx)
{
    static const std::vector<double> m = {0.05, 0.10, 0.15, 0.20, 0.25};
    static const std::vector<double> rtab = {0.0580, 0.126, 0.2025, 0.29, 0.391};
    static const std::vector<double> ktab = {361.4, 51.64, 15.957, 6.643, 3.23};
    static const std::vector<double> mr = {0.10, 0.15, 0.20, 0.25};
    static const std::vector<double> rrtab = {0.13, 0.217, 0.318, 0.441};
    static const std::vector<double> krtab = {51.99, 15.793, 6.52, 3.191};

    if (!af.reflex) {
	const double r = linear_table_lookup(m, rtab, af.xmaxc);
	const double k1 = linear_table_lookup(m, ktab, af.xmaxc);
	const double r2 = r * r;
	const double r3 = r2 * r;

	if (x < r) {
	    yc = x * (x * (x - 3.0 * r) + r2 * (3.0 - r));
	    dyc_dx = 3.0 * x * (x - r - r) + r2 * (3.0 - r);
	} else {
	    yc = r3 * (1.0 - x);
	    dyc_dx = -r3;
	}

	yc *= k1 * af.cl / 1.8;
	dyc_dx *= k1 * af.cl / 1.8;
	return;
    }

    const double r = linear_table_lookup(mr, rrtab, af.xmaxc);
    const double k1 = linear_table_lookup(mr, krtab, af.xmaxc);
    const double r3 = r * r * r;
    const double mr3 = (1.0 - r) * (1.0 - r) * (1.0 - r);
    const double k21 = (3.0 * (r - af.xmaxc) * (r - af.xmaxc) - r3) / mr3;

    if (x < r) {
	yc = (x - r) * (x - r) * (x - r) - k21 * mr3 * x - x * r3 + r3;
	dyc_dx = 3.0 * (x - r) * (x - r) - k21 * mr3 - r3;
    } else {
	yc = k21 * (x - r) * (x - r) * (x - r) - k21 * mr3 * x - x * r3 + r3;
	dyc_dx = 3.0 * k21 * (x - r) * (x - r) - k21 * mr3 - r3;
    }

    yc *= k1 * af.cl / 1.8;
    dyc_dx *= k1 * af.cl / 1.8;
}


static void
mean_line6(double x, const airfoil_spec &af, double &yc, double &dyc_dx)
{
    const double eps = 1.0e-7;
    const double pi = M_PI;

    yc = 0.0;
    dyc_dx = 0.0;

    if (std::fabs(af.cl) <= eps)
	return;

    if (af.six_a) {
	const double a = 0.8;
	const double te_slope = -0.24521 * af.cl;
	if (x < eps)
	    return;
	if (1.0 - x < eps) {
	    dyc_dx = te_slope;
	    return;
	}

	const double oma = 1.0 - a;
	const double g = -(a * a * (0.5 * std::log(a) - 0.25) + 0.25) / oma;
	const double h = g + (0.5 * oma * oma * std::log(oma) - 0.25 * oma * oma) / oma;
	const double amx = a - x;
	const double term1 = std::fabs(amx) < eps ? 0.0 : amx * amx * (2.0 * std::log(std::fabs(amx)) - 1.0);
	const double term1p = std::fabs(amx) < eps ? 0.0 : -amx * std::log(std::fabs(amx));
	const double omx = 1.0 - x;
	const double term2 = omx * omx * (1.0 - 2.0 * std::log(omx));
	const double term2p = omx * std::log(omx);

	yc = 0.25 * (term1 + term2) / oma - x * std::log(x) + g - h * x;
	dyc_dx = (term1p + term2p) / oma - 1.0 - std::log(x) - h;
	yc *= af.cl * 0.97948 / (2.0 * pi * (a + 1.0));
	dyc_dx *= af.cl * 0.97948 / (2.0 * pi * (a + 1.0));

	if (x > 0.86) {
	    yc = te_slope * (x - 1.0);
	    dyc_dx = te_slope;
	}
	return;
    }

    const double a = af.six_a_param;
    const double oma = 1.0 - a;
    if (std::fabs(oma) < eps) {
	const double omx = 1.0 - x;
	if (x < eps || omx < eps)
	    return;
	yc = -((omx * std::log(omx) + x * std::log(x)) * af.cl * (0.25 / pi));
	dyc_dx = (std::log(omx) - std::log(x)) * af.cl * (0.25 / pi);
	return;
    }

    const double omx = 1.0 - x;
    if (x < eps || std::fabs(omx) < eps)
	return;

    double g = -0.25;
    double h = -0.5;
    if (std::fabs(a) >= eps) {
	g = -(a * a * (0.5 * std::log(a) - 0.25) + 0.25) / oma;
	h = g + (0.5 * oma * oma * std::log(oma) - 0.25 * oma * oma) / oma;
    }

    const double amx = a - x;
    const double term1 = std::fabs(amx) < eps ? 0.0 : amx * amx * (2.0 * std::log(std::fabs(amx)) - 1.0);
    const double term1p = std::fabs(amx) < eps ? 0.0 : -amx * std::log(std::fabs(amx));
    const double term2 = omx * omx * (1.0 - 2.0 * std::log(omx));
    const double term2p = omx * std::log(omx);

    yc = 0.25 * (term1 + term2) / oma - x * std::log(x) + g - h * x;
    dyc_dx = (term1p + term2p) / oma - 1.0 - std::log(x) - h;
    yc *= af.cl / (2.0 * pi * (a + 1.0));
    dyc_dx *= af.cl / (2.0 * pi * (a + 1.0));
}


static double
polynomial(const std::array<double, 5> &coeff, double x)
{
    double val = coeff.back();
    for (int i = static_cast<int>(coeff.size()) - 2; i >= 0; --i)
	val = val * x + coeff[static_cast<size_t>(i)];
    return val;
}


static double
six_scale_factor(int family, double tc)
{
    static const std::array<std::array<double, 5>, 8> coeff = {{
	{{0.0, 8.1827699, 1.3776209, -0.092851684, 7.5942563}},
	{{0.0, 4.6535511, 1.038063, -1.5041794, 4.7882784}},
	{{0.0, 6.5718716, 0.49376292, 0.7319794, 1.9491474}},
	{{0.0, 6.7581414, 0.19253769, 0.81282621, 0.85202897}},
	{{0.0, 6.627289, 0.098965859, 0.96759774, 0.90537584}},
	{{0.0, 8.1845925, 1.0492569, 1.31150930, 4.4515579}},
	{{0.0, 8.2125018, 0.76855961, 1.4922345, 3.6130133}},
	{{0.0, 8.2514822, 0.46569361, 1.50113018, 2.0908904}}
    }};

    if (family < 1 || family > 8 || tc <= 0.0)
	throw std::runtime_error("invalid 6-series family or thickness");

    return polynomial(coeff[static_cast<size_t>(family - 1)], tc);
}


struct six_family_table {
    std::array<double, 21> eps;
    std::array<double, 21> psi;
    std::array<double, 21> deps;
    std::array<double, 21> dpsi;
};


static const std::array<six_family_table, 8> &
six_tables()
{
    static const std::array<six_family_table, 8> tables = {{
	{{{0.00000, 0.01367, 0.01661, 0.01884, 0.02268, 0.02801, 0.03465, 0.04237, 0.05098, 0.06011, 0.06832, 0.07464, 0.07848, 0.07938, 0.07685, 0.07070, 0.06103, 0.04834, 0.03333, 0.01698, 0.00000}},
	 {{0.15066, 0.14497, 0.13753, 0.13576, 0.13547, 0.13511, 0.13414, 0.13194, 0.12808, 0.12160, 0.11218, 0.10029, 0.08657, 0.07153, 0.05591, 0.04065, 0.02682, 0.01527, 0.00680, 0.00169, 0.00000}},
	 {{0.10430, 0.05153, 0.00681, 0.01998, 0.02920, 0.03837, 0.04595, 0.05211, 0.05751, 0.05667, 0.04699, 0.03286, 0.01562, -0.00479, -0.02757, -0.05069, -0.07181, -0.08912, -0.10075, -0.10680, -0.10860}},
	 {{0.00000, -0.05528, -0.02966, -0.00199, -0.00173, -0.00351, -0.00963, -0.01852, -0.03205, -0.05077, -0.06853, -0.08210, -0.09217, -0.09850, -0.09941, -0.09362, -0.08169, -0.06433, -0.04335, -0.02163, 0.00000}}},
	{{{0.00000, 0.02035, 0.02812, 0.03090, 0.03538, 0.04250, 0.05208, 0.06395, 0.07808, 0.09462, 0.11125, 0.12386, 0.13182, 0.13459, 0.13166, 0.12261, 0.10735, 0.08645, 0.06052, 0.03113, 0.00000}},
	 {{0.25269, 0.24730, 0.23325, 0.22818, 0.22730, 0.22751, 0.22736, 0.22584, 0.22203, 0.21449, 0.19966, 0.17950, 0.15596, 0.13024, 0.10338, 0.07671, 0.05193, 0.03051, 0.01385, 0.00353, 0.00000}},
	 {{0.14800, 0.09163, 0.02253, 0.01975, 0.03711, 0.05334, 0.06847, 0.08243, 0.09837, 0.10984, 0.09576, 0.06556, 0.03485, -0.00004, -0.03776, -0.07771, -0.11568, -0.15018, -0.17797, -0.19447, -0.20000}},
	 {{0.00000, -0.07538, -0.06976, -0.01074, -0.00091, 0.00159, -0.00432, -0.01621, -0.03263, -0.07005, -0.11441, -0.14057, -0.15792, -0.16855, -0.17207, -0.16552, -0.14847, -0.12295, -0.08699, -0.04438, 0.00000}}},
	{{{0.00000, 0.01515, 0.01943, 0.01715, 0.01821, 0.02211, 0.02772, 0.03510, 0.04404, 0.05467, 0.06653, 0.07771, 0.08614, 0.09017, 0.08982, 0.08427, 0.07368, 0.05228, 0.02939, 0.01302, 0.00000}},
	 {{0.17464, 0.16808, 0.15523, 0.15235, 0.15350, 0.15536, 0.15678, 0.15731, 0.15653, 0.15393, 0.14779, 0.13680, 0.12154, 0.10353, 0.08401, 0.06385, 0.04422, 0.02590, 0.01154, 0.00289, 0.00000}},
	 {{0.10460, 0.06791, -0.00515, -0.00912, 0.01835, 0.03047, 0.04140, 0.05202, 0.06220, 0.07293, 0.07561, 0.06468, 0.04022, 0.01243, -0.01965, -0.04651, -0.10257, -0.15418, -0.12661, -0.08921, -0.07785}},
	 {{0.00000, -0.07862, -0.05623, 0.00311, 0.01077, 0.01132, 0.00659, -0.00045, -0.00957, -0.02581, -0.05410, -0.08493, -0.10751, -0.12044, -0.12748, -0.12746, -0.12261, -0.10688, -0.07399, -0.03660, 0.00000}}},
	{{{0.00000, 0.01340, 0.01940, 0.01450, 0.01350, 0.01530, 0.01890, 0.02390, 0.03020, 0.03770, 0.04670, 0.05730, 0.07020, 0.08180, 0.08850, 0.08940, 0.08390, 0.07190, 0.05290, 0.02810, 0.00000}},
	 {{0.16457, 0.16037, 0.14604, 0.14236, 0.14366, 0.14601, 0.14790, 0.14917, 0.14961, 0.14908, 0.14725, 0.14360, 0.13615, 0.12108, 0.10180, 0.07989, 0.05733, 0.03566, 0.01705, 0.00439, 0.00000}},
	 {{0.09184, 0.07101, -0.00538, -0.02849, 0.00665, 0.01719, 0.02775, 0.03608, 0.04375, 0.05249, 0.06143, 0.07614, 0.08282, 0.06048, 0.02475, -0.01433, -0.05527, -0.09880, -0.14157, -0.17144, -0.18300}},
	 {{0.00000, -0.07127, -0.06881, 0.00254, 0.01318, 0.01443, 0.01006, 0.00567, -0.00008, -0.00709, -0.01666, -0.03095, -0.07154, -0.11299, -0.13253, -0.14356, -0.14253, -0.13104, -0.10260, -0.05576, 0.00000}}},
	{{{0.00000, 0.01591, 0.02259, 0.02126, 0.01979, 0.01997, 0.02135, 0.02386, 0.02752, 0.03232, 0.03849, 0.04622, 0.05610, 0.06814, 0.08009, 0.08659, 0.08602, 0.06908, 0.04301, 0.02049, 0.00000}},
	 {{0.18028, 0.17403, 0.16085, 0.15278, 0.15048, 0.14982, 0.15002, 0.15046, 0.15095, 0.15110, 0.15076, 0.14945, 0.14628, 0.13974, 0.12560, 0.10386, 0.07840, 0.04935, 0.02327, 0.00615, 0.00000}},
	 {{0.10740, 0.07861, 0.00960, -0.01482, -0.00378, 0.00531, 0.01233, 0.01968, 0.02679, 0.03473, 0.04380, 0.05554, 0.07036, 0.08166, 0.06117, 0.02604, -0.05207, -0.15219, -0.16061, -0.13335, -0.12740}},
	 {{0.00000, -0.07383, -0.07578, -0.02892, -0.00661, -0.00118, 0.00255, 0.00322, 0.00235, -0.00039, -0.00442, -0.01344, -0.02739, -0.06246, -0.11774, -0.15184, -0.17636, -0.18379, -0.14138, -0.07576, 0.00000}}},
	{{{0.00000, 0.01405, 0.01751, 0.01813, 0.02134, 0.02569, 0.03125, 0.03759, 0.04496, 0.05243, 0.05921, 0.06422, 0.06714, 0.06720, 0.06441, 0.05896, 0.05152, 0.03959, 0.02589, 0.01279, 0.00000}},
	 {{0.15517, 0.14634, 0.13753, 0.13521, 0.13468, 0.13420, 0.13322, 0.13131, 0.12792, 0.12228, 0.11416, 0.10395, 0.09221, 0.07941, 0.06650, 0.05441, 0.04315, 0.03186, 0.02464, 0.02248, 0.02243}},
	 {{0.09522, 0.05936, 0.00176, 0.01151, 0.02535, 0.03149, 0.03797, 0.04390, 0.04828, 0.04641, 0.03824, 0.02582, 0.00995, -0.00869, -0.02732, -0.03940, -0.06125, -0.08553, -0.08613, -0.08180, -0.08114}},
	 {{0.00000, -0.07598, -0.03297, -0.00471, -0.00261, -0.00413, -0.00874, -0.01609, -0.02811, -0.04394, -0.05895, -0.07036, -0.07884, -0.08295, -0.08039, -0.07297, -0.07370, -0.06291, -0.02818, -0.00351, 0.00000}}},
	{{{0.00000, 0.01507, 0.01899, 0.01846, 0.02010, 0.02306, 0.02749, 0.03295, 0.03984, 0.04772, 0.05554, 0.06147, 0.06508, 0.06586, 0.06376, 0.05881, 0.05168, 0.03997, 0.02630, 0.01304, 0.00000}},
	 {{0.15365, 0.14473, 0.13407, 0.13038, 0.12949, 0.12942, 0.12925, 0.12841, 0.12652, 0.12281, 0.11554, 0.10568, 0.09416, 0.08147, 0.06848, 0.05604, 0.04450, 0.03288, 0.02524, 0.02266, 0.02243}},
	 {{0.10100, 0.06557, -0.00058, 0.00152, 0.01572, 0.02346, 0.03158, 0.03912, 0.04780, 0.05175, 0.04503, 0.03072, 0.01429, -0.00402, -0.02342, -0.03693, -0.05956, -0.08464, -0.08661, -0.08324, -0.08273}},
	 {{0.00000, -0.08211, -0.04550, -0.00997, -0.00211, 0.00007, -0.00276, -0.00833, -0.01605, -0.03440, -0.05603, -0.06863, -0.07777, -0.08268, -0.08196, -0.07514, -0.07546, -0.06533, -0.03105, -0.00565, 0.00000}}},
	{{{0.00000, 0.01413, 0.01804, 0.01642, 0.01545, 0.01708, 0.02075, 0.02522, 0.03122, 0.03806, 0.04610, 0.05356, 0.05931, 0.06227, 0.06254, 0.05948, 0.05343, 0.04234, 0.02847, 0.01430, 0.00000}},
	 {{0.14761, 0.13785, 0.12892, 0.12219, 0.12279, 0.12387, 0.12472, 0.12502, 0.12448, 0.12266, 0.11859, 0.11122, 0.10107, 0.08921, 0.07622, 0.06275, 0.04985, 0.03695, 0.02763, 0.02341, 0.02242}},
	 {{0.09403, 0.06306, -0.00175, -0.01234, 0.00163, 0.01841, 0.02595, 0.03326, 0.04099, 0.04800, 0.05120, 0.04325, 0.02811, 0.01066, -0.00906, -0.02769, -0.05416, -0.08301, -0.09051, -0.09048, -0.09132}},
	 {{0.00000, -0.07655, -0.05077, -0.01947, 0.01158, 0.00524, 0.00431, -0.00053, -0.00678, -0.01742, -0.03604, -0.05689, -0.07099, -0.07951, -0.08556, -0.08361, -0.08364, -0.07457, -0.04244, -0.01427, 0.00000}}}
    }};

    return tables;
}


static double
hermite21(const std::array<double, 21> &values, const std::array<double, 21> &derivs, double phi)
{
    const double h = M_PI / 20.0;
    if (phi <= 0.0)
	return values[0];
    if (phi >= M_PI)
	return values[20];

    int i = static_cast<int>(std::floor(phi / h));
    if (i < 0)
	i = 0;
    if (i > 19)
	i = 19;

    const double local = (phi - static_cast<double>(i) * h) / h;
    const double t2 = local * local;
    const double t3 = t2 * local;
    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + local;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;

    return h00 * values[static_cast<size_t>(i)] +
	h10 * h * derivs[static_cast<size_t>(i)] +
	h01 * values[static_cast<size_t>(i + 1)] +
	h11 * h * derivs[static_cast<size_t>(i + 1)];
}


static std::vector<point2d>
six_thickness_points(int family, double toc)
{
    const six_family_table &table = six_tables()[static_cast<size_t>(family - 1)];
    const double sf = six_scale_factor(family, toc);
    const int count = 401;
    const double psi0 = sf * table.psi[0];

    std::vector<point2d> points;
    points.reserve(static_cast<size_t>(count));

    std::complex<double> zeta0;
    std::complex<double> zeta_te;
    std::vector<std::complex<double>> zeta_values;
    zeta_values.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
	const double phi = M_PI * static_cast<double>(i) / static_cast<double>(count - 1);
	const double eps = sf * hermite21(table.eps, table.deps, phi);
	const double psi = sf * hermite21(table.psi, table.dpsi, phi);
	const std::complex<double> z = std::exp(std::complex<double>(psi0, phi));
	const std::complex<double> zprime = z * std::exp(std::complex<double>(psi - psi0, -eps));
	const std::complex<double> zeta = zprime + 1.0 / zprime;
	if (i == 0)
	    zeta0 = zeta;
	if (i == count - 1)
	    zeta_te = zeta;
	zeta_values.push_back(zeta);
    }

    const double scale = std::abs(zeta_te - zeta0);
    if (scale <= SMALL_FASTF)
	throw std::runtime_error("invalid 6-series conformal mapping scale");

    for (const std::complex<double> &zeta : zeta_values) {
	const std::complex<double> zfinal = (zeta0 - zeta) / scale;
	points.push_back({std::clamp(zfinal.real(), 0.0, 1.0), std::max(0.0, -zfinal.imag())});
    }

    std::sort(points.begin(), points.end(), [](const point2d &a, const point2d &b) {
	return a.x < b.x;
    });
    points.front().x = 0.0;
    points.front().z = 0.0;
    points.back().x = 1.0;
    return points;
}


static double
thickness6(double x, const std::vector<point2d> &points)
{
    if (x <= 0.0)
	return 0.0;
    if (x >= 1.0)
	return points.back().z;

    auto it = std::upper_bound(points.begin(), points.end(), x, [](double val, const point2d &pt) {
	return val < pt.x;
    });
    if (it == points.begin())
	return it->z;
    if (it == points.end())
	return points.back().z;

    const point2d &hi = *it;
    const point2d &lo = *(it - 1);
    const double dx = hi.x - lo.x;
    if (std::fabs(dx) <= SMALL_FASTF)
	return lo.z;
    const double t = (x - lo.x) / dx;
    return lo.z + t * (hi.z - lo.z);
}


static double
thickness6(double x, const airfoil_spec &af)
{
    const std::vector<point2d> points = six_thickness_points(af.six_family, af.thickness);
    return thickness6(x, points);
}


static void
mean_line_for_airfoil(double x, const airfoil_spec &af, double &yc, double &dyc_dx)
{
    yc = 0.0;
    dyc_dx = 0.0;

    switch (af.series) {
	case airfoil_series::four:
	    mean_line4(x, af.four, yc, dyc_dx);
	    break;
	case airfoil_series::five:
	    mean_line5(x, af, yc, dyc_dx);
	    break;
	case airfoil_series::six:
	    mean_line6(x, af, yc, dyc_dx);
	    break;
    }
}


static double
thickness_for_airfoil(double x, const airfoil_spec &af, bool sharp_te)
{
    switch (af.series) {
	case airfoil_series::four:
	    return thickness4(x, af.four, sharp_te);
	case airfoil_series::five: {
	    airfoil4 af4;
	    af4.thickness = af.thickness;
	    return thickness4(x, af4, sharp_te);
	}
	case airfoil_series::six:
	    return thickness6(x, af);
    }

    return 0.0;
}


static std::vector<point2d>
airfoil_outline(const section4 &section, bool omit_lower_te)
{
    const int samples = static_cast<int>(section.upper.size());
    std::vector<point2d> outline;
    outline.reserve(static_cast<size_t>(2 * samples - 1));

    for (int i = samples - 1; i >= 0; --i)
	outline.push_back(section.upper[static_cast<size_t>(i)]);

    const int lower_end = omit_lower_te ? samples - 1 : samples;

    for (int i = 1; i < lower_end; ++i)
	outline.push_back(section.lower[static_cast<size_t>(i)]);

    return outline;
}


static bool
section_has_duplicate_te(const section4 &section)
{
    const point2d &upper_te = section.upper.back();
    const point2d &lower_te = section.lower.back();
    const double te_dx = upper_te.x - lower_te.x;
    const double te_dz = upper_te.z - lower_te.z;
    return (te_dx * te_dx + te_dz * te_dz) < 1.0e-20;
}


static section4
airfoil_section(const airfoil_spec &af, int samples, bool sharp_te)
{
    section4 section;
    section.upper.reserve(static_cast<size_t>(samples));
    section.lower.reserve(static_cast<size_t>(samples));

    std::vector<point2d> six_points;
    if (af.series == airfoil_series::six)
	six_points = six_thickness_points(af.six_family, af.thickness);

    for (int i = 0; i < samples; ++i) {
	const double beta = M_PI * static_cast<double>(i) / static_cast<double>(samples - 1);
	const double x = 0.5 * (1.0 - std::cos(beta));
	const double yt = af.series == airfoil_series::six ? thickness6(x, six_points) : thickness_for_airfoil(x, af, sharp_te);
	double yc = 0.0;
	double dyc_dx = 0.0;
	mean_line_for_airfoil(x, af, yc, dyc_dx);
	const double theta = std::atan(dyc_dx);
	const double st = std::sin(theta);
	const double ct = std::cos(theta);

	section.upper.push_back({x - yt * st, yc + yt * ct});
	section.lower.push_back({x + yt * st, yc - yt * ct});
    }

    return section;
}


static const std::vector<double> &
classic_pdas_x_grid()
{
    static const std::vector<double> grid = {
	0.0, 0.005, 0.0075, 0.0125, 0.025, 0.05, 0.075, 0.10,
	0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50,
	0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90,
	0.95, 1.0
    };
    return grid;
}


static const std::vector<double> &
dense_pdas_x_grid()
{
    static const std::vector<double> grid = {
	0.0, 0.0002, 0.0005, 0.001, 0.0015, 0.002, 0.005,
	0.01, 0.015, 0.02, 0.03, 0.04, 0.05, 0.06, 0.08,
	0.10, 0.12, 0.14, 0.16, 0.18, 0.20, 0.22, 0.24,
	0.26, 0.28, 0.30, 0.32, 0.34, 0.36, 0.38, 0.40,
	0.42, 0.44, 0.46, 0.48, 0.50, 0.52, 0.54, 0.56,
	0.58, 0.60, 0.62, 0.64, 0.66, 0.68, 0.70, 0.72,
	0.74, 0.76, 0.78, 0.80, 0.82, 0.84, 0.86, 0.88,
	0.90, 0.92, 0.94, 0.96, 0.97, 0.98, 0.99, 0.995,
	1.0
    };
    return grid;
}


static section4
airfoil_section_at_x_grid(const airfoil_spec &af, const std::vector<double> &x_grid, bool sharp_te)
{
    section4 section;
    section.upper.reserve(x_grid.size());
    section.lower.reserve(x_grid.size());

    std::vector<point2d> six_points;
    if (af.series == airfoil_series::six)
	six_points = six_thickness_points(af.six_family, af.thickness);

    for (double x : x_grid) {
	const double yt = af.series == airfoil_series::six ? thickness6(x, six_points) : thickness_for_airfoil(x, af, sharp_te);
	double yc = 0.0;
	double dyc_dx = 0.0;
	mean_line_for_airfoil(x, af, yc, dyc_dx);
	const double theta = std::atan(dyc_dx);
	const double st = std::sin(theta);
	const double ct = std::cos(theta);

	section.upper.push_back({x - yt * st, yc + yt * ct});
	section.lower.push_back({x + yt * st, yc - yt * ct});
    }

    return section;
}


static int
write_section_file(const options &opts)
{
    if (bu_file_exists(opts.section_file.c_str(), NULL) && !opts.overwrite)
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists. Use -f to overwrite.\n", opts.section_file.c_str());

    const airfoil_spec af = parse_airfoil(opts.airfoil, opts.six_a_param);
    const std::vector<double> &x_grid = af.six_a ? dense_pdas_x_grid() : classic_pdas_x_grid();
    const section4 section = airfoil_section_at_x_grid(af, x_grid, opts.sharp_te);

    FILE *fp = fopen(opts.section_file.c_str(), "wb");
    if (!fp)
	bu_exit(EXIT_FAILURE, "ERROR: unable to open %s\n", opts.section_file.c_str());

    fprintf(fp, "# PDAS NACA456 combined-thickness-and-camber comparison format\n");
    fprintf(fp, "# airfoil xupper yupper xlower ylower\n");
    for (size_t i = 0; i < section.upper.size(); ++i) {
	fprintf(fp, "%s %.6f %.6f %.6f %.6f\n",
	    opts.airfoil.c_str(),
	    section.upper[i].x, section.upper[i].z,
	    section.lower[i].x, section.lower[i].z);
    }

    if (fclose(fp) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to close %s\n", opts.section_file.c_str());

    bu_log("Wrote %s with %zu normalized section points\n", opts.section_file.c_str(), section.upper.size());
    return EXIT_SUCCESS;
}


static section4
interpolate_section(const section4 &root, const section4 &tip, double f)
{
    if (root.upper.size() != tip.upper.size() || root.lower.size() != tip.lower.size()) {
	throw std::runtime_error("airfoil interpolation requires matched section sample counts");
    }

    section4 section;
    section.upper.reserve(root.upper.size());
    section.lower.reserve(root.lower.size());

    for (size_t i = 0; i < root.upper.size(); ++i) {
	section.upper.push_back({
	    root.upper[i].x + f * (tip.upper[i].x - root.upper[i].x),
	    root.upper[i].z + f * (tip.upper[i].z - root.upper[i].z)
	});
	section.lower.push_back({
	    root.lower[i].x + f * (tip.lower[i].x - root.lower[i].x),
	    root.lower[i].z + f * (tip.lower[i].z - root.lower[i].z)
	});
    }

    return section;
}


static std::vector<std::vector<point2d>>
station_outlines(const options &opts)
{
    const airfoil_spec root_af = parse_airfoil(opts.airfoil, opts.six_a_param);
    const airfoil_spec tip_af = parse_airfoil(opts.tip_airfoil.empty() ? opts.airfoil : opts.tip_airfoil, opts.six_a_param);
    const section4 root_section = airfoil_section(root_af, opts.samples, opts.sharp_te);
    const section4 tip_section = airfoil_section(tip_af, opts.samples, opts.sharp_te);
    const bool omit_lower_te = opts.sharp_te || section_has_duplicate_te(root_section) || section_has_duplicate_te(tip_section);

    std::vector<std::vector<point2d>> outlines;
    outlines.reserve(static_cast<size_t>(opts.stations));

    size_t outline_cnt = 0;
    for (int s = 0; s < opts.stations; ++s) {
	const double f = static_cast<double>(s) / static_cast<double>(opts.stations - 1);
	const section4 section = interpolate_section(root_section, tip_section, f);
	outlines.push_back(airfoil_outline(section, omit_lower_te));
	if (s == 0) {
	    outline_cnt = outlines.back().size();
	} else if (outlines.back().size() != outline_cnt) {
	    throw std::runtime_error("spanwise section interpolation changed outline topology");
	}
    }

    return outlines;
}


static point3d
station_point(const point2d &section_pt, const options &opts, int station)
{
    const double f = static_cast<double>(station) / static_cast<double>(opts.stations - 1);
    const double chord = opts.root_chord + f * (opts.tip_chord - opts.root_chord);
    const double y = f * opts.span;
    const double le_x = f * opts.sweep;
    const double dihedral = std::tan(opts.dihedral_deg * M_PI / 180.0) * y;
    const double twist = f * opts.tip_twist_deg * M_PI / 180.0;

    const double pivot = 0.25 * chord;
    const double sx = section_pt.x * chord;
    const double sz = section_pt.z * chord;
    const double dx = sx - pivot;
    const double ct = std::cos(twist);
    const double st = std::sin(twist);

    point3d p;
    p.x = opts.offset_x + le_x + pivot + dx * ct + sz * st;
    p.y = opts.offset_y + y;
    p.z = opts.offset_z + dihedral - dx * st + sz * ct;
    return p;
}


static void
add_face(std::vector<int> &faces, int a, int b, int c)
{
    faces.push_back(a);
    faces.push_back(b);
    faces.push_back(c);
}


static bool
finite_point(const point3d &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}


static double
triangle_area2(const fastf_t *vertices, int a, int b, int c)
{
    const fastf_t *pa = vertices + 3 * a;
    const fastf_t *pb = vertices + 3 * b;
    const fastf_t *pc = vertices + 3 * c;

    const double abx = pb[0] - pa[0];
    const double aby = pb[1] - pa[1];
    const double abz = pb[2] - pa[2];
    const double acx = pc[0] - pa[0];
    const double acy = pc[1] - pa[1];
    const double acz = pc[2] - pa[2];

    const double cx = aby * acz - abz * acy;
    const double cy = abz * acx - abx * acz;
    const double cz = abx * acy - aby * acx;
    return std::sqrt(cx * cx + cy * cy + cz * cz);
}


static void
validate_bot_mesh(const std::vector<fastf_t> &vertices, const std::vector<int> &faces)
{
    const int vertex_cnt = static_cast<int>(vertices.size() / 3);
    if (vertices.empty() || faces.empty() || vertices.size() % 3 != 0 || faces.size() % 3 != 0)
	throw std::runtime_error("internal BoT mesh assembly produced malformed arrays");

    for (fastf_t v : vertices) {
	if (!std::isfinite(static_cast<double>(v)))
	    throw std::runtime_error("internal BoT mesh assembly produced non-finite coordinates");
    }

    for (size_t i = 0; i < faces.size(); i += 3) {
	const int a = faces[i];
	const int b = faces[i + 1];
	const int c = faces[i + 2];
	if (a < 0 || a >= vertex_cnt || b < 0 || b >= vertex_cnt || c < 0 || c >= vertex_cnt)
	    throw std::runtime_error("internal BoT mesh assembly produced an invalid face index");
	if (a == b || b == c || a == c)
	    throw std::runtime_error("internal BoT mesh assembly produced a degenerate face");
	if (triangle_area2(vertices.data(), a, b, c) <= SMALL_FASTF)
	    throw std::runtime_error("internal BoT mesh assembly produced a near-zero-area face");
    }
}


static void
validate_station_outlines(const options &opts, const std::vector<std::vector<point2d>> &outlines)
{
    if (outlines.size() != static_cast<size_t>(opts.stations) || outlines.empty())
	throw std::runtime_error("internal station outline assembly failed");

    const size_t outline_cnt = outlines.front().size();
    if (outline_cnt < 4)
	throw std::runtime_error("airfoil outline must contain at least four vertices");

    for (int s = 0; s < opts.stations; ++s) {
	if (outlines[static_cast<size_t>(s)].size() != outline_cnt)
	    throw std::runtime_error("all station outlines must have identical topology");

	for (const point2d &pt : outlines[static_cast<size_t>(s)]) {
	    const point3d p = station_point(pt, opts, s);
	    if (!finite_point(p))
		throw std::runtime_error("generated non-finite wing coordinate");
	}
    }
}


static struct rt_wdb *
open_output_database(const options &opts)
{
    const bool exists = bu_file_exists(opts.outfile.c_str(), NULL);
    if (exists && !opts.append && !opts.overwrite) {
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists. Use -f to overwrite or -j to append.\n", opts.outfile.c_str());
    }

    if (exists && opts.append) {
	struct db_i *dbip = db_open(opts.outfile.c_str(), DB_OPEN_READWRITE);
	if (!dbip)
	    bu_exit(EXIT_FAILURE, "ERROR: unable to open %s for append\n", opts.outfile.c_str());
	if (db_dirbuild(dbip) < 0) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to read directory from %s\n", opts.outfile.c_str());
	}
	struct rt_wdb *fp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	if (!fp) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to attach writer to %s\n", opts.outfile.c_str());
	}
	return fp;
    }

    struct rt_wdb *fp = wdb_fopen(opts.outfile.c_str());
    if (!fp)
	bu_exit(EXIT_FAILURE, "ERROR: unable to open %s\n", opts.outfile.c_str());

    mk_id(fp, "NACA procedural wing");
    return fp;
}


static void
check_output_names_available(struct rt_wdb *fp, const std::string &solid_name, const std::string &region_name)
{
    if (db_lookup(fp->dbip, solid_name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: object %s already exists in output database\n", solid_name.c_str());
    if (db_lookup(fp->dbip, region_name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: object %s already exists in output database\n", region_name.c_str());
}


static void
set_attr(struct rt_wdb *fp, const std::string &obj_name, const char *key, const std::string &value)
{
    if (db5_update_attribute(obj_name.c_str(), key, value.c_str(), fp->dbip) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to set %s attribute on %s\n", key, obj_name.c_str());
}


static void
write_metadata(struct rt_wdb *fp, const std::string &solid_name, const std::string &region_name, const options &opts)
{
    const std::string tip_airfoil = opts.tip_airfoil.empty() ? opts.airfoil : opts.tip_airfoil;
    const std::vector<std::string> objects = {solid_name, region_name};

    for (const std::string &obj_name : objects) {
	set_attr(fp, obj_name, "naca456::generator", "naca456");
	set_attr(fp, obj_name, "naca456::metadata_version", "1");
	set_attr(fp, obj_name, "naca456::algorithm_source", "PDAS NACA456 nacax.f90 epspsi.f90 splprocs.f90");
	set_attr(fp, obj_name, "naca456::geometry_mode", mode_name(opts.mode));
	set_attr(fp, obj_name, "naca456::brep_surface", brep_surface_name(opts.brep_surface));
	set_attr(fp, obj_name, "naca456::root_airfoil", opts.airfoil);
	set_attr(fp, obj_name, "naca456::tip_airfoil", tip_airfoil);
	set_attr(fp, obj_name, "naca456::span", num_string(opts.span));
	set_attr(fp, obj_name, "naca456::root_chord", num_string(opts.root_chord));
	set_attr(fp, obj_name, "naca456::tip_chord", num_string(opts.tip_chord));
	set_attr(fp, obj_name, "naca456::sweep", num_string(opts.sweep));
	set_attr(fp, obj_name, "naca456::dihedral_deg", num_string(opts.dihedral_deg));
	set_attr(fp, obj_name, "naca456::tip_twist_deg", num_string(opts.tip_twist_deg));
	set_attr(fp, obj_name, "naca456::six_a", num_string(opts.six_a_param));
	set_attr(fp, obj_name, "naca456::stations", num_string(opts.stations));
	set_attr(fp, obj_name, "naca456::samples", num_string(opts.samples));
	set_attr(fp, obj_name, "naca456::sharp_te", bool_name(opts.sharp_te));
	set_attr(fp, obj_name, "naca456::x_offset", num_string(opts.offset_x));
	set_attr(fp, obj_name, "naca456::y_offset", num_string(opts.offset_y));
	set_attr(fp, obj_name, "naca456::z_offset", num_string(opts.offset_z));
    }
}


static point3d
station_center(const std::vector<point2d> &outline, const options &opts, int station)
{
    point3d center;
    for (const point2d &pt : outline) {
	const point3d p = station_point(pt, opts, station);
	center.x += p.x;
	center.y += p.y;
	center.z += p.z;
    }

    const double scale = 1.0 / static_cast<double>(outline.size());
    center.x *= scale;
    center.y *= scale;
    center.z *= scale;
    return center;
}


static int
write_bot(const options &opts, const std::vector<std::vector<point2d>> &outlines)
{
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    const int outline_cnt = static_cast<int>(outlines.front().size());

    vertices.reserve(static_cast<size_t>(opts.stations * outline_cnt + 2) * 3);
    for (int s = 0; s < opts.stations; ++s) {
	for (const point2d &pt : outlines[static_cast<size_t>(s)]) {
	    const point3d p = station_point(pt, opts, s);
	    vertices.push_back(static_cast<fastf_t>(p.x));
	    vertices.push_back(static_cast<fastf_t>(p.y));
	    vertices.push_back(static_cast<fastf_t>(p.z));
	}
    }

    point3d center = station_center(outlines.front(), opts, 0);

    const int root_center = static_cast<int>(vertices.size() / 3);
    vertices.push_back(static_cast<fastf_t>(center.x));
    vertices.push_back(static_cast<fastf_t>(center.y));
    vertices.push_back(static_cast<fastf_t>(center.z));

    center = station_center(outlines.back(), opts, opts.stations - 1);
    const int tip_center = static_cast<int>(vertices.size() / 3);
    vertices.push_back(static_cast<fastf_t>(center.x));
    vertices.push_back(static_cast<fastf_t>(center.y));
    vertices.push_back(static_cast<fastf_t>(center.z));

    faces.reserve(static_cast<size_t>((opts.stations - 1) * outline_cnt * 2 + outline_cnt * 2) * 3);
    for (int s = 0; s < opts.stations - 1; ++s) {
	for (int k = 0; k < outline_cnt; ++k) {
	    const int kn = (k + 1) % outline_cnt;
	    const int a = s * outline_cnt + k;
	    const int b = s * outline_cnt + kn;
	    const int c = (s + 1) * outline_cnt + kn;
	    const int d = (s + 1) * outline_cnt + k;
	    add_face(faces, a, d, c);
	    add_face(faces, a, c, b);
	}
    }

    for (int k = 0; k < outline_cnt; ++k) {
	const int kn = (k + 1) % outline_cnt;
	add_face(faces, root_center, k, kn);
    }

    const int tip_offset = (opts.stations - 1) * outline_cnt;
    for (int k = 0; k < outline_cnt; ++k) {
	const int kn = (k + 1) % outline_cnt;
	add_face(faces, tip_center, tip_offset + kn, tip_offset + k);
    }

    validate_bot_mesh(vertices, faces);

    const std::string solid_name = opts.name + ".bot";
    const std::string region_name = opts.name + ".r";
    struct rt_wdb *fp = open_output_database(opts);
    check_output_names_available(fp, solid_name, region_name);
    if (mk_bot(fp, solid_name.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
	    vertices.size() / 3, faces.size() / 3, vertices.data(), faces.data(),
	    NULL, NULL) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write BoT solid\n");
    }

    unsigned char rgb[] = {180, 190, 205};
    mk_region1(fp, region_name.c_str(), solid_name.c_str(), "plastic", "", rgb);
    write_metadata(fp, solid_name, region_name, opts);
    db_close(fp->dbip);

    bu_log("Wrote %s with %zu vertices and %zu triangles\n", opts.outfile.c_str(), vertices.size() / 3, faces.size() / 3);
    return EXIT_SUCCESS;
}


static ON_Curve *
line_curve_3d(const ON_3dPoint &from, const ON_3dPoint &to)
{
    ON_Curve *curve = new ON_LineCurve(from, to);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static ON_Curve *
line_curve_2d(const ON_2dPoint &from, const ON_2dPoint &to)
{
    ON_Curve *curve = new ON_LineCurve(from, to);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static ON_Surface *
quad_surface(const ON_3dPoint &sw, const ON_3dPoint &se, const ON_3dPoint &ne, const ON_3dPoint &nw)
{
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 2, 2, 2, 2);
    surface->SetCV(0, 0, sw);
    surface->SetCV(1, 0, se);
    surface->SetCV(1, 1, ne);
    surface->SetCV(0, 1, nw);
    surface->SetKnot(0, 0, 0.0);
    surface->SetKnot(0, 1, 1.0);
    surface->SetKnot(1, 0, 0.0);
    surface->SetKnot(1, 1, 1.0);
    return surface;
}


static int
builder_edge(brep_builder &builder, int from, int to)
{
    const std::pair<int, int> key = std::minmax(from, to);
    auto e_it = builder.edges.find(key);
    if (e_it != builder.edges.end())
	return e_it->second;

    const int c3i = builder.brep->m_C3.Count();
    builder.brep->m_C3.Append(line_curve_3d(builder.points[static_cast<size_t>(from)], builder.points[static_cast<size_t>(to)]));
    ON_BrepEdge &edge = builder.brep->NewEdge(builder.brep->m_V[from], builder.brep->m_V[to], c3i);
    edge.m_tolerance = SMALL_FASTF;

    const int ei = edge.m_edge_index;
    builder.edges[key] = ei;
    return ei;
}


static int
builder_edge_with_curve(brep_builder &builder, int from, int to, ON_Curve *curve)
{
    const std::pair<int, int> key = std::minmax(from, to);
    auto e_it = builder.edges.find(key);
    if (e_it != builder.edges.end()) {
	delete curve;
	return e_it->second;
    }

    const int c3i = builder.brep->m_C3.Count();
    builder.brep->m_C3.Append(curve);
    ON_BrepEdge &edge = builder.brep->NewEdge(builder.brep->m_V[from], builder.brep->m_V[to], c3i);
    edge.m_tolerance = SMALL_FASTF;

    const int ei = edge.m_edge_index;
    builder.edges[key] = ei;
    return ei;
}


static ON_NurbsCurve *
linear_nurbs_curve(const std::vector<ON_3dPoint> &points)
{
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, 2, static_cast<int>(points.size()));
    for (size_t i = 0; i < points.size(); ++i)
	curve->SetCV(static_cast<int>(i), points[i]);

    const int knot_count = static_cast<int>(points.size());
    if (knot_count == 2) {
	curve->SetKnot(0, 0.0);
	curve->SetKnot(1, 1.0);
	return curve;
    }

    for (int i = 0; i < knot_count; ++i)
	curve->SetKnot(i, static_cast<double>(i) / static_cast<double>(knot_count - 1));

    return curve;
}


static ON_NurbsCurve *
cubic_bezier_curve(const ON_3dPoint &from, const ON_3dPoint &to, const ON_3dVector &from_tangent, const ON_3dVector &to_tangent)
{
    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, 4, 4);
    curve->SetCV(0, from);
    curve->SetCV(1, from + from_tangent / 3.0);
    curve->SetCV(2, to - to_tangent / 3.0);
    curve->SetCV(3, to);
    curve->SetKnot(0, 0.0);
    curve->SetKnot(1, 0.0);
    curve->SetKnot(2, 0.0);
    curve->SetKnot(3, 1.0);
    curve->SetKnot(4, 1.0);
    curve->SetKnot(5, 1.0);
    curve->SetDomain(0.0, 1.0);
    return curve;
}


static int
builder_span_edge(brep_builder &builder, const std::vector<int> &verts)
{
    const int from = verts.front();
    const int to = verts.back();
    const std::pair<int, int> key = std::minmax(from, to);
    auto e_it = builder.edges.find(key);
    if (e_it != builder.edges.end())
	return e_it->second;

    std::vector<ON_3dPoint> points;
    points.reserve(verts.size());
    for (int vi : verts)
	points.push_back(builder.points[static_cast<size_t>(vi)]);

    int c3i = -1;
    if (points.size() >= 4) {
	c3i = brep_curve_interpCrv(builder.brep, points);
    }
    if (c3i < 0) {
	c3i = builder.brep->m_C3.Count();
	builder.brep->m_C3.Append(linear_nurbs_curve(points));
    }

    ON_BrepEdge &edge = builder.brep->NewEdge(builder.brep->m_V[from], builder.brep->m_V[to], c3i);
    edge.m_tolerance = SMALL_FASTF;

    const int ei = edge.m_edge_index;
    builder.edges[key] = ei;
    return ei;
}


static int
builder_smooth_edge(brep_builder &builder, int from, int to, const ON_3dVector &from_tangent, const ON_3dVector &to_tangent)
{
    return builder_edge_with_curve(builder, from, to, cubic_bezier_curve(builder.points[static_cast<size_t>(from)],
	    builder.points[static_cast<size_t>(to)], from_tangent, to_tangent));
}


static bool
edge_reversed(const ON_Brep &brep, int edge_index, int trim_from, int trim_to)
{
    const ON_BrepEdge &edge = brep.m_E[edge_index];
    return !(edge.m_vi[0] == trim_from && edge.m_vi[1] == trim_to);
}


static ON_3dVector
point_delta(const ON_3dPoint &from, const ON_3dPoint &to)
{
    return to - from;
}


static int
grid_vertex_index(int station, int chord, int outline_cnt)
{
    return station * outline_cnt + chord;
}


static ON_3dVector
span_tangent(const brep_builder &builder, int station, int chord, int stations, int outline_cnt)
{
    const int vi = grid_vertex_index(station, chord, outline_cnt);
    if (stations < 2)
	return ON_3dVector::ZeroVector;
    if (station == 0) {
	const int vn = grid_vertex_index(station + 1, chord, outline_cnt);
	return point_delta(builder.points[static_cast<size_t>(vi)], builder.points[static_cast<size_t>(vn)]);
    }
    if (station == stations - 1) {
	const int vp = grid_vertex_index(station - 1, chord, outline_cnt);
	return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vi)]);
    }

    const int vp = grid_vertex_index(station - 1, chord, outline_cnt);
    const int vn = grid_vertex_index(station + 1, chord, outline_cnt);
    return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vn)]) * 0.5;
}


static ON_3dVector
chord_tangent(const brep_builder &builder, int station, int chord, int outline_cnt)
{
    const int kp = (chord + outline_cnt - 1) % outline_cnt;
    const int kn = (chord + 1) % outline_cnt;
    const int vp = grid_vertex_index(station, kp, outline_cnt);
    const int vn = grid_vertex_index(station, kn, outline_cnt);
    return point_delta(builder.points[static_cast<size_t>(vp)], builder.points[static_cast<size_t>(vn)]) * 0.5;
}


static ON_NurbsSurface *
ruled_surface_from_edges(const ON_Brep &brep, int edge0, int edge1)
{
    const ON_Curve *c0 = brep.m_E[edge0].EdgeCurveOf();
    const ON_Curve *c1 = brep.m_E[edge1].EdgeCurveOf();
    if (!c0 || !c1)
	return nullptr;

    ON_NurbsCurve *nc0 = c0->NurbsCurve();
    ON_NurbsCurve *nc1 = c1->NurbsCurve();
    if (!nc0 || !nc1) {
	delete nc0;
	delete nc1;
	return nullptr;
    }

    ON_NurbsSurface *surface = ON_NurbsSurface::New();
    if (!surface->CreateRuledSurface(*nc0, *nc1)) {
	delete nc0;
	delete nc1;
	delete surface;
	return nullptr;
    }

    delete nc0;
    delete nc1;
    return surface;
}


static ON_NurbsSurface *
polyline_ruled_surface(const brep_builder &builder, const std::vector<int> &edge0_verts, const std::vector<int> &edge1_verts)
{
    if (edge0_verts.size() != edge1_verts.size() || edge0_verts.size() < 2)
	return nullptr;

    const int u_cv_count = static_cast<int>(edge0_verts.size());
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 2, 2, u_cv_count, 2);
    for (int i = 0; i < u_cv_count; ++i) {
	surface->SetCV(i, 0, builder.points[static_cast<size_t>(edge0_verts[static_cast<size_t>(i)])]);
	surface->SetCV(i, 1, builder.points[static_cast<size_t>(edge1_verts[static_cast<size_t>(i)])]);
    }

    for (int i = 0; i < u_cv_count; ++i)
	surface->SetKnot(0, i, static_cast<double>(i) / static_cast<double>(u_cv_count - 1));
    surface->SetKnot(1, 0, 0.0);
    surface->SetKnot(1, 1, 1.0);
    return surface;
}


static void
add_trim(brep_builder &builder, ON_BrepLoop &loop, int from, int to, const ON_2dPoint &uv_from, const ON_2dPoint &uv_to, ON_Surface::ISO iso)
{
    const int edge_index = builder_edge(builder, from, to);
    const int c2i = builder.brep->m_C2.Count();
    builder.brep->m_C2.Append(line_curve_2d(uv_from, uv_to));

    ON_BrepTrim &trim = builder.brep->NewTrim(builder.brep->m_E[edge_index], edge_reversed(*builder.brep, edge_index, from, to), loop, c2i);
    trim.m_iso = iso;
    trim.m_type = ON_BrepTrim::mated;
    trim.m_tolerance[0] = SMALL_FASTF;
    trim.m_tolerance[1] = SMALL_FASTF;
}


static void
add_trim_for_edge(brep_builder &builder, ON_BrepLoop &loop, int edge_index, int from, int to, const ON_2dPoint &uv_from, const ON_2dPoint &uv_to, ON_Surface::ISO iso)
{
    const int c2i = builder.brep->m_C2.Count();
    builder.brep->m_C2.Append(line_curve_2d(uv_from, uv_to));

    ON_BrepTrim &trim = builder.brep->NewTrim(builder.brep->m_E[edge_index], edge_reversed(*builder.brep, edge_index, from, to), loop, c2i);
    trim.m_iso = iso;
    trim.m_type = ON_BrepTrim::mated;
    trim.m_tolerance[0] = SMALL_FASTF;
    trim.m_tolerance[1] = SMALL_FASTF;
}


static void
add_ruled_strip_face(brep_builder &builder, const std::vector<int> &edge0_verts, const std::vector<int> &edge1_verts)
{
    const int v0 = edge0_verts.front();
    const int v1 = edge0_verts.back();
    const int v2 = edge1_verts.back();
    const int v3 = edge1_verts.front();

    const int e0 = builder_span_edge(builder, edge0_verts);
    const int e2 = builder_span_edge(builder, edge1_verts);
    const int e1 = builder_edge(builder, v1, v2);
    const int e3 = builder_edge(builder, v3, v0);

    ON_NurbsSurface *surface = ruled_surface_from_edges(*builder.brep, e0, e2);
    if (!surface)
	surface = polyline_ruled_surface(builder, edge0_verts, edge1_verts);
    if (!surface)
	throw std::runtime_error("unable to construct NURBS strip");

    const int si = builder.brep->m_S.Count();
    builder.brep->m_S.Append(surface);
    ON_BrepFace &face = builder.brep->NewFace(si);
    ON_BrepLoop &loop = builder.brep->NewLoop(ON_BrepLoop::outer, face);

    add_trim_for_edge(builder, loop, e0, v0, v1, ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0), ON_Surface::S_iso);
    add_trim_for_edge(builder, loop, e1, v1, v2, ON_2dPoint(1.0, 0.0), ON_2dPoint(1.0, 1.0), ON_Surface::E_iso);
    add_trim_for_edge(builder, loop, e2, v2, v3, ON_2dPoint(1.0, 1.0), ON_2dPoint(0.0, 1.0), ON_Surface::N_iso);
    add_trim_for_edge(builder, loop, e3, v3, v0, ON_2dPoint(0.0, 1.0), ON_2dPoint(0.0, 0.0), ON_Surface::W_iso);
}


static ON_NurbsSurface *
cubic_hermite_surface(const ON_3dPoint &p00, const ON_3dPoint &p10, const ON_3dPoint &p11, const ON_3dPoint &p01,
    const ON_3dVector &du00, const ON_3dVector &du10, const ON_3dVector &du11, const ON_3dVector &du01,
    const ON_3dVector &dv00, const ON_3dVector &dv10, const ON_3dVector &dv11, const ON_3dVector &dv01)
{
    ON_NurbsSurface *surface = ON_NurbsSurface::New(3, false, 4, 4, 4, 4);

    surface->SetCV(0, 0, p00);
    surface->SetCV(1, 0, p00 + du00 / 3.0);
    surface->SetCV(2, 0, p10 - du10 / 3.0);
    surface->SetCV(3, 0, p10);

    surface->SetCV(0, 3, p01);
    surface->SetCV(1, 3, p01 + du01 / 3.0);
    surface->SetCV(2, 3, p11 - du11 / 3.0);
    surface->SetCV(3, 3, p11);

    surface->SetCV(0, 1, p00 + dv00 / 3.0);
    surface->SetCV(0, 2, p01 - dv01 / 3.0);
    surface->SetCV(3, 1, p10 + dv10 / 3.0);
    surface->SetCV(3, 2, p11 - dv11 / 3.0);

    surface->SetCV(1, 1, p00 + du00 / 3.0 + dv00 / 3.0);
    surface->SetCV(2, 1, p10 - du10 / 3.0 + dv10 / 3.0);
    surface->SetCV(1, 2, p01 + du01 / 3.0 - dv01 / 3.0);
    surface->SetCV(2, 2, p11 - du11 / 3.0 - dv11 / 3.0);

    for (int dir = 0; dir < 2; ++dir) {
	surface->SetKnot(dir, 0, 0.0);
	surface->SetKnot(dir, 1, 0.0);
	surface->SetKnot(dir, 2, 0.0);
	surface->SetKnot(dir, 3, 1.0);
	surface->SetKnot(dir, 4, 1.0);
	surface->SetKnot(dir, 5, 1.0);
    }

    return surface;
}


static void
add_smooth_patch_face(brep_builder &builder, int station, int chord, int outline_cnt, int stations)
{
    const int kn = (chord + 1) % outline_cnt;
    const int v00 = grid_vertex_index(station, chord, outline_cnt);
    const int v10 = grid_vertex_index(station + 1, chord, outline_cnt);
    const int v11 = grid_vertex_index(station + 1, kn, outline_cnt);
    const int v01 = grid_vertex_index(station, kn, outline_cnt);

    const ON_3dVector du00 = span_tangent(builder, station, chord, stations, outline_cnt);
    const ON_3dVector du10 = span_tangent(builder, station + 1, chord, stations, outline_cnt);
    const ON_3dVector du11 = span_tangent(builder, station + 1, kn, stations, outline_cnt);
    const ON_3dVector du01 = span_tangent(builder, station, kn, stations, outline_cnt);

    const ON_3dVector dv00 = chord_tangent(builder, station, chord, outline_cnt);
    const ON_3dVector dv10 = chord_tangent(builder, station + 1, chord, outline_cnt);
    const ON_3dVector dv11 = chord_tangent(builder, station + 1, kn, outline_cnt);
    const ON_3dVector dv01 = chord_tangent(builder, station, kn, outline_cnt);

    const int e0 = builder_smooth_edge(builder, v00, v10, du00, du10);
    const int e1 = builder_smooth_edge(builder, v10, v11, dv10, dv11);
    const int e2 = builder_smooth_edge(builder, v01, v11, du01, du11);
    const int e3 = builder_smooth_edge(builder, v00, v01, dv00, dv01);

    const int si = builder.brep->m_S.Count();
    builder.brep->m_S.Append(cubic_hermite_surface(
	    builder.points[static_cast<size_t>(v00)],
	    builder.points[static_cast<size_t>(v10)],
	    builder.points[static_cast<size_t>(v11)],
	    builder.points[static_cast<size_t>(v01)],
	    du00, du10, du11, du01,
	    dv00, dv10, dv11, dv01));
    ON_BrepFace &face = builder.brep->NewFace(si);
    ON_BrepLoop &loop = builder.brep->NewLoop(ON_BrepLoop::outer, face);

    add_trim_for_edge(builder, loop, e0, v00, v10, ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0), ON_Surface::S_iso);
    add_trim_for_edge(builder, loop, e1, v10, v11, ON_2dPoint(1.0, 0.0), ON_2dPoint(1.0, 1.0), ON_Surface::E_iso);
    add_trim_for_edge(builder, loop, e2, v11, v01, ON_2dPoint(1.0, 1.0), ON_2dPoint(0.0, 1.0), ON_Surface::N_iso);
    add_trim_for_edge(builder, loop, e3, v01, v00, ON_2dPoint(0.0, 1.0), ON_2dPoint(0.0, 0.0), ON_Surface::W_iso);
}


static void
add_tri_face(brep_builder &builder, int v0, int v1, int v2)
{
    const ON_3dPoint &p0 = builder.points[static_cast<size_t>(v0)];
    const ON_3dPoint &p1 = builder.points[static_cast<size_t>(v1)];
    const ON_3dPoint &p2 = builder.points[static_cast<size_t>(v2)];
    const ON_3dPoint p3 = p0 + (p2 - p1);

    const int si = builder.brep->m_S.Count();
    builder.brep->m_S.Append(quad_surface(p0, p1, p2, p3));
    ON_BrepFace &face = builder.brep->NewFace(si);
    ON_BrepLoop &loop = builder.brep->NewLoop(ON_BrepLoop::outer, face);

    add_trim(builder, loop, v0, v1, ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 0.0), ON_Surface::S_iso);
    add_trim(builder, loop, v1, v2, ON_2dPoint(1.0, 0.0), ON_2dPoint(1.0, 1.0), ON_Surface::E_iso);
    add_trim(builder, loop, v2, v0, ON_2dPoint(1.0, 1.0), ON_2dPoint(0.0, 0.0), ON_Surface::not_iso);
}


static point3d
outline_center(const std::vector<point2d> &outline, const options &opts, int station)
{
    point3d center;
    for (const point2d &pt : outline) {
	const point3d p = station_point(pt, opts, station);
	center.x += p.x;
	center.y += p.y;
	center.z += p.z;
    }

    const double scale = 1.0 / static_cast<double>(outline.size());
    center.x *= scale;
    center.y *= scale;
    center.z *= scale;
    return center;
}


static int
write_brep(const options &opts, const std::vector<std::vector<point2d>> &outlines)
{
    ON::Begin();
    brep_builder builder;
    builder.brep = ON_Brep::New();

    const int outline_cnt = static_cast<int>(outlines.front().size());
    builder.points.reserve(static_cast<size_t>(opts.stations * outline_cnt + 2));
    for (int s = 0; s < opts.stations; ++s) {
	for (const point2d &pt : outlines[static_cast<size_t>(s)]) {
	    const point3d p = station_point(pt, opts, s);
	    builder.points.push_back(ON_3dPoint(p.x, p.y, p.z));
	}
    }

    point3d center = outline_center(outlines.front(), opts, 0);
    const int root_center = static_cast<int>(builder.points.size());
    builder.points.push_back(ON_3dPoint(center.x, center.y, center.z));

    center = outline_center(outlines.back(), opts, opts.stations - 1);
    const int tip_center = static_cast<int>(builder.points.size());
    builder.points.push_back(ON_3dPoint(center.x, center.y, center.z));

    for (const ON_3dPoint &p : builder.points) {
	ON_BrepVertex &v = builder.brep->NewVertex(p);
	v.m_tolerance = SMALL_FASTF;
    }

    if (opts.brep_surface == brep_surface_mode::smooth) {
	for (int s = 0; s < opts.stations - 1; ++s) {
	    for (int k = 0; k < outline_cnt; ++k) {
		add_smooth_patch_face(builder, s, k, outline_cnt, opts.stations);
	    }
	}
    } else {
	for (int k = 0; k < outline_cnt; ++k) {
	    const int kn = (k + 1) % outline_cnt;
	    std::vector<int> edge0_verts;
	    std::vector<int> edge1_verts;
	    edge0_verts.reserve(static_cast<size_t>(opts.stations));
	    edge1_verts.reserve(static_cast<size_t>(opts.stations));
	    for (int s = 0; s < opts.stations; ++s) {
		edge0_verts.push_back(s * outline_cnt + k);
		edge1_verts.push_back(s * outline_cnt + kn);
	    }
	    add_ruled_strip_face(builder, edge0_verts, edge1_verts);
	}
    }

    for (int k = 0; k < outline_cnt; ++k) {
	const int kn = (k + 1) % outline_cnt;
	add_tri_face(builder, root_center, k, kn);
    }

    const int tip_offset = (opts.stations - 1) * outline_cnt;
    for (int k = 0; k < outline_cnt; ++k) {
	const int kn = (k + 1) % outline_cnt;
	add_tri_face(builder, tip_center, tip_offset + kn, tip_offset + k);
    }

    builder.brep->Standardize();
    builder.brep->Compact();

    ON_TextLog error_log;
    if (!builder.brep->IsValid(&error_log)) {
	error_log.Print("Invalid NACA wing BREP\n");
	delete builder.brep;
	ON::End();
	bu_exit(EXIT_FAILURE, "ERROR: generated BREP did not pass OpenNURBS validation\n");
    }

    if (!builder.brep->IsSolid()) {
	delete builder.brep;
	ON::End();
	bu_exit(EXIT_FAILURE, "ERROR: generated BREP is valid but not classified as a solid\n");
    }

    const std::string solid_name = opts.name + ".brep";
    const std::string region_name = opts.name + ".r";
    struct rt_wdb *fp = open_output_database(opts);
    check_output_names_available(fp, solid_name, region_name);
    if (mk_brep(fp, solid_name.c_str(), builder.brep) != 0) {
	db_close(fp->dbip);
	delete builder.brep;
	ON::End();
	bu_exit(EXIT_FAILURE, "ERROR: unable to write BREP solid\n");
    }

    unsigned char rgb[] = {180, 190, 205};
    mk_region1(fp, region_name.c_str(), solid_name.c_str(), "plastic", "", rgb);
    write_metadata(fp, solid_name, region_name, opts);
    db_close(fp->dbip);
    delete builder.brep;
    ON::End();

    bu_log("Wrote %s as solid BREP with explicit %s topology\n", opts.outfile.c_str(), brep_surface_name(opts.brep_surface).c_str());
    return EXIT_SUCCESS;
}


static int
write_single_wing(const options &opts)
{
    const std::vector<std::vector<point2d>> outlines = station_outlines(opts);
    validate_station_outlines(opts, outlines);

    switch (opts.mode) {
	case output_mode::bot:
	    return write_bot(opts, outlines);
	case output_mode::brep:
	    return write_brep(opts, outlines);
    }

    return EXIT_FAILURE;
}


struct demo_spec {
    output_mode mode = output_mode::bot;
    const char *name = nullptr;
    const char *airfoil = nullptr;
    const char *tip_airfoil = nullptr;
    double span = 900.0;
    double root_chord = 240.0;
    double tip_chord = 120.0;
    double sweep = 0.0;
    double dihedral_deg = 0.0;
    double tip_twist_deg = 0.0;
    double six_a_param = 1.0;
    int stations = 7;
    int samples = 33;
    bool sharp_te = false;
};


static options
demo_options(const options &demo_opts, const demo_spec &spec, int index)
{
    const int cols = 6;
    const double dx = 650.0;
    const double dy = 1400.0;

    options opts;
    opts.outfile = demo_opts.demo_file;
    opts.name = spec.name;
    opts.airfoil = spec.airfoil;
    if (spec.tip_airfoil)
	opts.tip_airfoil = spec.tip_airfoil;
    opts.mode = spec.mode;
    opts.brep_surface = spec.mode == output_mode::brep ? demo_opts.brep_surface : brep_surface_mode::ruled;
    opts.span = spec.span;
    opts.root_chord = spec.root_chord;
    opts.tip_chord = spec.tip_chord;
    opts.sweep = spec.sweep;
    opts.dihedral_deg = spec.dihedral_deg;
    opts.tip_twist_deg = spec.tip_twist_deg;
    opts.six_a_param = spec.six_a_param;
    opts.stations = spec.stations;
    opts.samples = spec.samples;
    opts.sharp_te = spec.sharp_te;
    opts.offset_x = static_cast<double>(index % cols) * dx;
    opts.offset_y = static_cast<double>(index / cols) * dy;
    opts.overwrite = index == 0;
    opts.append = index != 0;
    return opts;
}


static int
write_demo_file(const options &demo_opts)
{
    const std::vector<demo_spec> specs = {
	{output_mode::bot, "bot_00_baseline_2412", "2412", nullptr, 900, 240, 120, 0, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_01_symmetric_0012", "0012", nullptr, 850, 230, 160, 0, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_02_high_camber_4415", "4415", nullptr, 820, 230, 130, 70, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_03_sharp_te_2412", "2412", nullptr, 850, 230, 115, 60, 0, 0, 1, 7, 33, true},
	{output_mode::bot, "bot_04_five_23012", "23012", nullptr, 850, 230, 120, 80, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_05_five_reflex_23112", "23112", nullptr, 820, 220, 120, 60, 0, -2, 1, 7, 33, false},
	{output_mode::bot, "bot_06_swept_taper", "2312", nullptr, 920, 260, 90, 220, 0, 0, 1, 8, 37, false},
	{output_mode::bot, "bot_07_forward_sweep", "2412", nullptr, 820, 220, 130, -120, 0, 0, 1, 8, 37, false},
	{output_mode::bot, "bot_08_dihedral", "2415", nullptr, 900, 240, 130, 0, 8, 0, 1, 8, 37, false},
	{output_mode::bot, "bot_09_washout", "2412", nullptr, 900, 240, 120, 0, 0, -6, 1, 8, 37, false},
	{output_mode::bot, "bot_10_six_63215", "63-215", nullptr, 860, 240, 115, 120, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_11_six_lowload", "64-210", nullptr, 850, 230, 115, 90, 0, 0, 0.45, 7, 33, false},
	{output_mode::bot, "bot_12_root_tip_sections", "2418", "0008", 900, 260, 105, 120, 4, -3, 1, 9, 41, false},
	{output_mode::bot, "bot_13_tip_cambered", "0012", "4412", 820, 220, 130, 80, 0, 0, 1, 9, 41, false},
	{output_mode::bot, "bot_14_five_to_six", "23012", "65-210", 880, 245, 110, 130, 3, -3, 1, 9, 41, false},
	{output_mode::bot, "bot_15_short_stub", "4418", nullptr, 520, 280, 220, 0, 0, 0, 1, 6, 33, false},
	{output_mode::bot, "bot_16_six_66212", "66-212", nullptr, 900, 230, 110, 100, 0, -3, 1, 8, 37, false},
	{output_mode::bot, "bot_17_six_67215", "67-215", nullptr, 860, 235, 115, 80, 2, 0, 1, 8, 37, false},
	{output_mode::bot, "bot_18_sixa_63a210", "63A210", nullptr, 820, 230, 120, 80, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_19_sixa_64a210", "64A210", nullptr, 820, 230, 120, 80, 4, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_20_sixa_65a010", "65A010", nullptr, 860, 220, 100, 130, 0, -4, 1, 8, 37, false},
	{output_mode::bot, "bot_21_long_slender", "0009", nullptr, 1150, 210, 80, 160, 0, -4, 1, 10, 45, false},
	{output_mode::bot, "bot_22_thick_root_0024", "0024", nullptr, 780, 260, 170, 0, 0, 0, 1, 7, 33, false},
	{output_mode::bot, "bot_23_compound", "2415", "63-212", 980, 260, 95, 180, 7, -5, 1, 10, 45, false},
	{output_mode::brep, "brep_24_baseline_2412", "2412", nullptr, 850, 230, 120, 0, 0, 0, 1, 6, 21, false},
	{output_mode::brep, "brep_25_five_23012", "23012", nullptr, 760, 220, 120, 70, 0, 0, 1, 6, 21, false},
	{output_mode::brep, "brep_26_five_reflex", "23112", nullptr, 760, 220, 120, 70, 0, -2, 1, 6, 21, false},
	{output_mode::brep, "brep_27_six_63215", "63-215", nullptr, 800, 230, 115, 100, 0, 0, 1, 6, 21, false},
	{output_mode::brep, "brep_28_six_lowload", "64-210", nullptr, 800, 230, 115, 100, 0, 0, 0.45, 6, 21, false},
	{output_mode::brep, "brep_29_sixa_64a210", "64A210", nullptr, 760, 220, 120, 0, 4, 0, 1, 6, 21, false},
	{output_mode::brep, "brep_30_sharp_te", "0012", nullptr, 760, 220, 110, 0, 0, 0, 1, 6, 21, true},
	{output_mode::brep, "brep_31_swept_twist", "2415", nullptr, 850, 240, 125, 160, 6, -4, 1, 7, 25, false},
	{output_mode::brep, "brep_32_root_tip_thin", "2418", "0008", 850, 260, 95, 120, 4, -3, 1, 7, 25, false},
	{output_mode::brep, "brep_33_five_to_six", "23012", "65-210", 820, 240, 105, 110, 3, -3, 1, 7, 25, false},
	{output_mode::brep, "brep_34_sixa_thin", "65A010", nullptr, 820, 220, 95, 120, 0, -4, 1, 7, 25, false},
	{output_mode::brep, "brep_35_high_resolution", "63-212", nullptr, 820, 240, 115, 100, 3, -2, 1, 9, 33, false}
    };

    for (size_t i = 0; i < specs.size(); ++i) {
	options opts = demo_options(demo_opts, specs[i], static_cast<int>(i));
	bu_log("[%zu] %s %s\n", i, opts.mode == output_mode::bot ? "bot" : "brep", opts.name.c_str());
	const int ret = write_single_wing(opts);
	if (ret != EXIT_SUCCESS)
	    return ret;
    }

    bu_log("Wrote %s with %zu sample wings\n", demo_opts.demo_file.c_str(), specs.size());
    return EXIT_SUCCESS;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    try {
	const options opts = parse_args(argc, argv);
	if (!opts.section_file.empty())
	    return write_section_file(opts);
	if (!opts.demo_file.empty())
	    return write_demo_file(opts);
	return write_single_wing(opts);
    } catch (const std::exception &e) {
	bu_log("ERROR: %s\n", e.what());
	options opts;
	struct bu_vls outfile = BU_VLS_INIT_ZERO;
	struct bu_vls name = BU_VLS_INIT_ZERO;
	struct bu_vls airfoil = BU_VLS_INIT_ZERO;
	struct bu_vls tip_airfoil = BU_VLS_INIT_ZERO;
	struct bu_vls mode = BU_VLS_INIT_ZERO;
	struct bu_vls brep_surface = BU_VLS_INIT_ZERO;
	struct bu_vls demo_file = BU_VLS_INIT_ZERO;
	struct bu_vls section_file = BU_VLS_INIT_ZERO;
	bool help = false;
	usage(argv[0], option_desc(opts, outfile, name, airfoil, tip_airfoil, mode, brep_surface, demo_file, section_file, help));
	bu_vls_free(&outfile);
	bu_vls_free(&name);
	bu_vls_free(&airfoil);
	bu_vls_free(&tip_airfoil);
	bu_vls_free(&mode);
	bu_vls_free(&brep_surface);
	bu_vls_free(&demo_file);
	bu_vls_free(&section_file);
	return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
