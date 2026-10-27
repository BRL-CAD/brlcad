/**
 * Phase 5 frontend-side CSG fact capture helpers.
 */

#ifndef CREO_FRONTEND_CSG_CAPTURE_H
#define CREO_FRONTEND_CSG_CAPTURE_H

#include <stddef.h>

#include <vector>

#include "creo-brl.h"
#include "conversion_snapshot.h"


struct snapshot_part_capture;


typedef int (*creo_brl_feature_change_fn)(void *context, int feature_id);


enum creo_brl_feature_suppression_result {
    CREO_BRL_FEATURE_SUPPRESSION_CAPTURE_FAILED = -2,
    CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED = -1,
    CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED = 0,
    CREO_BRL_FEATURE_SUPPRESSION_APPLIED = 1
};


enum creo_brl_csg_feature_action {
    CREO_BRL_CSG_FEATURE_KEEP = 0,
    CREO_BRL_CSG_FEATURE_SUPPRESS = 1,
    CREO_BRL_CSG_FEATURE_SUBTRACT = 2
};


enum creo_brl_csg_sequence_feature {
    CREO_BRL_CSG_SEQUENCE_OTHER = 0,
    CREO_BRL_CSG_SEQUENCE_HOLE = 1,
    CREO_BRL_CSG_SEQUENCE_MATERIAL_ADDING = 2
};


int creo_brl_suppress_features_transactionally(
    const std::vector<int>& feature_ids,
    creo_brl_feature_change_fn suppress_feature,
    creo_brl_feature_change_fn resume_feature,
    void *context,
    std::vector<int> *suppressed_features);
int creo_brl_restore_features(const std::vector<int>& feature_ids,
                              creo_brl_feature_change_fn resume_feature,
                              void *context);
int creo_brl_csg_hole_action(int eliminate_small_features,
                             int facets_only,
                             int hole_supported,
                             double diameter,
                             double minimum_diameter);
int creo_brl_csg_feature_adds_material(ProFeattype feature_type);
int creo_brl_csg_hole_is_eligible(const std::vector<int>& feature_sequence,
                                  size_t hole_index);


int creo_brl_capture_feature_suppression(ProMdl model,
                                         double scale,
                                         const struct creo_brl_snapshot_settings *settings,
                                         struct snapshot_part_capture *capture,
                                         std::vector<int> *suppressed_features);

int creo_brl_resume_suppressed_features(ProMdl model,
                                        const std::vector<int>& feature_ids);


#endif /* CREO_FRONTEND_CSG_CAPTURE_H */
