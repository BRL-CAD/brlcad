#include "../ged_private.h"

extern void *ged_cmds;

struct ged_cmd_impl {
    const char *cname;
    ged_func_ptr cmd;
    int update_view;
    int interactive;
};

#define GED_CMD_IMPL_NULL {NULL, 0}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
