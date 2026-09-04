/*                O R C H E S T R A T O R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_FACETIZE_ORCHESTRATOR_H
#define LIBGED_FACETIZE_ORCHESTRATOR_H

#include <string>
#include <vector>

#include "bu/defines.h"

struct ged;
struct _ged_facetize_state;
class method_options_t;

enum class FacetizeOutputFormat {
    Bot,
    Nmg
};

enum class FacetizeBooleanEngine {
    Manifold,
    Nmg
};

enum class FacetizeScope {
    Objects,
    Regions,
    NonOverlappingBrep
};

enum class FacetizeCommitMode {
    NewObject,
    InPlace
};

enum class FacetizePerturbMode {
    Disabled,
    Enabled
};

struct FacetizeExecutionOptions {
    FacetizeOutputFormat output_format = FacetizeOutputFormat::Bot;
    FacetizeBooleanEngine boolean_engine = FacetizeBooleanEngine::Manifold;
    FacetizeScope scope = FacetizeScope::Objects;
    FacetizeCommitMode commit_mode = FacetizeCommitMode::NewObject;
    FacetizePerturbMode perturb_mode = FacetizePerturbMode::Disabled;

    bool writes_nmg() const { return output_format == FacetizeOutputFormat::Nmg; }
    bool uses_nmg_boolean() const { return boolean_engine == FacetizeBooleanEngine::Nmg; }
    bool processes_regions() const { return scope == FacetizeScope::Regions; }
    bool processes_breps() const { return scope == FacetizeScope::NonOverlappingBrep; }
    bool writes_in_place() const { return commit_mode == FacetizeCommitMode::InPlace; }
    bool uses_perturbation() const { return perturb_mode == FacetizePerturbMode::Enabled; }
};

struct FacetizePlan {
    FacetizeExecutionOptions execution;
    std::vector<std::string> inputs;
    std::string output;

    std::vector<const char *> input_argv() const;
};

class FacetizeSession {
    public:
	explicit FacetizeSession(struct ged *gedp);
	~FacetizeSession();

	FacetizeSession(const FacetizeSession &) = delete;
	FacetizeSession &operator=(const FacetizeSession &) = delete;

	struct _ged_facetize_state *state() const { return session_state; }
	method_options_t *method_options() const { return methods; }

    private:
	struct _ged_facetize_state *session_state = nullptr;
	method_options_t *methods = nullptr;
};

int facetize_build_plan(struct _ged_facetize_state *s, int argc,
	const char **argv, FacetizePlan &plan);
int facetize_prepare_workspace(struct _ged_facetize_state *s,
	const FacetizePlan &plan);
int facetize_execute_plan(struct _ged_facetize_state *s,
	const FacetizePlan &plan);

#endif /* LIBGED_FACETIZE_ORCHESTRATOR_H */
