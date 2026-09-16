/*                       D E F A U L T . C
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
/** @addtogroup db_default */
/** @{ */
/** @file librt/default.c
 *
 */

#include "common.h"

#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "rt/db5.h"
#include "rt/db_instance.h"
#include "rt/directory.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/default.h"
#include "rt/search.h"


static int
db_default_object_cmp(const void *a, const void *b, void *UNUSED(data))
{
    const struct directory *da = *(const struct directory * const *)a;
    const struct directory *db = *(const struct directory * const *)b;

    if (!da && !db) return 0;
    if (!da) return -1;
    if (!db) return 1;

    return bu_strcmp(da->d_namep, db->d_namep);
}


static void
db_default_object_sort(struct directory **dpv, size_t dp_cnt)
{
    if (!dpv || dp_cnt < 2)
	return;

    bu_sort(dpv, dp_cnt, sizeof(struct directory *), db_default_object_cmp, NULL);
}


static int
db_default_object_geom(const struct directory *dp)
{
    if (!dp)
	return 0;

    if (dp->d_addr == RT_DIR_PHONY_ADDR)
	return 0;

    return (dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)) ? 1 : 0;
}


static char *
db_default_object_trim(const char *str)
{
    const char *start;
    const char *end;
    char *ret;
    size_t len;

    if (!str)
	return NULL;

    start = str;
    while (*start && isspace((unsigned char)*start))
	start++;

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
	end--;

    len = (size_t)(end - start);
    ret = (char *)bu_malloc(len + 1, "trimmed default object attribute");
    if (len)
	memcpy(ret, start, len);
    ret[len] = '\0';

    return ret;
}


static int
db_default_object_disabled(const char *str)
{
    if (!str)
	return 1;

    if (str[0] == '-')
	return 1;

    return bu_str_false(str);
}


static void
db_default_object_msg_tops(struct bu_vls *msg, struct directory **tops, size_t tops_cnt)
{
    size_t i;

    if (!msg)
	return;

    if (!tops || !tops_cnt) {
	bu_vls_printf(msg, "No top-level objects found in this database.\n");
	return;
    }

    bu_vls_printf(msg, "Available top-level objects:\n");
    for (i = 0; i < tops_cnt; i++)
	bu_vls_printf(msg, "  %s\n", tops[i]->d_namep);
}


static void
db_default_object_msg(struct bu_vls *msg, struct directory **tops, size_t tops_cnt, const char *fmt, ...)
{
    va_list ap;

    if (!msg)
	return;

    va_start(ap, fmt);
    bu_vls_vprintf(msg, fmt, ap);
    va_end(ap);

    db_default_object_msg_tops(msg, tops, tops_cnt);
}


static size_t
db_default_object_top_geom(struct directory **tops, size_t tops_cnt, struct directory **selected)
{
    size_t i;
    size_t matches = 0;

    if (selected)
	*selected = RT_DIR_NULL;

    for (i = 0; i < tops_cnt; i++) {
	if (!db_default_object_geom(tops[i]))
	    continue;

	matches++;
	if (matches == 1 && selected)
	    *selected = tops[i];
    }

    return matches;
}


static size_t
db_default_object_count(struct directory **dpv, size_t dp_cnt, struct directory **selected)
{
    size_t i;
    size_t matches = 0;

    if (selected)
	*selected = RT_DIR_NULL;

    for (i = 0; i < dp_cnt; i++) {
	if (!db_default_object_geom(dpv[i]))
	    continue;

	matches++;
	if (matches == 1 && selected)
	    *selected = dpv[i];
    }

    return matches;
}


static size_t
db_default_object_filename_matches(struct db_i *dbip, struct directory **combs, size_t combs_cnt, struct directory **selected)
{
    struct bu_vls db_base = BU_VLS_INIT_ZERO;
    size_t matches = 0;
    size_t i;

    if (selected)
	*selected = RT_DIR_NULL;

    if (!dbip->dbi_filename || !bu_path_component(&db_base, dbip->dbi_filename, BU_PATH_BASENAME_EXTLESS)) {
	bu_vls_free(&db_base);
	return 0;
    }

    for (i = 0; i < combs_cnt; i++) {
	struct bu_vls comb_base = BU_VLS_INIT_ZERO;

	if (!db_default_object_geom(combs[i]))
	    continue;

	if (bu_path_component(&comb_base, combs[i]->d_namep, BU_PATH_BASENAME_EXTLESS) &&
	    BU_STR_EQUAL(bu_vls_cstr(&comb_base), bu_vls_cstr(&db_base))) {
	    matches++;
	    if (matches == 1 && selected)
		*selected = combs[i];
	}

	bu_vls_free(&comb_base);
    }

    bu_vls_free(&db_base);
    return matches;
}


