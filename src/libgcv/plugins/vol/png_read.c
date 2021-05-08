/*                      P N G _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file png_read.c
 *
 * Brief description
 *
 */

#include "common.h"
#include "icv.h"
#include "bu.h"
#include "gcv/api.h"
#include "bu/mime.h"
#include "wdb.h"

typedef unsigned char uchar; // Get a short form of unsigned char
typedef struct vol_db        // Struct which stores the VOL's BW data in "vol_data" and the amount of data in "data"
{
    uchar *vol_data;
    int data;
} vol_db;

struct png_read_opts{
    icv_image_t **pngs;
    int coloured;
};


static void create_opts(struct bu_opt_desc **opts_desc, void **dest_options_data)
{
    struct png_read_opts *opts_data;

    bu_log("VOL_PLUGIN: entered create_opts()\n");

    BU_ALLOC(opts_data, struct png_read_opts);
    *dest_options_data = opts_data;
    *opts_desc = (struct bu_opt_desc *)bu_calloc(3, sizeof(struct bu_opt_desc), "options_desc");

    opts_data->coloured = 0;

    BU_OPT((*opts_desc)[0], "c", "colored", NULL, bu_opt_bool,
	   &opts_data->coloured, "Check if it is coloured");
    BU_OPT_NULL((*opts_desc)[1]);
}


static void free_opts(void *options_data)
{
    bu_log("VOL_PLUGIN: entered free_opts()\n");

    bu_free(options_data, "options_data");
}


HIDDEN void make_vol(struct gcv_context *context, int num_of_files, icv_image_t **images, vol_db data)
{
    /* Some variables which have to be initialized after the for loops */                     // BRLCAD database object
    //int num_of_files = 1;
    vect_t h = {1.0, 1.0, 5.0};                                          // Cell size variable of VOL
    mat_t l = {1.0, 0.0, 0.0, 0.0, 0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}; // matrix of VOL

    /* Make the VOL inside the database */
    mk_binunif(context->dbip->dbi_wdbp, "dbobj", data.vol_data, WDB_BINUNIF_UCHAR, data.data); // Make binary object in the database containing VOL data
    mk_vol(context->dbip->dbi_wdbp, "vol", RT_VOL_SRC_OBJ, "dbobj",                            // Make the VOL
           images[0]->width,                                                                   // in the database using
           images[0]->height,                                                                  // the data from
           num_of_files, 1, 255, h,                                                            // the binary object
           (matp_t)l);
}


HIDDEN uchar *cat(unsigned char *dest, const unsigned char *src, int l1, int l2)
{
    unsigned char *str = (uchar *)bu_malloc(l1 + l2, "vol_data");
    for (int i = 0; i < l1; i++)
    {
        str[i] = dest[i];
    }

    for (int i = 0; i < l2; i++)
    {
        str[l1 + i] = src[i];
    }
    return str;
}


static int png_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *options_data, const char *source_paths)
{
    size_t num_png = 1;
    struct bu_vls fmt_prefix = BU_VLS_INIT_ZERO;
    size_t argc = 0;
    const char **argv = (const char **)bu_malloc(sizeof(argv), "arguments");

    struct png_read_opts *opts = (struct png_read_opts *)options_data;
    const char **files = (const char **)bu_malloc(sizeof(files) * (argc + 1), "the filenames");
    files[0] = source_paths;
    for (size_t j=0; j < argc; j++){
        bu_path_component(&fmt_prefix, argv[j], BU_PATH_EXT);
        if (bu_file_mime(bu_vls_cstr(&fmt_prefix), BU_MIME_IMAGE) != -1){
        files[num_png] = argv[j];
        bu_log("%ld %ld %ld\n", num_png, j, argc);
        num_png++;
        }
    }

    vol_db *ret = (vol_db *)bu_malloc(sizeof(ret), "data for making the vol"); // The struct variable which will store the required information for making the VOL afterwards.
    int imgs_data[num_png]; // array of the amount of data per ICV images object.
    opts->pngs = (icv_image_t **)bu_malloc(sizeof(opts->pngs) * num_png, "png files");
    uchar *data[num_png];
    for (size_t i = 0; i < num_png; i++){
        opts->pngs[i] = icv_read(files[i], BU_MIME_IMAGE_PNG, 0, 0);
    }
    for (size_t i = 0; i < num_png; i++)
    {
        icv_rgb2gray(opts->pngs[i], (ICV_COLOR)0, 0, 0, 0);
    }

    /* Save the BW data to an array of uchar variables  */
    for (size_t i = 0; i < num_png; i++)
    {
        data[i] = icv_data2uchar(opts->pngs[i]);
    }
    //data[0] = icv_data2uchar(opts->pngs);

    /* Get the amount of BW data */
    for (size_t i = 0; i < num_png; i++)
    {
        imgs_data[i] = opts->pngs[i]->width * opts->pngs[i]->height;
    }

    ret->data = imgs_data[0]; // Amount of VOL data
    ret->vol_data = data[0];
    //ret->vol_data = icv_data2uchar(opts->pngs); // Concatenated BW data of the provided PNG files

    /* Concatenate the BW data of all the files */
    for (size_t i = 1; i < num_png; i++)
    {
        ret->vol_data = cat(ret->vol_data, data[i], ret->data, imgs_data[i]);
        ret->data += imgs_data[i];
    }
    make_vol(context, num_png, opts->pngs, *ret);
    bu_log("importing from PNG file '%s'\n", source_paths);

    mk_id(context->dbip->dbi_wdbp, "GCV plugin test");
    //bu_log("%ld\n", gcv_options->unknown_argc);
    //for(size_t i = 0; i< gcv_options->unknown_argc; i++){
      //  bu_log("%s\n", gcv_options->unknown_argv[i]);
    //}

    return 1;
}


HIDDEN int png_can_read(const char * data)
{
    bu_log("VOL_PLUGIN: entered png_can_read, data=%p\n", (void *)data);

    if (!data)
	return 0;
    return 1;
}


const struct gcv_filter gcv_conv_png_read = {
    "PNG Reader", GCV_FILTER_READ, BU_MIME_IMAGE_PNG, png_can_read,
    create_opts, free_opts, png_read
};


static const struct gcv_filter * const filters[] = {&gcv_conv_png_read, NULL};

const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info()
{
    return &gcv_plugin_info_s;
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
