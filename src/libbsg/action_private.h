/*                   A C T I O N _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
#ifndef LIBBSG_ACTION_PRIVATE_H
#define LIBBSG_ACTION_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "vmath.h"
#include "bu/list.h"
#include "bsg/action.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "bsg/state.h"

__BEGIN_DECLS

struct bsg_appearance_settings;
struct bsg_action;

#define BSG_ACTION_TRAVERSE_VISIBLE_ONLY        0x01U
#define BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT    0x02U

struct bsg_action_state {
    bsg_state_ref state;
    struct bsg_view *view;
    bsg_node *root;
    bsg_node *node;
    mat_t model_mat;
    const struct bsg_appearance_settings *inherited_settings;
    int inherited_visible;
    int visible;
    int force_draw;
    bsg_lod_source_ref lod_source;
    int lod_level;
    int lod_level_count;
};

typedef int (*bsg_action_node_cb)(bsg_node *node,
				  const struct bsg_action_state *state,
				  void *userdata);

struct bsg_action_traversal {
    struct bsg_view *view;
    bsg_node *root;
    unsigned int flags;
    bsg_action_node_cb enter_cb;
    bsg_action_node_cb shape_cb;
    bsg_action_node_cb leave_cb;
    void *userdata;
};

typedef int (*bsg_action_apply_cb)(bsg_action_ref action,
				   bsg_node_ref root,
				   void *data);
typedef void (*bsg_action_data_destroy_cb)(bsg_action_ref action,
					   void *data);
typedef int (*bsg_action_method_cb)(bsg_action_ref action,
				    bsg_node_ref node,
				    bsg_state_ref state,
				    void *userdata);

BSG_EXPORT extern bsg_action_ref
bsg_action_ref_create(bsg_type_id action_type);

BSG_EXPORT extern int
bsg_action_method_register(bsg_type_id action_type,
			   bsg_type_id node_type,
			   bsg_action_method_cb cb,
			   void *userdata);

BSG_EXPORT extern int
bsg_action_traverse(const struct bsg_action_traversal *traversal);

bsg_action_ref
bsg_action_ref_create_internal(bsg_type_id action_type,
			       bsg_action_apply_cb apply_cb,
			       bsg_action_data_destroy_cb destroy_cb,
			       void *data,
			       const char *label);

void *
bsg_action_ref_data(bsg_action_ref action);

bsg_node *
bsg_action_node_from_ref(bsg_node_ref ref);

bsg_node_ref
bsg_action_node_ref_from_node(bsg_node *node);

__END_DECLS

#endif /* LIBBSG_ACTION_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
