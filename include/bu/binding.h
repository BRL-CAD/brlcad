/*                        B I N D I N G . H
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

/** @addtogroup bu_binding
 * @brief
 * Universal input binding schema and action dispatch.
 */
/** @{ */
/** @file bu/binding.h */

#ifndef BU_BINDING_H
#define BU_BINDING_H

#include "common.h"
#include "bu/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

/* Standardized Attribute Keys (for W3C validation logic) */
#define BU_BINDING_ATTR_TYPE       "type"
#define BU_BINDING_ATTR_KEY        "key"      /* Values MUST be W3C standard strings */
#define BU_BINDING_ATTR_BUTTON     "button"   /* Values MUST be W3C button integers */
#define BU_BINDING_ATTR_MODIFIERS  "modifiers"

/* Standardized Event Types */
#define BU_BINDING_EVENT_KEY_PRESS      "key-press"
#define BU_BINDING_EVENT_KEY_RELEASE    "key-release"
#define BU_BINDING_EVENT_MOUSE_PRESS    "mouse-press"
#define BU_BINDING_EVENT_MOUSE_RELEASE  "mouse-release"
#define BU_BINDING_EVENT_MOUSE_MOTION   "mouse-motion"
#define BU_BINDING_EVENT_MOUSE_SCROLL   "mouse-scroll"

/**
 * @brief Action callback signature
 * 
 * Invoked when an incoming event matches a loaded binding.
 * The callback receives the full JSON string of the triggering event.
 * 
 * @param event_json Raw JSON string describing the incoming event
 * @param registered_data Opaque pointer passed during registration
 * @param event_data Opaque pointer passed during event processing
 */
typedef void (*bu_action_func_t)(const char *event_json, void *registered_data, void *event_data);



/**
 * @brief Load JSON bindings from a file
 * 
 * Parses a JSON configuration specifying bindings for various contexts.
 * Performs two-phase validation:
 * 1. Checks standard fields (like W3C key strings) for correctness.
 * 2. Invokes context-specific validation hooks (if registered).
 * 
 * @param json_path Path to the JSON file
 * @return 0 on success, <0 on error
 */
BU_EXPORT extern int bu_binding_load(const char *json_path);

/**
 * @brief Load JSON bindings from a raw string
 * 
 * Parses a JSON configuration string specifying bindings for various contexts.
 * Allows applications to embed overriding bindings natively without external files.
 * 
 * @param json_string Raw JSON string containing bindings
 * @return 0 on success, <0 on error
 */
BU_EXPORT extern int bu_binding_load_str(const char *json_string);

/**
 * @brief Register a JSON schema for a specific context
 * 
 * Defines the allowed vocabulary for a context using a JSON schema file.
 * The schema file is lazily parsed during bu_binding_load().
 * 
 * @param context The interaction context (e.g., "dm", "fb")
 * @param json_path Absolute path to the schema JSON file
 */
BU_EXPORT extern void bu_binding_register_schema(const char *context, const char *json_path);

/**
 * @brief Augment an existing schema with a custom action
 * 
 * @param context The interaction context (e.g., "dm")
 * @param action_name The new abstract action string
 * @param description Human-readable description of the action
 */
BU_EXPORT extern void bu_binding_schema_add_action(const char *context, const char *action_name, const char *description);

/**
 * @brief Remove a custom action from an existing schema
 * 
 * @param context The interaction context (e.g., "dm")
 * @param action_name The abstract action string to remove
 */
BU_EXPORT extern void bu_binding_schema_remove_action(const char *context, const char *action_name);

/**
 * @brief Register a C callback for a semantic action
 * 
 * @param action_name The abstract action string (e.g., "fb_close_window")
 * @param func The C function to invoke when the action is triggered
 * @param client_data Opaque pointer passed to the callback
 */
BU_EXPORT extern void bu_binding_register_action(const char *action_name, bu_action_func_t func, void *client_data);

/**
 * @brief Process an incoming native event
 * 
 * Matches the event JSON against the loaded bindings for the given context.
 * If a matching action is found and a handler is registered for that action,
 * the callback is invoked.
 * 
 * @param context The interaction context (e.g., "view3d", "fb_raw")
 * @param event_json A JSON string containing the event attributes
 * @param event_data Opaque pointer passed to the callback representing instance state
 * @return 1 if an action was triggered and handled, 0 otherwise
 */
BU_EXPORT extern int bu_binding_process_event(const char *context, const char *event_json, void *event_data);

/**
 * @brief Check if a specific action is bound in a given context
 * 
 * Use this to verify that an application has provided a binding for an essential action.
 * 
 * @param context The interaction context (e.g., "view3d", "fb_raw")
 * @param action_name The abstract action string (e.g., "view_mouse_drag_rotate")
 * @return 1 if bound, 0 if not
 */
BU_EXPORT extern int bu_binding_action_is_bound(const char *context, const char *action_name);

__END_DECLS

#endif /* BU_BINDING_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
