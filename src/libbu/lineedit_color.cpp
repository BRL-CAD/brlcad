/*              L I N E E D I T _ C O L O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/lineedit.h"
#include "bu/log.h"
#include "bu/str.h"

#include "json.hpp"


static const char *lineedit_role_names[BU_LINEEDIT_ROLE_COUNT] = {
    "command",
    "option",
    "valid",
    "invalid",
    "incomplete",
    "completion-preview"
};


static int
lineedit_role_from_name(const std::string &name)
{
    std::string normalized(name);
    for (size_t i = 0; i < normalized.size(); i++) {
	if (normalized[i] == '_')
	    normalized[i] = '-';
	else
	    normalized[i] = (char)std::tolower((unsigned char)normalized[i]);
    }

    for (int i = 0; i < BU_LINEEDIT_ROLE_COUNT; i++) {
	if (normalized == lineedit_role_names[i])
	    return i;
    }

    return -1;
}


static int
lineedit_hex_value(char c)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    return -1;
}


static bool
lineedit_parse_color(const std::string &value, unsigned char rgb[3])
{
    if (value.length() == 4 && value[0] == '#') {
	for (int i = 0; i < 3; i++) {
	    int v = lineedit_hex_value(value[(size_t)i + 1]);
	    if (v < 0)
		return false;
	    rgb[i] = (unsigned char)((v << 4) | v);
	}
	return true;
    }

    if (value.length() == 7 && value[0] == '#') {
	for (int i = 0; i < 3; i++) {
	    int hi = lineedit_hex_value(value[(size_t)(i * 2) + 1]);
	    int lo = lineedit_hex_value(value[(size_t)(i * 2) + 2]);
	    if (hi < 0 || lo < 0)
		return false;
	    rgb[i] = (unsigned char)((hi << 4) | lo);
	}
	return true;
    }

    return false;
}


static bool
lineedit_parse_color_value(struct bu_lineedit_style *style, const nlohmann::json &value)
{
    if (!value.is_string())
	return false;

    std::string color = value.get<std::string>();
    if (BU_STR_EQUIV(color.c_str(), "default") || BU_STR_EQUIV(color.c_str(), "inherit")) {
	style->flags &= ~BU_LINEEDIT_STYLE_COLOR;
	return true;
    }

    if (!lineedit_parse_color(color, style->rgb))
	return false;

    style->flags |= BU_LINEEDIT_STYLE_COLOR;
    return true;
}


static bool
lineedit_parse_style_value(struct bu_lineedit_style *style, const nlohmann::json &value)
{
    if (!value.is_string())
	return false;

    std::string name = value.get<std::string>();
    if (BU_STR_EQUIV(name.c_str(), "dim")) {
	style->flags |= BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM;
	return true;
    }
    if (BU_STR_EQUIV(name.c_str(), "normal")) {
	style->flags |= BU_LINEEDIT_STYLE_DIM_SET;
	style->flags &= ~BU_LINEEDIT_STYLE_DIM;
	return true;
    }

    return false;
}


static bool
lineedit_parse_role(struct bu_lineedit_style *style, const nlohmann::json &value)
{
    if (value.is_string())
	return lineedit_parse_color_value(style, value);

    if (!value.is_object())
	return false;

    if (value.contains("color") && !lineedit_parse_color_value(style, value["color"]))
	return false;
    if (value.contains("style") && !lineedit_parse_style_value(style, value["style"]))
	return false;
    if (value.contains("dim")) {
	if (!value["dim"].is_boolean())
	    return false;
	style->flags |= BU_LINEEDIT_STYLE_DIM_SET;
	if (value["dim"].get<bool>())
	    style->flags |= BU_LINEEDIT_STYLE_DIM;
	else
	    style->flags &= ~BU_LINEEDIT_STYLE_DIM;
    }

    return true;
}


extern "C" void
bu_lineedit_palette_init(struct bu_lineedit_palette *palette)
{
    if (palette)
	memset(palette, 0, sizeof(*palette));
}


extern "C" const char *
bu_lineedit_role_name(bu_lineedit_role_t role)
{
    if (role < 0 || role >= BU_LINEEDIT_ROLE_COUNT)
	return NULL;
    return lineedit_role_names[role];
}


extern "C" int
bu_lineedit_palette_load_file(struct bu_lineedit_palette *palette, const char *path)
{
    if (!palette || !path || !path[0])
	return BRLCAD_ERROR;

    std::ifstream input(path);
    if (!input)
	return BRLCAD_ERROR;

    try {
	nlohmann::json config;
	input >> config;
	if (!config.is_object())
	    return BRLCAD_ERROR;
	if (!config.contains("version") || !config["version"].is_number_integer() ||
		config["version"].get<int>() != 1)
	    return BRLCAD_ERROR;
	if (!config.contains("colors"))
	    return BRLCAD_OK;
	if (!config["colors"].is_object())
	    return BRLCAD_ERROR;

	struct bu_lineedit_palette candidate = *palette;
	for (nlohmann::json::const_iterator i = config["colors"].begin(); i != config["colors"].end(); ++i) {
	    int role = lineedit_role_from_name(i.key());
	    if (role < 0)
		continue;
	    if (!lineedit_parse_role(&candidate.roles[role], i.value()))
		return BRLCAD_ERROR;
	}

	*palette = candidate;
	return BRLCAD_OK;
    } catch (const nlohmann::json::exception &) {
	return BRLCAD_ERROR;
    }
}


static std::string
lineedit_user_path()
{
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0]) {
	std::string path(xdg_config);
	if (path[path.length() - 1] != BU_DIR_SEPARATOR)
	    path += BU_DIR_SEPARATOR;
	path += "brlcad";
	path += BU_DIR_SEPARATOR;
	path += "lineedit.json";
	return path;
    }

    char path[MAXPATHLEN] = {0};
    bu_dir(path, MAXPATHLEN, BU_DIR_CONFIG, "brlcad", "lineedit.json", NULL);
    return std::string(path);
}


extern "C" int
bu_lineedit_palette_load_user(struct bu_lineedit_palette *palette)
{
    if (!palette)
	return BRLCAD_ERROR;

    const char *selected = getenv(BU_LINEEDIT_COLORS_ENV);
    if (selected && selected[0]) {
	if (BU_STR_EQUIV(selected, "off") || BU_STR_EQUIV(selected, "none"))
	    return BRLCAD_OK;
	if (bu_lineedit_palette_load_file(palette, selected) == BRLCAD_OK)
	    return BRLCAD_OK;
	bu_log("Warning: ignoring invalid line-editor color configuration %s\n", selected);
	return BRLCAD_ERROR;
    }

    std::string path = lineedit_user_path();
    if (!bu_file_exists(path.c_str(), NULL))
	return BRLCAD_OK;
    if (bu_lineedit_palette_load_file(palette, path.c_str()) == BRLCAD_OK)
	return BRLCAD_OK;

    bu_log("Warning: ignoring invalid line-editor color configuration %s\n", path.c_str());
    return BRLCAD_ERROR;
}
