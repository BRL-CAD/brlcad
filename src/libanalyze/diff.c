/*                        D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file g_diff.c
 *
 * Routines to determine the differences between two BRL-CAD databases.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "bio.h"

#include "tcl.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "mater.h"

#define HUMAN 0
#define SCRIPT 1

/* type of adjustment, for do_compare() */
#define PARAMS 1
#define ATTRS  2
#if 0
HIDDEN int
compare_color_tables(struct bu_vls *diff_log, int mode, struct mater *mater_hd1, struct mater *mater_hd2)
{
    struct mater *mp1, *mp2;
    int found1 = 0, found2 = 0;
    int is_diff = 0;

    /* find a match for all color table entries of file1 in file2 */
    for (mp1 = mater_hd1; is_diff == 0 && mp1 != MATER_NULL; mp1 = mp1->mt_forw) {
	found1 = 0;
	mp2 = mater_hd2;
	while (mp2 != MATER_NULL) {
	    if (mp1->mt_low == mp2->mt_low
		&& mp1->mt_high == mp2->mt_high
		&& mp1->mt_r == mp2->mt_r
		&& mp1->mt_g == mp2->mt_g
		&& mp1->mt_b == mp2->mt_b)
	    {
		found1 = 1;
		break;
	    }
	    mp2 = mp2->mt_forw;
	}
	if (!found1) {
	    /* bu_log("could not find %d..%d: %d %d %d\n", mp1->mt_low, mp1->mt_high, mp1->mt_r, mp1->mt_g, mp1->mt_b); */
	    is_diff = 1;
	}
    }

    /* find a match for all color table entries of file2 in file1 */
    for (mp2 = mater_hd2; is_diff == 0 && mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	found2 = 0;
	mp1 = mater_hd1;
	while (mp1 != MATER_NULL) {
	    if (mp1->mt_low == mp2->mt_low
		&& mp1->mt_high == mp2->mt_high
		&& mp1->mt_r == mp2->mt_r
		&& mp1->mt_g == mp2->mt_g
		&& mp1->mt_b == mp2->mt_b)
	    {
		found2 = 1;
		break;
	    }
	    mp1 = mp1->mt_forw;
	}
	if (!found2) {
	    /* bu_log("could not find %d..%d: %d %d %d\n", mp1->mt_low, mp1->mt_high, mp1->mt_r, mp1->mt_g, mp1->mt_b); */
	    is_diff = 1;
	}
    }

    if (is_diff == 0) {
	return 0;
    }

    if (mode == HUMAN) {
	bu_vls_printf(diff_log, "Color table has changed from:\n");
	for (mp1 = mater_hd1; mp1 != MATER_NULL; mp1 = mp1->mt_forw) {
	    bu_vls_printf(diff_log, "\t%d..%d %d %d %d\n", mp1->mt_low, mp1->mt_high,
		   mp1->mt_r, mp1->mt_g, mp1->mt_b);
	}
	bu_vls_printf(diff_log, "\t\tto:\n");
	for (mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	    bu_vls_printf(diff_log, "\t%d..%d %d %d %d\n", mp2->mt_low, mp2->mt_high,
		   mp2->mt_r, mp2->mt_g, mp2->mt_b);
	}
    } else {
	/* punt, just delete the existing colortable and print a new one */
	bu_vls_printf(diff_log, "attr rm _GLOBAL regionid_colortable\n");
	for (mp2 = mater_hd2; mp2 != MATER_NULL; mp2 = mp2->mt_forw) {
	    bu_vls_printf(diff_log, "color %d %d %d %d %d\n", mp2->mt_low, mp2->mt_high,
		    mp2->mt_r, mp2->mt_g, mp2->mt_b);
	}
    }
    return 1;
}


HIDDEN void
kill_obj(char *name, int mode)
{
    if (mode == HUMAN) {
	printf("%s has been killed\n", name);
    } else {
	printf("kill %s\n", name);
    }
}

