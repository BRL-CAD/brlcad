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
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "tcl.h"
#include "vmath.h"
#include "bn/mat.h"
#include "bn/qmath.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/units.h"
#include "raytrace.h"

#include "../libbu/json.hpp"
#include "animation.h"
#include "animation_plan.h"
#include "preset_plan.h"
#include "scene_query.h"
#include "settings.h"

using json = nlohmann::json;

namespace {

namespace fs = std::filesystem;

struct JKey {
    double time;
    json value;
};

struct CompiledTrack {
    json definition;
    std::string type;
    std::map<std::string, std::vector<JKey>> keys;
    std::map<std::string, std::string> interpolation;
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

static CompiledTrack
compile_track(const json &track, double duration)
{
    CompiledTrack compiled;
    compiled.definition = track;
    compiled.type = track.value("type", "");
    validate_track(track, compiled.type);

    std::vector<std::string> properties;
    if (compiled.type == "camera")
	properties = {"eye", "look_at", "up", "orientation", "view_size", "perspective"};
    else if (compiled.type == "cut_plane")
	properties = {"point", "normal"};
    else if (compiled.type == "transform")
	properties = {"translation", "rotation", "scale", "matrix"};
    else if (compiled.type == "material")
	properties = {"value", "material"};
    else if (compiled.type == "color")
	properties = {"value", "color"};
    else if (compiled.type == "temperature")
	properties = {"value", "temperature"};

    for (const std::string &property : properties) {
	compiled.keys[property] = property_keys(track, property.c_str(), duration);
	compiled.interpolation[property] = interpolation_for(track, property.c_str());
    }
    return compiled;
}

static const std::vector<JKey> &
track_keys(const CompiledTrack &track, const std::string &property)
{
    static const std::vector<JKey> empty;
    std::map<std::string, std::vector<JKey>>::const_iterator found = track.keys.find(property);
    return found == track.keys.end() ? empty : found->second;
}

static const std::string &
track_interpolation(const CompiledTrack &track, const std::string &property)
{
    static const std::string linear = "linear";
    std::map<std::string, std::string>::const_iterator found = track.interpolation.find(property);
    return found == track.interpolation.end() ? linear : found->second;
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
	double result = 0.5 * ((2.0*b) + (-a+c)*f + (2.0*a-5.0*b+4.0*c-d)*f2 + (-a+3.0*b-3.0*c+d)*f3);
	return std::max(std::min(b, c), std::min(std::max(b, c), result));
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

static DatabaseOperation
matrix_operation(const CompiledTrack &track, double t, double local2base)
{
    const json &definition = track.definition;
    std::string path = command_path(definition, "transform");
    std::string op = definition.value("operation", "right_multiply");
    if (op == "replace_stack") op = "rstack";
    else if (op == "replace_arc") op = "rarc";
    else if (op == "left_multiply") op = "lmul";
    else if (op == "right_multiply") op = "rmul";
    else if (op == "replace_both") op = "rboth";
    else throw std::runtime_error("unknown transform operation");

    const std::vector<JKey> &mk = track_keys(track, "matrix");
    const std::vector<JKey> &tk = track_keys(track, "translation");
    const std::vector<JKey> &rk = track_keys(track, "rotation");
    const std::vector<JKey> &sk = track_keys(track, "scale");
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
	if (!tk.empty()) tr = number_array(evaluate_keys(tk, t, track_interpolation(track, "translation")), 3, "translation");
	if (!rk.empty()) q = evaluate_quat(rk, t, track_interpolation(track, "rotation"));
	if (!sk.empty()) {
	    json sv = evaluate_keys(sk, t, track_interpolation(track, "scale"));
	    if (!finite_number(sv) || std::fabs(sv.get<double>()) < 1.0e-12)
		throw std::runtime_error("transform scale must be finite and nonzero");
	    scale = sv.get<double>();
	}
	quat_t qq = {q[0], q[1], q[2], q[3]};
	quat_quat2mat(mat, qq);
	mat[15] = 1.0 / scale;
	if (definition.contains("pivot")) {
	    std::vector<double> pv = number_array(definition["pivot"], 3, "transform pivot");
	    point_t p = {pv[0]*local2base, pv[1]*local2base, pv[2]*local2base};
	    mat_t about;
	    bn_mat_xform_about_pnt(about, mat, p);
	    MAT_COPY(mat, about);
	}
	mat[3] += tr[0]*local2base; mat[7] += tr[1]*local2base; mat[11] += tr[2]*local2base;
    }

    DatabaseOperation operation;
    operation.type = DatabaseOperationType::Transform;
    operation.path = std::move(path);
    operation.action = std::move(op);
    operation.values.assign(mat, mat + 16);
    return operation;
}

static DatabaseOperation
value_operation(const CompiledTrack &track, double t)
{
    const json &definition = track.definition;
    const std::string &type = track.type;
    std::string path = command_path(definition, type);
    const std::vector<JKey> *keys = &track_keys(track, "value");
    const char *property = type == "color" ? "color" : (type == "temperature" ? "temperature" : "material");
    const char *interpolation_property = "value";
    if (keys->empty()) {
	keys = &track_keys(track, property);
	interpolation_property = property;
    }
    if (keys->empty()) throw std::runtime_error(type + " track has no values");

    DatabaseOperation operation;
    operation.path = std::move(path);
    if (type == "color") {
	json v = evaluate_keys(*keys, t, track_interpolation(track, interpolation_property));
	std::vector<double> rgb = number_array(v, 3, "color");
	operation.type = DatabaseOperationType::Color;
	for (double &component : rgb) {
	    if (component < 0.0 || component > 255.0) throw std::runtime_error("color channels must be in [0,255]");
	    component = (double)std::lround(component);
	}
	operation.values = std::move(rgb);
    } else if (type == "temperature") {
	json v = evaluate_keys(*keys, t, track_interpolation(track, interpolation_property));
	if (!finite_number(v)) throw std::runtime_error("temperature must be numeric");
	operation.type = DatabaseOperationType::Temperature;
	operation.values.push_back(v.get<double>());
    } else {
	json v = evaluate_keys(*keys, t, "step");
	std::string op = definition.value("operation", "replace");
	if (op != "replace" && op != "append") throw std::runtime_error("material operation must be replace or append");
	std::string material = material_string(v);
	if (material.find(';') != std::string::npos || material.find('\n') != std::string::npos || material.find('\r') != std::string::npos)
	    throw std::runtime_error("material value may not contain command separators");
	operation.type = DatabaseOperationType::Material;
	operation.action = std::move(op);
	operation.text = std::move(material);
    }
    return operation;
}

static std::string
operation_command(const DatabaseOperation &operation)
{
    std::ostringstream command;
    command.precision(17);
    command << "anim " << operation.path;
    switch (operation.type) {
	case DatabaseOperationType::Transform:
	    command << " matrix " << operation.action;
	    for (double value : operation.values) command << ' ' << value;
	    break;
	case DatabaseOperationType::Material:
	    command << " material " << operation.action << ' ' << operation.text;
	    break;
	case DatabaseOperationType::Color:
	    command << " color";
	    for (double value : operation.values) command << ' ' << (int)value;
	    break;
	case DatabaseOperationType::Temperature:
	    command << " temperature " << operation.values.front();
	    break;
    }
    return command.str();
}

static Tcl_Obj *
animation_plan_obj(Tcl_Interp *interp, const AnimationPlan &plan)
{
    Tcl_Obj *frames = Tcl_NewListObj(0, NULL);
    for (const AnimationFrame &frame : plan.frames) {
	Tcl_Obj *frame_dict = Tcl_NewDictObj();
	dict_put(interp, frame_dict, "time", Tcl_NewDoubleObj(frame.time));
	dict_put(interp, frame_dict, "viewsize", Tcl_NewDoubleObj(frame.viewsize));
	dict_put(interp, frame_dict, "orientation", tcl_vec(frame.orientation));
	dict_put(interp, frame_dict, "eye_pt", tcl_vec(frame.eye));
	dict_put(interp, frame_dict, "perspective", Tcl_NewDoubleObj(frame.perspective));
	dict_put(interp, frame_dict, "cut_plane", Tcl_NewStringObj(frame.cut_plane.c_str(), -1));
	Tcl_Obj *commands = Tcl_NewListObj(0, NULL);
	for (const DatabaseOperation &operation : frame.operations) {
	    std::string command = operation_command(operation);
	    Tcl_ListObjAppendElement(interp, commands, Tcl_NewStringObj(command.c_str(), -1));
	}
	dict_put(interp, frame_dict, "anim_commands", commands);
	Tcl_ListObjAppendElement(interp, frames, frame_dict);
    }
    Tcl_Obj *result = Tcl_NewDictObj();
    dict_put(interp, result, "duration", Tcl_NewDoubleObj(plan.duration));
    dict_put(interp, result, "fps", Tcl_NewIntObj(plan.fps));
    dict_put(interp, result, "cyclic", Tcl_NewBooleanObj(plan.cyclic));
    dict_put(interp, result, "plays", Tcl_NewIntObj(plan.plays));
    dict_put(interp, result, "frames", frames);
    return result;
}

static Tcl_Obj *
config_value(Tcl_Interp *interp, Tcl_Obj *config, const char *key, bool required = true)
{
    Tcl_Obj *value = NULL;
    Tcl_Obj *key_object = Tcl_NewStringObj(key, -1);
    Tcl_IncrRefCount(key_object);
    int status = Tcl_DictObjGet(interp, config, key_object, &value);
    Tcl_DecrRefCount(key_object);
    if (status != TCL_OK)
	throw std::runtime_error("animation preset configuration must be a dictionary");
    if (!value && required) throw std::runtime_error(std::string("animation preset configuration is missing '") + key + "'");
    return value;
}

static double
config_double(Tcl_Interp *interp, Tcl_Obj *config, const char *key)
{
    double value;
    if (Tcl_GetDoubleFromObj(interp, config_value(interp, config, key), &value) != TCL_OK || !std::isfinite(value))
	throw std::runtime_error(std::string("animation preset '") + key + "' must be finite");
    return value;
}

static int
config_int(Tcl_Interp *interp, Tcl_Obj *config, const char *key)
{
    int value;
    if (Tcl_GetIntFromObj(interp, config_value(interp, config, key), &value) != TCL_OK)
	throw std::runtime_error(std::string("animation preset '") + key + "' must be an integer");
    return value;
}

static std::string
config_string(Tcl_Interp *interp, Tcl_Obj *config, const char *key)
{
    return Tcl_GetString(config_value(interp, config, key));
}

static std::vector<double>
tcl_number_list(Tcl_Interp *interp, Tcl_Obj *value, size_t count, const std::string &where)
{
    int object_count = 0;
    Tcl_Obj **objects = NULL;
    if (Tcl_ListObjGetElements(interp, value, &object_count, &objects) != TCL_OK || object_count != (int)count)
	throw std::runtime_error(where + " must contain " + std::to_string(count) + " numbers");
    std::vector<double> result(count);
    for (size_t i = 0; i < count; ++i) {
	if (Tcl_GetDoubleFromObj(interp, objects[i], &result[i]) != TCL_OK || !std::isfinite(result[i]))
	    throw std::runtime_error(where + " must contain finite numbers");
    }
    return result;
}

static std::vector<double>
config_vector(Tcl_Interp *interp, Tcl_Obj *config, const char *key, size_t count)
{
    return tcl_number_list(interp, config_value(interp, config, key), count, std::string("animation preset '") + key + "'");
}

static std::vector<std::string>
config_strings(Tcl_Interp *interp, Tcl_Obj *config, const char *key)
{
    Tcl_Obj *value = config_value(interp, config, key);
    int object_count = 0;
    Tcl_Obj **objects = NULL;
    if (Tcl_ListObjGetElements(interp, value, &object_count, &objects) != TCL_OK)
	throw std::runtime_error(std::string("animation preset '") + key + "' must be a list");
    std::vector<std::string> result;
    result.reserve((size_t)object_count);
    for (int i = 0; i < object_count; ++i) result.emplace_back(Tcl_GetString(objects[i]));
    return result;
}

static bool
config_optional_double(Tcl_Interp *interp, Tcl_Obj *config, const char *key, double &result)
{
    Tcl_Obj *value = config_value(interp, config, key, false);
    if (!value) return false;
    if (Tcl_GetDoubleFromObj(interp, value, &result) != TCL_OK || !std::isfinite(result))
	throw std::runtime_error(std::string("animation preset '") + key + "' must be finite");
    return true;
}

static PresetSettings
parse_preset_settings(Tcl_Interp *interp, Tcl_Obj *config)
{
    PresetSettings settings;
    settings.preset = config_string(interp, config, "preset");
    settings.database = config_string(interp, config, "database");
    settings.objects = config_strings(interp, config, "objects");
    settings.duration = config_double(interp, config, "duration");
    settings.fps = config_int(interp, config, "fps");
    settings.frame_count = config_int(interp, config, "frames");
    settings.cyclic_override = config_int(interp, config, "cyclic");
    settings.plays = config_int(interp, config, "plays");
    settings.viewsize = config_double(interp, config, "viewsize");
    settings.perspective = config_double(interp, config, "perspective");
    settings.orientation = config_vector(interp, config, "orientation", 4);
    settings.eye = config_vector(interp, config, "eye", 3);

    if (settings.preset == "cut") {
	settings.cut_steps = config_int(interp, config, "cut_steps");
	settings.width = config_int(interp, config, "width");
	settings.height = config_int(interp, config, "height");
	Tcl_Obj *direction = config_value(interp, config, "cut_direction", false);
	if (direction) {
	    settings.direction = tcl_number_list(interp, direction, 3, "cut direction");
	} else {
	    quat_t quaternion = {settings.orientation[0], settings.orientation[1],
		settings.orientation[2], settings.orientation[3]};
	    mat_t matrix;
	    quat_quat2mat(matrix, quaternion);
	    settings.direction = {-matrix[8], -matrix[9], -matrix[10]};
	}
	normalize_animation_vector(settings.direction, "cut direction");
	return settings;
    }

    if (settings.preset == "orbit") {
	settings.angle = config_double(interp, config, "orbit_angle");
	settings.axis = config_vector(interp, config, "orbit_axis", 3);
	normalize_animation_vector(settings.axis, "orbit axis");
	Tcl_Obj *center = config_value(interp, config, "orbit_center", false);
	if (center) {
	    settings.center = tcl_number_list(interp, center, 3, "orbit center");
	    settings.has_center = true;
	}
	settings.has_radius = config_optional_double(interp, config, "orbit_radius", settings.radius);
	settings.has_elevation = config_optional_double(interp, config, "orbit_elevation", settings.elevation);
	return settings;
    }

    if (settings.preset == "turntable") {
	settings.angle = config_double(interp, config, "turntable_angle");
	settings.axis = config_vector(interp, config, "turntable_axis", 3);
	normalize_animation_vector(settings.axis, "turntable axis");
	settings.object = config_string(interp, config, "turntable_object");
	if (!settings.object.empty() && settings.object.find_first_of(";\r\n\t ") != std::string::npos)
	    throw std::runtime_error("turntable object path may not contain whitespace or command separators");
	Tcl_Obj *center = config_value(interp, config, "turntable_center", false);
	if (center) {
	    settings.center = tcl_number_list(interp, center, 3, "turntable center");
	    settings.has_center = true;
	}
	return settings;
    }

    throw std::runtime_error("unknown animation preset '" + settings.preset + "'");
}

static std::vector<double>
scene_center(const SceneBounds &bounds)
{
    return {
	(bounds.minimum[X]+bounds.maximum[X])/2.0,
	(bounds.minimum[Y]+bounds.maximum[Y])/2.0,
	(bounds.minimum[Z]+bounds.maximum[Z])/2.0
    };
}

static PresetSceneFacts
query_preset_scene(const PresetSettings &settings)
{
    PresetSceneFacts facts;
    if (settings.preset == "cut") {
	SceneQuery query(settings.database);
	facts.sweep = query.projected_bounds(settings.objects, settings.direction);
	return facts;
    }
    if (settings.has_center) {
	facts.center = settings.center;
	return facts;
    }
    SceneQuery query(settings.database);
    if (settings.preset == "orbit") facts.center = scene_center(query.bounds(settings.objects));
    else if (settings.preset == "turntable") {
	std::vector<std::string> objects = settings.object.empty() ? settings.objects : std::vector<std::string>{settings.object};
	facts.center = scene_center(query.bounds(objects));
    }
    return facts;
}

static AnimationPlan
plan_preset(Tcl_Interp *interp, Tcl_Obj *config)
{
    PresetSettings settings = parse_preset_settings(interp, config);
    PresetSceneFacts facts = query_preset_scene(settings);
    return plan_animation_preset(settings, facts);
}

static int
animation_preset_cmd(ClientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("usage: rtwizard_animation_preset configuration_dict", -1));
	return TCL_ERROR;
    }
    try {
	Tcl_SetObjResult(interp, animation_plan_obj(interp, plan_preset(interp, objv[1])));
	return TCL_OK;
    } catch (const std::exception &exception) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(exception.what(), -1));
	return TCL_ERROR;
    }
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
	    if (root.contains("schema") && root.value("schema", "") != "brlcad.rtwizard.animation")
		throw std::runtime_error("unsupported animation schema");
	    if (root.contains("version") && root.value("version", 0) != 1)
		throw std::runtime_error("unsupported animation version");
	}
	check_keys(root, {"schema", "version", "preset", "units", "timing", "options", "tracks"}, "animation");
	if (root.contains("schema") && root.value("schema", "") != "brlcad.rtwizard.animation")
	    throw std::runtime_error("animation schema must be brlcad.rtwizard.animation");
	if (root.contains("version") && root.value("version", 0) != 1)
	    throw std::runtime_error("animation version must be 1");
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
	std::vector<CompiledTrack> tracks;
	tracks.reserve(root["tracks"].size());
	for (const json &track : root["tracks"])
	    tracks.push_back(compile_track(track, duration));

	AnimationPlan plan;
	plan.duration = duration;
	plan.fps = fps;
	plan.cyclic = cyclic;
	plan.plays = plays;
	plan.frames.reserve((size_t)frame_count);
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
	    std::vector<DatabaseOperation> operations;

	    for (const CompiledTrack &track : tracks) {
		const std::string &type = track.type;
		if (type == "camera") {
		    const std::vector<JKey> &ek = track_keys(track, "eye");
		    const std::vector<JKey> &lk = track_keys(track, "look_at");
		    const std::vector<JKey> &uk = track_keys(track, "up");
		    const std::vector<JKey> &ok = track_keys(track, "orientation");
		    if (!lk.empty() && !ok.empty()) throw std::runtime_error("camera track may not mix look_at and orientation");
		    if (!uk.empty() && lk.empty()) throw std::runtime_error("camera up requires look_at values in the same track");
		    if (!ek.empty()) {
			eye = number_array(evaluate_keys(ek, time, track_interpolation(track, "eye")), 3, "camera eye");
			for (double &v : eye) v *= local2base;
		    }
		    if (!lk.empty()) {
			look = number_array(evaluate_keys(lk, time, track_interpolation(track, "look_at")), 3, "camera look_at");
			for (double &v : look) v *= local2base;
			up.clear();
		    }
		    if (!uk.empty()) up = number_array(evaluate_keys(uk, time, track_interpolation(track, "up")), 3, "camera up");
		    if (!ok.empty()) {
			orient = evaluate_quat(ok, time, track_interpolation(track, "orientation"));
			look.clear();
			up.clear();
		    }
		    const std::vector<JKey> &vk = track_keys(track, "view_size");
		    if (!vk.empty()) {
			json v = evaluate_keys(vk, time, track_interpolation(track, "view_size"));
			if (!finite_number(v) || v.get<double>() <= 0.0) throw std::runtime_error("view_size must be positive");
			viewsize = v.get<double>() * local2base;
		    }
		    const std::vector<JKey> &pk = track_keys(track, "perspective");
		    if (!pk.empty()) {
			json v = evaluate_keys(pk, time, track_interpolation(track, "perspective"));
			if (!finite_number(v)) throw std::runtime_error("perspective must be finite");
			perspective = v.get<double>();
		    }
		} else if (type == "cut_plane") {
		    const std::vector<JKey> &pk = track_keys(track, "point");
		    const std::vector<JKey> &nk = track_keys(track, "normal");
		    if (pk.empty() || nk.empty()) throw std::runtime_error("cut_plane requires point and normal values");
		    std::vector<double> p = number_array(evaluate_keys(pk, time, track_interpolation(track, "point")), 3, "cut point");
		    std::vector<double> n = number_array(evaluate_keys(nk, time, track_interpolation(track, "normal")), 3, "cut normal");
		    double nm = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
		    if (!(nm > 1.0e-12)) throw std::runtime_error("cut normal must be nonzero");
		    std::ostringstream cs; cs.precision(17);
		    cs << p[0]*local2base << ',' << p[1]*local2base << ',' << p[2]*local2base << ','
		       << n[0]/nm << ',' << n[1]/nm << ',' << n[2]/nm;
		    cut = cs.str();
		} else if (type == "transform") {
		    operations.push_back(matrix_operation(track, time, local2base));
		} else if (type == "material" || type == "color" || type == "temperature") {
		    operations.push_back(value_operation(track, time));
		} else {
		    throw std::runtime_error("unknown animation track type '" + type + "'");
		}
	    }

	    if (!look.empty()) {
		std::vector<double> direction = {look[0]-eye[0], look[1]-eye[1], look[2]-eye[2]};
		normalize_animation_vector(direction, "camera viewing direction");
		orient = animation_look_at_quaternion(direction, up);
	    }

	    AnimationFrame frame = make_animation_frame(time, viewsize, orient, eye, perspective);
	    frame.cut_plane = std::move(cut);
	    frame.operations = std::move(operations);
	    plan.frames.push_back(std::move(frame));
	}
	Tcl_SetObjResult(interp, animation_plan_obj(interp, plan));
	return TCL_OK;
    } catch (const std::exception &e) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(e.what(), -1));
	return TCL_ERROR;
    }
}

