/*                 BRLCADWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/BRLCADWrapper.cpp
 *
 * C++ wrapper to BRL-CAD database functions.
 *
 */

#include "common.h"
#include <stdlib.h>

/* interface header */
#include "./BRLCADWrapper.h"
#include "STEPString.h"

/* system headers */
#include <sstream>
#include <iostream>


BRLCADWrapper::BRLCADWrapper()
    : outfp(NULL), dbip(NULL), anonymous_name_counter(0), dry_run(false)
{
}


BRLCADWrapper::~BRLCADWrapper()
{
    Close();
}

bool
BRLCADWrapper::load(std::string &flnm)
{

    if (dry_run)
	return true;

    /* open brlcad instance */
    if ((dbip = db_open(flnm.c_str(), DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Cannot open input file (%s)\n", flnm.c_str());
	return false;
    }
    if (db_dirbuild(dbip)) {
	bu_log("ERROR: db_dirbuild failed: (%s)\n", flnm.c_str());
	return false;
    }

    return true;
}


bool
BRLCADWrapper::OpenFile(std::string &flnm)
{
    //TODO: need to check to make sure we aren't overwriting

    if (dry_run)
	return true;

    /* open brlcad instance */
    if ((outfp = wdb_fopen(flnm.c_str())) == NULL) {
	bu_log("Cannot open output file (%s)\n", flnm.c_str());
	return false;
    }

    // hold on to output filename
    filename = flnm.c_str();

    mk_id(outfp, "Output from STEP converter step-g.");

    return true;
}


bool
BRLCADWrapper::WriteHeader()
{
    if (dry_run)
	return true;

    db5_update_attribute("_GLOBAL", "HEADERINFO", "test header attributes", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERCLASS", "test header classification", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERAPPROVED", "test header approval", outfp->dbip);
    return true;
}


bool
BRLCADWrapper::WriteSphere(const std::string &name, const double *center, double radius,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run)
	return true;
    if (!outfp || !center || radius <= 0.0)
	return false;
    point_t pnt = {center[0], center[1], center[2]};
    if (mk_sph(outfp, name.c_str(), pnt, radius) < 0)
	return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteRcc(const std::string &name, const double *base, const double *height,
	double radius, int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !base || !height || radius <= 0.0) return false;
    point_t b = {base[0], base[1], base[2]};
    vect_t h = {height[0], height[1], height[2]};
    if (mk_rcc(outfp, name.c_str(), b, h, radius) < 0) return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteTgc(const std::string &name, const double *base, const double *height,
	const double *a, const double *b, const double *c, const double *d,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !base || !height || !a || !b || !c || !d) return false;
    point_t bp = {base[0], base[1], base[2]};
    vect_t hv = {height[0], height[1], height[2]};
    vect_t av = {a[0], a[1], a[2]};
    vect_t bv = {b[0], b[1], b[2]};
    vect_t cv = {c[0], c[1], c[2]};
    vect_t dv = {d[0], d[1], d[2]};
    if (mk_tgc(outfp, name.c_str(), bp, hv, av, bv, cv, dv) < 0) return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteTorus(const std::string &name, const double *center, const double *normal,
	double major_radius, double minor_radius, int64_t step_id,
	const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !center || !normal || major_radius <= 0.0 || minor_radius <= 0.0 ||
	minor_radius >= major_radius) return false;
    point_t c = {center[0], center[1], center[2]};
    vect_t n = {normal[0], normal[1], normal[2]};
    if (mk_tor(outfp, name.c_str(), c, n, major_radius, minor_radius) < 0) return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteHalf(const std::string &name, const double *normal, double distance,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !normal) return false;
    vect_t outward = {normal[0], normal[1], normal[2]};
    if (MAGNITUDE(outward) <= SMALL_FASTF) return false;
    VUNITIZE(outward);
    if (mk_half(outfp, name.c_str(), outward, distance) < 0) return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteArb8(const std::string &name, const double *points, int64_t step_id,
	const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !points) return false;
    fastf_t vertices[24];
    for (size_t i = 0; i < 24; ++i) vertices[i] = points[i];
    if (mk_arb8(outfp, name.c_str(), vertices) < 0) return false;
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}


/* This simple routine will replace diacritic characters(code >= 192) from the extended
 * ASCII set with a specific mapping from the standard ASCII set. This code was copied
 * and modified from a solution provided on stackoverflow.com at:
 *     (http://stackoverflow.com/questions/14094621/)
 */
std::string
BRLCADWrapper::ReplaceAccented( std::string &str ) {
    std::string retStr = "";
    const char *p = str.c_str();
    while ( (*p)!=0 ) {
        const char*
        //   "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ"
        tr = "AAAAAAECEEEEIIIIDNOOOOOx0UUUUYPsaaaaaaeceeeeiiiiOnooooo/0uuuuypy";
        unsigned char ch = (*p);
        if ( ch >=192 ) {
            retStr += tr[ ch-192 ];
        } else {
            retStr += *p;
        }
        ++p;
    }
    return retStr;
}


/*
 * Simplifying names for better behavior under our Tcl based tools. This routine
 * replaces spaces and non-alphanumeric characters with underscores. It also replaces
 * ASCII extended characters representing diacritics (code >= 192)  with specific
 * mapped ASCII characters below ASCII code 128.
 */
std::string
BRLCADWrapper::CleanBRLCADName(std::string &inname)
{
    return brlcad::step::sanitize_name(inname);
}


std::string
BRLCADWrapper::StableBRLCADName(const std::string &inname, int64_t step_id)
{
    std::string base = brlcad::step::sanitize_name(inname);
    std::map<std::string, int64_t>::const_iterator found = allocated_names.find(base);
    if (found == allocated_names.end() || found->second == step_id) {
	allocated_names[base] = step_id;
	return base;
    }

    const std::string stem = base + "_step" + std::to_string(step_id);
    std::string result = stem;
    while (allocated_names.find(result) != allocated_names.end() && allocated_names[result] != step_id)
	result = stem + "_" + std::to_string(++anonymous_name_counter);
    allocated_names[result] = step_id;
    return result;
}


std::string
BRLCADWrapper::GetBRLCADName(std::string &name)
{
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    int len = 0;
    char *cp,*tp;
    int start = static_cast<int>(++anonymous_name_counter);

    for (cp = (char *)name.c_str(), len = 0; *cp != '\0'; ++cp, ++len) {
	if (*cp == '@') {
	    if (*(cp + 1) == '@')
		++cp;
	    else
		break;
	}
	if (*cp == '\'') {
	    // remove single quotes
	    continue;
	}
	if (*cp == ' ') {
	    // simply replace spaces with underscores
	    bu_vls_putc(&obj_name, '_');
	} else {
	    bu_vls_putc(&obj_name, *cp);
	}
    }
    bu_vls_putc(&obj_name, '\0');

    tp = (char *)((*cp == '\0') ? "" : cp + 1);

    /* TODO - We don't have db_lookup in a dry run */
    if (dry_run) {
	std::string result = bu_vls_cstr(&obj_name);
	bu_vls_free(&obj_name);
	return result;
    }

    do {
	bu_vls_trunc(&obj_name, len);
	bu_vls_printf(&obj_name, "%d", start++);
	bu_vls_strcat(&obj_name, tp);
    }
    while (db_lookup(outfp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != RT_DIR_NULL);

    std::string rstr(bu_vls_cstr(&obj_name));
    bu_vls_free(&obj_name);
    return rstr;
}

bool
BRLCADWrapper::AddMember(const std::string &combname, const std::string &member, mat_t mat,
	int operation)
{
    MAP_OF_BU_LIST_HEADS::iterator i = heads.find(combname);
    if (i != heads.end()) {
	struct bu_list *head = (*i).second;
	if (mk_addmember(member.c_str(), head, mat, operation) == WMEMBER_NULL)
	    return false;
    } else {
	struct bu_list *head = NULL;

	BU_ALLOC(head, struct bu_list);

	BU_LIST_INIT(head);
	if (mk_addmember(member.c_str(), head, mat, operation) == WMEMBER_NULL)
	    return false;
	heads[combname] = head;
    }

    return true;
}

bool
BRLCADWrapper::SetCombinationProperties(const std::string &combname, bool is_region,
	int64_t step_id, const std::string &original_name,
	const brlcad::step::Style *style)
{
    if (combname.empty())
	return false;
    CombinationProperties &properties = combination_properties[combname];
    properties.is_region = is_region;
    properties.step_id = step_id;
    properties.original_name = original_name;
    properties.has_style = style != NULL;
    if (style)
	properties.style = *style;
    return true;
}

bool
BRLCADWrapper::WriteCombs()
{
    MAP_OF_BU_LIST_HEADS::iterator i = heads.begin();
    bool success = true;

    if (dry_run) {
	while (i != heads.end()) {
	    struct bu_list *head = (i++)->second;
	    mk_freemembers(head);
	    BU_FREE(head, struct bu_list);
	}
	heads.clear();
	combination_properties.clear();
	return true;
    }

    while (i != heads.end()) {
	std::string combname = (*i).first;
	struct bu_list *head = (*i++).second;

	std::map<std::string, CombinationProperties>::const_iterator property =
	    combination_properties.find(combname);
	const CombinationProperties *properties = property == combination_properties.end() ?
	    NULL : &property->second;
	unsigned char rgb[3] = {200, 180, 180};
	std::string shader_args;
	const char *shader = NULL;
	unsigned char *colour = NULL;
	if (properties && properties->is_region) {
	    shader = "plastic";
	    colour = rgb;
	    if (properties->has_style && properties->style.has_rgb) {
		for (size_t component = 0; component < 3; ++component) {
		    double value = properties->style.rgb[component];
		    if (value < 0.0) value = 0.0;
		    if (value > 1.0) value = 1.0;
		    rgb[component] = static_cast<unsigned char>(value * 255.0 + 0.5);
		}
	    } else {
		getRandomColor(rgb);
	    }
	    if (properties->has_style && properties->style.has_transparency) {
		std::ostringstream args;
		args << "tr " << properties->style.transparency;
		shader_args = args.str();
	    }
	}

	/* Product, assembly, and intermediate Boolean combinations remain
	 * non-regions.  Exact CSG roots explicitly opt into region semantics. */
	if (mk_comb(outfp, combname.c_str(), head,
	    properties && properties->is_region ? 1 : 0, shader,
	    shader_args.empty() ? NULL : shader_args.c_str(), colour,
	    0, 0, 0, 0, 0, 0, 0) < 0) {
	    success = false;
	} else if (properties) {
	    if (properties->step_id > 0)
		SetAttribute(combname, "step:source_id", std::to_string(properties->step_id));
	    if (!properties->original_name.empty())
		SetAttribute(combname, "step:original_name",
		    brlcad::step::decode_string(properties->original_name));
	    if (properties->has_style) {
		const brlcad::step::Style &style = properties->style;
		if (!style.name.empty()) SetAttribute(combname, "step:style_name", style.name);
		if (style.has_rgb) {
		    std::ostringstream value;
		    value << style.rgb[0] << ' ' << style.rgb[1] << ' ' << style.rgb[2];
		    SetAttribute(combname, "step:color_rgb", value.str());
		}
		if (!style.layers.empty()) {
		    std::ostringstream value;
		    for (size_t layer = 0; layer < style.layers.size(); ++layer) {
			if (layer) value << ';';
			value << style.layers[layer];
		    }
		    SetAttribute(combname, "step:layers", value.str());
		}
		if (!style.source_entity_ids.empty()) {
		    std::ostringstream value;
		    for (size_t source = 0; source < style.source_entity_ids.size(); ++source) {
			if (source) value << ' ';
			value << style.source_entity_ids[source];
		    }
		    SetAttribute(combname, "step:style_source_ids", value.str());
		}
	    }
	}

	BU_FREE(head, struct bu_list);

    }
    heads.clear();
    combination_properties.clear();
    return success;
}


void
BRLCADWrapper::getRandomColor(unsigned char *rgb)
{
    /* golden ratio */
    static fastf_t hsv[3] = { 0.0, 0.5, 0.95 };
    static double golden_ratio_conjugate = 0.618033988749895;
    static fastf_t h = drand48();

    h = fmod(h+golden_ratio_conjugate,1.0);
    *hsv = h * 360.0;
    bu_hsv_to_rgb(hsv,rgb);
}


static void
getStableEntityColor(unsigned char *rgb, int64_t step_id)
{
    if (step_id <= 0) {
	BRLCADWrapper::getRandomColor(rgb);
	return;
    }
    /* Preserve the historical pleasant saturation/value while deriving hue
     * from source identity.  Completion-order spooling must not make an
     * otherwise deterministic database assign different fallback colors. */
    uint64_t hash = 1469598103934665603ULL;
    uint64_t value = static_cast<uint64_t>(step_id);
    for (size_t byte = 0; byte < sizeof(value); ++byte) {
	hash ^= value & 0xffU;
	hash *= 1099511628211ULL;
	value >>= 8;
    }
    fastf_t hsv[3] = {
	static_cast<fastf_t>(hash % 360U), 0.5, 0.95
    };
    bu_hsv_to_rgb(hsv, rgb);
}


bool
BRLCADWrapper::WriteBrep(std::string name, ON_Brep *brep, mat_t &mat, bool is_region,
	int64_t step_id, const std::string &original_name, const brlcad::step::Style *style)
{
    std::string sol = name + ".s";
    std::string reg = name;
    if (dry_run)
	return true;
    if (!outfp || !brep)
	return false;

    if (mk_brep(outfp, sol.c_str(), (void *)brep) < 0)
	return false;
    unsigned char rgb[] = {200, 180, 180};

    if (style && style->has_rgb) {
	for (size_t i = 0; i < 3; ++i) {
	    double component = style->rgb[i];
	    if (component < 0.0) component = 0.0;
	    if (component > 1.0) component = 1.0;
	    rgb[i] = static_cast<unsigned char>(component * 255.0 + 0.5);
	}
    } else {
	getStableEntityColor(rgb, step_id);
    }

    std::string shader_args;
    if (style && style->has_transparency) {
	std::ostringstream args;
	args << "tr " << style->transparency;
	shader_args = args.str();
    }

    struct bu_list head;
    BU_LIST_INIT(&head);
    if (mk_addmember(sol.c_str(), &head, mat, WMOP_UNION) == WMEMBER_NULL)
	return false;

    if (mk_comb(outfp, reg.c_str(), &head, is_region ? 1 : 0, "plastic",
	shader_args.c_str(), rgb, 0, 0, 0, 0, 0, 0, 0) < 0)
	return false;

    if (step_id > 0) {
	const std::string id = std::to_string(step_id);
	SetAttribute(sol, "step:source_id", id);
	SetAttribute(reg, "step:source_id", id);
    }
    if (!original_name.empty()) {
	const std::string decoded = brlcad::step::decode_string(original_name);
	SetAttribute(sol, "step:original_name", decoded);
	SetAttribute(reg, "step:original_name", decoded);
    }
    if (style) {
	if (!style->name.empty()) {
	    SetAttribute(sol, "step:style_name", style->name);
	    SetAttribute(reg, "step:style_name", style->name);
	}
	if (style->has_rgb) {
	    std::ostringstream value;
	    value << style->rgb[0] << ' ' << style->rgb[1] << ' ' << style->rgb[2];
	    SetAttribute(sol, "step:color_rgb", value.str());
	    SetAttribute(reg, "step:color_rgb", value.str());
	}
	if (!style->layers.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->layers.size(); ++i) {
		if (i) value << ';';
		value << style->layers[i];
	    }
	    SetAttribute(sol, "step:layers", value.str());
	    SetAttribute(reg, "step:layers", value.str());
	}
	if (!style->source_entity_ids.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->source_entity_ids.size(); ++i) {
		if (i) value << ' ';
		value << style->source_entity_ids[i];
	    }
	    SetAttribute(sol, "step:style_source_ids", value.str());
	    SetAttribute(reg, "step:style_source_ids", value.str());
	}
    }
    return true;
}


