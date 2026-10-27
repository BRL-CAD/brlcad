/**
 * Frontend-side Phase 5 small-feature suppression and CSG fact capture.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <stdio.h>

#include "frontend/csg_capture.h"
#include "frontend/model_capture.h"


static const double CREO_BRL_CSG_MIN_RADIUS = 1.0e-7;


enum creo_brl_csg_feature_class {
    CREO_BRL_CSG_FEATURE_HOLE = 1,
    CREO_BRL_CSG_FEATURE_ROUND = 2,
    CREO_BRL_CSG_FEATURE_CHAMFER = 3
};

struct creo_brl_csg_feature_inputs {
    int feature_class;
    int eliminate_small_features;
    int facets_only;
    int hole_supported;
    int has_diameter;
    int has_distances;
    double min_hole;
    double min_round;
    double min_chamfer;
    double diameter;
    double radius;
    double dist1;
    double dist2;
};


int
creo_brl_csg_hole_action(int eliminate_small_features,
                         int facets_only,
                         int hole_supported,
                         double diameter,
                         double minimum_diameter)
{
    if (!eliminate_small_features)
        return CREO_BRL_CSG_FEATURE_KEEP;
    if (diameter < minimum_diameter)
        return CREO_BRL_CSG_FEATURE_SUPPRESS;
    if (facets_only || !hole_supported)
        return CREO_BRL_CSG_FEATURE_KEEP;

    return CREO_BRL_CSG_FEATURE_SUBTRACT;
}


int
creo_brl_csg_feature_adds_material(ProFeattype feature_type)
{
    if (feature_type >= PRO_FEAT_UDF_THREAD)
        return 1;

    switch (feature_type) {
        case PRO_FEAT_FIRST_FEAT:
        case PRO_FEAT_SHAFT:
        case PRO_FEAT_PROTRUSION:
        case PRO_FEAT_NECK:
        case PRO_FEAT_FLANGE:
        case PRO_FEAT_RIB:
        case PRO_FEAT_EAR:
        case PRO_FEAT_DOME:
        case PRO_FEAT_LOC_PUSH:
        case PRO_FEAT_UDF:
        case PRO_FEAT_DRAFT:
        case PRO_FEAT_SHELL:
        case PRO_FEAT_DOME2:
        case PRO_FEAT_IMPORT:
        case PRO_FEAT_MERGE:
        case PRO_FEAT_MOLD:
        case PRO_FEAT_OFFSET:
        case PRO_FEAT_REPLACE_SURF:
        case PRO_FEAT_PIPE:
            return 1;
        default:
            return 0;
    }
}


int
creo_brl_csg_hole_is_eligible(const std::vector<int>& feature_sequence,
                               size_t hole_index)
{
    if (hole_index >= feature_sequence.size() ||
        feature_sequence[hole_index] != CREO_BRL_CSG_SEQUENCE_HOLE)
        return 0;

    for (size_t index = hole_index + 1u; index < feature_sequence.size(); ++index) {
        if (feature_sequence[index] == CREO_BRL_CSG_SEQUENCE_MATERIAL_ADDING)
            return 0;
    }

    return 1;
}


int
creo_brl_restore_features(const std::vector<int>& feature_ids,
                           creo_brl_feature_change_fn resume_feature,
                           void *context)
{
    int restored = 1;

    if (!resume_feature)
        return 0;
    for (size_t index = 0; index < feature_ids.size(); ++index) {
        if (!resume_feature(context, feature_ids[index]))
            restored = 0;
    }

    return restored;
}


int
creo_brl_suppress_features_transactionally(
    const std::vector<int>& feature_ids,
    creo_brl_feature_change_fn suppress_feature,
    creo_brl_feature_change_fn resume_feature,
    void *context,
    std::vector<int> *suppressed_features)
{
    if (!suppress_feature || !resume_feature || !suppressed_features)
        return CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED;

    suppressed_features->clear();
    if (feature_ids.empty())
        return CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED;
    suppressed_features->reserve(feature_ids.size());
    for (size_t index = 0; index < feature_ids.size(); ++index) {
        const int feature_id = feature_ids[index];

        if (std::find(suppressed_features->begin(), suppressed_features->end(), feature_id) !=
            suppressed_features->end())
            continue;
        if (!suppress_feature(context, feature_id)) {
            if (!creo_brl_restore_features(*suppressed_features, resume_feature, context))
                return CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED;
            suppressed_features->clear();
            return CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED;
        }
        suppressed_features->push_back(feature_id);
    }

    return suppressed_features->empty() ? CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED :
                                          CREO_BRL_FEATURE_SUPPRESSION_APPLIED;
}


static int
should_capture_hole_subtractions(const struct creo_brl_csg_feature_inputs *inputs)
{
    return inputs && inputs->feature_class == CREO_BRL_CSG_FEATURE_HOLE &&
           creo_brl_csg_hole_action(inputs->eliminate_small_features,
                                    inputs->facets_only, 1, inputs->diameter,
                                    inputs->min_hole) == CREO_BRL_CSG_FEATURE_SUBTRACT;
}


static int
decide_feature_action(const struct creo_brl_csg_feature_inputs *inputs)
{
    if (!inputs || !inputs->eliminate_small_features)
        return CREO_BRL_CSG_FEATURE_KEEP;

    switch (inputs->feature_class) {
        case CREO_BRL_CSG_FEATURE_HOLE:
            return creo_brl_csg_hole_action(inputs->eliminate_small_features,
                                            inputs->facets_only,
                                            inputs->hole_supported,
                                            inputs->diameter,
                                            inputs->min_hole);
        case CREO_BRL_CSG_FEATURE_ROUND:
            return (inputs->has_diameter && inputs->radius < inputs->min_round) ?
                       CREO_BRL_CSG_FEATURE_SUPPRESS :
                       CREO_BRL_CSG_FEATURE_KEEP;
        case CREO_BRL_CSG_FEATURE_CHAMFER:
            return (inputs->has_distances && inputs->dist1 < inputs->min_chamfer &&
                    inputs->dist2 < inputs->min_chamfer) ?
                       CREO_BRL_CSG_FEATURE_SUPPRESS :
                       CREO_BRL_CSG_FEATURE_KEEP;
    }

    return CREO_BRL_CSG_FEATURE_KEEP;
}


static void
unit_perpendicular_vector(vect_t perpendicular, const vect_t direction)
{
    vect_t reference = VINIT_ZERO;

    if (std::fabs(direction[0]) <= std::fabs(direction[1]) &&
        std::fabs(direction[0]) <= std::fabs(direction[2]))
        VSET(reference, 1.0, 0.0, 0.0);
    else if (std::fabs(direction[1]) <= std::fabs(direction[2]))
        VSET(reference, 0.0, 1.0, 0.0);
    else
        VSET(reference, 0.0, 0.0, 1.0);

    VCROSS(perpendicular, direction, reference);
    VUNITIZE(perpendicular);
}


struct csg_hole_candidate {
    size_t sequence_index;
    std::vector<int> feature_ids;
    std::vector<struct creo_brl_snapshot_tgc_subtraction> subtractions;
};


struct feature_capture_context {
    ProMdl model;
    const struct creo_brl_snapshot_settings *settings;
    double model_to_mm;
    std::vector<int> suppressed_features;
    std::vector<int> feature_sequence;
    std::vector<struct csg_hole_candidate> hole_candidates;
    std::vector<struct creo_brl_snapshot_tgc_subtraction> *tgc_subtractions;
    double radius;
    double diameter;
    double dist1;
    double dist2;
    int has_diameter;
    int has_distances;
};


struct hole_capture_info {
    ProFeature *feature;
    double radius;
    double diameter;
    int hole_type;
    int add_cbore;
    int add_csink;
    int hole_depth_type;
    double cb_depth;
    double cb_diam;
    double cs_diam;
    double cs_angle;
    double hole_diam;
    double hole_depth;
    double drill_angle;
    Pro3dPnt end1;
    Pro3dPnt end2;
};


static void
report_capture_warning(const char *message, int feature_id)
{
    std::fprintf(stderr, "creo-g: %s for feature ID #%d\n", message, feature_id);
}


static ProError
feature_dimension_filter(ProDimension *UNUSED(dim), ProAppData UNUSED(data))
{
    return PRO_TK_NO_ERROR;
}


static ProError
capture_feature_dimensions(ProDimension *dimension, ProError UNUSED(status), ProAppData app_data)
{
    ProDimensiontype dimension_type;
    struct feature_capture_context *context = (struct feature_capture_context *)app_data;
    double value = 0.0;

    if (!context)
        return PRO_TK_BAD_INPUTS;
    if (ProDimensionTypeGet(dimension, &dimension_type) != PRO_TK_NO_ERROR)
        return PRO_TK_GENERAL_ERROR;

    switch (dimension_type) {
        case PRODIMTYPE_RADIUS:
            if (ProDimensionValueGet(dimension, &context->radius) == PRO_TK_NO_ERROR) {
                context->diameter = 2.0 * context->radius;
                context->has_diameter = 1;
            }
            break;
        case PRODIMTYPE_DIAMETER:
            if (ProDimensionValueGet(dimension, &context->diameter) == PRO_TK_NO_ERROR) {
                context->radius = context->diameter / 2.0;
                context->has_diameter = 1;
            }
            break;
        case PRODIMTYPE_LINEAR:
            if (ProDimensionValueGet(dimension, &value) == PRO_TK_NO_ERROR) {
                if (context->has_distances)
                    context->dist2 = value;
                else {
                    context->dist1 = value;
                    context->has_distances = 1;
                }
            }
            break;
    }

    return PRO_TK_NO_ERROR;
}


static void
append_dependent_features(ProMdl model, ProFeature *feature, std::vector<int> *feature_ids)
{
    ProFeature child_feature;
    ProFeatStatus child_status;
    int *child_ids = NULL;
    int child_count = 0;

    if (!model || !feature || !feature_ids)
        return;

    (void)ProFeatureChildrenGet(feature, &child_ids, &child_count);
    for (int index = 0; index < child_count; ++index) {
        if (ProFeatureInit(ProMdlToSolid(model), child_ids[index], &child_feature) != PRO_TK_NO_ERROR ||
            ProFeatureStatusGet(&child_feature, &child_status) != PRO_TK_NO_ERROR ||
            child_status != PRO_FEAT_ACTIVE)
            continue;
        feature_ids->push_back(child_ids[index]);
    }

    (void)ProArrayFree((ProArray *)&child_ids);
}


struct toolkit_feature_change_context {
    ProMdl model;
    int *feature_id;
    ProFeatureDeleteOptions *delete_options;
    ProFeatureResumeOptions *resume_options;
};


static int
suppress_toolkit_feature(void *data, int feature_id)
{
    struct toolkit_feature_change_context *context =
        static_cast<struct toolkit_feature_change_context *>(data);

    if (!context || !context->model || !context->feature_id || !context->delete_options)
        return 0;
    context->feature_id[0] = feature_id;
    return ProFeatureWithoptionsSuppress(ProMdlToSolid(context->model),
                                         context->feature_id,
                                         context->delete_options,
                                         PRO_REGEN_NO_FLAGS) == PRO_TK_NO_ERROR;
}


static int
resume_toolkit_feature(void *data, int feature_id)
{
    struct toolkit_feature_change_context *context =
        static_cast<struct toolkit_feature_change_context *>(data);
    ProFeature feature;
    ProFeatStatus status;

    if (!context || !context->model || !context->feature_id || !context->resume_options ||
        ProFeatureInit(ProMdlToSolid(context->model), feature_id, &feature) != PRO_TK_NO_ERROR ||
        ProFeatureStatusGet(&feature, &status) != PRO_TK_NO_ERROR)
        return 0;
    if (status == PRO_FEAT_ACTIVE)
        return 1;
    if (status != PRO_FEAT_SUPPRESSED)
        return 0;

    context->feature_id[0] = feature_id;
    if (ProFeatureWithoptionsResume(ProMdlToSolid(context->model),
                                    context->feature_id,
                                    context->resume_options,
                                    PRO_REGEN_NO_FLAGS) != PRO_TK_NO_ERROR)
        return 0;

    return ProFeatureStatusGet(&feature, &status) == PRO_TK_NO_ERROR &&
           status == PRO_FEAT_ACTIVE;
}


static int
allocate_feature_change_arrays(struct toolkit_feature_change_context *context,
                               int include_delete_options)
{
    if (!context ||
        ProArrayAlloc(1, sizeof(int), 1, (ProArray *)&context->feature_id) !=
            PRO_TK_NO_ERROR ||
        ProArrayAlloc(1, sizeof(ProFeatureResumeOptions), 1,
                      (ProArray *)&context->resume_options) != PRO_TK_NO_ERROR ||
        (include_delete_options &&
         ProArrayAlloc(1, sizeof(ProFeatureDeleteOptions), 1,
                       (ProArray *)&context->delete_options) != PRO_TK_NO_ERROR))
        return 0;

    context->resume_options[0] =
        (ProFeatureResumeOptions)PRO_FEAT_RESUME_INCLUDE_PARENTS;
    if (include_delete_options)
        context->delete_options[0] =
            (ProFeatureDeleteOptions)PRO_FEAT_DELETE_NO_OPTS;
    return 1;
}


static void
free_feature_change_arrays(struct toolkit_feature_change_context *context)
{
    if (!context)
        return;
    if (context->delete_options)
        (void)ProArrayFree((ProArray *)&context->delete_options);
    if (context->resume_options)
        (void)ProArrayFree((ProArray *)&context->resume_options);
    if (context->feature_id)
        (void)ProArrayFree((ProArray *)&context->feature_id);
}


static int
suppress_feature_ids(ProMdl model,
                     const std::vector<int>& feature_ids,
                     std::vector<int> *suppressed_features)
{
    struct toolkit_feature_change_context context = {};
    int result = CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED;

    if (!model || !suppressed_features)
        return result;
    context.model = model;
    if (allocate_feature_change_arrays(&context, 1)) {
        result = creo_brl_suppress_features_transactionally(
            feature_ids, suppress_toolkit_feature, resume_toolkit_feature,
            &context, suppressed_features);
    }
    free_feature_change_arrays(&context);
    return result;
}


int
creo_brl_resume_suppressed_features(ProMdl model,
                                    const std::vector<int>& feature_ids)
{
    struct toolkit_feature_change_context context = {};
    int result = 0;

    if (!model)
        return 0;
    if (feature_ids.empty())
        return 1;

    context.model = model;
    if (allocate_feature_change_arrays(&context, 0))
        result = creo_brl_restore_features(feature_ids, resume_toolkit_feature, &context);
    free_feature_change_arrays(&context);
    return result;
}


static ProError
hole_element_filter(ProElement UNUSED(elem_tree), ProElement UNUSED(elem), ProElempath UNUSED(elem_path), ProAppData UNUSED(data))
{
    return PRO_TK_NO_ERROR;
}


static ProError
hole_element_visit(ProElement UNUSED(elem_tree), ProElement element, ProElempath UNUSED(elem_path), ProAppData data)
{
    ProElemId element_id;
    ProValueDataType value_type;
    struct hole_capture_info *hole_info = (struct hole_capture_info *)data;

    if (!hole_info || ProElementValuetypeGet(element, &value_type) != PRO_TK_NO_ERROR)
        return PRO_TK_GENERAL_ERROR;

    switch (value_type) {
        case PRO_VALUE_TYPE_INT: {
            int value = 0;

            if (ProElementIntegerGet(element, NULL, &value) != PRO_TK_NO_ERROR ||
                ProElementIdGet(element, &element_id) != PRO_TK_NO_ERROR)
                return PRO_TK_GENERAL_ERROR;
            switch (element_id) {
                case PRO_E_HLE_ADD_CBORE:
                    hole_info->add_cbore = value;
                    break;
                case PRO_E_HLE_ADD_CSINK:
                    hole_info->add_csink = value;
                    break;
                case PRO_E_HLE_DEPTH:
                    hole_info->hole_depth_type = value;
                    break;
                case PRO_E_HLE_TYPE_NEW:
                    hole_info->hole_type = value;
                    break;
            }
            break;
        }
        case PRO_VALUE_TYPE_DOUBLE: {
            double value = 0.0;

            if (ProElementDoubleGet(element, NULL, &value) != PRO_TK_NO_ERROR ||
                ProElementIdGet(element, &element_id) != PRO_TK_NO_ERROR)
                return PRO_TK_GENERAL_ERROR;
            switch (element_id) {
                case PRO_E_DIAMETER:
                    hole_info->hole_diam = value;
                    break;
                case PRO_E_HLE_CBOREDEPTH:
                    hole_info->cb_depth = value;
                    break;
                case PRO_E_HLE_CBOREDIAM:
                    hole_info->cb_diam = value;
                    break;
                case PRO_E_HLE_CSINKANGLE:
                    hole_info->cs_angle = value;
                    break;
                case PRO_E_HLE_CSINKDIAM:
                    hole_info->cs_diam = value;
                    break;
                case PRO_E_HLE_DRILLANGLE:
                    hole_info->drill_angle = value;
                    break;
                case PRO_E_HLE_DRILLDEPTH:
                    hole_info->hole_depth = value;
                    break;
                case PRO_E_HLE_HOLEDIAM:
                    hole_info->hole_diam = value;
                    break;
            }
            break;
        }
    }

    return PRO_TK_NO_ERROR;
}


static ProError
hole_axis_filter(ProDimension *UNUSED(dim), ProAppData UNUSED(data))
{
    return PRO_TK_NO_ERROR;
}


static ProError
hole_axis_visit(ProGeomitem *item, ProError UNUSED(status), ProAppData data)
{
    ProGeomitemdata *geometry = NULL;
    ProCurvedata *curve = NULL;
    struct hole_capture_info *hole_info = (struct hole_capture_info *)data;

    if (!hole_info || ProGeomitemdataGet(item, &geometry) != PRO_TK_NO_ERROR)
        return PRO_TK_GENERAL_ERROR;

    curve = PRO_CURVE_DATA(geometry);
    return ProLinedataGet(curve, hole_info->end1, hole_info->end2);
}


static int
append_tgc_subtraction(std::vector<struct creo_brl_snapshot_tgc_subtraction> *subtractions,
                       const point_t base,
                       const vect_t height,
                       const vect_t a,
                       const vect_t b,
                       const vect_t c,
                       const vect_t d)
{
    struct creo_brl_snapshot_tgc_subtraction subtraction = {};

    if (!subtractions)
        return 0;

    VMOVE(subtraction.base.coordinates, base);
    VMOVE(subtraction.height.coordinates, height);
    VMOVE(subtraction.a.coordinates, a);
    VMOVE(subtraction.b.coordinates, b);
    VMOVE(subtraction.c.coordinates, c);
    VMOVE(subtraction.d.coordinates, d);
    subtractions->push_back(subtraction);
    return 1;
}


static int
capture_hole_subtractions(const struct feature_capture_context *context,
                          ProFeature *feature,
                          std::vector<struct creo_brl_snapshot_tgc_subtraction> *subtractions)
{
    ProElement element_tree = NULL;
    ProElempath element_path = NULL;
    struct hole_capture_info hole_info = {};
    const double scale = context ? context->model_to_mm : 0.0;
    const double min_hole = (context && scale > 0.0) ? context->settings->min_hole / scale : 0.0;

    if (!context || !context->settings || !feature || !subtractions || scale <= 0.0)
        return 0;

    hole_info.feature = feature;
    hole_info.radius = context->radius;
    hole_info.diameter = context->diameter;
    if (ProFeatureElemtreeExtract(feature, NULL, PRO_FEAT_EXTRACT_NO_OPTS, &element_tree) == PRO_TK_NO_ERROR) {
        if (ProElemtreeElementVisit(element_tree, element_path, hole_element_filter, hole_element_visit,
                                    (ProAppData)&hole_info) != PRO_TK_NO_ERROR) {
            (void)ProElementFree(&element_tree);
            report_capture_warning("failed to inspect hole feature", feature->id);
            return 0;
        }
        (void)ProElementFree(&element_tree);
    }
    if (ProFeatureGeomitemVisit(feature, PRO_AXIS, hole_axis_visit, hole_axis_filter,
                                (ProAppData)&hole_info) != PRO_TK_NO_ERROR) {
        report_capture_warning("failed to capture hole axis", feature->id);
        return 0;
    }

    if (hole_info.hole_type == PRO_HLE_NEW_TYPE_STRAIGHT) {
        point_t base;
        vect_t height;
        vect_t a;
        vect_t b;

        if (hole_info.diameter < min_hole)
            return 0;
        VSUB2(height, hole_info.end1, hole_info.end2);
        unit_perpendicular_vector(a, height);
        VCROSS(b, a, height);
        VUNITIZE(b);
        VSCALE(base, hole_info.end2, scale);
        VSCALE(a, a, hole_info.radius * scale);
        VSCALE(b, b, hole_info.radius * scale);
        VSCALE(height, height, scale);
        return append_tgc_subtraction(subtractions, base, height, a, b, a, b);
    }
    if (hole_info.hole_type == PRO_HLE_NEW_TYPE_STANDARD) {
        point_t start;
        vect_t direction;
        double accumulated_depth = 0.0;
        const double hole_radius = hole_info.hole_diam / 2.0;

        if (hole_info.hole_diam < min_hole)
            return 0;
        VSUB2(direction, hole_info.end1, hole_info.end2);
        VUNITIZE(direction);
        VSCALE(start, hole_info.end2, scale);

        if (hole_info.add_cbore == PRO_HLE_ADD_CBORE) {
            vect_t a;
            vect_t b;
            vect_t height;
            const double cbore_radius = hole_info.cb_diam * scale / 2.0;

            unit_perpendicular_vector(a, direction);
            VCROSS(b, a, direction);
            VUNITIZE(b);
            VSCALE(a, a, cbore_radius);
            VSCALE(b, b, cbore_radius);
            VSCALE(height, direction, hole_info.cb_depth * scale);
            if (!append_tgc_subtraction(subtractions, start, height, a, b, a, b))
                return 0;
            VADD2(start, start, height);
            accumulated_depth += hole_info.cb_depth;
        }
        if (hole_info.add_csink == PRO_HLE_ADD_CSINK) {
            vect_t a;
            vect_t b;
            vect_t c;
            vect_t d;
            vect_t height;
            const double csink_radius = hole_info.cs_diam / 2.0;
            const double csink_depth = (hole_info.cs_diam - hole_info.hole_diam) /
                                       (2.0 * tan(hole_info.cs_angle * M_PI / 360.0));

            unit_perpendicular_vector(a, direction);
            VCROSS(b, a, direction);
            VUNITIZE(b);
            VMOVE(c, a);
            VMOVE(d, b);
            VSCALE(height, direction, csink_depth * scale);
            VSCALE(a, a, csink_radius * scale);
            VSCALE(b, b, csink_radius * scale);
            VSCALE(c, c, hole_info.hole_diam * scale / 2.0);
            VSCALE(d, d, hole_info.hole_diam * scale / 2.0);
            if (!append_tgc_subtraction(subtractions, start, height, a, b, c, d))
                return 0;
            VADD2(start, start, height);
            accumulated_depth += csink_depth;
        }
        {
            vect_t a;
            vect_t b;
            vect_t c;
            vect_t d;
            vect_t height;

            unit_perpendicular_vector(a, direction);
            VCROSS(b, a, direction);
            VUNITIZE(b);
            VMOVE(c, a);
            VMOVE(d, b);
            VSCALE(a, a, hole_radius * scale);
            VSCALE(b, b, hole_radius * scale);
            VSCALE(c, c, hole_radius * scale);
            VSCALE(d, d, hole_radius * scale);
            VSCALE(height, direction, (hole_info.hole_depth - accumulated_depth) * scale);
            if (!append_tgc_subtraction(subtractions, start, height, a, b, c, d))
                return 0;
            VADD2(start, start, height);
        }
        if (hole_info.hole_depth_type == PRO_HLE_STD_VAR_DEPTH) {
            vect_t a;
            vect_t b;
            vect_t c;
            vect_t d;
            vect_t height;
            const double tip_depth = (hole_radius - CREO_BRL_CSG_MIN_RADIUS) /
                                     tan(hole_info.drill_angle * M_PI / 360.0);

            unit_perpendicular_vector(a, direction);
            VCROSS(b, a, direction);
            VUNITIZE(b);
            VMOVE(c, a);
            VMOVE(d, b);
            VSCALE(height, direction, tip_depth * scale);
            VSCALE(a, a, hole_radius * scale);
            VSCALE(b, b, hole_radius * scale);
            VSCALE(c, c, CREO_BRL_CSG_MIN_RADIUS * scale);
            VSCALE(d, d, CREO_BRL_CSG_MIN_RADIUS * scale);
            if (!append_tgc_subtraction(subtractions, start, height, a, b, c, d))
                return 0;
        }
        return 1;
    }

    report_capture_warning("unsupported hole type fell back to tessellation", feature->id);
    return 0;
}


static ProError
small_feature_filter(ProFeature *feature, ProAppData UNUSED(data))
{
    ProFeatStatus feature_status;
    ProError error = PRO_TK_GENERAL_ERROR;

    if (!feature)
        return PRO_TK_BAD_INPUTS;
    error = ProFeatureStatusGet(feature, &feature_status);
    if (error != PRO_TK_NO_ERROR)
        return error;

    return feature_status == PRO_FEAT_ACTIVE ? PRO_TK_NO_ERROR : PRO_TK_CONTINUE;
}


static void
append_unique_feature_ids(std::vector<int> *destination,
                          const std::vector<int>& feature_ids)
{
    if (!destination)
        return;

    for (size_t index = 0; index < feature_ids.size(); ++index) {
        if (std::find(destination->begin(), destination->end(), feature_ids[index]) ==
            destination->end())
            destination->push_back(feature_ids[index]);
    }
}


static ProError
capture_small_feature(ProFeature *feature, ProError UNUSED(status), ProAppData app_data)
{
    struct feature_capture_context *context = (struct feature_capture_context *)app_data;
    ProFeattype feature_type;
    struct creo_brl_csg_feature_inputs inputs = {};
    std::vector<struct creo_brl_snapshot_tgc_subtraction> hole_subtractions;
    size_t sequence_index = 0u;
    const double scale = context ? context->model_to_mm : 0.0;

    if (!context || !context->settings || scale <= 0.0)
        return PRO_TK_BAD_INPUTS;
    if (ProFeatureTypeGet(feature, &feature_type) != PRO_TK_NO_ERROR)
        return PRO_TK_GENERAL_ERROR;

    sequence_index = context->feature_sequence.size();
    if (feature_type == PRO_FEAT_HOLE)
        context->feature_sequence.push_back(CREO_BRL_CSG_SEQUENCE_HOLE);
    else if (creo_brl_csg_feature_adds_material(feature_type)) {
        context->feature_sequence.push_back(CREO_BRL_CSG_SEQUENCE_MATERIAL_ADDING);
        return PRO_TK_NO_ERROR;
    } else {
        context->feature_sequence.push_back(CREO_BRL_CSG_SEQUENCE_OTHER);
    }

    if (feature_type != PRO_FEAT_HOLE && feature_type != PRO_FEAT_ROUND &&
        feature_type != PRO_FEAT_CHAMFER)
        return PRO_TK_NO_ERROR;

    context->radius = 0.0;
    context->diameter = 0.0;
    context->dist1 = 0.0;
    context->dist2 = 0.0;
    context->has_diameter = 0;
    context->has_distances = 0;
    if (ProFeatureDimensionVisit(feature, capture_feature_dimensions, feature_dimension_filter,
                                 (ProAppData)context) != PRO_TK_NO_ERROR)
        return PRO_TK_GENERAL_ERROR;

    inputs.eliminate_small_features = 1;
    inputs.facets_only = (context->settings->flags & CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY) != 0;
    inputs.has_diameter = context->has_diameter;
    inputs.has_distances = context->has_distances;
    inputs.min_hole = context->settings->min_hole / scale;
    inputs.min_round = context->settings->min_round / scale;
    inputs.min_chamfer = context->settings->min_chamfer / scale;
    inputs.diameter = context->diameter;
    inputs.radius = context->radius;
    inputs.dist1 = context->dist1;
    inputs.dist2 = context->dist2;

    switch (feature_type) {
        case PRO_FEAT_HOLE:
            inputs.feature_class = CREO_BRL_CSG_FEATURE_HOLE;
            if (should_capture_hole_subtractions(&inputs))
                inputs.hole_supported = capture_hole_subtractions(context, feature, &hole_subtractions);
            break;
        case PRO_FEAT_ROUND:
            inputs.feature_class = CREO_BRL_CSG_FEATURE_ROUND;
            break;
        case PRO_FEAT_CHAMFER:
            inputs.feature_class = CREO_BRL_CSG_FEATURE_CHAMFER;
            break;
        default:
            return PRO_TK_NO_ERROR;
    }

    switch (decide_feature_action(&inputs)) {
        case CREO_BRL_CSG_FEATURE_SUPPRESS: {
            std::vector<int> feature_ids;

            append_dependent_features(context->model, feature, &feature_ids);
            feature_ids.push_back(feature->id);
            append_unique_feature_ids(&context->suppressed_features, feature_ids);
            break;
        }
        case CREO_BRL_CSG_FEATURE_SUBTRACT: {
            struct csg_hole_candidate candidate = {};

            candidate.sequence_index = sequence_index;
            append_dependent_features(context->model, feature, &candidate.feature_ids);
            candidate.feature_ids.push_back(feature->id);
            candidate.subtractions.swap(hole_subtractions);
            context->hole_candidates.push_back(candidate);
            break;
        }
    }

    return PRO_TK_NO_ERROR;
}


static void
commit_eligible_hole_candidates(struct feature_capture_context *context)
{
    if (!context || !context->tgc_subtractions)
        return;

    for (size_t index = 0; index < context->hole_candidates.size(); ++index) {
        const struct csg_hole_candidate& candidate = context->hole_candidates[index];

        if (!creo_brl_csg_hole_is_eligible(context->feature_sequence,
                                            candidate.sequence_index))
            continue;
        append_unique_feature_ids(&context->suppressed_features,
                                  candidate.feature_ids);
        context->tgc_subtractions->insert(context->tgc_subtractions->end(),
                                          candidate.subtractions.begin(),
                                          candidate.subtractions.end());
    }
}


int
creo_brl_capture_feature_suppression(ProMdl model,
                                     double scale,
                                     const struct creo_brl_snapshot_settings *settings,
                                     struct snapshot_part_capture *capture,
                                     std::vector<int> *suppressed_features)
{
    struct feature_capture_context context = {};

    if (!model || !settings || !capture || !suppressed_features)
        return CREO_BRL_FEATURE_SUPPRESSION_CAPTURE_FAILED;

    suppressed_features->clear();
    if ((settings->flags & CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES) == 0)
        return CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED;

    context.model = model;
    context.settings = settings;
    context.model_to_mm = scale;
    context.tgc_subtractions = &capture->tgc_subtractions;
    if (ProSolidFeatVisit(ProMdlToSolid(model), capture_small_feature,
                          (ProFeatureFilterAction)small_feature_filter,
                          (ProAppData)&context) != PRO_TK_NO_ERROR) {
        capture->tgc_subtractions.clear();
        report_capture_warning("failed to inspect small features", 0);
        return CREO_BRL_FEATURE_SUPPRESSION_CAPTURE_FAILED;
    }

    commit_eligible_hole_candidates(&context);
    *suppressed_features = context.suppressed_features;
    if (suppressed_features->empty())
        return CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED;
    const int suppression_result = suppress_feature_ids(model, context.suppressed_features,
                                                        suppressed_features);
    if (suppression_result == CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED) {
        capture->tgc_subtractions.clear();
        report_capture_warning("failed to restore a partial feature suppression", 0);
        return CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED;
    }
    if (suppression_result == CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED) {
        capture->tgc_subtractions.clear();
        report_capture_warning("failed to suppress captured features; falling back to unsuppressed tessellation", 0);
        return CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED;
    }

    return CREO_BRL_FEATURE_SUPPRESSION_APPLIED;
}
