#include "common.h"
#include <string.h>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#include "dm.h"
#include "./include/private.h"

extern "C" struct dm *
dm_open(void *interp, const char *type, int argc, const char *argv[])
{

    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    std::map<std::string, const struct dm *>::iterator d_it = dmb->find(std::string(type));
    if (d_it == dmb->end()) {
	return DM_NULL;
    }
    const struct dm *d = d_it->second;
    struct dm *dmp = d->i->dm_open(interp, argc, argv);
    return dmp;
}

void
dm_list_backends(const char *separator)
{
    struct bu_vls *list;
    BU_GET(list, struct bu_vls);
    bu_vls_init(list);
    bu_vls_trunc(list, 0);

    // We've got something, and may need a separator
    struct bu_vls sep = BU_VLS_INIT_ZERO;
    if (!separator) {
	bu_vls_sprintf(&sep, " ");
    } else {
	bu_vls_sprintf(&sep, "%s", separator);
    }

    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    std::map<std::string, const struct dm *>::iterator d_it;
    for (d_it = dmb->begin(); d_it != dmb->end(); d_it++) {
	std::string key = d_it->first;
	const struct dm *d = d_it->second;
	if (strlen(bu_vls_cstr(list)) > 0) bu_vls_printf(list, "%s", bu_vls_cstr(&sep));
	bu_vls_printf(list, "%s: %s", key.c_str(), dm_get_name(d));
    }

    bu_log("%s\n", bu_vls_cstr(list));
    bu_vls_free(list);
    BU_PUT(list, struct bu_vls);
}

int
dm_valid_type(const char *name, const char *dpy_string)
{
    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    std::map<std::string, const struct dm *>::iterator d_it = dmb->find(std::string(name));
    if (d_it == dmb->end()) {
	return 0;
    }
    const struct dm *d = d_it->second;
    int is_valid = d->i->dm_viable(dpy_string);
    return is_valid;
}

/** dm_recommend_type determines what mged will normally
  * use as the default display manager
  */
const char *
dm_recommend_type(const char *dpy_string)
{
    static const char *priority_list[] = {"osgl", "wgl", "ogl", "X", "tk", "nu"};
    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    const char *ret = NULL;

    int i = 0;
    const char *b = priority_list[i];
    while (!BU_STR_EQUAL(b, "nu")) {
	std::map<std::string, const struct dm *>::iterator d_it = dmb->find(std::string(b));
	if (d_it == dmb->end()) {
	    i++;
	    b = priority_list[i];
	    continue;
	}
	const struct dm *d = d_it->second;
	if (d->i->dm_viable(dpy_string) == 1) {
	    ret = b;
	    break;
	}
	i++;
	b = priority_list[i];
    }

    return (ret) ? ret : b;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
