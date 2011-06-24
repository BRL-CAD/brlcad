/*                      M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file liboptical/material.c
 *
 * Routines to coordinate the implementation of material properties
 *
 */

#include "common.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


static const char *mdefault = "default"; /* Name of default material */


/**
 * M L I B _ A D D _ S H A D E R
 *
 * Routine to add an array of mfuncs structures to the linked list of
 * material (shader) routines.
 */
void
mlib_add_shader(struct mfuncs **headp, struct mfuncs *mfp1)
{
    register struct mfuncs *mfp;

    RT_CK_MF(mfp1);
    for (mfp = mfp1; mfp && mfp->mf_name != (char *)0; mfp++) {
	RT_CK_MF(mfp);
	mfp->mf_forw = *headp;
	*headp = mfp;
    }
}


/**
 * T R Y _ L O A D
 *
 * Try to load a DSO from the specified path.  If we succeed in
 * opening the DSO, then retrieve the symbol "shader_mfuncs" and look
 * up the shader named "material" in the table.
 */
static struct mfuncs *
try_load(const char *path, const char *material, const char *shader_name)
{
    void *handle;
    struct mfuncs *shader_mfuncs;
    struct mfuncs *mfp;
    const char *dl_error_str;
    char sym[MAXPATHLEN];

    if (! (handle = bu_dlopen(path, BU_RTLD_NOW))) {
	if (R_DEBUG&RDEBUG_MATERIAL)
	    bu_log("bu_dlopen failed on \"%s\"\n", path);
	return (struct mfuncs *)NULL;
    } else if (R_DEBUG&RDEBUG_MATERIAL) {
	bu_log("%s open... ", path);
    }

    /* Find the {shader}_mfuncs symbol in the library */
    snprintf(sym, MAXPATHLEN, "%s_mfuncs", shader_name);
    shader_mfuncs = bu_dlsym(handle, sym);
    if ((dl_error_str=bu_dlerror()) == (char *)NULL) goto found;


    /* We didn't find a {shader}_mfuncs symbol, so
     * try the generic "shader_mfuncs" symbol.
     */
    shader_mfuncs = bu_dlsym(handle, "shader_mfuncs");
    if ((dl_error_str=bu_dlerror()) != (char *)NULL) {
	/* didn't find anything appropriate, give up */
	if (R_DEBUG&RDEBUG_MATERIAL) bu_log("%s has no %s table, %s\n", material, sym, dl_error_str);
	bu_dlclose(handle);
	return (struct mfuncs *)NULL;
    }

found:
    if (R_DEBUG&RDEBUG_MATERIAL)
	bu_log("%s_mfuncs table found\n", shader_name);

    /* make sure the shader we were looking for is in the mfuncs table */
    for (mfp = shader_mfuncs; mfp && mfp->mf_name != (char *)NULL; mfp++) {
	RT_CK_MF(mfp);

	if (BU_STR_EQUAL(mfp->mf_name, shader_name))
	    return shader_mfuncs; /* found ! */
    }

    if (R_DEBUG&RDEBUG_MATERIAL) bu_log("shader '%s' not found in library\n", shader_name);

    /* found the library, but not the shader */
    bu_dlclose(handle);
    return (struct mfuncs *)NULL;
}


struct mfuncs *
load_dynamic_shader(const char *material)
{
    struct mfuncs *shader_mfuncs = (struct mfuncs *)NULL;
    char libname[MAXPATHLEN];
    char libpath[MAXPATHLEN];
    char *cwd = (char *)NULL;
    int old_rdebug = R_DEBUG;
    char sh_name[128]; /* XXX constants are bogus */

    if (strlen(material) < sizeof(sh_name)) {
	bu_strlcpy(sh_name, material, sizeof(sh_name));
    } else {
	bu_log("shader name too long \"%s\" %zu > %d\n",
	       material, strlen(material), sizeof(sh_name));
	return (struct mfuncs *)NULL;
    }

    if (R_DEBUG&RDEBUG_MATERIAL)
	bu_log("load_dynamic_shader(\"%s\")\n", sh_name);

    cwd = getcwd((char *)NULL, (size_t)MAXPATHLEN);

    if (cwd) {
	/* Look in the current working directory for {sh_name}.so */
	snprintf(libname, sizeof(libname), "%s/%s.so", cwd, sh_name);
	if ((shader_mfuncs = try_load(libname, material, sh_name)))
	    goto done;


	/* Look in the current working directory for shaders.so */
	snprintf(libname, sizeof(libname), "%s/shaders.so", cwd);
	if ((shader_mfuncs = try_load(libname, material, sh_name)))
	    goto done;

    } else {
	bu_log("Cannot get current working directory\n\tSkipping local shader load\n");
    }

    /* Look in the location indicated by $LD_LIBRARY_PATH for
     * lib{sh_name}.so
     */
    snprintf(libname, sizeof(libname), "lib%s.so", sh_name);
    if ((shader_mfuncs = try_load(libname, material, sh_name)))
	goto done;

    /* Look in BRL-CAD install dir under lib dir for lib{sh_name}.so */
    snprintf(libpath, sizeof(libpath), "/lib/lib%s.so", sh_name);
    bu_strlcpy(libname, bu_brlcad_root(libpath, 1), sizeof(libname));
    if ((shader_mfuncs = try_load(libname, material, sh_name)))
	goto done;

done:
    /* clean up memory allocated */
    if (cwd) free(cwd);

    /* print appropriate log messages */
    if (shader_mfuncs)
	bu_log("loaded from %s\n", libname);
    else
	bu_log("WARNING: shader [%s] not found\n", sh_name);

    rdebug = old_rdebug;

    return shader_mfuncs;
}