static int
db_default_object_all_scene_name(const char *name)
{
    if (!name)
	return 0;

    if (!bu_strncasecmp(name, "all", 3))
	return 1;

    if (!bu_strncasecmp(name, "scene", 5))
	return 1;

    return 0;
}


static size_t
db_default_object_all_scene_matches(struct directory **tops, size_t tops_cnt, struct directory **selected)
{
    size_t i;
    size_t matches = 0;

    if (selected)
	*selected = RT_DIR_NULL;

    for (i = 0; i < tops_cnt; i++) {
	if (!(tops[i]->d_flags & RT_DIR_COMB))
	    continue;

	if (!db_default_object_all_scene_name(tops[i]->d_namep))
	    continue;

	matches++;
	if (matches == 1 && selected)
	    *selected = tops[i];
    }

    return matches;
}


int
db_default_object(struct db_i *dbip, struct directory **dp, struct bu_vls *msg)
{
    struct directory **tops = NULL;
    struct directory **combs = NULL;
    struct directory *selected = RT_DIR_NULL;
    struct directory *global_dp = RT_DIR_NULL;
    size_t tops_cnt = 0;
    size_t combs_cnt = 0;
    size_t matches = 0;
    int ret = 0;

    if (dp)
	*dp = RT_DIR_NULL;

    if (!dbip) {
	if (msg)
	    bu_vls_printf(msg, "Cannot choose a default object. No database is open.\n");
	return -1;
    }

    RT_CK_DBI(dbip);

    db_update_nref(dbip);

    tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &tops);
    db_default_object_sort(tops, tops_cnt);

    if (db_version(dbip) > 4 &&
	(global_dp = db_lookup(dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET)) != RT_DIR_NULL) {
	struct bu_attribute_value_set avs;
	const char *attr;

	BU_AVS_INIT(&avs);
	if (db5_get_attributes(dbip, &avs, global_dp) < 0) {
	    db_default_object_msg(msg, tops, tops_cnt,
				  "Cannot choose a default object. Unable to read database attributes.\n");
	    bu_avs_free(&avs);
	    ret = -1;
	    goto cleanup;
	}

	attr = bu_avs_get(&avs, DB_DEFAULT_OBJECT_ATTR);
	if (attr) {
	    char *attr_name = db_default_object_trim(attr);

	    if (db_default_object_disabled(attr_name)) {
		db_default_object_msg(msg, tops, tops_cnt,
				      "No object specified. Database 'default_object' attribute on _GLOBAL disabled automatic selection.\n");
		bu_free(attr_name, "trimmed default object attribute");
		bu_avs_free(&avs);
		ret = 0;
		goto cleanup;
	    }

	    selected = db_lookup(dbip, attr_name, LOOKUP_QUIET);
	    if (!db_default_object_geom(selected)) {
		db_default_object_msg(msg, tops, tops_cnt,
				      "Database default_object=\"%s\" is not usable geometry.\n",
				      attr_name);
		bu_free(attr_name, "trimmed default object attribute");
		bu_avs_free(&avs);
		ret = 0;
		goto cleanup;
	    }

	    if (dp)
		*dp = selected;
	    bu_free(attr_name, "trimmed default object attribute");
	    bu_avs_free(&avs);
	    ret = 1;
	    goto cleanup;
	}

	bu_avs_free(&avs);
    }

    matches = db_default_object_top_geom(tops, tops_cnt, &selected);
    if (matches == 1) {
	if (dp)
	    *dp = selected;
	ret = 1;
	goto cleanup;
    }

    combs_cnt = db_ls(dbip, DB_LS_COMB, NULL, &combs);
    db_default_object_sort(combs, combs_cnt);

    matches = db_default_object_count(combs, combs_cnt, &selected);
    if (matches == 1) {
	if (dp)
	    *dp = selected;
	ret = 1;
	goto cleanup;
    }

    matches = db_default_object_filename_matches(dbip, combs, combs_cnt, &selected);
    if (matches == 1) {
	if (dp)
	    *dp = selected;
	ret = 1;
	goto cleanup;
    }

    if (matches > 1) {
	db_default_object_msg(msg, tops, tops_cnt,
			      "No object(s) specified, and more than one matches database filename.\n");
	ret = 0;
	goto cleanup;
    }

    matches = db_default_object_all_scene_matches(tops, tops_cnt, &selected);
    if (matches == 1) {
	if (dp)
	    *dp = selected;
	ret = 1;
	goto cleanup;
    }

    if (matches > 1) {
	db_default_object_msg(msg, tops, tops_cnt,
			      "No object(s) specified, and more than one top-level is prefixed all* or scene*.\n");
	ret = 0;
	goto cleanup;
    }

    db_default_object_msg(msg, tops, tops_cnt,
			  "No object(s) specified, and BRL-CAD could not infer one automatically.\n");

cleanup:
    if (tops)
	bu_free(tops, "db_default_object tops");
    if (combs)
	bu_free(combs, "db_default_object combs");

    return ret;
}


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
