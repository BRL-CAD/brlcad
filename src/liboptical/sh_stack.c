/*                      S H _ S T A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file liboptical/sh_stack.c
 *
 * Stack multiple material modules together
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


HIDDEN int sh_stk_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mf_p, struct rt_i *rtip);
HIDDEN int sh_stk_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void sh_stk_print(register struct region *rp, genptr_t dp);
HIDDEN void sh_stk_free(genptr_t cp);
HIDDEN int ext_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mf_p, struct rt_i *rtip);

struct mfuncs stk_mfuncs[] = {
    {MF_MAGIC,	"stack",	0,		0,	0,     sh_stk_setup,	sh_stk_render,	sh_stk_print,	sh_stk_free},
    {MF_MAGIC,	"extern",	0,		0,	0,     ext_setup,	sh_stk_render,	sh_stk_print,	sh_stk_free},
    {0,		(char *)0,	0,		0,	0,     0,		0,		0,		0}
};


struct stk_specific {
    struct mfuncs *mfuncs[16];
    genptr_t udata[16];
};
#define STK_NULL ((struct stk_specific *)0)
#define STK_O(m) bu_offsetof(struct stk_specific, m)

struct bu_structparse stk_parse[] = {
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/*
 * E X T _ S E T U P
 *
 * Returns 0 on failure, 1 on success.
 */
HIDDEN int
ext_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mf_p, struct rt_i *rtip)

/* parameter string */
/* pointer to user data pointer */


{
    struct bu_mapped_file *parameter_file;
    struct bu_vls parameter_data;
    char *filename;
    int status;

    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    filename = bu_vls_addr(matparm);
    parameter_file = bu_open_mapped_file(filename, (char *)NULL);
    if (!parameter_file) {
	bu_log("cannot open external shader file \"%s\"\n", filename);
	bu_bomb("ext_setup()\n");
    }

    bu_vls_init(&parameter_data);
    bu_vls_strncpy(&parameter_data, (char *)parameter_file->buf,
		   parameter_file->buflen);

    if (rdebug&RDEBUG_SHADE) {
	bu_log("ext_setup(%s): {%s}\n",
	       filename, bu_vls_addr(&parameter_data));
    }

    bu_close_mapped_file(parameter_file);

    status = sh_stk_setup(rp, &parameter_data, dpp, mf_p, rtip);

    bu_vls_free(&parameter_data);

    return status;
}


/*
 * S T K _ D O S E T U P
 */
HIDDEN int
sh_stk_dosetup(char *cp, struct region *rp, genptr_t *dpp, struct mfuncs **mpp, struct rt_i *rtip)


/* udata pointer address */
/* mfuncs pointer address */


{
    register struct mfuncs *mfp;
    register struct mfuncs *mfp_new;
    struct bu_vls arg;
    char matname[32];
    int ret;
    int i;
    struct mfuncs *shaders = MF_NULL;

    RT_CK_RTI(rtip);

    if (rdebug&RDEBUG_MATERIAL)
	bu_log("...starting \"%s\"\n", cp);

    /* skip leading white space */
    while (*cp == ' ' || *cp == '\t')
	cp++;

    for (i = 0; i < 31 && *cp != '\0'; i++, cp++) {
	if (*cp == ' ' || *cp == '\t') {
	    matname[i++] = '\0';
	    break;
	} else
	    matname[i] = *cp;
    }
    matname[i] = '\0';	/* ensure null termination */

retry:
    /* get list of available shaders */
    if (!shaders) {
	optical_shader_init(&shaders);
    }

    for (mfp = shaders; mfp && mfp->mf_name != NULL; mfp = mfp->mf_forw) {
	if (matname[0] != mfp->mf_name[0]  ||
	    !BU_STR_EQUAL(matname, mfp->mf_name))
	    continue;
	goto found;
    }

    /* If we get here, then the shader wasn't found in the list of
     * compiled-in (or previously loaded) shaders.  See if we can
     * dynamically load it.
     */

    bu_log("Shader \"%s\"... ", matname);

    if ((mfp_new = load_dynamic_shader(matname))) {
	mlib_add_shader(&shaders, mfp_new);
	goto retry;
    }

    bu_log("stack_setup(%s):  material not known\n",
	   matname);
    ret = -1;
    goto out;

found:
    *mpp = mfp;
    *dpp = (char *)0;
    bu_vls_init(&arg);
    if (*cp != '\0')
	bu_vls_strcat(&arg, ++cp);
    if (rdebug&RDEBUG_MATERIAL)
	bu_log("calling %s with %s\n", mfp->mf_name, bu_vls_addr(&arg));
    if (!mfp || !mfp->mf_setup || mfp->mf_setup(rp, &arg, dpp, mfp, rtip) < 0) {
	/* Setup has failed */
	bu_vls_free(&arg);
	ret = -1;		/* BAD */
	goto out;
    }
    bu_vls_free(&arg);
    ret = 0;			/* OK */
out:
    if (rdebug&RDEBUG_MATERIAL)
	bu_log("...finished \"%s\", ret=%d\n", matname, ret);
    return ret;
}