static int
save_view_keyframe_cmd(ClientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc != 9) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "usage: rtwizard_save_view_keyframe file time replace eye orientation viewsize perspective local2base", -1));
	return TCL_ERROR;
    }
    try {
	const char *path = Tcl_GetString(objv[1]);
	double time, viewsize, perspective, database_local2base;
	int replace;
	if (Tcl_GetDoubleFromObj(interp, objv[2], &time) != TCL_OK ||
	    Tcl_GetBooleanFromObj(interp, objv[3], &replace) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[6], &viewsize) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[7], &perspective) != TCL_OK ||
	    Tcl_GetDoubleFromObj(interp, objv[8], &database_local2base) != TCL_OK)
	    return TCL_ERROR;
	if (time < 0.0 || !std::isfinite(time) || viewsize <= 0.0 || !(database_local2base > 0.0))
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
	if (animation.is_null()) animation = json::object();
	if (!animation.is_object()) throw std::runtime_error("animation must be an object");
	double capture_local2base = 1.0;
	if (animation.contains("units")) {
	    if (!animation["units"].is_string()) throw std::runtime_error("animation units must be a string");
	    std::string units = animation["units"].get<std::string>();
	    capture_local2base = units == "database" ? database_local2base : bu_units_conversion(units.c_str());
	    if (!(capture_local2base > 0.0)) throw std::runtime_error("unknown animation distance unit '" + units + "'");
	} else {
	    animation["units"] = "mm";
	}
	for (json &coordinate : eye) coordinate = coordinate.get<double>() / capture_local2base;
	viewsize /= capture_local2base;
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
	    if (!tracks[i].is_object() || tracks[i].value("type", "") != "camera") continue;
	    bool uses_look_at = false;
	    if (tracks[i].contains("keyframes") && tracks[i]["keyframes"].is_array()) {
		for (const json &existing : tracks[i]["keyframes"]) {
		    if (existing.is_object() && (existing.contains("look_at") || existing.contains("up"))) {
			uses_look_at = true;
			break;
		    }
		}
	    }
	    if (!uses_look_at) { camera_index = i; break; }
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
		for (json::const_iterator item = key.begin(); item != key.end(); ++item)
		    existing[item.key()] = item.value();
		found = true;
		break;
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
	    bu_file_delete(temp.c_str());
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
    (void)Tcl_CreateObjCommand(interp, "rtwizard_animation_preset", animation_preset_cmd, NULL, NULL);
    (void)Tcl_CreateObjCommand(interp, "rtwizard_save_view_keyframe", save_view_keyframe_cmd, NULL, NULL);
}

