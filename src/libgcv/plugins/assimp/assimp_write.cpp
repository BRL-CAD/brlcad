#include "common.h"

#include <string>

#include "gcv.h"

#include <assimp/scene.h>

struct assimp_write_options
{
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct assimp_write_options *assimp_write_options;

    const char *output_file;	        /* output filename */

    struct db_i *dbip;
    FILE *fp;			        /* Output file pointer */
    int bfd;		    	        /* Output binary file descriptor */

    struct model *the_model;
    std::string file_name;	        /* file name built from region name */

    int regions_tried;
    int regions_converted;
    int regions_written;
    size_t tot_polygons;
};
#define CONVERSION_STATE_NULL { NULL, NULL, NULL, NULL, NULL, 0, NULL, "", 0, 0, 0, 0 }

HIDDEN void
nmg_to_assimp(struct nmgregion *UNUSED(r), const struct db_full_path *UNUSED(pathp), int UNUSED(region_id), int UNUSED(material_id), float UNUSED(color[3]), void *UNUSED(client_data))
{

}

HIDDEN int
assimp_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    struct conversion_state state = CONVERSION_STATE_NULL;
    struct gcv_region_end_data gcvwriter;

    gcvwriter.write_region = nmg_to_assimp;
    gcvwriter.client_data = &state;

    state.gcv_options = gcv_options;
    state.assimp_write_options = (struct assimp_write_options*)options_data;
    state.output_file = dest_path;
    state.dbip = context->dbip;

    bu_log("Assimp writer\n");

    return 1;
}

/* filter setup */
extern "C"
{
    extern const struct gcv_filter gcv_conv_assimp_write = {
        "Assimp Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_ASSIMP, NULL,
        NULL, NULL, assimp_write
    };
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
