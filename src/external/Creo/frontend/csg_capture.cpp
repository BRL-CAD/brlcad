/**
 * Frontend-side Phase 5 small-feature suppression and CSG fact capture.
 */

#include "common.h"

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

enum creo_brl_csg_feature_action {
    CREO_BRL_CSG_FEATURE_KEEP = 0,
    CREO_BRL_CSG_FEATURE_SUPPRESS = 1,
    CREO_BRL_CSG_FEATURE_SUBTRACT = 2
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


static int
should_capture_hole_subtractions(const struct creo_brl_csg_feature_inputs *inputs)
{
    return inputs && inputs->feature_class == CREO_BRL_CSG_FEATURE_HOLE &&
           inputs->eliminate_small_features && !inputs->facets_only;
}


static int
decide_feature_action(const struct creo_brl_csg_feature_inputs *inputs)
{
    if (!inputs || !inputs->eliminate_small_features)
        return CREO_BRL_CSG_FEATURE_KEEP;

    switch (inputs->feature_class) {
        case CREO_BRL_CSG_FEATURE_HOLE:
            if (inputs->diameter < inputs->min_hole)
                return CREO_BRL_CSG_FEATURE_SUPPRESS;
            if (inputs->facets_only || !inputs->hole_supported)
                return CREO_BRL_CSG_FEATURE_KEEP;
            return CREO_BRL_CSG_FEATURE_SUBTRACT;
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


struct feature_capture_context {
    ProMdl model;
    const struct creo_brl_snapshot_settings *settings;
    double model_to_mm;
    std::vector<int> suppressed_features;
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


static int
suppress_feature_ids(ProMdl model, const std::vector<int>& feature_ids)
{
    ProFeatureDeleteOptions *delete_options = NULL;
    int *feature_id = NULL;
    int ok = 1;

    if (!model)
        return 0;
    if (feature_ids.empty())
        return 1;
    if (ProArrayAlloc(1, sizeof(ProFeatureDeleteOptions), 1, (ProArray *)&delete_options) != PRO_TK_NO_ERROR ||
        ProArrayAlloc(1, sizeof(int), 1, (ProArray *)&feature_id) != PRO_TK_NO_ERROR) {
        ok = 0;
        goto cleanup;
    }

    delete_options[0] = (ProFeatureDeleteOptions)PRO_FEAT_DELETE_CLIP;
    for (size_t index = 0; index < feature_ids.size(); ++index) {
        feature_id[0] = feature_ids[index];
        if (ProFeatureWithoptionsSuppress(ProMdlToSolid(model), feature_id,
                                          delete_options, PRO_REGEN_NO_FLAGS) != PRO_TK_NO_ERROR) {
            ok = 0;
            break;
        }
    }

cleanup:
    if (delete_options)
        (void)ProArrayFree((ProArray *)&delete_options);
    if (feature_id)
        (void)ProArrayFree((ProArray *)&feature_id);
    return ok;
}


void
creo_brl_resume_suppressed_features(ProMdl model, const std::vector<int>& feature_ids)
{
    ProFeatureResumeOptions *resume_options = NULL;
    int *feature_id = NULL;

    if (!model || feature_ids.empty())
        return;
    if (ProArrayAlloc(1, sizeof(ProFeatureResumeOptions), 1, (ProArray *)&resume_options) != PRO_TK_NO_ERROR ||
        ProArrayAlloc(1, sizeof(int), 1, (ProArray *)&feature_id) != PRO_TK_NO_ERROR)
        goto cleanup;

    resume_options[0] = (ProFeatureResumeOptions)PRO_FEAT_RESUME_INCLUDE_PARENTS;
    for (size_t index = 0; index < feature_ids.size(); ++index) {
        feature_id[0] = feature_ids[index];
        (void)ProFeatureResume(ProMdlToSolid(model), feature_id, 1, resume_options, PRO_REGEN_NO_FLAGS);
    }

cleanup:
    if (resume_options)
        (void)ProArrayFree((ProArray *)&resume_options);
    if (feature_id)
        (void)ProArrayFree((ProArray *)&feature_id);
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
    ProFeattype feature_type;
    ProFeatStatus feature_status;

    if (ProFeatureTypeGet(feature, &feature_type) != PRO_TK_NO_ERROR ||
        ProFeatureStatusGet(feature, &feature_status) != PRO_TK_NO_ERROR ||
        feature_status != PRO_FEAT_ACTIVE)
        return PRO_TK_CONTINUE;

    switch (feature_type) {
        case PRO_FEAT_HOLE:
        case PRO_FEAT_ROUND:
        case PRO_FEAT_CHAMFER:
            return PRO_TK_NO_ERROR;
        default:
            return PRO_TK_CONTINUE;
    }
}


static ProError
capture_small_feature(ProFeature *feature, ProError UNUSED(status), ProAppData app_data)
{
    struct feature_capture_context *context = (struct feature_capture_context *)app_data;
    ProFeattype feature_type;
    struct creo_brl_csg_feature_inputs inputs = {};
    std::vector<struct creo_brl_snapshot_tgc_subtraction> hole_subtractions;
    const size_t previous_count = context ? context->suppressed_features.size() : 0;
    const double scale = context ? context->model_to_mm : 0.0;

    if (!context || !context->settings || scale <= 0.0)
        return PRO_TK_BAD_INPUTS;
    context->radius = 0.0;
    context->diameter = 0.0;
    context->dist1 = 0.0;
    context->dist2 = 0.0;
    context->has_diameter = 0;
    context->has_distances = 0;
    if (ProFeatureTypeGet(feature, &feature_type) != PRO_TK_NO_ERROR ||
        ProFeatureDimensionVisit(feature, capture_feature_dimensions, feature_dimension_filter,
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
        case CREO_BRL_CSG_FEATURE_SUPPRESS:
            context->suppressed_features.push_back(feature->id);
            break;
        case CREO_BRL_CSG_FEATURE_SUBTRACT:
            context->suppressed_features.push_back(feature->id);
            context->tgc_subtractions->insert(context->tgc_subtractions->end(),
                                              hole_subtractions.begin(), hole_subtractions.end());
            break;
    }

    if (context->suppressed_features.size() > previous_count)
        append_dependent_features(context->model, feature, &context->suppressed_features);
    return PRO_TK_NO_ERROR;
}


int
creo_brl_capture_feature_suppression(ProMdl model,
                                     double scale,
                                     const struct creo_brl_snapshot_settings *settings,
                                     struct snapshot_part_capture *capture,
                                     std::vector<int> *suppressed_features,
                                     int *suppression_applied)
{
    struct feature_capture_context context = {};

    if (!model || !settings || !capture || !suppressed_features || !suppression_applied)
        return 0;

    *suppression_applied = 0;
    suppressed_features->clear();
    if ((settings->flags & CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES) == 0)
        return 1;

    context.model = model;
    context.settings = settings;
    context.model_to_mm = scale;
    context.tgc_subtractions = &capture->tgc_subtractions;
    if (ProSolidFeatVisit(ProMdlToSolid(model), capture_small_feature,
                          (ProFeatureFilterAction)small_feature_filter,
                          (ProAppData)&context) != PRO_TK_NO_ERROR) {
        capture->tgc_subtractions.clear();
        report_capture_warning("failed to inspect small features; falling back to unsuppressed tessellation", 0);
        return 1;
    }

    *suppressed_features = context.suppressed_features;
    if (suppressed_features->empty())
        return 1;
    if (!suppress_feature_ids(model, *suppressed_features)) {
        creo_brl_resume_suppressed_features(model, *suppressed_features);
        suppressed_features->clear();
        capture->tgc_subtractions.clear();
        report_capture_warning("failed to suppress captured features; falling back to unsuppressed tessellation", 0);
        return 1;
    }

    *suppression_applied = 1;
    return 1;
}