bool
BRLCADWrapper::WriteBot(std::string name, size_t num_vertices, size_t num_faces,
	fastf_t *vertices, int *faces, mat_t &mat, int64_t step_id,
	const std::string &original_name, const brlcad::step::Style *style)
{
    if (dry_run)
	return true;
    if (!outfp || !vertices || !faces || num_vertices < 4 || num_faces < 4)
	return false;

    const std::string sol = name + ".s";
    if (mk_bot(outfp, sol.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
	num_vertices, num_faces, vertices, faces, NULL, NULL) < 0)
	return false;

    unsigned char rgb[] = {200, 180, 180};
    if (style && style->has_rgb) {
	for (size_t i = 0; i < 3; ++i) {
	    double component = style->rgb[i];
	    if (component < 0.0) component = 0.0;
	    if (component > 1.0) component = 1.0;
	    rgb[i] = static_cast<unsigned char>(component * 255.0 + 0.5);
	}
    } else {
	getStableEntityColor(rgb, step_id);
    }

    std::string shader_args;
    if (style && style->has_transparency) {
	std::ostringstream args;
	args << "tr " << style->transparency;
	shader_args = args.str();
    }

    struct bu_list head;
    BU_LIST_INIT(&head);
    if (mk_addmember(sol.c_str(), &head, mat, WMOP_UNION) == WMEMBER_NULL)
	return false;
    if (mk_comb(outfp, name.c_str(), &head, 1, "plastic", shader_args.c_str(),
	rgb, 0, 0, 0, 0, 0, 0, 0) < 0)
	return false;

    if (step_id > 0) {
	const std::string id = std::to_string(step_id);
	SetAttribute(sol, "step:source_id", id);
	SetAttribute(name, "step:source_id", id);
    }
    if (!original_name.empty()) {
	const std::string decoded = brlcad::step::decode_string(original_name);
	SetAttribute(sol, "step:original_name", decoded);
	SetAttribute(name, "step:original_name", decoded);
    }
    if (style) {
	if (!style->name.empty()) {
	    SetAttribute(sol, "step:style_name", style->name);
	    SetAttribute(name, "step:style_name", style->name);
	}
	if (style->has_rgb) {
	    std::ostringstream value;
	    value << style->rgb[0] << ' ' << style->rgb[1] << ' ' << style->rgb[2];
	    SetAttribute(sol, "step:color_rgb", value.str());
	    SetAttribute(name, "step:color_rgb", value.str());
	}
	if (!style->layers.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->layers.size(); ++i) {
		if (i) value << ';';
		value << style->layers[i];
	    }
	    SetAttribute(sol, "step:layers", value.str());
	    SetAttribute(name, "step:layers", value.str());
	}
	if (!style->source_entity_ids.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->source_entity_ids.size(); ++i) {
		if (i) value << ' ';
		value << style->source_entity_ids[i];
	    }
	    SetAttribute(sol, "step:style_source_ids", value.str());
	    SetAttribute(name, "step:style_source_ids", value.str());
	}
    }
    SetAttribute(sol, "step:representation", "FACETED_BREP");
    SetAttribute(name, "step:representation", "FACETED_BREP");
    return true;
}