namespace {

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
boolean_value(const json &section, const char *key, const std::string &where)
{
    if (!section.contains(key)) return false;
    if (!section[key].is_boolean())
	throw std::runtime_error(where + "." + key + " must be true or false");
    return section[key].get<bool>();
}

static double
number_value(const json &value, const std::string &where)
{
    if (!finite_number(value)) throw std::runtime_error(where + " must be finite");
    return value.get<double>();
}

static int
integer_value(const json &value, const std::string &where)
{
    if (!value.is_number_integer()) throw std::runtime_error(where + " must be an integer");
    long long result = value.get<long long>();
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
	throw std::runtime_error(where + " is outside the supported integer range");
    return (int)result;
}

static size_t
size_value(const json &value, const std::string &where)
{
    int result = integer_value(value, where);
    if (result <= 0) throw std::runtime_error(where + " must be positive");
    return (size_t)result;
}

static void
vector_value(fastf_t *out, const json &value, size_t count, const std::string &where)
{
    std::vector<double> numbers = number_array(value, count, where);
    for (size_t i = 0; i < count; ++i) out[i] = numbers[i];
}

static void
color_value(struct bu_color *color, const json &value, const std::string &where)
{
    if (value.is_string()) {
	if (!bu_color_from_str(color, value.get<std::string>().c_str()))
	    throw std::runtime_error(where + " is not a recognized color");
	return;
    }
    std::vector<double> channels = number_array(value, 3, where);
    unsigned char rgb[3];
    for (size_t i = 0; i < 3; ++i) {
	if (channels[i] < 0.0 || channels[i] > 255.0)
	    throw std::runtime_error(where + " channels must be in [0,255]");
	rgb[i] = (unsigned char)std::lround(channels[i]);
    }
    (void)bu_color_from_rgb_chars(color, rgb);
}

static void
set_file(struct bu_vls *target, const json &value, const fs::path &base, const std::string &where)
{
    bu_vls_strcpy(target, file_arg(value, base, where).c_str());
}

static void
set_string(struct bu_vls *target, const json &value, const std::string &where)
{
    if (!value.is_string()) throw std::runtime_error(where + " must be a string");
    bu_vls_strcpy(target, value.get<std::string>().c_str());
}

static void
apply_animation(const json &animation, const fs::path &source_path, struct rtwizard_settings *settings)
{
    check_keys(animation, {"schema", "version", "preset", "units", "timing", "options", "tracks"}, "animation");
    if (animation.contains("schema") && animation.value("schema", "") != "brlcad.rtwizard.animation")
	throw std::runtime_error("animation schema must be brlcad.rtwizard.animation");
    if (animation.contains("version") && animation.value("version", 0) != 1)
	throw std::runtime_error("animation version must be 1");
    if (animation.contains("preset") && animation.contains("tracks"))
	throw std::runtime_error("animation may contain a preset or tracks, not both");

    if (animation.contains("preset")) {
	set_string(settings->animation_preset, animation["preset"], "animation.preset");
	bu_vls_trunc(settings->animation_file, 0);
    }
    if (animation.contains("tracks")) {
	bu_vls_trunc(settings->animation_preset, 0);
	bu_vls_strcpy(settings->animation_file, source_path.string().c_str());
    }
    if (!bu_vls_strlen(settings->animation_preset) && !bu_vls_strlen(settings->animation_file))
	throw std::runtime_error("animation requires --animate, a preset, or tracks");
    if (animation.contains("options") && !bu_vls_strlen(settings->animation_preset))
	throw std::runtime_error("animation options require a preset");

    if (animation.contains("timing")) {
	const json &timing = animation["timing"];
	check_keys(timing, {"duration", "fps", "frames", "cyclic", "plays"}, "animation.timing");
	if (timing.contains("duration")) settings->animation_duration = number_value(timing["duration"], "animation.timing.duration");
	if (timing.contains("fps")) settings->animation_fps = integer_value(timing["fps"], "animation.timing.fps");
	if (timing.contains("frames")) settings->animation_frames = integer_value(timing["frames"], "animation.timing.frames");
	if (timing.contains("cyclic")) settings->animation_cyclic = boolean_value(timing, "cyclic", "animation.timing") ? 1 : 0;
	if (timing.contains("plays")) settings->animation_plays = integer_value(timing["plays"], "animation.timing.plays");
    }
    if (animation.contains("options")) {
	const json &options = animation["options"];
	check_keys(options, {"cut_direction", "orbit_angle", "orbit_axis", "orbit_center", "orbit_elevation", "orbit_radius",
		"turntable_object", "turntable_angle", "turntable_axis", "turntable_center"}, "animation.options");
	if (options.contains("cut_direction")) { vector_value(settings->cut_direction, options["cut_direction"], 3, "animation.options.cut_direction"); settings->cut_direction_set = 1; }
	if (options.contains("orbit_angle")) settings->orbit_angle = number_value(options["orbit_angle"], "animation.options.orbit_angle");
	if (options.contains("orbit_axis")) vector_value(settings->orbit_axis, options["orbit_axis"], 3, "animation.options.orbit_axis");
	if (options.contains("orbit_center")) vector_value(settings->orbit_center, options["orbit_center"], 3, "animation.options.orbit_center");
	if (options.contains("orbit_elevation")) settings->orbit_elevation = number_value(options["orbit_elevation"], "animation.options.orbit_elevation");
	if (options.contains("orbit_radius")) settings->orbit_radius = number_value(options["orbit_radius"], "animation.options.orbit_radius");
	if (options.contains("turntable_object")) set_string(settings->turntable_object, options["turntable_object"], "animation.options.turntable_object");
	if (options.contains("turntable_angle")) settings->turntable_angle = number_value(options["turntable_angle"], "animation.options.turntable_angle");
	if (options.contains("turntable_axis")) vector_value(settings->turntable_axis, options["turntable_axis"], 3, "animation.options.turntable_axis");
	if (options.contains("turntable_center")) vector_value(settings->turntable_center, options["turntable_center"], 3, "animation.options.turntable_center");
    }
}

static void
apply_render_spec(const char *path, struct rtwizard_settings *settings)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error(std::string("unable to open render specification '") + path + "'");
    json root;
    in >> root;
    check_keys(root, {"schema", "version", "name", "description", "database", "objects", "image", "output",
	    "view", "style", "runtime", "animation"}, "render specification");
    if (root.value("schema", "") != "brlcad.rtwizard.render" || root.value("version", 0) != 1)
	throw std::runtime_error("render specification must use brlcad.rtwizard.render version 1");