/*
 * S T K _ S E T U P
 *
 * Returns 0 on failure, 1 on success.
 */
HIDDEN int
sh_stk_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mf_p), struct rt_i *rtip)

/* parameter string */
/* pointer to user data pointer */


{
    register struct stk_specific *sp;
    char *cp, *start;
    int i;
    int inputs = 0;
    struct mfuncs *mfp;

    BU_CK_VLS(matparm);
    RT_CK_RTI(rtip);

    BU_GET(sp, struct stk_specific);
    *dpp = sp;

    /*bu_struct_parse(matparm, sh_stk_parse, (char *)sp);*/

    if (rdebug&RDEBUG_MATERIAL || rdebug&RDEBUG_SHADE)
	bu_log("sh_stk_setup called with \"%s\"\n", bu_vls_addr(matparm));

    i = 0;
    start = cp = bu_vls_addr(matparm);
    while (*cp != '\0') {
	if (*cp == ';') {
	    *cp = '\0';
	    if (i >= 16) {
		bu_log("sh_stk_setup: max levels exceeded\n");
		return 0;
	    }
	    /* add one */
	    if (sh_stk_dosetup(start, rp, &sp->udata[i], &sp->mfuncs[i], rtip) == 0) {
		inputs |= sp->mfuncs[i]->mf_inputs;
		i++;
	    } else {
		/* XXX else clear entry? */
		bu_log("Problem in stack shader setup\n");
	    }
	    start = ++cp;
	} else {
	    cp++;
	}
    }
    if (start != cp) {
	if (i >= 16) {
	    bu_log("sh_stk_setup: max levels exceeded\n");
	    return 0;
	}
	/* add one */
	if (sh_stk_dosetup(start, rp, &sp->udata[i], &sp->mfuncs[i], rtip) == 0) {
	    inputs |= sp->mfuncs[i]->mf_inputs;
	    i++;
	} else {
	    /* XXX else clear entry? */
	}
    }

    /* Request only those input bits needed by subordinate shaders */
    BU_GET(mfp, struct mfuncs);
    memcpy((char *)mfp, (char *)rp->reg_mfuncs, sizeof(*mfp));
    mfp->mf_inputs = inputs;
    rp->reg_mfuncs = (genptr_t)mfp;
    return 1;
}


/*
 * S T K _ R E N D E R
 *
 * Evaluate all of the rendering functions in the stack.
 *
 * Returns:
 * 0 stack processing aborted
 * 1 stack processed to completion
 */
HIDDEN int
sh_stk_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)
{
    register struct stk_specific *sp =
	(struct stk_specific *)dp;
    int i;
    int ret_status = 0;
    char tmp[128];

    for (i = 0; i < 16 && sp->mfuncs[i] != NULL; i++) {
	if (rdebug&RDEBUG_SHADE) {
	    snprintf(tmp, 128, "before stacked \"%s\" shader", sp->mfuncs[i]->mf_name);

	    pr_shadework(tmp, swp);
	}

	/*
	 * Every shader takes the shadework structure as its
	 * input and updates it as the "output".
	 */
	if (sp && sp->mfuncs[i] && sp->mfuncs[i]->mf_render)
	    ret_status = sp->mfuncs[i]->mf_render(ap, pp, swp, sp->udata[i]);
	if (ret_status != 1)
	    return 0;

    }
    return 1;
}


/*
 * S T K _ P R I N T
 */
HIDDEN void
sh_stk_print(register struct region *rp, genptr_t dp)
{
    register struct stk_specific *sp =
	(struct stk_specific *)dp;
    int i;

    bu_log("~~~~starting stack print\n");

    for (i = 0; i < 16 && sp->mfuncs[i] != NULL; i++) {
	bu_log("~~~~stack entry %d:\n", i);
	if (sp && sp->mfuncs[i] && sp->mfuncs[i]->mf_print) {
	    sp->mfuncs[i]->mf_print(rp, sp->udata[i]);
	} else {
	    bu_log("WARNING: unexpected NULL print function encountered\n");
	}
    }

    bu_log("~~~~ending stack print\n");
}


/*
 * S T K _ F R E E
 */
HIDDEN void
sh_stk_free(genptr_t cp)
{
    register struct stk_specific *sp =
	(struct stk_specific *)cp;
    int i;

    for (i = 0; i < 16 && sp->mfuncs[i] != NULL; i++) {
	if (sp && sp->mfuncs[i] && sp->mfuncs[i]->mf_free) {
	    sp->mfuncs[i]->mf_free(sp->udata[i]);
	}
    }

    bu_free(cp, "stk_specific");
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