bool
BRLCADWrapper::SetAttribute(const std::string &object, const std::string &key, const std::string &value)
{
    if (dry_run)
	return true;
    if (!outfp)
	return false;
    if (db_lookup(outfp->dbip, object.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	return false;
    return db5_update_attribute(object.c_str(), key.c_str(), value.c_str(), outfp->dbip) == 0;
}


bool
BRLCADWrapper::CopyObjectFrom(BRLCADWrapper &source, const std::string &object)
{
    if (dry_run)
	return true;
    if (!outfp)
	return false;
    struct db_i *source_dbip = source.dbip;
    if (!source_dbip && source.outfp)
	source_dbip = source.outfp->dbip;
    if (!source_dbip)
	return false;
    struct directory *source_dp = db_lookup(source_dbip, object.c_str(), LOOKUP_QUIET);
    if (source_dp == RT_DIR_NULL)
	return false;

    struct bu_external external;
    if (db_get_external(&external, source_dp, source_dbip) < 0)
	return false;
    const int result = wdb_export_external(outfp, &external, object.c_str(),
	source_dp->d_flags, source_dp->d_minor_type);
    bu_free_external(&external);
    return result >= 0;
}

struct db_i *
BRLCADWrapper::GetDBIP()
{
    return dbip;
}


bool
BRLCADWrapper::Close()
{
    if (dry_run)
	return true;

    if (outfp) {
	db_close(outfp->dbip);
	outfp = NULL;
    }
    if (dbip) {
	db_close(dbip);
	dbip = NULL;
    }

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
