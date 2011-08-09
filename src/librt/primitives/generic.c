/*                       G E N E R I C . C
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
/** @addtogroup g_ */
/** @{ */
/** @file primitives/generic.c
 *
 * Generic routines applicable across most primitives
 *
 */
/** @} */

#include "common.h"

#include <string.h>

#include "bu.h"
#include "bn.h"
#include "raytrace.h"


/**
 * R T _ G E N E R I C _ X F O R M
 *
 * Apply a 4x4 transformation matrix to the internal form of a solid.
 *
 * If "free" flag is non-zero, storage for the original solid is
 * released.  If "os" is same as "is", storage for the original solid
 * is overwritten with the new, transformed solid.
 *
 * Returns -
 * -1 FAIL
 *  0 OK
 */
int
rt_generic_xform(
    struct rt_db_internal *op,
    const mat_t mat,
    struct rt_db_internal *ip,
    int release,
    struct db_i *dbip,
    struct resource *resp)
{
    struct bu_external ext;
    int id;
    struct bu_attribute_value_set avs;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resp);

    memset(&avs, 0, sizeof(struct bu_attribute_value_set));

    id = ip->idb_type;
    BU_EXTERNAL_INIT(&ext);
    /* Scale change on export is 1.0 -- no change */
    switch (db_version(dbip)) {
	case 4:
	    if (rt_functab[id].ft_export4(&ext, ip, 1.0, dbip, resp) < 0) {
		bu_log("rt_generic_xform():  %s export failure\n",
		       rt_functab[id].ft_name);
		return -1;			/* FAIL */
	    }
	    if ((release || op == ip)) rt_db_free_internal(ip);

	    RT_DB_INTERNAL_INIT(op);
	    if (rt_functab[id].ft_import4(op, &ext, mat, dbip, resp) < 0) {
		bu_log("rt_generic_xform():  solid import failure\n");
		return -1;			/* FAIL */
	    }
	    break;
	case 5:
	    if (rt_functab[id].ft_export5(&ext, ip, 1.0, dbip, resp) < 0) {
		bu_log("rt_generic_xform():  %s export failure\n",
		       rt_functab[id].ft_name);
		return -1;			/* FAIL */
	    }

	    if ((release || op == ip)) {
		if (ip->idb_avs.magic == BU_AVS_MAGIC) {
		    /* grab the attributes before they are lost
		     * by rt_db_free_internal or RT_DB_INTERNAL_INIT
		     */
		    bu_avs_init(&avs, ip->idb_avs.count, "avs");
		    bu_avs_merge(&avs, &ip->idb_avs);
		}
		rt_db_free_internal(ip);
	    }

	    RT_DB_INTERNAL_INIT(op);

	    if (!release && op != ip) {
		/* just copy the attributes from ip to op */
		if (ip->idb_avs.magic == BU_AVS_MAGIC) {
		    bu_avs_init(&op->idb_avs, ip->idb_avs.count, "avs");
		    bu_avs_merge(&op->idb_avs, &ip->idb_avs);
		}
	    } else if (avs.magic == BU_AVS_MAGIC) {
		/* put the saved attributes in the output */
		bu_avs_init(&op->idb_avs, avs.count, "avs");
		bu_avs_merge(&op->idb_avs, &avs);
		bu_avs_free(&avs);
	    }

	    if (rt_functab[id].ft_import5(op, &ext, mat, dbip, resp) < 0) {
		bu_log("rt_generic_xform():  solid import failure\n");
		return -1;			/* FAIL */
	    }
	    break;
    }

    bu_free_external(&ext);

    RT_CK_DB_INTERNAL(op);
    return 0;				/* OK */
}


/**
 * R T _ G E N E R I C _ G E T
 *
 * This is the generic routine to be listed in rt_functab[].ft_get
 * for those solid types which are fully described by their
 * ft_parsetab entry.
 *
 * 'attr' is specified to retrieve only one attribute, rather than
 * all.  Example: "db get ell.s B" to get only the B vector.
 */
int
rt_generic_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register const struct bu_structparse *sp = NULL;
    register const struct rt_functab *ftp;

    RT_CK_DB_INTERNAL(intern);
    ftp = intern->idb_meth;
    RT_CK_FUNCTAB(ftp);

    sp = ftp->ft_parsetab;
    if (!sp) {
	bu_vls_printf(logstr,
		      "%s {a Tcl output routine for this type of object has not yet been implemented}",
		      ftp->ft_label);
	return BRLCAD_ERROR;
    }

    if (attr == (char *)0) {
	struct bu_vls str;

	bu_vls_init(&str);

	/* Print out solid type and all attributes */
	bu_vls_printf(logstr, "%s", ftp->ft_label);
	while (sp->sp_name != NULL) {
	    bu_vls_printf(logstr, " %s", sp->sp_name);

	    bu_vls_trunc(&str, 0);
	    bu_vls_struct_item(&str, sp,
			       (char *)intern->idb_ptr, ' ');

	    if (sp->sp_count < 2)
		bu_vls_printf(logstr, " %V", &str);
	    else {
		bu_vls_printf(logstr, " {");
		bu_vls_printf(logstr, "%V", &str);
		bu_vls_printf(logstr, "} ");
	    }

	    ++sp;
	}

	bu_vls_free(&str);
    } else {
	if (bu_vls_struct_item_named(logstr, sp, attr, (char *)intern->idb_ptr, ' ') < 0) {
	    bu_vls_printf(logstr,
			  "Objects of type %s do not have a %s attribute.",
			  ftp->ft_label, attr);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


/**
 * R T _ G E N E R I C _ M A K E
 *
 * This one assumes that making all the parameters null is fine.
 */
void
rt_generic_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    intern->idb_type = ftp - rt_functab;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    BU_ASSERT(&rt_functab[intern->idb_type] == ftp);

    intern->idb_meth = ftp;
    intern->idb_ptr = bu_calloc((unsigned int)ftp->ft_internal_size, 1, "rt_generic_make");
    *((uint32_t *)(intern->idb_ptr)) = ftp->ft_internal_magic;
}


/**
 * R T _ G E N E R I C _ A D J U S T
 *
 * For those solids entirely defined by their parsetab.  Invoked via
 * rt_functab[].ft_adjust()
 */
int
rt_generic_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    const struct rt_functab *ftp;

    RT_CK_DB_INTERNAL(intern);
    ftp = intern->idb_meth;
    RT_CK_FUNCTAB(ftp);

    if (ftp->ft_parsetab == (struct bu_structparse *)NULL) {
	bu_vls_printf(logstr, "%s type objects do not yet have a 'db put' (adjust) handler.", ftp->ft_label);
	return BRLCAD_ERROR;
    }

    return bu_structparse_argv(logstr, argc, argv, ftp->ft_parsetab, (char *)intern->idb_ptr);
}


/**
 * R T _ G E N E R I C _ F O R M
 *
 * Invoked via rt_functab[].ft_form() on solid types which are
 * fully described by their bu_structparse table in ft_parsetab.
 */
int
rt_generic_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    if (ftp->ft_parsetab) {
	bu_structparse_get_terse_form(logstr, ftp->ft_parsetab);
	return BRLCAD_OK;
    }
    bu_vls_printf(logstr,
		  "%s is a valid object type, but a 'form' routine has not yet been implemented.",
		  ftp->ft_label);

    return BRLCAD_ERROR;
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