    fs::path spec_path = fs::absolute(fs::path(path));
    fs::path base = spec_path.parent_path();
    bu_vls_strcpy(settings->render_spec, spec_path.string().c_str());
    if (root.contains("database")) set_file(settings->input_file, root["database"], base, "database");

    if (root.contains("objects")) {
	const json &objects = root["objects"];
	check_keys(objects, {"color", "ghost", "line"}, "objects");
	for (const auto &entry : {std::make_pair("color", settings->color), std::make_pair("ghost", settings->ghost),
		std::make_pair("line", settings->line)}) {
	    if (!objects.contains(entry.first)) continue;
	    if (!objects[entry.first].is_array()) throw std::runtime_error(std::string("objects.") + entry.first + " must be an array");
	    for (const json &name : objects[entry.first]) {
		if (!name.is_string() || name.get<std::string>().empty()) throw std::runtime_error("object names must be nonempty strings");
		bu_ptbl_ins(entry.second, (long *)bu_strdup(name.get<std::string>().c_str()));
	    }
	}
    }

    if (root.contains("image")) {
	const json &image = root["image"];
	check_keys(image, {"size", "width", "height", "type"}, "image");
	if (image.contains("size")) {
	    settings->size = size_value(image["size"], "image.size");
	    settings->size_set = 1;
	    settings->width = settings->height = settings->size;
	}
	if (image.contains("width")) { settings->width = size_value(image["width"], "image.width"); settings->width_set = 1; }
	if (image.contains("height")) { settings->height = size_value(image["height"], "image.height"); settings->height_set = 1; }
	if (image.contains("type")) {
	    if (!image["type"].is_string() || image["type"].get<std::string>().size() != 1)
		throw std::runtime_error("image.type must be one letter A through F");
	    settings->image_type = image["type"].get<std::string>()[0];
	    if (settings->image_type < 'A' || settings->image_type > 'F') throw std::runtime_error("image.type must be one letter A through F");
	}
    }

