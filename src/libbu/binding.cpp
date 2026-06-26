/*                      B I N D I N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>

#include "bu/binding.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "json.hpp"

using json = nlohmann::json;

/* 
 * Internal data structures to hold the parsed bindings and callbacks
 */
struct ActionCallback {
    bu_action_func_t func;
    void *client_data;
};

struct BindingDef {
    json match;
    std::string action;
};

/* Global state for libbu bindings */
static std::map<std::string, std::vector<BindingDef>>& get_contexts() {
    static auto* ptr = new std::map<std::string, std::vector<BindingDef>>();
    return *ptr;
}
static std::map<std::string, ActionCallback>& get_action_registry() {
    static auto* ptr = new std::map<std::string, ActionCallback>();
    return *ptr;
}
static std::map<std::string, std::string>& get_schema_paths() {
    static auto* ptr = new std::map<std::string, std::string>();
    return *ptr;
}
static std::map<std::string, std::set<std::string>>& get_schemas() {
    static auto* ptr = new std::map<std::string, std::set<std::string>>();
    return *ptr;
}

static void
ensure_schema_loaded(const std::string& ctx_name)
{
    if (get_schemas().find(ctx_name) != get_schemas().end()) {
        return;
    }

    auto it = get_schema_paths().find(ctx_name);
    if (it == get_schema_paths().end()) {
        return;
    }

    std::ifstream file(it->second);
    if (!file.is_open()) {
        bu_log("ensure_schema_loaded: Failed to open schema %s\n", it->second.c_str());
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        bu_log("ensure_schema_loaded: JSON parse error in schema %s: %s\n", it->second.c_str(), e.what());
        return;
    }

    std::set<std::string> valid_actions;
    if (j.contains("actions") && j["actions"].is_object()) {
        for (auto el = j["actions"].begin(); el != j["actions"].end(); ++el) {
            valid_actions.insert(el.key());
        }
    }
    get_schemas()[ctx_name] = valid_actions;
}

/* W3C Key validation placeholder. A full implementation would check against
 * an exhaustive list of W3C standard keys (e.g., "Escape", "ArrowUp").
 * For demonstration, we'll just implement a simple sanity check.
 */
static int
validate_w3c_key(const std::string& key)
{
    /* Example: Reject completely upper-case strings that might be native C macros */
    if (key.length() > 1 && key == "RETURN") return 0;
    if (key.length() > 1 && key == "SPACE") return 0;
    
    return 1;
}

/* W3C Button validation. Values 0, 1, 2, 3, 4 are standard. */
static int
validate_w3c_button(int button)
{
    return (button >= 0 && button <= 4);
}


