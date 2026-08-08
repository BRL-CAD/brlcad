/*                  A N I M A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Parse complete rtwizard render specifications and evaluate their typed
 * animation tracks.  This file deliberately produces data, not rt command
 * text supplied by the user: only commands assembled from validated fields
 * leave this module.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "tcl.h"
#include "vmath.h"
#include "bn/mat.h"
#include "bn/qmath.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/units.h"

#include "../libbu/json.hpp"
#include "animation.h"

using json = nlohmann::json;

namespace {

namespace fs = std::filesystem;

struct JKey {
    double time;
    json value;
};

static bool
finite_number(const json &v)
{
    return v.is_number() && std::isfinite(v.get<double>());
}

static void
check_keys(const json &j, const std::vector<std::string> &allowed, const std::string &where)
{
    if (!j.is_object())
	throw std::runtime_error(where + " must be an object");
    for (json::const_iterator i = j.begin(); i != j.end(); ++i) {
	if (std::find(allowed.begin(), allowed.end(), i.key()) == allowed.end())
	    throw std::runtime_error(where + ": unknown field '" + i.key() + "'");
    }
}

static std::vector<double>
number_array(const json &v, size_t count, const std::string &where)
{
    if (!v.is_array() || v.size() != count)
	throw std::runtime_error(where + " must contain exactly " + std::to_string(count) + " numbers");
    std::vector<double> out(count);
    for (size_t i = 0; i < count; ++i) {
	if (!finite_number(v[i]))
	    throw std::runtime_error(where + " contains a non-finite value");
	out[i] = v[i].get<double>();
    }
    return out;
}

static std::vector<JKey>
property_keys(const json &track, const char *property, double duration)
{
    std::vector<JKey> out;
    if (!track.contains("keyframes") || !track["keyframes"].is_array())
	throw std::runtime_error("track keyframes must be an array");
    for (const json &k : track["keyframes"]) {
	check_keys(k, {"time", "value", "eye", "look_at", "orientation", "up",
		"view_size", "perspective", "point", "normal", "translation",
		"rotation", "scale", "matrix", "color", "temperature", "material"},
		"keyframe");
	if (!k.contains("time") || !finite_number(k["time"]))
	    throw std::runtime_error("keyframe time must be finite");
	double t = k["time"].get<double>();
	if (t < 0.0 || t > duration)
	    throw std::runtime_error("keyframe time is outside the animation duration");
	if (k.contains(property))
	    out.push_back({t, k[property]});
	else if (std::string(property) == "value" && k.contains("value"))
	    out.push_back({t, k["value"]});
    }
    std::sort(out.begin(), out.end(), [](const JKey &a, const JKey &b) { return a.time < b.time; });
    for (size_t i = 1; i < out.size(); ++i) {
	if (std::fabs(out[i].time - out[i-1].time) <= std::numeric_limits<double>::epsilon())
	    throw std::runtime_error(std::string("duplicate keyframe time for '") + property + "'");
    }
    return out;
}

static std::string
interpolation_for(const json &track, const char *property)
{
    if (!track.contains("interpolation"))
	return "linear";
    const json &in = track["interpolation"];
    std::string result;
    if (in.is_string())
	result = in.get<std::string>();
    else if (in.is_object() && in.contains(property) && in[property].is_string())
	result = in[property].get<std::string>();
    else if (in.is_object() && in.contains("default") && in["default"].is_string())
	result = in["default"].get<std::string>();
    else
	result = "linear";
    if (result != "step" && result != "linear" && result != "smooth")
	throw std::runtime_error("interpolation must be step, linear, or smooth");
    return result;
}

static void
validate_track(const json &track, const std::string &type)
{
    std::vector<std::string> properties;
    if (type == "camera") {
	check_keys(track, {"type", "name", "interpolation", "keyframes"}, "camera track");
	properties = {"eye", "look_at", "up", "orientation", "view_size", "perspective"};
    } else if (type == "cut_plane") {
	check_keys(track, {"type", "name", "interpolation", "keyframes"}, "cut_plane track");
	properties = {"point", "normal"};
    } else if (type == "transform") {
	check_keys(track, {"type", "name", "path", "operation", "pivot", "interpolation", "keyframes"}, "transform track");
	properties = {"translation", "rotation", "scale", "matrix"};
    } else if (type == "material") {
	check_keys(track, {"type", "name", "path", "operation", "interpolation", "keyframes"}, "material track");
	properties = {"value", "material"};
    } else if (type == "color") {
	check_keys(track, {"type", "name", "path", "interpolation", "keyframes"}, "color track");
	properties = {"value", "color"};
    } else if (type == "temperature") {
	check_keys(track, {"type", "name", "path", "interpolation", "keyframes"}, "temperature track");
	properties = {"value", "temperature"};
    } else {
	throw std::runtime_error("unknown animation track type '" + type + "'");
    }

    if (!track.contains("keyframes") || !track["keyframes"].is_array())
	throw std::runtime_error(type + " track keyframes must be an array");
    std::vector<std::string> key_fields = {"time"};
    key_fields.insert(key_fields.end(), properties.begin(), properties.end());
    bool has_value = false;
    for (const json &key : track["keyframes"]) {
	check_keys(key, key_fields, type + " keyframe");
	if (key.size() > 1) has_value = true;
    }
    if (!has_value) throw std::runtime_error(type + " track contains no animated values");

    if (track.contains("interpolation") && track["interpolation"].is_string()) {
	std::string choice = track["interpolation"].get<std::string>();
	if (choice != "step" && choice != "linear" && choice != "smooth")
	    throw std::runtime_error(type + " interpolation must be step, linear, or smooth");
    } else if (track.contains("interpolation") && track["interpolation"].is_object()) {
	std::vector<std::string> interpolation_fields = {"default"};
	interpolation_fields.insert(interpolation_fields.end(), properties.begin(), properties.end());
	check_keys(track["interpolation"], interpolation_fields, type + " interpolation");
	for (json::const_iterator i = track["interpolation"].begin(); i != track["interpolation"].end(); ++i) {
	    if (!i.value().is_string()) throw std::runtime_error(type + " interpolation choices must be strings");
	    std::string choice = i.value().get<std::string>();
	    if (choice != "step" && choice != "linear" && choice != "smooth")
		throw std::runtime_error(type + " interpolation must be step, linear, or smooth");
	}
    } else if (track.contains("interpolation")) {
	throw std::runtime_error(type + " interpolation must be a string or object");
    }
}

static size_t
key_segment(const std::vector<JKey> &keys, double t)
{
    if (keys.size() < 2 || t <= keys.front().time)
	return 0;
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
	if (t <= keys[i+1].time)
	    return i;
    }
    return keys.size() - 2;
}

static json
json_lerp(const json &a, const json &b, double f)
{
    if (finite_number(a) && finite_number(b))
	return a.get<double>() + (b.get<double>() - a.get<double>()) * f;
    if (a.is_array() && b.is_array() && a.size() == b.size()) {
	json out = json::array();
	for (size_t i = 0; i < a.size(); ++i)
	    out.push_back(json_lerp(a[i], b[i], f));
	return out;
    }
    throw std::runtime_error("linear interpolation requires matching numeric values");
}

static json
json_smooth(const json &p0, const json &p1, const json &p2, const json &p3, double f)
{
    if (finite_number(p0) && finite_number(p1) && finite_number(p2) && finite_number(p3)) {
	double a = p0.get<double>(), b = p1.get<double>();
	double c = p2.get<double>(), d = p3.get<double>();
	double f2 = f*f, f3 = f2*f;
	return 0.5 * ((2.0*b) + (-a+c)*f + (2.0*a-5.0*b+4.0*c-d)*f2 + (-a+3.0*b-3.0*c+d)*f3);
    }
    if (p0.is_array() && p1.is_array() && p2.is_array() && p3.is_array() &&
	p0.size() == p1.size() && p1.size() == p2.size() && p2.size() == p3.size()) {
	json out = json::array();
	for (size_t i = 0; i < p0.size(); ++i)
	    out.push_back(json_smooth(p0[i], p1[i], p2[i], p3[i], f));
	return out;
    }
    throw std::runtime_error("smooth interpolation requires matching numeric values");
}

static json
evaluate_keys(const std::vector<JKey> &keys, double t, const std::string &interp)
{
    if (keys.empty())
	return json();
    if (keys.size() == 1 || t <= keys.front().time)
	return keys.front().value;
    if (t >= keys.back().time)
	return keys.back().value;
    size_t i = key_segment(keys, t);
    if (interp == "step")
	return keys[i].value;
    double span = keys[i+1].time - keys[i].time;
    double f = span > 0.0 ? (t - keys[i].time) / span : 0.0;
    if (interp == "smooth") {
	const json &p0 = keys[i ? i-1 : i].value;
	const json &p3 = keys[(i+2 < keys.size()) ? i+2 : i+1].value;
	return json_smooth(p0, keys[i].value, keys[i+1].value, p3, f);
    }
    return json_lerp(keys[i].value, keys[i+1].value, f);
}

static void
normalize_quat(std::vector<double> &q)
{
    double m = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (!(m > 1.0e-12))
	throw std::runtime_error("orientation quaternion must be nonzero");
    for (double &v : q) v /= m;
}

static std::vector<double>
rotation_quat(const json &v, const std::string &where)
{
    std::vector<double> q(4);
    if (v.is_array()) {
	q = number_array(v, 4, where);
    } else if (v.is_object() && v.contains("quaternion")) {
	check_keys(v, {"quaternion"}, where);
	q = number_array(v["quaternion"], 4, where + ".quaternion");
    } else if (v.is_object() && v.contains("axis") && v.contains("degrees")) {
	check_keys(v, {"axis", "degrees"}, where);
	std::vector<double> axis = number_array(v["axis"], 3, where + ".axis");
	if (!finite_number(v["degrees"]))
	    throw std::runtime_error(where + ".degrees must be finite");
	double m = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
	if (!(m > 1.0e-12))
	    throw std::runtime_error(where + ".axis must be nonzero");
	double a = v["degrees"].get<double>() * DEG2RAD * 0.5;
	double s = std::sin(a) / m;
	q = {axis[0]*s, axis[1]*s, axis[2]*s, std::cos(a)};
    } else {
	throw std::runtime_error(where + " must be a quaternion array or axis/degrees object");
    }
    normalize_quat(q);
    return q;
}

static std::vector<double>
evaluate_quat(const std::vector<JKey> &keys, double t, const std::string &interp)
{
    if (keys.empty()) return {};
    if (keys.size() == 1 || t <= keys.front().time)
	return rotation_quat(keys.front().value, "rotation");
    if (t >= keys.back().time)
	return rotation_quat(keys.back().value, "rotation");
    size_t i = key_segment(keys, t);
    if (interp == "step")
	return rotation_quat(keys[i].value, "rotation");
    double f = (t - keys[i].time) / (keys[i+1].time - keys[i].time);
    std::vector<double> a = rotation_quat(keys[i].value, "rotation");
    std::vector<double> b = rotation_quat(keys[i+1].value, "rotation");
    quat_t qa = {a[0], a[1], a[2], a[3]};
    quat_t qb = {b[0], b[1], b[2], b[3]};
    quat_t qo;
    quat_slerp(qo, qa, qb, f);
    return {qo[0], qo[1], qo[2], qo[3]};
}

static Tcl_Obj *
tcl_vec(const std::vector<double> &v)
{
    Tcl_Obj *o = Tcl_NewListObj(0, NULL);
    for (double n : v)
	Tcl_ListObjAppendElement(NULL, o, Tcl_NewDoubleObj(n));
    return o;
}

static void
dict_put(Tcl_Interp *interp, Tcl_Obj *dict, const char *key, Tcl_Obj *value)
{
    Tcl_DictObjPut(interp, dict, Tcl_NewStringObj(key, -1), value);
}

static std::string
command_path(const json &track, const std::string &type)
{
    std::string path = track.value("path", "");
    if (path.empty()) throw std::runtime_error(type + " path may not be empty");
    if (path.find_first_of(";\r\n\t ") != std::string::npos)
	throw std::runtime_error(type + " path may not contain whitespace or command separators");
    return path;
}

static std::string
material_string(const json &value)
{
    if (value.is_string()) return value.get<std::string>();
    check_keys(value, {"shader", "parameters"}, "structured material");
    if (!value.contains("shader") || !value["shader"].is_string() || value["shader"].get<std::string>().empty())
	throw std::runtime_error("structured material requires a nonempty shader string");
    std::string shader = value["shader"].get<std::string>();
    if (shader.find_first_of(";\r\n\t ") != std::string::npos)
	throw std::runtime_error("structured material shader must be one command word");
    std::ostringstream ss;
    ss.precision(17);
    ss << shader;
    if (value.contains("parameters")) {
	if (!value["parameters"].is_object())
	    throw std::runtime_error("structured material parameters must be an object");
	for (json::const_iterator i = value["parameters"].begin(); i != value["parameters"].end(); ++i) {
	    if (i.key().empty() || i.key().find_first_of("=;\r\n\t ") != std::string::npos)
		throw std::runtime_error("structured material parameter names must be command words");
	    ss << ' ' << i.key() << '=';
	    if (finite_number(i.value())) ss << i.value().get<double>();
	    else if (i.value().is_boolean()) ss << (i.value().get<bool>() ? 1 : 0);
	    else if (i.value().is_string()) {
		std::string parameter = i.value().get<std::string>();
		if (parameter.find_first_of(";\r\n\t ") != std::string::npos)
		    throw std::runtime_error("structured material string parameters may not contain whitespace or command separators");
		ss << parameter;
	    } else {
		throw std::runtime_error("structured material parameters must be finite numbers, booleans, or strings");
	    }
	}
    }
    return ss.str();
}

static std::string
matrix_command(const json &track, double t, double duration, double local2base)
{
    std::string path = command_path(track, "transform");
    std::string op = track.value("operation", "right_multiply");
    if (op == "replace_stack") op = "rstack";
    else if (op == "replace_arc") op = "rarc";
    else if (op == "left_multiply") op = "lmul";
    else if (op == "right_multiply") op = "rmul";
    else if (op == "replace_both") op = "rboth";
    else throw std::runtime_error("unknown transform operation");

    std::vector<JKey> mk = property_keys(track, "matrix", duration);
    std::vector<JKey> tk = property_keys(track, "translation", duration);
    std::vector<JKey> rk = property_keys(track, "rotation", duration);
    std::vector<JKey> sk = property_keys(track, "scale", duration);
    if (!mk.empty() && (!tk.empty() || !rk.empty() || !sk.empty()))
	throw std::runtime_error("transform track may not mix matrix and decomposed values");
    mat_t mat;
    MAT_IDN(mat);
    if (!mk.empty()) {
	json mv = evaluate_keys(mk, t, "step");
	std::vector<double> m = number_array(mv, 16, "transform matrix");
	for (int i = 0; i < 16; ++i) mat[i] = m[(size_t)i];
	mat[3] *= local2base; mat[7] *= local2base; mat[11] *= local2base;
    } else {
	std::vector<double> tr = {0, 0, 0};
	std::vector<double> q = {0, 0, 0, 1};
	double scale = 1.0;
	if (!tk.empty()) tr = number_array(evaluate_keys(tk, t, interpolation_for(track, "translation")), 3, "translation");
	if (!rk.empty()) q = evaluate_quat(rk, t, interpolation_for(track, "rotation"));
	if (!sk.empty()) {
	    json sv = evaluate_keys(sk, t, interpolation_for(track, "scale"));
	    if (!finite_number(sv) || std::fabs(sv.get<double>()) < 1.0e-12)
		throw std::runtime_error("transform scale must be finite and nonzero");
	    scale = sv.get<double>();
	}
	quat_t qq = {q[0], q[1], q[2], q[3]};
	quat_quat2mat(mat, qq);
	mat[15] = 1.0 / scale;
	if (track.contains("pivot")) {
	    std::vector<double> pv = number_array(track["pivot"], 3, "transform pivot");
	    point_t p = {pv[0]*local2base, pv[1]*local2base, pv[2]*local2base};
	    mat_t about;
	    bn_mat_xform_about_pnt(about, mat, p);
	    MAT_COPY(mat, about);
	}
	mat[3] += tr[0]*local2base; mat[7] += tr[1]*local2base; mat[11] += tr[2]*local2base;
    }

    std::ostringstream ss;
    ss.precision(17);
    ss << "anim " << path << " matrix " << op;
    for (int i = 0; i < 16; ++i) ss << ' ' << mat[i];
    return ss.str();
}

static std::string
value_command(const json &track, double t, double duration)
{
    std::string type = track.value("type", "");
    std::string path = command_path(track, type);
    std::vector<JKey> keys = property_keys(track, "value", duration);
    if (keys.empty()) {
	const char *prop = type == "color" ? "color" : (type == "temperature" ? "temperature" : "material");
	keys = property_keys(track, prop, duration);
    }
    if (keys.empty()) throw std::runtime_error(type + " track has no values");

    std::ostringstream ss;
    ss.precision(17);
    if (type == "color") {
	json v = evaluate_keys(keys, t, interpolation_for(track, "color"));
	std::vector<double> rgb = number_array(v, 3, "color");
	ss << "anim " << path << " color";
	for (double c : rgb) {
	    if (c < 0.0 || c > 255.0) throw std::runtime_error("color channels must be in [0,255]");
	    ss << ' ' << (int)std::lround(c);
	}
    } else if (type == "temperature") {
	json v = evaluate_keys(keys, t, interpolation_for(track, "temperature"));
	if (!finite_number(v)) throw std::runtime_error("temperature must be numeric");
	ss << "anim " << path << " temperature " << v.get<double>();
    } else {
	json v = evaluate_keys(keys, t, "step");
	std::string op = track.value("operation", "replace");
	if (op != "replace" && op != "append") throw std::runtime_error("material operation must be replace or append");
	std::string material = material_string(v);
	if (material.find(';') != std::string::npos || material.find('\n') != std::string::npos || material.find('\r') != std::string::npos)
	    throw std::runtime_error("material value may not contain command separators");
	ss << "anim " << path << " material " << op << ' ' << material;
    }
    return ss.str();
}

static int
animation_json_cmd(ClientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 12) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "usage: rtwizard_animation_json file viewsize orientation eye perspective duration fps frames plays cyclic local2base", -1));
	return TCL_ERROR;
    }
    try {
	const char *path = Tcl_GetString(objv[1]);
	double base_viewsize, base_perspective, duration_override, local2base;
	int fps_override, frames_override, plays_override, cyclic_override;
	if (Tcl_GetDoubleFromObj(interp, objv[2], &base_viewsize) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[5], &base_perspective) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[6], &duration_override) != TCL_OK ||
	    Tcl_GetIntFromObj(interp, objv[7], &fps_override) != TCL_OK ||
	    Tcl_GetIntFromObj(interp, objv[8], &frames_override) != TCL_OK ||
	    Tcl_GetIntFromObj(interp, objv[9], &plays_override) != TCL_OK ||
	    Tcl_GetIntFromObj(interp, objv[10], &cyclic_override) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[11], &local2base) != TCL_OK)
	    return TCL_ERROR;

	int oc = 0;
	Tcl_Obj **ov = NULL;
	if (Tcl_ListObjGetElements(interp, objv[3], &oc, &ov) != TCL_OK || oc != 4)
	    throw std::runtime_error("base orientation must contain four numbers");
	std::vector<double> base_q(4);
	for (int i = 0; i < 4; ++i) if (Tcl_GetDoubleFromObj(interp, ov[i], &base_q[(size_t)i]) != TCL_OK) return TCL_ERROR;
	if (Tcl_ListObjGetElements(interp, objv[4], &oc, &ov) != TCL_OK || oc != 3)
	    throw std::runtime_error("base eye point must contain three numbers");
	std::vector<double> base_eye(3);
	for (int i = 0; i < 3; ++i) if (Tcl_GetDoubleFromObj(interp, ov[i], &base_eye[(size_t)i]) != TCL_OK) return TCL_ERROR;

	std::ifstream in(path);
	if (!in) throw std::runtime_error(std::string("unable to open animation file '") + path + "'");
	json root;
	in >> root;
	if (root.value("schema", "") == "brlcad.rtwizard.render") {
	    check_keys(root, {"schema", "version", "name", "description", "database", "objects", "image", "output",
		    "view", "style", "runtime", "animation"}, "render specification");
	    if (root.value("version", 0) != 1 || !root.contains("animation"))
		throw std::runtime_error("render specification has no version 1 animation section");
	    json animation = root["animation"];
	    root = animation;
	} else {
	    check_keys(root, {"schema", "version", "units", "timing", "tracks"}, "animation");
	    if (root.value("schema", "") != "brlcad.rtwizard.animation" || root.value("version", 0) != 1)
		throw std::runtime_error("unsupported animation schema or version");
	}
	check_keys(root, {"schema", "version", "preset", "units", "timing", "options", "tracks"}, "animation");
	if (root.contains("preset")) throw std::runtime_error("animation track files may not contain a preset");
	if (root.contains("options")) throw std::runtime_error("animation options are only valid with a preset");
	if (root.contains("units")) {
	    if (!root["units"].is_string()) throw std::runtime_error("animation units must be a string");
	    std::string units = root["units"].get<std::string>();
	    if (units != "database") {
		local2base = bu_units_conversion(units.c_str());
		if (!(local2base > 0.0)) throw std::runtime_error("unknown animation distance unit '" + units + "'");
	    }
	}
	if (!root.contains("timing") || !root["timing"].is_object()) throw std::runtime_error("animation timing is required");
	check_keys(root["timing"], {"duration", "fps", "frames", "cyclic", "plays"}, "timing");
	double duration = duration_override > 0.0 ? duration_override : root["timing"].value("duration", 0.0);
	int fps = fps_override > 0 ? fps_override : root["timing"].value("fps", 10);
	bool cyclic = cyclic_override >= 0 ? cyclic_override != 0 : root["timing"].value("cyclic", false);
	int plays = plays_override >= 0 ? plays_override : root["timing"].value("plays", cyclic ? 0 : 1);
	if (!(duration > 0.0) || !std::isfinite(duration) || fps <= 0 || plays < 0)
	    throw std::runtime_error("duration and fps must be positive and plays nonnegative");
	int specified_frames = frames_override > 0 ? frames_override : root["timing"].value("frames", 0);
	int frame_count = specified_frames > 0 ? specified_frames : (int)std::llround(duration * fps) + (cyclic ? 0 : 1);
	if (frame_count < 2) throw std::runtime_error("animation must contain at least two frames");
	if (!root.contains("tracks") || !root["tracks"].is_array() || root["tracks"].empty())
	    throw std::runtime_error("animation tracks must be a nonempty array");

	Tcl_Obj *frame_list = Tcl_NewListObj(0, NULL);
	for (int fi = 0; fi < frame_count; ++fi) {
	    double denom = cyclic ? (double)frame_count : (double)(frame_count - 1);
	    double time = duration * (double)fi / denom;
	    std::vector<double> eye = base_eye;
	    std::vector<double> orient = base_q;
	    std::vector<double> look;
	    std::vector<double> up;
	    double viewsize = base_viewsize;
	    double perspective = base_perspective;
	    std::string cut;
	    std::vector<std::string> commands;

	    for (const json &track : root["tracks"]) {
		std::string type = track.value("type", "");
		validate_track(track, type);
		if (type == "camera") {
		    std::vector<JKey> ek = property_keys(track, "eye", duration);
		    std::vector<JKey> lk = property_keys(track, "look_at", duration);
		    std::vector<JKey> uk = property_keys(track, "up", duration);
		    std::vector<JKey> ok = property_keys(track, "orientation", duration);
		    if (!lk.empty() && !ok.empty()) throw std::runtime_error("camera track may not mix look_at and orientation");
		    if (!uk.empty() && lk.empty()) throw std::runtime_error("camera up requires look_at values in the same track");
		    if (!ek.empty()) {
			eye = number_array(evaluate_keys(ek, time, interpolation_for(track, "eye")), 3, "camera eye");
			for (double &v : eye) v *= local2base;
		    }
		    if (!lk.empty()) {
			look = number_array(evaluate_keys(lk, time, interpolation_for(track, "look_at")), 3, "camera look_at");
			for (double &v : look) v *= local2base;
			up.clear();
		    }
		    if (!uk.empty()) up = number_array(evaluate_keys(uk, time, interpolation_for(track, "up")), 3, "camera up");
		    if (!ok.empty()) {
			orient = evaluate_quat(ok, time, interpolation_for(track, "orientation"));
			look.clear();
			up.clear();
		    }
		    std::vector<JKey> vk = property_keys(track, "view_size", duration);
		    if (!vk.empty()) {
			json v = evaluate_keys(vk, time, interpolation_for(track, "view_size"));
			if (!finite_number(v) || v.get<double>() <= 0.0) throw std::runtime_error("view_size must be positive");
			viewsize = v.get<double>() * local2base;
		    }
		    std::vector<JKey> pk = property_keys(track, "perspective", duration);
		    if (!pk.empty()) {
			json v = evaluate_keys(pk, time, interpolation_for(track, "perspective"));
			if (!finite_number(v)) throw std::runtime_error("perspective must be finite");
			perspective = v.get<double>();
		    }
		} else if (type == "cut_plane") {
		    std::vector<JKey> pk = property_keys(track, "point", duration);
		    std::vector<JKey> nk = property_keys(track, "normal", duration);
		    if (pk.empty() || nk.empty()) throw std::runtime_error("cut_plane requires point and normal values");
		    std::vector<double> p = number_array(evaluate_keys(pk, time, interpolation_for(track, "point")), 3, "cut point");
		    std::vector<double> n = number_array(evaluate_keys(nk, time, interpolation_for(track, "normal")), 3, "cut normal");
		    double nm = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
		    if (!(nm > 1.0e-12)) throw std::runtime_error("cut normal must be nonzero");
		    std::ostringstream cs; cs.precision(17);
		    cs << p[0]*local2base << ',' << p[1]*local2base << ',' << p[2]*local2base << ','
		       << n[0]/nm << ',' << n[1]/nm << ',' << n[2]/nm;
		    cut = cs.str();
		} else if (type == "transform") {
		    commands.push_back(matrix_command(track, time, duration, local2base));
		} else if (type == "material" || type == "color" || type == "temperature") {
		    commands.push_back(value_command(track, time, duration));
		} else {
		    throw std::runtime_error("unknown animation track type '" + type + "'");
		}
	    }

	    if (!look.empty()) {
		vect_t dir = {look[0]-eye[0], look[1]-eye[1], look[2]-eye[2]};
		if (MAGNITUDE(dir) <= SMALL_FASTF) throw std::runtime_error("camera eye and look_at point coincide");
		VUNITIZE(dir);
		mat_t m; quat_t q;
		if (up.empty()) {
		    bn_mat_lookat(m, dir, 0);
		} else {
		    vect_t upv = {up[0], up[1], up[2]};
		    vect_t right, camera_up;
		    if (MAGNITUDE(upv) <= SMALL_FASTF) throw std::runtime_error("camera up vector must be nonzero");
		    VUNITIZE(upv);
		    VCROSS(right, dir, upv);
		    if (MAGNITUDE(right) <= SMALL_FASTF) throw std::runtime_error("camera up vector is parallel to the viewing direction");
		    VUNITIZE(right);
		    VCROSS(camera_up, right, dir);
		    MAT_IDN(m);
		    VMOVE(&m[0], right);
		    VMOVE(&m[4], camera_up);
		    VREVERSE(&m[8], dir);
		}
		quat_mat2quat(q, m);
		orient = {q[0], q[1], q[2], q[3]};
	    }

	    Tcl_Obj *fd = Tcl_NewDictObj();
	    dict_put(interp, fd, "time", Tcl_NewDoubleObj(time));
	    dict_put(interp, fd, "viewsize", Tcl_NewDoubleObj(viewsize));
	    dict_put(interp, fd, "orientation", tcl_vec(orient));
	    dict_put(interp, fd, "eye_pt", tcl_vec(eye));
	    dict_put(interp, fd, "perspective", Tcl_NewDoubleObj(perspective));
	    dict_put(interp, fd, "cut_plane", Tcl_NewStringObj(cut.c_str(), -1));
	    Tcl_Obj *cl = Tcl_NewListObj(0, NULL);
	    for (const std::string &c : commands)
		Tcl_ListObjAppendElement(interp, cl, Tcl_NewStringObj(c.c_str(), -1));
	    dict_put(interp, fd, "anim_commands", cl);
	    Tcl_ListObjAppendElement(interp, frame_list, fd);
	}
	Tcl_Obj *result = Tcl_NewDictObj();
	dict_put(interp, result, "duration", Tcl_NewDoubleObj(duration));
	dict_put(interp, result, "fps", Tcl_NewIntObj(fps));
	dict_put(interp, result, "cyclic", Tcl_NewBooleanObj(cyclic));
	dict_put(interp, result, "plays", Tcl_NewIntObj(plays));
	dict_put(interp, result, "frames", frame_list);
	Tcl_SetObjResult(interp, result);
	return TCL_OK;
    } catch (const std::exception &e) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(e.what(), -1));
	return TCL_ERROR;
    }
}

static int
save_view_keyframe_cmd(ClientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 8) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "usage: rtwizard_save_view_keyframe file time replace eye orientation viewsize perspective", -1));
	return TCL_ERROR;
    }
    try {
	const char *path = Tcl_GetString(objv[1]);
	double time, viewsize, perspective;
	int replace;
	if (Tcl_GetDoubleFromObj(interp, objv[2], &time) != TCL_OK ||
	    Tcl_GetBooleanFromObj(interp, objv[3], &replace) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[6], &viewsize) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[7], &perspective) != TCL_OK)
	    return TCL_ERROR;
	if (time < 0.0 || !std::isfinite(time) || viewsize <= 0.0)
	    throw std::runtime_error("keyframe time must be nonnegative and view size positive");
	int oc; Tcl_Obj **ov;
	if (Tcl_ListObjGetElements(interp, objv[4], &oc, &ov) != TCL_OK || oc != 3)
	    throw std::runtime_error("eye must contain three numbers");
	json eye = json::array();
	for (int i = 0; i < 3; ++i) { double v; if (Tcl_GetDoubleFromObj(interp, ov[i], &v) != TCL_OK) return TCL_ERROR; eye.push_back(v); }
	if (Tcl_ListObjGetElements(interp, objv[5], &oc, &ov) != TCL_OK || oc != 4)
	    throw std::runtime_error("orientation must contain four numbers");
	json orientation = json::array();
	for (int i = 0; i < 4; ++i) { double v; if (Tcl_GetDoubleFromObj(interp, ov[i], &v) != TCL_OK) return TCL_ERROR; orientation.push_back(v); }

	json root;
	{
	    std::ifstream in(path);
	    if (in) in >> root;
	}
	if (root.is_null()) {
	    root = {{"schema", "brlcad.rtwizard.render"}, {"version", 1}};
	}
	if (root.value("schema", "") != "brlcad.rtwizard.render" || root.value("version", 0) != 1)
	    throw std::runtime_error("view keyframes may only be saved to a version 1 rtwizard render specification");
	json &animation = root["animation"];
	if (!animation.is_object()) animation = json::object();
	animation["units"] = "mm";
	if (animation.contains("preset"))
	    throw std::runtime_error("replace the render specification's animation preset before adding camera keyframes");
	json &timing = animation["timing"];
	if (!timing.is_object()) timing = json::object();
	timing["duration"] = std::max(time, timing.value("duration", time > 0.0 ? time : 1.0));
	if (!timing.contains("fps")) timing["fps"] = 10;
	if (!timing.contains("cyclic")) timing["cyclic"] = false;
	json &tracks = animation["tracks"];
	if (!tracks.is_array()) tracks = json::array();
	size_t camera_index = tracks.size();
	for (size_t i = 0; i < tracks.size(); ++i) {
	    if (tracks[i].is_object() && tracks[i].value("type", "") == "camera") { camera_index = i; break; }
	}
	if (camera_index == tracks.size()) {
	    tracks.push_back({{"type", "camera"}, {"interpolation", "smooth"}, {"keyframes", json::array()}});
	}
	json &keys = tracks[camera_index]["keyframes"];
	if (!keys.is_array()) keys = json::array();
	json key = {{"time", time}, {"eye", eye}, {"orientation", orientation},
	    {"view_size", viewsize}, {"perspective", perspective}};
	bool found = false;
	for (json &existing : keys) {
	    if (existing.is_object() && existing.contains("time") && finite_number(existing["time"]) &&
		std::fabs(existing["time"].get<double>() - time) <= std::numeric_limits<double>::epsilon()) {
		if (!replace) throw std::runtime_error("a camera keyframe already exists at that time; use --replace-keyframe");
		existing = key; found = true; break;
	    }
	}
	if (!found) keys.push_back(key);
	std::sort(keys.begin(), keys.end(), [](const json &a, const json &b) { return a.value("time", 0.0) < b.value("time", 0.0); });

	std::string temp = std::string(path) + ".tmp-" + std::to_string(bu_pid());
	{
	    std::ofstream out(temp, std::ios::trunc);
	    if (!out) throw std::runtime_error("unable to create temporary keyframe file");
	    out << root.dump(2) << '\n';
	    if (!out) throw std::runtime_error("unable to write keyframe file");
	}
	Tcl_Obj *renamev[6] = {
	    Tcl_NewStringObj("file", -1), Tcl_NewStringObj("rename", -1),
	    Tcl_NewStringObj("-force", -1), Tcl_NewStringObj("--", -1),
	    Tcl_NewStringObj(temp.c_str(), -1), Tcl_NewStringObj(path, -1)
	};
	for (Tcl_Obj *obj : renamev) Tcl_IncrRefCount(obj);
	int rename_status = Tcl_EvalObjv(interp, 6, renamev, TCL_EVAL_GLOBAL);
	for (Tcl_Obj *obj : renamev) Tcl_DecrRefCount(obj);
	if (rename_status != TCL_OK) {
	    std::remove(temp.c_str());
	    throw std::runtime_error("unable to replace keyframe file");
	}
	return TCL_OK;
    } catch (const std::exception &e) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(e.what(), -1));
	return TCL_ERROR;
    }
}

} // namespace

extern "C" void
rtwizard_animation_init(Tcl_Interp *interp)
{
    (void)Tcl_CreateObjCommand(interp, "rtwizard_animation_json", animation_json_cmd, NULL, NULL);
    (void)Tcl_CreateObjCommand(interp, "rtwizard_save_view_keyframe", save_view_keyframe_cmd, NULL, NULL);
}

namespace {

static void
push_arg(std::vector<std::string> &args, const char *option, const std::string &value)
{
    args.push_back(option);
    args.push_back(value);
}

static std::string
json_arg(const json &v, const std::string &where)
{
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_float()) {
	if (!finite_number(v)) throw std::runtime_error(where + " must be finite");
	std::ostringstream ss; ss.precision(17); ss << v.get<double>(); return ss.str();
    }
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_array()) {
	std::ostringstream ss;
	for (size_t i = 0; i < v.size(); ++i) {
	    if (!finite_number(v[i])) throw std::runtime_error(where + " must contain numbers");
	    if (i) ss << '/';
	    ss.precision(17); ss << v[i].get<double>();
	}
	return ss.str();
    }
    throw std::runtime_error(where + " has an unsupported value");
}

static std::string
file_arg(const json &v, const fs::path &base, const std::string &where)
{
    if (!v.is_string() || v.get<std::string>().empty())
	throw std::runtime_error(where + " must be a nonempty filename");
    fs::path p(v.get<std::string>());
    if (p.is_relative()) p = base / p;
    return p.lexically_normal().string();
}

static bool
boolean_arg(const json &section, const char *key, const std::string &where)
{
    if (!section.contains(key)) return false;
    if (!section[key].is_boolean())
	throw std::runtime_error(where + "." + key + " must be true or false");
    return section[key].get<bool>();
}

static void
section_args(std::vector<std::string> &args, const json &section,
	const std::vector<std::pair<std::string, std::string> > &mapping,
	const std::string &where)
{
    std::vector<std::string> allowed;
    for (const auto &m : mapping) allowed.push_back(m.first);
    check_keys(section, allowed, where);
    for (const auto &m : mapping) {
	if (section.contains(m.first)) push_arg(args, m.second.c_str(), json_arg(section[m.first], where + "." + m.first));
    }
}

static std::vector<std::string>
render_spec_args(const char *path)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error(std::string("unable to open render specification '") + path + "'");
    json root; in >> root;
    check_keys(root, {"schema", "version", "name", "description", "database", "objects", "image", "output",
	    "view", "style", "runtime", "animation"}, "render specification");
    if (root.value("schema", "") != "brlcad.rtwizard.render" || root.value("version", 0) != 1)
	throw std::runtime_error("render specification must use brlcad.rtwizard.render version 1");
    fs::path spec_path = fs::absolute(fs::path(path));
    fs::path base = spec_path.parent_path();
    std::vector<std::string> args;
    if (root.contains("database")) push_arg(args, "--input-file", file_arg(root["database"], base, "database"));
    if (root.contains("objects")) {
	const json &o = root["objects"];
	check_keys(o, {"color", "ghost", "line"}, "objects");
	for (const auto &entry : {std::make_pair("color", "--color-objects"),
		std::make_pair("ghost", "--ghost-objects"), std::make_pair("line", "--line-objects")}) {
	    if (!o.contains(entry.first)) continue;
	    if (!o[entry.first].is_array()) throw std::runtime_error(std::string("objects.") + entry.first + " must be an array");
	    std::ostringstream ss;
	    for (size_t i = 0; i < o[entry.first].size(); ++i) {
		if (!o[entry.first][i].is_string()) throw std::runtime_error("object names must be strings");
		if (i) ss << ',';
		ss << o[entry.first][i].get<std::string>();
	    }
	    push_arg(args, entry.second, ss.str());
	}
    }
    if (root.contains("image")) section_args(args, root["image"], {
	{"size", "--size"}, {"width", "--width"}, {"height", "--height"}, {"type", "--type"}}, "image");
    if (root.contains("output")) {
	const json &o = root["output"];
	check_keys(o, {"file", "frame_dir", "framebuffer", "framebuffer_port", "resume"}, "output");
	if (o.contains("file")) push_arg(args, "--output-file", file_arg(o["file"], base, "output.file"));
	if (o.contains("frame_dir")) push_arg(args, "--frame-dir", file_arg(o["frame_dir"], base, "output.frame_dir"));
	if (o.contains("framebuffer")) push_arg(args, "--fbserv-device", json_arg(o["framebuffer"], "output.framebuffer"));
	if (o.contains("framebuffer_port")) push_arg(args, "--fbserv-port", json_arg(o["framebuffer_port"], "output.framebuffer_port"));
	if (boolean_arg(o, "resume", "output")) args.push_back("--resume");
    }
    if (root.contains("view")) section_args(args, root["view"], {
	{"azimuth", "--azimuth"}, {"elevation", "--elevation"}, {"twist", "--twist"},
	{"zoom", "--zoom"}, {"center", "--center"}, {"eye", "--eye_pt"},
	{"view_size", "--viewsize"}, {"orientation", "--orientation"}, {"perspective", "--perspective"}}, "view");
    if (root.contains("style")) section_args(args, root["style"], {
	{"background", "--background-color"}, {"line_color", "--line-color"},
	{"non_line_color", "--non-line-color"}, {"ghost_intensity", "--ghost-intensity"},
	{"occlusion", "--occlusion"}, {"ao_samples", "--ao-samples"}, {"ao_radius", "--ao-radius"}}, "style");
    if (root.contains("runtime")) {
	const json &r = root["runtime"];
	check_keys(r, {"benchmark", "cpu_count", "verbose", "log_file", "pid_file", "gui", "no_gui"}, "runtime");
	if (boolean_arg(r, "benchmark", "runtime")) args.push_back("--benchmark");
	if (r.contains("cpu_count")) push_arg(args, "--cpu-count", json_arg(r["cpu_count"], "runtime.cpu_count"));
	if (r.contains("verbose")) push_arg(args, "--verbose", json_arg(r["verbose"], "runtime.verbose"));
	if (r.contains("log_file")) push_arg(args, "--log-file", file_arg(r["log_file"], base, "runtime.log_file"));
	if (r.contains("pid_file")) push_arg(args, "--pid-file", file_arg(r["pid_file"], base, "runtime.pid_file"));
	if (boolean_arg(r, "gui", "runtime")) args.push_back("--gui");
	if (boolean_arg(r, "no_gui", "runtime")) args.push_back("--no-gui");
    }
    if (root.contains("animation")) {
	const json &a = root["animation"];
	check_keys(a, {"schema", "version", "preset", "units", "timing", "options", "tracks"}, "animation");
	if (!a.contains("preset") && !a.contains("tracks"))
	    throw std::runtime_error("animation requires a preset or tracks");
	if (a.contains("options") && !a.contains("preset"))
	    throw std::runtime_error("animation options require a preset");
	if (a.contains("timing")) {
	    const json &tm = a["timing"];
	    check_keys(tm, {"duration", "fps", "frames", "cyclic", "plays"}, "animation.timing");
	    if (tm.contains("duration")) push_arg(args, "--animation-duration", json_arg(tm["duration"], "animation.timing.duration"));
	    if (tm.contains("fps")) push_arg(args, "--animation-fps", json_arg(tm["fps"], "animation.timing.fps"));
	    if (tm.contains("frames")) push_arg(args, "--animation-frames", json_arg(tm["frames"], "animation.timing.frames"));
	    if (tm.contains("plays")) push_arg(args, "--animation-plays", json_arg(tm["plays"], "animation.timing.plays"));
	    if (tm.contains("cyclic")) {
		if (!tm["cyclic"].is_boolean()) throw std::runtime_error("animation.timing.cyclic must be true or false");
		push_arg(args, "--animation-cyclic", tm["cyclic"].get<bool>() ? "1" : "0");
	    }
	}
	if (a.contains("options")) section_args(args, a["options"], {
	    {"cut_direction", "--cut-direction"}, {"orbit_angle", "--orbit-angle"},
	    {"orbit_axis", "--orbit-axis"}, {"orbit_center", "--orbit-center"},
	    {"orbit_elevation", "--orbit-elevation"}, {"orbit_radius", "--orbit-radius"},
	    {"turntable_object", "--turntable-object"}, {"turntable_angle", "--turntable-angle"},
	    {"turntable_axis", "--turntable-axis"}, {"turntable_center", "--turntable-center"}}, "animation.options");
	if (a.contains("preset")) push_arg(args, "--animation", json_arg(a["preset"], "animation.preset"));
	if (a.contains("tracks")) {
	    if (a.contains("preset")) throw std::runtime_error("animation may contain a preset or tracks, not both");
	    if (a.contains("options")) throw std::runtime_error("animation options are only valid with a preset");
	    push_arg(args, "--animation-file", spec_path.string());
	}
    }
    return args;
}

} // namespace

extern "C" int
rtwizard_spec_to_argv(const char *path, int *argc, char ***argv, char **errmsg)
{
    if (!path || !argc || !argv) return -1;
    try {
	std::vector<std::string> a = render_spec_args(path);
	*argc = (int)a.size();
	*argv = (char **)bu_calloc(a.size() + 1, sizeof(char *), "rtwizard render spec argv");
	for (size_t i = 0; i < a.size(); ++i) (*argv)[i] = bu_strdup(a[i].c_str());
	if (errmsg) *errmsg = NULL;
	return 0;
    } catch (const std::exception &e) {
	if (errmsg) *errmsg = bu_strdup(e.what());
	return -1;
    }
}

extern "C" void
rtwizard_spec_argv_free(int argc, char **argv)
{
    if (!argv) return;
    for (int i = 0; i < argc; ++i) bu_free(argv[i], "rtwizard render spec arg");
    bu_free(argv, "rtwizard render spec argv");
}