    if (root.contains("output")) {
	const json &output = root["output"];
	check_keys(output, {"file", "frame_dir", "framebuffer", "framebuffer_port", "resume"}, "output");
	if (output.contains("file")) set_file(settings->output_file, output["file"], base, "output.file");
	if (output.contains("frame_dir")) set_file(settings->frame_dir, output["frame_dir"], base, "output.frame_dir");
	if (output.contains("framebuffer")) set_string(settings->fb_dev, output["framebuffer"], "output.framebuffer");
	if (output.contains("framebuffer_port")) settings->port = integer_value(output["framebuffer_port"], "output.framebuffer_port");
	if (output.contains("resume")) settings->resume = boolean_value(output, "resume", "output");
    }

    if (root.contains("view")) {
	const json &view = root["view"];
	check_keys(view, {"azimuth", "elevation", "twist", "zoom", "center", "eye", "view_size", "orientation", "perspective"}, "view");
	if (view.contains("azimuth")) settings->az = number_value(view["azimuth"], "view.azimuth");
	if (view.contains("elevation")) settings->el = number_value(view["elevation"], "view.elevation");
	if (view.contains("twist")) settings->tw = number_value(view["twist"], "view.twist");
	if (view.contains("zoom")) settings->zoom = number_value(view["zoom"], "view.zoom");
	if (view.contains("center")) vector_value(settings->center, view["center"], 3, "view.center");
	if (view.contains("eye")) vector_value(settings->eye_pt, view["eye"], 3, "view.eye");
	if (view.contains("view_size")) settings->viewsize = number_value(view["view_size"], "view.view_size");
	if (view.contains("orientation")) vector_value(settings->orientation, view["orientation"], 4, "view.orientation");
	if (view.contains("perspective")) settings->perspective = number_value(view["perspective"], "view.perspective");
    }

