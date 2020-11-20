/*                      M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2020 United States Government as represented by
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

#ifdef HAVE_DIRECT_H
#  include <direct.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "bio.h"
#include "vmath.h"
#include "bu/app.h"
#include "bu/dylib.h"
#include "raytrace.h"
#include "optical.h"


static const char *mdefault = "default"; /* Name of default material */


/**
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
 * Try to load a DSO from the specified path.  If we succeed in
 * opening the DSO, then retrieve the symbol "shader_mfuncs" and look
 * up the shader named "material" in the table.
 */
static struct mfuncs *
try_load(const char *path, const char *material)
{
    char sym[MAXPATHLEN];
    const char *dl_error_str;
    struct mfuncs *mfp;
    struct mfuncs *shader_mfuncs;
    void *handle;

    if (! (handle = bu_dlopen(path, BU_RTLD_NOW))) {
	if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
	    bu_log("bu_dlopen failed on \"%s\"\n", path);
	return (struct mfuncs *)NULL;
    } else if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL) {
	bu_log("%s open... ", path);
    }

    /* Find the {shader}_mfuncs symbol in the library */
    snprintf(sym, MAXPATHLEN, "%s_mfuncs", material);
    shader_mfuncs = (struct mfuncs *)bu_dlsym((struct mfuncs *)handle, sym);

    dl_error_str=bu_dlerror();
    if (dl_error_str == (char *)NULL) {

	/* We didn't find a {shader}_mfuncs symbol, so try the generic
	 * "shader_mfuncs" symbol.
	 */
	shader_mfuncs = (struct mfuncs *)bu_dlsym((struct mfuncs *)handle, "shader_mfuncs");
	if ((dl_error_str=bu_dlerror()) != (char *)NULL) {
	    /* didn't find anything appropriate, give up */
	    if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
		bu_log("%s has no %s table, %s\n", material, sym, dl_error_str);
	    bu_dlclose(handle);
	    return (struct mfuncs *)NULL;
	}
    }

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
	bu_log("%s_mfuncs table found\n", material);

    /* make sure the shader we were looking for is in the mfuncs table */
    for (mfp = shader_mfuncs; mfp && mfp->mf_name != (char *)NULL; mfp++) {
	RT_CK_MF(mfp);

	if (BU_STR_EQUAL(mfp->mf_name, material)) {
	    bu_dlclose(handle);
	    return shader_mfuncs; /* found it! */
	}
    }

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
	bu_log("shader '%s' not found in library\n", material);

    /* found the library, but not the shader */
    bu_dlclose(handle);
    return (struct mfuncs *)NULL;
}


struct mfuncs *
load_dynamic_shader(const char *material)
{
    char libpath[MAXPATHLEN]; /* path to shader lib */
    char mat_fname[MAXPATHLEN]; /* {material}.{EXT}, no path */
    char mat_libfname[MAXPATHLEN]; /* lib{material}.{EXT}, no path */
    struct mfuncs *shader_mfuncs = (struct mfuncs *)NULL;

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
	bu_log("load_dynamic_shader(\"%s\")\n", material);

    if (sizeof(mat_fname) <= strlen(material)) {
	bu_log("shader name too long \"%s\" %zu > %lu\n",
	       material, strlen(material), sizeof(mat_fname));
	return (struct mfuncs *)NULL;
    }

    /* precalculate the potential file names, with+without lib prefix */
    bu_dir(mat_fname, sizeof(mat_fname), material, BU_DIR_LIBEXT, NULL);
    bu_strlcat(mat_libfname, "lib", sizeof(mat_libfname));
    bu_strlcat(mat_libfname, mat_fname, sizeof(mat_libfname));

    /* Look in current working directory for {material}.so */
    bu_dir(libpath, sizeof(libpath), BU_DIR_CURR, mat_fname, NULL);
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

    /* Look in current working directory for lib{material}.so */
    bu_dir(libpath, sizeof(libpath), BU_DIR_CURR, mat_libfname, NULL);
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

    /* Don't specify a path in order to look in system locations
     * indicated by ld (e.g., $LD_LIBRARY_PATH) for {material}.so
     */
    bu_strlcpy(libpath, mat_fname, sizeof(libpath));
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

    /* Don't specify a path in order to look in the system locations
     * indicated by ld (e.g., $LD_LIBRARY_PATH) for lib{material}.so
     */
    bu_strlcpy(libpath, mat_libfname, sizeof(libpath));
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

    /* Look in installation library path for {material}.so */
    bu_dir(libpath, sizeof(libpath), BU_DIR_LIB, mat_fname, NULL);
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

    /* Look in installation library path for lib{material}.so */
    bu_dir(libpath, sizeof(libpath), BU_DIR_LIB, mat_libfname, NULL);
    shader_mfuncs = try_load(libpath, material);
    if (shader_mfuncs)
	goto done;

done:
    /* print appropriate log messages */
    if (shader_mfuncs)
	bu_log("loaded shader [%s] from %s\n", material, libpath);
    else
	bu_log("WARNING: shader [%s] not found\n", material);

    return shader_mfuncs;
}


/**
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
    struct bu_vls params = BU_VLS_INIT_ZERO;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    const char *material;
    size_t mlen;

    RT_CK_REGION(rp);
    RT_CK_RTI(rtip);

    if (rp->reg_mfuncs != (char *)0) {
	bu_log("mlib_setup:  region %s already setup\n", rp->reg_name);
	return -1;
    }

    material = rp->reg_mater.ma_shader;
    if (material == NULL || material[0] == '\0') {
	material = mdefault;
	mlen = strlen(mdefault);
    } else {
	const char *endp;
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

    bu_log("Shader (name: \"%s\" parameters: \"%s\")... ", bu_vls_addr(&name), bu_vls_addr(&params));

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

    bu_log("WARNING Unknown shader settings on %s\nDefault (plastic) material used instead of '%s'.\n\n",
	   rp->reg_name, bu_vls_addr(&name));

    if (material != mdefault) {
	material = mdefault;
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

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_MATERIAL)
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
