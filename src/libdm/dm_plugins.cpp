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
    if (BU_STR_EQUIV(type, "nu")) {
	return dm_null.i->dm_open(interp, argc, argv);
    }
    if (BU_STR_EQUIV(type, "null")) {
	return dm_null.i->dm_open(interp, argc, argv);
    }
    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    std::map<std::string, const struct dm *>::iterator d_it = dmb->find(std::string(type));
    if (d_it == dmb->end()) {
	return DM_NULL;
    }
    const struct dm *d = d_it->second;
    struct dm *dmp = d->i->dm_open(interp, argc, argv);
    return dmp;
}

extern "C" struct bu_vls *
dm_list_types(const char *separator)
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
	bu_vls_printf(list, "%s", dm_get_name(d));
    }
    if (strlen(bu_vls_cstr(list)) > 0) bu_vls_printf(list, "%s", bu_vls_cstr(&sep));
    bu_vls_printf(list, "nu");

    return list;
}

extern "C" int
dm_validXType(const char *dpy_string, const char *name)
{
    if (BU_STR_EQUIV(name, "nu")) {
	return 1;
    }
    if (BU_STR_EQUIV(name, "null")) {
	return 1;
    }
    std::map<std::string, const struct dm *> *dmb = (std::map<std::string, const struct dm *> *)dm_backends;
    std::map<std::string, const struct dm *>::iterator d_it = dmb->find(std::string(name));
    if (d_it == dmb->end()) {
	return 0;
    }
    const struct dm *d = d_it->second;
    int is_valid = d->i->dm_viable(dpy_string);
    return is_valid;
}
extern "C" int
dm_valid_type(const char *name, const char *dpy_string)
{
    return dm_validXType(dpy_string, name);
}


/**
 * dm_bestXType determines what mged will normally use as the default display
 * manager.  Checks if the display manager backend can work at runtime, if the
 * backend supports that check, and will report the "best" available WORKING
 * backend rather than simply the first one present in the list that is also
 * in the plugins directory.
  */
extern "C" const char *
dm_bestXType(const char *dpy_string)
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

/**
 * dm_default_type suggests a display manager.  Checks if a plugin supplies the
 * specified backend type before reporting it, but does NOT perform a runtime
 * test to verify its suggestion will work (unlike dm_bestXType) before
 * reporting back.
  */
extern "C" const char *
dm_default_type()
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
	ret = b;
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