    if (root.contains("style")) {
	const json &style = root["style"];
	check_keys(style, {"background", "line_color", "non_line_color", "ghost_intensity", "occlusion", "ao_samples", "ao_radius"}, "style");
	if (style.contains("background")) color_value(settings->bkg_color, style["background"], "style.background");
	if (style.contains("line_color")) color_value(settings->line_color, style["line_color"], "style.line_color");
	if (style.contains("non_line_color")) color_value(settings->non_line_color, style["non_line_color"], "style.non_line_color");
	if (style.contains("ghost_intensity")) settings->ghost_intensity = number_value(style["ghost_intensity"], "style.ghost_intensity");
	if (style.contains("occlusion")) settings->occlusion = integer_value(style["occlusion"], "style.occlusion");
	if (style.contains("ao_samples")) settings->ao_samples = integer_value(style["ao_samples"], "style.ao_samples");
	if (style.contains("ao_radius")) settings->ao_radius = number_value(style["ao_radius"], "style.ao_radius");
    }

    if (root.contains("runtime")) {
	const json &runtime = root["runtime"];
	check_keys(runtime, {"benchmark", "cpu_count", "verbose", "log_file", "pid_file", "gui", "no_gui"}, "runtime");
	if (runtime.contains("benchmark")) settings->benchmark = boolean_value(runtime, "benchmark", "runtime");
	if (runtime.contains("cpu_count")) settings->cpus = integer_value(runtime["cpu_count"], "runtime.cpu_count");
	if (runtime.contains("verbose")) settings->verbose = integer_value(runtime["verbose"], "runtime.verbose");
	if (runtime.contains("log_file")) set_file(settings->log_file, runtime["log_file"], base, "runtime.log_file");
	if (runtime.contains("pid_file")) set_file(settings->pid_file, runtime["pid_file"], base, "runtime.pid_file");
	if (runtime.contains("gui")) settings->use_gui = boolean_value(runtime, "gui", "runtime");
	if (runtime.contains("no_gui")) settings->no_gui = boolean_value(runtime, "no_gui", "runtime");
    }

    if (root.contains("animation")) {
	apply_animation(root["animation"], spec_path, settings);
    }
}

} // namespace

extern "C" int
rtwizard_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg)
{
    if (!path || !settings) return -1;
    try {
	apply_render_spec(path, settings);
	if (errmsg) *errmsg = NULL;
	return 0;
    } catch (const std::exception &e) {
	if (errmsg) *errmsg = bu_strdup(e.what());
	return -1;
    }
}

extern "C" int
rtwizard_animation_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg)
{
    if (!path || !settings) return -1;
    try {
	std::ifstream in(path);
	if (!in) throw std::runtime_error(std::string("unable to open animation specification '") + path + "'");
	json animation;
	in >> animation;
	fs::path spec_path = fs::absolute(fs::path(path)).lexically_normal();
	apply_animation(animation, spec_path, settings);
	if (errmsg) *errmsg = NULL;
	return 0;
    } catch (const std::exception &e) {
	if (errmsg) *errmsg = bu_strdup(e.what());
	return -1;
    }
}
