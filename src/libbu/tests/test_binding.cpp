/*                 T E S T _ B I N D I N G . C P P
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

#include <stdio.h>
#include <string.h>

#include "bu/binding.h"
#include "bu/vls.h"

int action_triggered = 0;

void mock_action_handler(const char *event_json, void *registered_data, void *event_data) {
    action_triggered = 1;
    printf("Action handler triggered with JSON: %s\n", event_json);
}



int main(int argc, char *argv[]) {
    bu_setprogname(argv[0]);

    if (argc < 2) {
        printf("Usage: %s <path_to_test_bindings.json>\n", argv[0]);
        return -1;
    }

    const char *json_path = argv[1];

    /* Register schema to restrict allowed actions */
    bu_binding_schema_add_action("fb", "fb_zoom", "Zoom");
    bu_binding_schema_add_action("fb", "fb_close_window", "Close Window");

    /* Register hooks before loading JSON */
    bu_binding_register_action("fb_close_window", mock_action_handler, NULL);

    printf("Loading bindings from %s\n", json_path);
    if (bu_binding_load(json_path) < 0) {
        printf("Failed to load bindings.\n");
        return -1;
    }

    /* Test 1: Simulate a matching event */
    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-press\", \"button\": 2}");
    
    action_triggered = 0;
    bu_binding_process_event("fb", bu_vls_cstr(&ev_json), NULL);
    
    if (!action_triggered) {
        printf("Test 1 Failed: Expected action was not triggered.\n");
        return -1;
    }
    printf("Test 1 Passed: Action triggered successfully.\n");

    /* Test 2: Simulate a non-matching event */
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-press\", \"button\": 1}");
    action_triggered = 0;
    bu_binding_process_event("fb", bu_vls_cstr(&ev_json), NULL);
    
    if (action_triggered) {
        printf("Test 2 Failed: Action should not have been triggered.\n");
        return -1;
    }
    printf("Test 2 Passed: Non-matching event correctly ignored.\n");

    /* Test 3: The Escape key binding should have been rejected by the validator */
    bu_vls_sprintf(&ev_json, "{\"type\": \"key-press\", \"key\": \"Escape\"}");
    action_triggered = 0;
    bu_binding_process_event("fb", bu_vls_cstr(&ev_json), NULL);
    
    if (action_triggered) {
        printf("Test 3 Failed: Escape binding should have been filtered by the validator.\n");
        return -1;
    }
    printf("Test 3 Passed: Validator correctly rejected binding at load time.\n");

    bu_vls_free(&ev_json);

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
