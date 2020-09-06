#include "common.h"

#include "gcv/api.h"
#include "bu/mime.h"
#include "wdb.h"

struct png_read_opts{
  int coloured;
};

static void create_opts(struct bu_opt_desc **opts_desc,
                        void **dest_options_data)
{
  struct png_read_opts *opts_data;

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
  bu_free(options_data, "options_data");
}

static int png_read(struct gcv_context *context,
                    const struct gcv_opts *UNUSED(gcv_options),
                    const void *options_data, const char *source_path)
{
  const point_t center = {0.0, 0.0, 0.0};
  const fastf_t radius = 1.0;

  const struct png_read_opts *opts = (struct png_read_opts *)options_data;

  bu_log("importing from PNG file '%s'\n", source_path);
  bu_log("image is coloured: %s\n", opts->coloured ? "True" : "False");

  mk_id(context->dbip->dbi_wdbp, "GCV plugin test");

  mk_sph(context->dbip->dbi_wdbp, "test", center, radius);

  return 1;
}

HIDDEN int png_can_read(const char * data)
{
  if (!data) return 0;
  return 1;
}

const struct gcv_filter gcv_conv_png_read = {
  "PNG Reader", GCV_FILTER_READ, BU_MIME_MODEL_UNKNOWN, png_can_read,
  create_opts, free_opts, png_read
};

static const struct gcv_filter * const filters[] = {&gcv_conv_png_read, NULL};

const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info()
{
  return &gcv_plugin_info_s;
}