/**
 * M L I B _ S E T U P
 *
 * Returns -
 * -1 failed
 * 0 indicates that this region should be dropped
 * 1 success
 */
int
mlib_setup(struct mfuncs **headp,
	   register struct region *rp,
	   struct rt_i *rtip)
{
    struct mfuncs *mfp_new = NULL;

    const struct mfuncs *mfp;
    int ret = -1;
    struct bu_vls params;
    struct bu_vls name;
    const char *material;
    size_t mlen;

    RT_CK_REGION(rp);
    RT_CK_RTI(rtip);

    if (rp->reg_mfuncs != (char *)0) {
	bu_log("mlib_setup:  region %s already setup\n", rp->reg_name);
	return -1;
    }
    bu_vls_init(&name);
    bu_vls_init(&params);
    material = rp->reg_mater.ma_shader;
    if (material == NULL || material[0] == '\0') {
	material = mdefault;
	mlen = strlen(mdefault);
    } else {
	char *endp;
	endp = strchr(material, ' ');
	if (endp) {
	    mlen = endp - material;
	    bu_vls_strcpy(&params, rp->reg_mater.ma_shader+mlen+1);
	} else {
	    mlen = strlen(material);
	}
    }
    bu_vls_strncpy(&name, material, mlen);

retry:
    for (mfp = *headp; mfp && mfp->mf_name != NULL; mfp = mfp->mf_forw) {
	if (material[0] != mfp->mf_name[0] || !BU_STR_EQUAL(bu_vls_addr(&name), mfp->mf_name))
	    continue;
	goto found;
    }

    /* If we get here, then the shader wasn't found in the list of
     * compiled-in (or previously loaded) shaders.  See if we can
     * dynamically load it.
     */

    bu_log("Shader (name: \"%V\" parameters: \"%V\")... ", &name, &params);

    mfp_new = load_dynamic_shader(bu_vls_addr(&name));
    if (mfp_new) {
	mlib_add_shader(headp, mfp_new);
	bu_log("Found a dynamic load shader, retrying\n");
	goto retry;
    }


    /* If we get here, then the shader was not found at all (either in
     * the compiled-in or dynamically loaded shader sets).  We set the
     * shader name to "default" (which should match an entry in the
     * table) and search again.
     */

    bu_log("WARNING Unknown shader settings on %s\nDefault (plastic) material used instead of '%V'.\n\n",
	   rp->reg_name, &name);

    if (material != mdefault) {
	material = mdefault;
	mlen = strlen(mdefault);
	bu_vls_trunc(&params, 0);
	bu_vls_strcpy(&name, mdefault);
	goto retry;
    }
    bu_vls_free(&params);
    bu_vls_free(&name);
    return -1;
found:
    rp->reg_mfuncs = (char *)mfp;
    rp->reg_udata = (char *)0;

    if (R_DEBUG&RDEBUG_MATERIAL)
	bu_log("mlib_setup(%s) shader=%s\n", rp->reg_name, mfp->mf_name);

    if (mfp && mfp->mf_setup)
	ret = mfp->mf_setup(rp, &params, &rp->reg_udata, mfp, rtip);
    if (ret < 0) {
	bu_log("ERROR mlib_setup(%s) failed. material='%s', parameters='%s'.\n",
	       rp->reg_name, bu_vls_addr(&name), bu_vls_addr(&params));
	if (material != mdefault) {
	    /* If not default material, change to default & retry */
	    bu_log("\tChanging %s material to default and retrying.\n", rp->reg_name);
	    material = mdefault;
	    bu_vls_trunc(&params, 0);
	    bu_vls_strcpy(&name, mdefault);
	    goto retry;
	}
	/* What to do if default setup fails? */
	bu_log("mlib_setup(%s) error recovery failed.\n", rp->reg_name);
    }
    bu_vls_free(&params);
    bu_vls_free(&name);
    return ret;		/* Good or bad, as mf_setup says */
}


/**
 * M L I B _ F R E E
 *
 * Routine to free material-property specific data
 */
void
mlib_free(register struct region *rp)
{
    register const struct mfuncs *mfp = (struct mfuncs *)rp->reg_mfuncs;

    if (mfp == MF_NULL) {
	bu_log("mlib_free(%s):  reg_mfuncs NULL\n", rp->reg_name);
	return;
    }
    if (mfp->mf_magic != MF_MAGIC) {
	bu_log("mlib_free(%s):  reg_mfuncs bad magic, %x != %x\n",
	       rp->reg_name,
	       mfp->mf_magic, MF_MAGIC);
	return;
    }

    if (mfp->mf_free)
	mfp->mf_free(rp->reg_udata);
    rp->reg_mfuncs = (char *)0;
    rp->reg_udata = (char *)0;
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