HIDDEN int
compare_external(struct directory *dp1, struct directory *dp2, struct db_i *dbip1, struct db_i *dbip2, int mode)
{
    int killit = 0;
    struct bu_external ext1, ext2;

    if (db_get_external(&ext1, dp1, dbip1)) {
	fprintf(stderr, "ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename);
	bu_exit(1, NULL);
    }
    if (db_get_external(&ext2, dp2, dbip2)) {
	fprintf(stderr, "ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename);
	bu_exit(1, NULL);
    }

    if (ext1.ext_nbytes != ext2.ext_nbytes) {
	printf("Byte counts are different on %s (%ld != %ld)\n", dp1->d_namep, (long int)ext1.ext_nbytes, (long int)ext2.ext_nbytes);
	killit = 1;
    }

    if (memcmp((void *)ext1.ext_buf, (void *)ext2.ext_buf, ext1.ext_nbytes)) {
	printf("Byte value(s) are different on %s (has no Tcl list representation)\n", dp1->d_namep);
	killit = 1;
    }

    if (killit) {
	if (mode == HUMAN) {
	    printf("kill %s and import it from %s\n", dp1->d_namep, dbip1->dbi_filename);
	} else {
	    printf("kill %s\n# IMPORT %s from %s\n", dp1->d_namep, dp2->d_namep, dbip2->dbi_filename);
	}
	return 1;
    }
    return 0;
}

HIDDEN int
isNumber(char *s)
{
    while (*s != '\0') {
	if (isdigit((int)*s) || *s == '.' || *s == '-' || *s == '+' || *s == 'e' || *s == 'E') {
	    s++;
	} else {
	    return 0;
	}
    }

    return 1;
}

HIDDEN int
compare_values(int type, Tcl_Obj *val1, Tcl_Obj *val2, int use_floats)
{
    int len1, len2;
    int i;
    int str_eq;
    float a, b;
    Tcl_Obj *obj1, *obj2;

    str_eq = BU_STR_EQUAL(Tcl_GetStringFromObj(val1, NULL), Tcl_GetStringFromObj(val2, NULL));

    if (str_eq || type == ATTRS) {
	return 0;
    }

    Tcl_ListObjLength(NULL, val1, &len1);
    Tcl_ListObjLength(NULL, val2, &len2);

    if (len1 != len2) {
	return 1;
    }

    for (i = 0; i<len1; i++) {
	char *str1;
	char *str2;

	Tcl_ListObjIndex(NULL, val1, i, &obj1);
	Tcl_ListObjIndex(NULL, val2, i, &obj2);
	str1 = Tcl_GetString(obj1);
	str2 = Tcl_GetString(obj2);

	if (use_floats && (isNumber(str1) && isNumber(str2))) {
	    a = atof(str1);
	    b = atof(str2);

	    if (!ZERO(a - b)) {
		return 1;
	    }
	} else {
	    if (!BU_STR_EQUAL(str1, str2)) {
		return strstr(str2, str1)?2:1;
	    }
	}
    }

    return 0;
}

HIDDEN int
do_compare(int type, struct bu_vls *diff_log, struct bu_vls *vls, Tcl_Obj *obj1, Tcl_Obj *obj2, char *obj_name, int mode, int use_floats, int evolutionary)
{
    Tcl_Obj *key1, *val1, *key2, *val2;
    int len1, len2, found, junk;
    int i, j;
    int start_index;
    int found_diffs = 0;
    int ev = 0;

    Tcl_ListObjLength(NULL, obj1, &len1);
    Tcl_ListObjLength(NULL, obj2, &len2);

    if (!len1 && !len2)
	return 0;

    if (type == ATTRS) {
	start_index = 0;
    } else {
	start_index = 1;
    }

    /* check for changed values from object 1 to object2 */
    for (i=start_index; i<len1; i+=2) {
	Tcl_ListObjIndex(NULL, obj1, i, &key1);
	Tcl_ListObjIndex(NULL, obj1, i+1, &val1);
	found = 0;
	for (j=start_index; j<len2; j += 2) {
	    Tcl_ListObjIndex(NULL, obj2, j, &key2);
	    if (BU_STR_EQUAL(Tcl_GetStringFromObj(key1, &junk), Tcl_GetStringFromObj(key2, &junk))) {
		found = 1;
		Tcl_ListObjIndex(NULL, obj2, j+1, &val2);
		/* check if this value has changed */
		ev = compare_values(type, val1, val2, use_floats);
		if (ev) {
		    if (!found_diffs++) {
			if (mode == HUMAN) {
			    bu_vls_printf(diff_log, "%s has changed:\n", obj_name);
			}
		    }
		    if (mode == HUMAN) {
			if (type == PARAMS) {
			    bu_vls_printf(diff_log,"\tparameter %s has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
				   Tcl_GetStringFromObj(key1, &junk),
				   Tcl_GetStringFromObj(val1, &junk),
				   Tcl_GetStringFromObj(val2, &junk));
			} else {
			    bu_vls_printf(diff_log,"\t%s attribute \"%s\" has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
				   obj_name,
				   Tcl_GetStringFromObj(key1, &junk),
				   Tcl_GetStringFromObj(val1, &junk),
				   Tcl_GetStringFromObj(val2, &junk));
			}
		    } else {
			int val_len;

			if (type == ATTRS) {
			    bu_vls_printf(vls, "attr set %s ", obj_name);
			} else {
			    bu_vls_strcat(vls, " ");
			}
			bu_vls_strcat(vls, Tcl_GetStringFromObj(key1, &junk));
			bu_vls_strcat(vls, " ");
			Tcl_ListObjLength(NULL, val2, &val_len);
			if (val_len > 1)
			    bu_vls_putc(vls, '{');
			bu_vls_strcat(vls, Tcl_GetStringFromObj(val2, &junk));
			if (val_len > 1)
			    bu_vls_putc(vls, '}');
			if (type == ATTRS) {
			    bu_vls_putc(vls, '\n');
			}
		    }
		}
		break;
	    }
	}
	if (!found) {
	    /* this keyword value pair has been eliminated */
	    if (!found_diffs++) {
		if (mode == HUMAN) {
		    bu_vls_printf(diff_log,"%s has changed:\n", obj_name);
		}
	    }
	    if (mode == HUMAN) {
		if (type == PARAMS) {
		    bu_vls_printf(diff_log,"\tparameter %s has been eliminated\n",
			   Tcl_GetStringFromObj(key1, &junk));
		} else {
		    bu_vls_printf(diff_log,"\tattribute \"%s\" has been eliminated from %s\n",
			   Tcl_GetStringFromObj(key1, &junk), obj_name);
		}
	    } else {
		if (type == ATTRS) {
		    bu_vls_printf(vls, "attr rm %s %s\n", obj_name,
				  Tcl_GetStringFromObj(key1, &junk));
		} else {
		    bu_vls_strcat(vls, " ");
		    bu_vls_strcat(vls, Tcl_GetStringFromObj(key1, &junk));
		    bu_vls_strcat(vls, " none");
		}
	    }
	}
    }

    /* check for keyword value pairs in object 2 that don't appear in object 1 */
    for (i=start_index; i<len2; i+= 2) {
	/* get keyword/value pairs from object 2 */
	Tcl_ListObjIndex(NULL, obj2, i, &key2);
	Tcl_ListObjIndex(NULL, obj2, i+1, &val2);
	found = 0;
	/* look for this keyword in object 1 */
	for (j=start_index; j<len1; j += 2) {
	    Tcl_ListObjIndex(NULL, obj1, j, &key1);
	    if (BU_STR_EQUAL(Tcl_GetStringFromObj(key1, &junk), Tcl_GetStringFromObj(key2, &junk))) {
		found = 1;
		break;
	    }
	}
	if (found)
	    continue;

	/* This keyword/value pair in object 2 is not in object 1 */
	if (!found_diffs++) {
	    if (mode == HUMAN) {
		bu_vls_printf(diff_log,"%s has changed:\n", obj_name);
	    }
	}
	if (mode == HUMAN) {
	    if (type == PARAMS) {
		bu_vls_printf(diff_log,"\t%s has new parameter \"%s\" with value %s\n",
		       obj_name,
		       Tcl_GetStringFromObj(key2, &junk),
		       Tcl_GetStringFromObj(val2, &junk));
	    } else {
		bu_vls_printf(diff_log,"\t%s has new attribute \"%s\" with value {%s}\n",
		       obj_name,
		       Tcl_GetStringFromObj(key2, &junk),
		       Tcl_GetStringFromObj(val2, &junk));
	    }
	} else {
	    int val_len;

	    if (type == ATTRS) {
		bu_vls_printf(vls, "attr set %s ", obj_name);
	    } else {
		bu_vls_strcat(vls, " ");
	    }
	    bu_vls_strcat(vls, Tcl_GetStringFromObj(key2, &junk));
	    bu_vls_strcat(vls, " ");
	    Tcl_ListObjLength(NULL, val2, &val_len);
	    if (val_len > 1)
		bu_vls_putc(vls, '{');
	    bu_vls_strcat(vls, Tcl_GetStringFromObj(val2, &junk));
	    if (val_len > 1)
		bu_vls_putc(vls, '}');
	    if (type == ATTRS)
		bu_vls_putc(vls, '\n');
	}
    }

    if (evolutionary && found_diffs)
	bu_vls_strcat(vls, ev == 2 ? " (Evolutionary)" : " (Reworked)");

    return found_diffs;
}

HIDDEN int
do_attr_compare(struct bu_vls *diff_log, struct bu_vls *vls, struct bu_attribute_value_set *avs1, struct bu_attribute_value_set *avs2, char *obj_name, int mode, int use_floats, int evolutionary)
{
    Tcl_Obj *key1, *val1, *key2, *val2;
    int len1, len2, found, junk;
    int i, j;
    int start_index = 0;
    int found_diffs = 0;
    int ev = 0;

    Tcl_ListObjLength(NULL, obj1, &len1);
    Tcl_ListObjLength(NULL, obj2, &len2);

    if (!len1 && !len2)
	return 0;

    /* check for changed values from object 1 to object2 */
    for (i=start_index; i<len1; i+=2) {
	Tcl_ListObjIndex(NULL, obj1, i, &key1);
	Tcl_ListObjIndex(NULL, obj1, i+1, &val1);
	found = 0;
	for (j=start_index; j<len2; j += 2) {
	    Tcl_ListObjIndex(NULL, obj2, j, &key2);
	    if (BU_STR_EQUAL(Tcl_GetStringFromObj(key1, &junk), Tcl_GetStringFromObj(key2, &junk))) {
		found = 1;
		Tcl_ListObjIndex(NULL, obj2, j+1, &val2);
		/* check if this value has changed */
		ev = BU_STR_EQUAL(val1, val2);
		if (ev) {
		    if (!found_diffs++) {
			if (mode == HUMAN) {
			    bu_vls_printf(diff_log, "%s has changed:\n", obj_name);
			}
		    }
		    if (mode == HUMAN) {
			if (type == PARAMS) {
			    bu_vls_printf(diff_log,"\tparameter %s has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
				   Tcl_GetStringFromObj(key1, &junk),
				   Tcl_GetStringFromObj(val1, &junk),
				   Tcl_GetStringFromObj(val2, &junk));
			} else {
			    bu_vls_printf(diff_log,"\t%s attribute \"%s\" has changed from:\n\t\t%s\n\tto:\n\t\t%s\n",
				   obj_name,
				   Tcl_GetStringFromObj(key1, &junk),
				   Tcl_GetStringFromObj(val1, &junk),
				   Tcl_GetStringFromObj(val2, &junk));
			}
		    } else {
			int val_len;

			if (type == ATTRS) {
			    bu_vls_printf(vls, "attr set %s ", obj_name);
			} else {
			    bu_vls_strcat(vls, " ");
			}
			bu_vls_strcat(vls, Tcl_GetStringFromObj(key1, &junk));
			bu_vls_strcat(vls, " ");
			Tcl_ListObjLength(NULL, val2, &val_len);
			if (val_len > 1)
			    bu_vls_putc(vls, '{');
			bu_vls_strcat(vls, Tcl_GetStringFromObj(val2, &junk));
			if (val_len > 1)
			    bu_vls_putc(vls, '}');
			if (type == ATTRS) {
			    bu_vls_putc(vls, '\n');
			}
		    }
		}
		break;
	    }
	}
	if (!found) {
	    /* this keyword value pair has been eliminated */
	    if (!found_diffs++) {
		if (mode == HUMAN) {
		    bu_vls_printf(diff_log,"%s has changed:\n", obj_name);
		}
	    }
	    if (mode == HUMAN) {
		if (type == PARAMS) {
		    bu_vls_printf(diff_log,"\tparameter %s has been eliminated\n",
			   Tcl_GetStringFromObj(key1, &junk));
		} else {
		    bu_vls_printf(diff_log,"\tattribute \"%s\" has been eliminated from %s\n",
			   Tcl_GetStringFromObj(key1, &junk), obj_name);
		}
	    } else {
		if (type == ATTRS) {
		    bu_vls_printf(vls, "attr rm %s %s\n", obj_name,
				  Tcl_GetStringFromObj(key1, &junk));
		} else {
		    bu_vls_strcat(vls, " ");
		    bu_vls_strcat(vls, Tcl_GetStringFromObj(key1, &junk));
		    bu_vls_strcat(vls, " none");
		}
	    }
	}
    }

    /* check for keyword value pairs in object 2 that don't appear in object 1 */
    for (i=start_index; i<len2; i+= 2) {
	/* get keyword/value pairs from object 2 */
	Tcl_ListObjIndex(NULL, obj2, i, &key2);
	Tcl_ListObjIndex(NULL, obj2, i+1, &val2);
	found = 0;
	/* look for this keyword in object 1 */
	for (j=start_index; j<len1; j += 2) {
	    Tcl_ListObjIndex(NULL, obj1, j, &key1);
	    if (BU_STR_EQUAL(Tcl_GetStringFromObj(key1, &junk), Tcl_GetStringFromObj(key2, &junk))) {
		found = 1;
		break;
	    }
	}
	if (found)
	    continue;

	/* This keyword/value pair in object 2 is not in object 1 */
	if (!found_diffs++) {
	    if (mode == HUMAN) {
		bu_vls_printf(diff_log,"%s has changed:\n", obj_name);
	    }
	}
	if (mode == HUMAN) {
	    if (type == PARAMS) {
		bu_vls_printf(diff_log,"\t%s has new parameter \"%s\" with value %s\n",
		       obj_name,
		       Tcl_GetStringFromObj(key2, &junk),
		       Tcl_GetStringFromObj(val2, &junk));
	    } else {
		bu_vls_printf(diff_log,"\t%s has new attribute \"%s\" with value {%s}\n",
		       obj_name,
		       Tcl_GetStringFromObj(key2, &junk),
		       Tcl_GetStringFromObj(val2, &junk));
	    }
	} else {
	    int val_len;

	    if (type == ATTRS) {
		bu_vls_printf(vls, "attr set %s ", obj_name);
	    } else {
		bu_vls_strcat(vls, " ");
	    }
	    bu_vls_strcat(vls, Tcl_GetStringFromObj(key2, &junk));
	    bu_vls_strcat(vls, " ");
	    Tcl_ListObjLength(NULL, val2, &val_len);
	    if (val_len > 1)
		bu_vls_putc(vls, '{');
	    bu_vls_strcat(vls, Tcl_GetStringFromObj(val2, &junk));
	    if (val_len > 1)
		bu_vls_putc(vls, '}');
	    if (type == ATTRS)
		bu_vls_putc(vls, '\n');
	}
    }

    if (evolutionary && found_diffs)
	bu_vls_strcat(vls, ev == 2 ? " (Evolutionary)" : " (Reworked)");

    return found_diffs;
}

int
compare_tcl_solids(struct bu_vls *diff_log, char *str1, char *str2, struct directory *dp1, int mode, int use_floats, int evolutionary)
{
    char *c1, *c2;
    struct bu_vls adjust = BU_VLS_INIT_ZERO;
    int different = 0;

    /* check if same solid type */
    c1 = str1;
    c2 = str2;
    while (*c1 != ' ' && *c2 != ' ' && *c1++ == *c2++);

    if (*c1 != *c2) {
	/* different solid types */
	if (mode == HUMAN)
	    bu_vls_printf(diff_log,"solid %s:\n\twas: %s\n\tis now: %s\n\n", dp1->d_namep, str1, str2);
	else
	    bu_vls_printf(diff_log,"kill %s\ndb put %s %s\n", dp1->d_namep, dp1->d_namep, str2);

	return 1;
    } else if (BU_STR_EQUAL(str1, str2)) {
	return 0;		/* no difference */
    }

    /* same solid type, split tcl list into bu_attribute_value_set for comparison */

    if (mode == SCRIPT) {
	bu_vls_printf(&adjust, "db adjust %s", dp1->d_namep);
    }

    different = do_compare(PARAMS, diff_log, &adjust, str1, str2, dp1->d_namep, mode, use_floats, evolutionary);

    if (mode != HUMAN) {
	bu_vls_printf(diff_log,"%s\n", bu_vls_addr(&adjust));
    }

    bu_vls_free(&adjust);

    return different;
}

int
compare_tcl_combs(struct bu_vls *diff_log, Tcl_Obj *obj1, struct directory *dp1, Tcl_Obj *obj2, int mode, int use_floats, int evolutionary)
{
    int junk;
    struct bu_vls adjust = BU_VLS_INIT_ZERO;
    int different = 0;

    /* first check if there is any difference */
    if (BU_STR_EQUAL(Tcl_GetStringFromObj(obj1, &junk), Tcl_GetStringFromObj(obj2, &junk)))
	return 0;

    if (mode != HUMAN) {
	bu_vls_printf(&adjust, "db adjust %s", dp1->d_namep);
    }

    different = do_compare(PARAMS, diff_log, &adjust, obj1, obj2, dp1->d_namep, mode, use_floats, evolutionary);

    if (mode != HUMAN) {
	printf("%s\n", bu_vls_addr(&adjust));
    }

    bu_vls_free(&adjust);

    return different;
}

void
verify_region_attrs(struct directory *dp, struct db_i *dbip, Tcl_Obj *obj)
{
    Tcl_Obj **objs;
    int len = 0;
    int i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	fprintf(stderr, "Cannot import %s\n", dp->d_namep);
	return;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CHECK_COMB(comb);

    Tcl_ListObjGetElements(NULL, obj, &len, &objs);

    for (i=1; i<len; i += 2) {
	char *key, *value;

	key = Tcl_GetStringFromObj(objs[i-1], NULL);
	value = Tcl_GetStringFromObj(objs[i], NULL);
	if (BU_STR_EQUAL(key, "region_id")) {
	    long id;

	    id = strtol(value, NULL, 0);
	    if (id != comb->region_id) {
		fprintf(stderr, "WARNING: %s in %s: \"region_id\" attribute says %ld, while region says %ld\n",
			dp->d_namep, dbip->dbi_filename, id, comb->region_id);
	    }
	} else if (BU_STR_EQUAL(key, "giftmater")) {
	    long GIFTmater;

	    GIFTmater = strtol(value, NULL, 0);
	    if (GIFTmater != comb->GIFTmater) {
		fprintf(stderr, "WARNING: %s in %s: \"giftmater\" attribute says %ld, while region says %ld\n",
			dp->d_namep, dbip->dbi_filename, GIFTmater, comb->GIFTmater);
	    }
	} else if (BU_STR_EQUAL(key, "los")) {
	    long los;

	    los = strtol(value, NULL, 0);
	    if (los != comb->los) {
		fprintf(stderr, "WARNING: %s in %s: \"los\" attribute says %ld, while region says %ld\n",
			dp->d_namep, dbip->dbi_filename, los, comb->los);
	    }
	} else if (BU_STR_EQUAL(key, "material")) {
	    if (!bu_strncmp(value, "gift", 4)) {
		long GIFTmater;

		GIFTmater = strtol(&value[4], NULL, 0);
		if (GIFTmater != comb->GIFTmater) {
		    fprintf(stderr, "WARNING: %s in %s: \"material\" attribute says %s, while region says %ld\n",
			    dp->d_namep, dbip->dbi_filename, value, comb->GIFTmater);
		}
	    }
	} else if (BU_STR_EQUAL(key, "aircode")) {
	    long aircode;

	    aircode = strtol(value, NULL, 0);
	    if (aircode != comb->aircode) {
		fprintf(stderr, "WARNING: %s in %s: \"aircode\" attribute says %ld, while region says %ld\n",
			dp->d_namep, dbip->dbi_filename, aircode, comb->aircode);
	    }
	}
    }
    rt_db_free_internal(&intern);
}

static char *region_attrs[] = { "region",
				"region_id",
				"giftmater",
				"los",
				"aircode",
				NULL };
void
remove_region_attrs(Tcl_Obj *obj)
{
    int len = 0;
    Tcl_Obj **objs;
    char *key;
    int i, j;
    int found_material = 0;

    Tcl_ListObjGetElements(NULL, obj, &len, &objs);

    if (len == 0)
	return;

    for (i=len-1; i>0; i -= 2) {

	key = Tcl_GetStringFromObj(objs[i-1], NULL);
	j = 0;
	while (region_attrs[j]) {
	    if (BU_STR_EQUAL(key, region_attrs[j])) {
		Tcl_ListObjReplace(NULL, obj, i-1, 2, 0, NULL);
		break;
	    }
	    j++;
	}
	if (!found_material && BU_STR_EQUAL(key, "material")) {
	    found_material = 1;
	    if (!bu_strncmp(Tcl_GetStringFromObj(objs[i], NULL), "gift", 4)) {
		Tcl_ListObjReplace(NULL, obj, i-1, 2, 0, NULL);
	    }
	}
    }
}


int
compare_attrs(struct directory *dp1, struct directory *dp2, struct db_i *dbip1, struct db_i *dbip2)
{
    struct bu_attribute_value_set avs1, avs2;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *obj1, *obj2;
    int different = 0;

    int has_attributes_1 = 0;
    int has_attributes_2 = 0;

    has_attributes_1 = db5_get_attributes(dbip1, &avs1, dp1);
    has_attributes_2 = db5_get_attributes(dbip1, &avs2, dp2);

    if (has_attributes_1 != has_attributes_2) return 1;

    if ((dp1->d_flags & RT_DIR_REGION) && (dp2->d_flags & RT_DIR_REGION)) {
	/* don't complain about "region" attributes */
    }

    bu_vls_trunc(&vls, 0);
    different = do_compare(ATTRS, &vls, &avs1, &avs2, dp1->d_namep);

    printf("%s", bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return different;
}

#endif

/* Status:
 * 0 - valid diff info;
 * 1 - problem during diffing;
 *
 * Internal diff types:
 *
 * 0 - no difference;
 * 1 - in original only;
 * 2 - in new only;
 * 3 - type difference;
 * 4 - binary-only difference;
 * 5 - parameter differences;
 *
 * Additional diff types (valid only for internal diff types 0, 3, 4, and 5):
 *  0 - no difference;
 *  1 - in original only;
 *  2 - in new only;
 *  3 - additional attributes in original only, no common with differing values;
 *  4 - additional attributes in new only, no common with differing values;
 *  5 - complex attribute diffs
 */
struct gdiff_result {
    int status;
    struct directory *dp_orig;
    struct directory *dp_new;
    struct rt_db_internal *intern_orig;
    struct rt_db_internal *intern_new;
    int internal_diff_type;
    struct bu_attribute_value_set internal_shared;
    struct bu_attribute_value_set internal_orig_only;
    struct bu_attribute_value_set internal_new_only;
    struct bu_attribute_value_set internal_orig_diff;
    struct bu_attribute_value_set internal_new_diff;
    int additional_diff_type;
    struct bu_attribute_value_set additional_shared;
    struct bu_attribute_value_set additional_orig_only;
    struct bu_attribute_value_set additional_new_only;
    struct bu_attribute_value_set additional_orig_diff;
    struct bu_attribute_value_set additional_new_diff;
};

void
gdiff_init(struct gdiff_result *result){
    result->status = 0;
    result->internal_diff_type = 0;
    result->intern_orig = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_orig");
    result->intern_new = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_new");
    RT_DB_INTERNAL_INIT(result->intern_orig);
    RT_DB_INTERNAL_INIT(result->intern_new);
    BU_AVS_INIT(&result->internal_shared);
    BU_AVS_INIT(&result->internal_orig_only);
    BU_AVS_INIT(&result->internal_new_only);
    BU_AVS_INIT(&result->internal_orig_diff);
    BU_AVS_INIT(&result->internal_new_diff);
    BU_AVS_INIT(&result->additional_shared);
    BU_AVS_INIT(&result->additional_orig_only);
    BU_AVS_INIT(&result->additional_new_only);
    BU_AVS_INIT(&result->additional_orig_diff);
    BU_AVS_INIT(&result->additional_new_diff);
}

void
gdiff_print(struct gdiff_result *result){
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct directory *dp = result->dp_orig;
    if (result->internal_diff_type == 2) {
	dp = result->dp_new;
    }
    bu_log("\n\n%s(%d): (internal type: %d) (additional type: %d)\n", dp->d_namep, result->status, result->internal_diff_type, result->additional_diff_type);
    bu_vls_sprintf(&tmp, "Internal parameters: shared (%s)", dp->d_namep);
    bu_avs_print(&result->internal_shared, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(&result->internal_orig_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(&result->internal_new_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(&result->internal_orig_diff, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Internal parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(&result->internal_new_diff, bu_vls_addr(&tmp));

    bu_vls_sprintf(&tmp, "Additional parameters: shared (%s)", dp->d_namep);
    bu_avs_print(&result->additional_shared, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_only (%s)", dp->d_namep);
    bu_avs_print(&result->additional_orig_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_only (%s)", dp->d_namep);
    bu_avs_print(&result->additional_new_only, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: orig_diff (%s)", dp->d_namep);
    bu_avs_print(&result->additional_orig_diff, bu_vls_addr(&tmp));
    bu_vls_sprintf(&tmp, "Additional parameters: new_diff (%s)", dp->d_namep);
    bu_avs_print(&result->additional_new_diff, bu_vls_addr(&tmp));
}

int
bu_avs_diff(struct bu_attribute_value_set *shared,
	struct bu_attribute_value_set *orig_only,
	struct bu_attribute_value_set *new_only,
	struct bu_attribute_value_set *orig_diff,
	struct bu_attribute_value_set *new_diff,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2,
	struct bn_tol *tol)
{
    struct bu_attribute_value_pair *avp;
    int have_shared = 0;
    int have_orig_only = 0;
    int have_new_only = 0;
    int have_diff = 0;
    if (tol) bu_log("Need to do some floating point value foo\n");
    if (!BU_AVS_IS_INITIALIZED(shared)) BU_AVS_INIT(shared);
    if (!BU_AVS_IS_INITIALIZED(orig_only)) BU_AVS_INIT(orig_only);
    if (!BU_AVS_IS_INITIALIZED(new_only)) BU_AVS_INIT(new_only);
    if (!BU_AVS_IS_INITIALIZED(orig_diff)) BU_AVS_INIT(orig_diff);
    if (!BU_AVS_IS_INITIALIZED(new_diff)) BU_AVS_INIT(new_diff);
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val2 = bu_avs_get(avs2, avp->name);
	if (!val2) {
	    bu_avs_add(orig_only, avp->name, avp->value);
	    have_orig_only++;
	}
	if (BU_STR_EQUAL(avp->value, val2)) {
	    bu_avs_add(shared, avp->name, avp->value);
	    have_shared++;
	} else {
	    bu_avs_add(orig_diff, avp->name, avp->value);
	    bu_avs_add(new_diff, avp->name, val2);
	    have_diff++;
	}
    }
    for (BU_AVS_FOR(avp, avs2)) {
	const char *val1 = bu_avs_get(avs1, avp->name);
	if (!val1) {
	    bu_avs_add(new_only, avp->name, avp->value);
	    have_new_only++;
	}
    }
    if (have_diff) return 5;
    if (!have_orig_only && !have_new_only) return 0;
    if (have_orig_only && !have_new_only && !have_shared) return 1;
    if (!have_orig_only && have_new_only && !have_shared) return 2;
    if (have_orig_only && !have_new_only && have_shared) return 3;
    if (!have_orig_only && have_new_only && have_shared) return 4;
    return 5;
}

int
tcl_list_to_avs(const char *tcl_list, struct bu_attribute_value_set *avs, int offset) {
    int i = 0;
    int list_c = 0;
    const char **listv = (const char **)NULL;

    if (Tcl_SplitList(NULL, tcl_list, &list_c, (const char ***)&listv) != TCL_OK) {
	return -1;
    }

    if (!BU_AVS_IS_INITIALIZED(avs)) BU_AVS_INIT(avs);

    if (!list_c) {
	Tcl_Free((char *)listv);
	return 0;
    }

    for (i = offset; i < list_c; i += 2) {
	(void)bu_avs_add(avs, listv[i], listv[i+1]);
    }

    Tcl_Free((char *)listv);
    return 0;
}

int
diff_dbip(struct db_i *dbip1, struct db_i *dbip2)
{
    int i;
    struct directory *dp1, *dp2;
    struct bu_attribute_value_set avs1, avs2;
    struct bu_vls s1_tcl = BU_VLS_INIT_ZERO;
    struct bu_vls s2_tcl = BU_VLS_INIT_ZERO;
    struct gdiff_result *results = NULL;
    int diff_count = -1;
    int diff_total = 0;
    /*struct bu_vls diff_log = BU_VLS_INIT_ZERO;*/
    int has_diff = 0;
    int have_tcl1 = 1;
    int have_tcl2 = 1;

    /* Get a count of the number of objects in the
     * union of the two candidate databases */
    {
	int dbip1_only = 0;
	int dbip2_only = 0;
	int dbip1_dbip2 = 0;

	FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	    /* check if this object exists in the other database */
	    if (db_lookup(dbip2, dp1->d_namep, 0) == RT_DIR_NULL) {
		dbip1_only++;
	    } else {
		dbip1_dbip2++;
	    }
	} FOR_ALL_DIRECTORY_END;
	FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	    /* check if this object exists in the other database */
	    if (db_lookup(dbip1, dp2->d_namep, 0) == RT_DIR_NULL) {
		dbip2_only++;
	    } else {
		dbip1_dbip2++;
	    }
	} FOR_ALL_DIRECTORY_END;
	dbip1_dbip2 = dbip1_dbip2/2;
	diff_total = dbip1_dbip2 + dbip1_only + dbip2_only;
    }

    /* Populate the results array with enough containers to
     * hold the results of processing the two databases */

    results = (struct gdiff_result *)bu_calloc(diff_total + 1, sizeof(struct gdiff_result), "gdiff results array");

    for (i = 0; i < diff_total; i++) {
	gdiff_init(&(results[i]));
    }

    /* look at all objects in this database */
    FOR_ALL_DIRECTORY_START(dp1, dbip1) {
	struct gdiff_result *curr_result = NULL;
	diff_count++;
	curr_result = &results[diff_count];
	curr_result->dp_orig = dp1;

	/* skip the _GLOBAL object for now - need to deal with this, however */
	if (dp1->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    curr_result->dp_orig = dp1;
	    continue;
	}

	/* Get the internal objects */
	if (rt_db_get_internal(curr_result->intern_orig, dp1, dbip1, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_log("rt_db_get_internal(%s) failure\n", dp1->d_namep);
	    curr_result->status = 1;
	    continue;
	}
	/* check if this object exists in the other database */
	if ((dp2 = db_lookup(dbip2, dp1->d_namep, 0)) == RT_DIR_NULL) {
	    /*kill_obj(dp1->d_namep);*/
	    curr_result->dp_new = NULL;
	    curr_result->internal_diff_type = 1;
	    continue;
	} else {
	    curr_result->dp_new = dp2;
	    if (rt_db_get_internal(curr_result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
		curr_result->status = 1;
		continue;
	    }
	}

	/* Do some type based checking - if we've totally changed
	 * types we don't need to get into the details.
	 * TODO - is that true?  Do we perhaps want to be able
	 * to compare the vertex values of a tor and sphere, even
	 * though the rest of the parameters are different?  How
	 * fine grained do we want to be here? */
	if (curr_result->intern_orig->idb_minor_type != curr_result->intern_new->idb_minor_type) {
	    curr_result->internal_diff_type = 3;
	} else {
	    if (curr_result->intern_orig->idb_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
		struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
		int arb_type_1 = rt_arb_std_type(curr_result->intern_orig, &arb_tol);
		int arb_type_2 = rt_arb_std_type(curr_result->intern_new, &arb_tol);
		if (arb_type_1 != arb_type_2) curr_result->internal_diff_type = 3;
	    }
	}

	/* Try to create attribute/value set definitions for these
	 * objects from their Tcl list definitions.  We use an
	 * offset of one because for all objects the first entry
	 * in the list is the type of the object, which a) isn't
	 * an attribute/value pair and b) we can already get from
	 * the C data structures */
	if (curr_result->internal_diff_type != 3) {
	    bu_vls_trunc(&s1_tcl, 0);
	    if (curr_result->intern_orig->idb_meth->ft_get(&s1_tcl, curr_result->intern_orig, NULL) == BRLCAD_ERROR) have_tcl1 = 0;
	    /*bu_log("dp1: %s\n", bu_vls_addr(&s1_tcl));*/
	    bu_vls_trunc(&s2_tcl, 0);
	    if (curr_result->intern_new->idb_meth->ft_get(&s2_tcl, curr_result->intern_new, NULL) == BRLCAD_ERROR) have_tcl2 = 0;
	    /*bu_log("dp2: %s\n", bu_vls_addr(&s2_tcl));*/
	    if (have_tcl1 && have_tcl2) {
		if (tcl_list_to_avs(bu_vls_addr(&s1_tcl), &avs1, 1)) have_tcl1 = 0;
		/*bu_vls_sprintf(&temp_str, "dp1 core: %s", dp1->d_namep);*/
		if (tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)) have_tcl2 = 0;
		/*bu_vls_sprintf(&temp_str, "dp2 core: %s", dp2->d_namep);*/
	    }
	}
	/* If we have both avs sets, do the detailed comparison */
	if (have_tcl1 && have_tcl2) {
	    int avs_diff_result = bu_avs_diff(&curr_result->internal_shared, &curr_result->internal_orig_only,
		    &curr_result->internal_new_only, &curr_result->internal_orig_diff,
		    &curr_result->internal_new_diff, &avs1, &avs2, (struct bn_tol *)NULL);
	    switch(avs_diff_result) {
		case 0:
		    curr_result->internal_diff_type = 0;
		    break;
		default:
		    curr_result->internal_diff_type = 5;
		    break;
	    }
	} else {
	    /* If we reach this point and don't have successful avs creation, we are reduced
	     * to comparing the binary serializations of the two objects.  This is not ideal
	     * in that it precludes a nuanced description of the differences, but it is at
	     * least able to detect them */
	    struct bu_external ext1, ext2;

	    if (db_get_external(&ext1, dp1, dbip1)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp1->d_namep, dbip1->dbi_filename);
		continue;
	    }
	    if (db_get_external(&ext2, dp2, dbip2)) {
		bu_log("ERROR: db_get_external failed on solid %s in %s\n", dp2->d_namep, dbip2->dbi_filename);
		continue;
	    }

	    if (ext1.ext_nbytes != ext2.ext_nbytes) {
		curr_result->internal_diff_type = 4;
	    }

	    if (!(curr_result->internal_diff_type != 4) &&
		    memcmp((void *)ext1.ext_buf, (void *)ext2.ext_buf, ext1.ext_nbytes)) {
		curr_result->internal_diff_type = 4;
	    }
	}

	/* If we might care, look at the extra attributes as well */
	if (!curr_result->internal_diff_type || curr_result->internal_diff_type >= 3) {
	    if (curr_result->intern_orig->idb_avs.magic == BU_AVS_MAGIC &&
		    curr_result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
		curr_result->additional_diff_type = bu_avs_diff(&curr_result->additional_shared,
			&curr_result->additional_orig_only,
			&curr_result->additional_new_only, &curr_result->additional_orig_diff,
			&curr_result->additional_new_diff,
			&curr_result->intern_orig->idb_avs,
			&curr_result->intern_new->idb_avs,
			(struct bn_tol *)NULL);
	    } else {
		if (curr_result->intern_orig->idb_avs.magic == BU_AVS_MAGIC) {
		    curr_result->additional_diff_type = 1;
		    bu_avs_merge(&curr_result->additional_orig_only, &curr_result->intern_orig->idb_avs);
		}
		if (curr_result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
		    curr_result->additional_diff_type = 2;
		    bu_avs_merge(&curr_result->additional_new_only, &curr_result->intern_new->idb_avs);
		}
	    }
	}

	bu_avs_free(&avs1);
	bu_avs_free(&avs2);

    } FOR_ALL_DIRECTORY_END;

    /* now look for objects in the other database that aren't here */
    FOR_ALL_DIRECTORY_START(dp2, dbip2) {
	struct gdiff_result *curr_result = NULL;
	/* check if this object exists in the other database */
	if (db_lookup(dbip1, dp2->d_namep, 0) == RT_DIR_NULL) {
	    diff_count++;
	    curr_result = &results[diff_count];
	    curr_result->dp_new = dp2;
	    curr_result->internal_diff_type = 2;
	    if (rt_db_get_internal(curr_result->intern_new, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_log("rt_db_get_internal(%s) failure\n", dp2->d_namep);
		curr_result->status = 1;
		continue;
	    }
	    if (curr_result->intern_new->idb_meth->ft_get(&s2_tcl, curr_result->intern_new, NULL) != BRLCAD_ERROR) {
		if (!tcl_list_to_avs(bu_vls_addr(&s2_tcl), &avs2, 1)){
		    curr_result->internal_diff_type = 2;
		    bu_avs_merge(&curr_result->internal_new_only, &avs2);
		    bu_avs_free(&avs2);
		}
	    }
	    curr_result->additional_diff_type = 0;
	    if (curr_result->intern_new->idb_avs.magic == BU_AVS_MAGIC) {
		if (curr_result->intern_new->idb_avs.count > 0) curr_result->additional_diff_type = 2;
		bu_avs_merge(&curr_result->additional_new_only, &curr_result->intern_new->idb_avs);
	    }

	}
    } FOR_ALL_DIRECTORY_END;


    for (i = 0; i < diff_total; i++) {
	gdiff_print(&(results[i]));
    }


    bu_vls_free(&s1_tcl);
    bu_vls_free(&s2_tcl);
    return has_diff;
}

#if 0

int
main(int argc, char **argv)
{
    char *invoked_as;
    char *file1, *file2;
    struct rt_wdb *wdb1, *wdb2;
    int c;
    int different = 0;

    invoked_as = argv[0];

    while ((c = bu_getopt(argc, argv, "emfvh?")) != -1) {
	switch (c) {
	    case 'e':
		evolutionary = 1;
		/* no break, evolutionary mode assumes mged readable */
	    case 'm':	/* mged readable */
		mode = MGED;
		break;
	    case 'f':
		use_floats = 1;
		break;
	    case 'v':	/* verify region attributes */
		verify_region_attribs = 1;
		break;
	    default:
		Usage(invoked_as);
	}
    }

    argc -= bu_optind;
    argv+= bu_optind;

    if (argc != 2)
	Usage(invoked_as);

    file1 = *argv++;
    file2 = *argv;

    if (!bu_file_exists(file1, NULL)) {
	perror(file1);
	bu_exit(1, "Cannot stat file %s\n", file1);
    }

    if (!bu_file_exists(file2, NULL)) {
	perror(file2);
	bu_exit(1, "Cannot stat file %s\n", file2);
    }

    bu_log("Comparing %s and %s\n", file1, file2);

    if (bu_same_file(file1, file2)) {
	bu_exit(1, "%s and %s are the same file\n", file1, file2);
    }

    Tcl_FindExecutable(invoked_as);
    INTERP = Tcl_CreateInterp();
    tclcad_auto_path(INTERP);

    if (Tcl_Init(INTERP) == TCL_ERROR) {
	bu_log("Tcl_Init failure:\n%s\n", Tcl_GetStringResult(INTERP));
    }

    Rt_Init(INTERP);

    if ((dbip1 = db_open(file1, DB_OPEN_READONLY)) == DBI_NULL) {
	perror(file1);
	bu_exit(1, "Cannot open geometry database file %s\n", file1);
    }

    RT_CK_DBI(dbip1);

    if ((wdb1 = wdb_dbopen(dbip1, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	bu_exit(1, "wdb_dbopen failed for %s\n", file1);
    }

    if (db_dirbuild(dbip1) < 0) {
	db_close(dbip1);
	bu_exit(1, "db_dirbuild failed on %s\n", file1);
    }

    Go_Init(INTERP);
    wdb1->wdb_interp = INTERP;
    {
	int ac = 4;
	const char *av[5];

	av[0] = "to_open";
	av[1] = "_db1";
	av[2] = "db";
	av[3] = file1;
	av[4] = NULL;

	if (to_open_tcl(NULL, INTERP, ac, av) != TCL_OK) {
	    bu_exit(1, "ERROR: failed to initialize reading from %s\n", file1);
	}
    }

    /* save regionid colortable */
    mater_hd1 = rt_dup_material_head();
    rt_color_free();

    if (db_version(dbip1) < 5) {
	pre_5_vers++;
    }

    if ((dbip2 = db_open(file2, DB_OPEN_READONLY)) == DBI_NULL) {
	perror(file2);
	bu_exit(1, "Cannot open geometry database file %s\n", file2);
    }

    RT_CK_DBI(dbip2);

    if (db_dirbuild(dbip2) < 0) {
	db_close(dbip1);
	db_close(dbip2);
	bu_exit(1, "db_dirbuild failed on %s\n", file2);
    }

    if ((wdb2 = wdb_dbopen(dbip2, RT_WDB_TYPE_DB_DISK)) == RT_WDB_NULL) {
	db_close(dbip2);
	wdb_close(wdb1);
	bu_exit(1, "wdb_dbopen failed for %s\n", file2);
    }

    wdb2->wdb_interp = INTERP;
    {
	int ac = 4;
	const char *av[5];

	av[0] = "to_open";
	av[1] = "_db2";
	av[2] = "db";
	av[3] = file2;
	av[4] = NULL;

	if (to_open_tcl(NULL, INTERP, ac, av) != TCL_OK) {
	    bu_exit(1, "ERROR: failed to initialize reading from %s\n", file2);
	}
    }

    /* save regionid colortable */
    mater_hd2 = rt_dup_material_head();
    rt_color_free();

    if (db_version(dbip2) < 5) {
	pre_5_vers++;
	version2 = 4;
    } else {
	version2 = 5;
    }

    if (mode == HUMAN) {
	printf("\nChanges from %s to %s\n\n", dbip1->dbi_filename, dbip2->dbi_filename);
    }

    /* compare titles */
    if (!BU_STR_EQUAL(dbip1->dbi_title, dbip2->dbi_title)) {
	different = 1;
	if (mode == HUMAN) {
	    printf("Title has changed from: \"%s\" to: \"%s\"\n\n", dbip1->dbi_title, dbip2->dbi_title);
	} else {
	    printf("title %s\n", dbip2->dbi_title);
	}
    }

    /* and units */
    if (!ZERO(dbip1->dbi_local2base - dbip2->dbi_local2base)) {
	different = 1;
	if (mode == HUMAN) {
	    printf("Units changed from %s to %s\n", bu_units_string(dbip1->dbi_local2base), bu_units_string(dbip2->dbi_local2base));
	} else {
	    printf("units %s\n", bu_units_string(dbip2->dbi_local2base));
	}
    }

    /* and color table */
    if (compare_colors()) {
	different = 1;
    }

    /* finally, compare all the objects */
    if (diff_objs(INTERP, "_db1", "_db2")) {
	different = 1;
    }

    if (!different) {
	/* let the user know if there are no differences */
	printf("No differences.\n");
    }

    return different;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