int
bu_binding_load(const char *json_path)
{
    if (!json_path) return -1;

    std::ifstream file(json_path);
    if (!file.is_open()) {
        bu_log("bu_binding_load: Failed to open %s\n", json_path);
        return -1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return bu_binding_load_str(buffer.str().c_str());
}

int
bu_binding_load_str(const char *json_string)
{
    if (!json_string) return -1;

    json j;
    try {
        j = json::parse(json_string);
    } catch (const json::parse_error& e) {
        bu_log("bu_binding_load_str: JSON parse error: %s\n", e.what());
        return -1;
    }

    if (!j.contains("contexts") || !j["contexts"].is_object()) {
        bu_log("bu_binding_load_str: Invalid JSON structure (missing 'contexts' object)\n");
        return -1;
    }

    for (auto it = j["contexts"].begin(); it != j["contexts"].end(); ++it) {
        std::string ctx_name = it.key();
        const auto& ctx_obj = it.value();

        if (!ctx_obj.contains("bindings") || !ctx_obj["bindings"].is_array()) {
            continue;
        }

        std::vector<BindingDef> defs;
        
        ensure_schema_loaded(ctx_name);
        bool has_schema = (get_schemas().find(ctx_name) != get_schemas().end());

        for (const auto& binding : ctx_obj["bindings"]) {
            if (!binding.contains("match") || !binding.contains("action")) {
                bu_log("bu_binding_load_str: Binding missing 'match' or 'action' in context %s. Skipping.\n", ctx_name.c_str());
                continue;
            }

            const json& match = binding["match"];
            std::string action = binding["action"].get<std::string>();

            /* Phase 1 Validation: Check W3C standard compliance */
            int is_valid = 1;
            if (match.contains(BU_BINDING_ATTR_KEY)) {
                if (!match[BU_BINDING_ATTR_KEY].is_string() || !validate_w3c_key(match[BU_BINDING_ATTR_KEY].get<std::string>())) {
                    bu_log("bu_binding_load_str: Invalid W3C key '%s' in context %s. Binding rejected.\n", 
                           match[BU_BINDING_ATTR_KEY].dump().c_str(), ctx_name.c_str());
                    is_valid = 0;
                }
            }
            if (match.contains(BU_BINDING_ATTR_BUTTON)) {
                if (!match[BU_BINDING_ATTR_BUTTON].is_number_integer() || !validate_w3c_button(match[BU_BINDING_ATTR_BUTTON].get<int>())) {
                    bu_log("bu_binding_load_str: Invalid W3C button '%s' in context %s. Binding rejected.\n", 
                           match[BU_BINDING_ATTR_BUTTON].dump().c_str(), ctx_name.c_str());
                    is_valid = 0;
                }
            }

            if (!is_valid) continue;

            /* Phase 2 Validation: Schema constraints */
            if (has_schema) {
                if (get_schemas()[ctx_name].find(action) == get_schemas()[ctx_name].end()) {
                    bu_log("bu_binding_load_str: Binding for action '%s' rejected by schema for context '%s'.\n", 
                           action.c_str(), ctx_name.c_str());
                    continue;
                }
            }

            BindingDef def;
            def.match = match;
            def.action = action;
            get_contexts()[ctx_name].insert(get_contexts()[ctx_name].begin(), def);
        }
    }

    return 0;
}

void
bu_binding_register_schema(const char *context, const char *json_path)
{
    if (!context || !json_path) return;
    get_schema_paths()[std::string(context)] = std::string(json_path);
}

void
bu_binding_schema_add_action(const char *context, const char *action_name, const char *description)
{
    if (!context || !action_name) return;
    std::string ctx(context);
    ensure_schema_loaded(ctx);
    get_schemas()[ctx].insert(std::string(action_name));
}

void
bu_binding_schema_remove_action(const char *context, const char *action_name)
{
    if (!context || !action_name) return;
    std::string ctx(context);
    ensure_schema_loaded(ctx);
    get_schemas()[ctx].erase(std::string(action_name));
}

void
bu_binding_register_action(const char *action_name, bu_action_func_t func, void *client_data)
{
    if (!action_name || !func) return;

    ActionCallback cb;
    cb.func = func;
    cb.client_data = client_data;

    get_action_registry()[std::string(action_name)] = cb;
}

/* Helper to check if the json 'event' contains all the key-value pairs required by 'match' */
static int
is_match(const json& match, const json& event)
{
    for (auto it = match.begin(); it != match.end(); ++it) {
        if (!event.contains(it.key())) {
            return 0;
        }
        
        /* If it's an array (like modifiers), we need to ensure all elements in 'match' exist in 'event' */
        if (it.value().is_array()) {
            if (!event[it.key()].is_array()) return 0;
            
            for (const auto& req_mod : it.value()) {
                int found = 0;
                for (const auto& ev_mod : event[it.key()]) {
                    if (ev_mod == req_mod) {
                        found = 1;
                        break;
                    }
                }
                if (!found) return 0;
            }
        } 
        /* Otherwise, simple equality */
        else if (event[it.key()] != it.value()) {
            return 0;
        }
    }
    return 1;
}


int
bu_binding_process_event(const char *context, const char *event_json, void *event_data)
{
    if (!context || !event_json) return 0;

    std::string ctx_name(context);
    auto it = get_contexts().find(ctx_name);
    if (it == get_contexts().end()) {
        return 0; /* Context not found */
    }

    const char *json_str = event_json;
    
    json event_json_obj;
    try {
        event_json_obj = json::parse(json_str);
    } catch (const json::parse_error& e) {
        bu_log("bu_binding_process_event: Invalid event JSON: %s\n", e.what());
        return 0;
    }

    if (!event_json_obj.is_object()) {
        return 0;
    }

    for (const auto& def : it->second) {
        if (is_match(def.match, event_json_obj)) {
            /* Match found */
            auto reg_it = get_action_registry().find(def.action);
            if (reg_it != get_action_registry().end()) {
                reg_it->second.func(event_json, reg_it->second.client_data, event_data);
                return 1;
            } else {
                /* We have a binding, but no callback registered for this action */
                bu_log("Warning: Event mapped to action '%s', but no callback is registered.\n", def.action.c_str());
                return 1; /* Still mark as handled since it matched a binding */
            }
        }
    }

    return 0; /* Unhandled */
}

extern "C" int
bu_binding_action_is_bound(const char *context, const char *action_name)
{
    if (!context || !action_name) return 0;
    
    std::string ctx(context);
    std::string act(action_name);

    auto it = get_contexts().find(ctx);
    if (it == get_contexts().end()) return 0;
    
    for (const auto& def : it->second) {
        if (def.action == act) {
            return 1;
        }
    }
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
