/*                     R E G I O N F I X . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/regionfix.c
 *
 * Subroutines for adjusting old GIFT-style region-IDs, to take into
 * account the presence of instancing.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"


/**
 * R T_ R E G I O N F I X
 *
 * Apply any deltas to reg_regionid values to allow old applications
 * that use the reg_regionid number to distinguish between different
 * instances of the same prototype region.
 *
 * Called once, from rt_prep(), before raytracing begins.
 */
void
rt_regionfix(struct rt_i *rtip)
{
    FILE *fp;
    char *file;
    char *line;
    char *tabp;
    int linenum = 0;
    register struct region *rp;
    int ret;
    int oldid;
    int newid;
    struct bu_vls name;

    RT_CK_RTI(rtip);

    /* If application has provided an alternative file name before
     * rt_prep() was called, then use that.  Otherwise, replace ".g"
     * suffix on database name with ".regexp".
     */
    bu_vls_init(&name);
    file = rtip->rti_region_fix_file;
    if (file == (char *)NULL) {
	bu_vls_strcpy(&name, rtip->rti_dbip->dbi_filename);
	if ((tabp = strrchr(bu_vls_addr(&name), '.')) != NULL) {
	    /* Chop off "." and suffix */
	    bu_vls_trunc(&name, tabp-bu_vls_addr(&name));
	}
	bu_vls_strcat(&name, ".regexp");
	file = bu_vls_addr(&name);
    }

    fp = fopen(file, "rb");
    if (fp == NULL) {
	if (rtip->rti_region_fix_file) perror(file);
	bu_vls_free(&name);
	return;
    }
    bu_log("librt/rt_regionfix(%s):  Modifying instanced region-ids.\n", file);

    while ((line = rt_read_cmd(fp)) != (char *) 0) {
	regex_t re_space;
	linenum++;

	/* For now, establish a simple format:
	 * regexp TAB [more_white_space] formula SEMICOLON
	 */
	if ((tabp = strchr(line, '\t')) == (char *)0) {
	    bu_log("%s: missing TAB on line %d:\n%s\n", file, linenum, line);
	    continue;		/* just ignore it */
	}

	*tabp++ = '\0';
	while (*tabp && isspace(*tabp)) tabp++;
	if ((ret = regcomp(&re_space, line, 0)) != 0) {
	    bu_log("%s: line %d, regcomp error '%d'\n", file, line, ret);
	    continue;		/* just ignore it */
	}

	for (BU_LIST_FOR(rp, region, &(rtip->HeadRegion))) {
	    ret = regexec(&re_space, (char *)rp->reg_name, 0, 0, 0);
	    if (RT_G_DEBUG&DEBUG_INSTANCE) {
		bu_log("'%s' %s '%s'\n", line,
		       ret==1 ? "==" : "!=",
		       rp->reg_name);
	    }
	    if ((ret) == 0)
		continue;	/* didn't match */
	    if (ret == -1) {
		bu_log("%s: line %d, invalid regular expression\n", file, linenum);
		break;		/* on to next RE */
	    }

	    /*
	     * RE matched this name, perform indicated operation
	     * For now, choices are limited.  Later this might
	     * become an interpreted expression.  For now:
	     * 99	replace old region id with "num"
	     * +99	increment old region id with "num"
	     *          (which may itself be a negative number)
	     * +uses	increment old region id by the
	     *          current instance (use) count.
	     */
	    oldid = rp->reg_regionid;
	    if (BU_STR_EQUAL(tabp, "+uses")) {
		newid = oldid + rp->reg_instnum;
	    } else if (*tabp == '+') {
		newid = oldid + atoi(tabp+1);
	    } else {
		newid = atoi(tabp);
		if (newid == 0) bu_log("%s, line %d Warning:  new id = 0\n", file, linenum);
	    }
	    if (RT_G_DEBUG&DEBUG_INSTANCE) {
		bu_log("%s instance %d:  region id changed from %d to %d\n",
		       rp->reg_name, rp->reg_instnum,
		       oldid, newid);
	    }
	    rp->reg_regionid = newid;
	}
	regfree(&re_space);
	bu_free(line, "reg_expr line");
    }
    fclose(fp);
    bu_vls_free(&name);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
