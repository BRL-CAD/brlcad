/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include <memory>

#include "bu/app.h"
#include "bu/log.h"

#include "brep.h"
#include "brep/boolean.h"


static ON_Brep *
make_box(double offset)
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(offset, 0.0, 0.0),
	ON_3dPoint(offset + 1.0, 0.0, 0.0),
	ON_3dPoint(offset + 1.0, 1.0, 0.0),
	ON_3dPoint(offset, 1.0, 0.0),
	ON_3dPoint(offset, 0.0, 1.0),
	ON_3dPoint(offset + 1.0, 0.0, 1.0),
	ON_3dPoint(offset + 1.0, 1.0, 1.0),
	ON_3dPoint(offset, 1.0, 1.0)
    };
    return ON_BrepBox(corners);
}


static bool
is_empty(const ON_Brep &brep)
{
    return brep.m_F.Count() == 0 && brep.m_E.Count() == 0 &&
	brep.m_V.Count() == 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    std::unique_ptr<ON_Brep> first(make_box(0.0));
    std::unique_ptr<ON_Brep> second(make_box(2.0));
    if (!first || !second || !first->IsValid() || !first->IsSolid() ||
	!second->IsValid() || !second->IsSolid())
    {
	bu_log("unable to create Boolean failure test inputs\n");
	return 1;
    }

    struct brep_boolean_failure_info failure =
	BREP_BOOLEAN_FAILURE_INFO_INIT;
    if (ON_Boolean(NULL, first.get(), second.get(), BOOLEAN_UNION,
	    &failure) != BREP_BOOLEAN_INVALID_INPUT ||
	failure.stage != BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION ||
	failure.reason != BREP_BOOLEAN_FAILURE_REASON_INVALID_INPUT)
    {
	bu_log("null output did not return detailed invalid-input status\n");
	return 2;
    }

    ON_Brep output(*first);
    failure.stage = BREP_BOOLEAN_FAILURE_STAGE_INTERNAL;
    if (ON_Boolean(&output, NULL, second.get(), BOOLEAN_UNION, &failure) !=
	    BREP_BOOLEAN_INVALID_INPUT || !is_empty(output) ||
	failure.stage != BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION ||
	failure.reason != BREP_BOOLEAN_FAILURE_REASON_INVALID_INPUT)
    {
	bu_log("null input did not fail transactionally\n");
	return 3;
    }

    if (ON_Boolean(first.get(), first.get(), second.get(), BOOLEAN_UNION) !=
	    BREP_BOOLEAN_INVALID_INPUT || !first->IsValid() || !first->IsSolid())
    {
	bu_log("aliased output did not preserve its input\n");
	return 4;
    }

    output = *first;
    const op_type invalid_operation = static_cast<op_type>(BOOLEAN_XOR + 1);
    if (ON_Boolean(&output, first.get(), second.get(), invalid_operation,
	    &failure) != BREP_BOOLEAN_UNSUPPORTED || !is_empty(output) ||
	failure.stage != BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION ||
	failure.reason != BREP_BOOLEAN_FAILURE_REASON_UNSUPPORTED_OPERATION)
    {
	bu_log("unknown operation did not return unsupported status\n");
	return 5;
    }

    std::unique_ptr<ON_Brep> invalid(make_box(0.0));
    invalid->m_F[0].m_si = -1;
    output = *second;
    if (invalid->IsValid() ||
	ON_Boolean(&output, invalid.get(), second.get(), BOOLEAN_UNION,
	    &failure) != BREP_BOOLEAN_INVALID_INPUT || !is_empty(output) ||
	failure.stage != BREP_BOOLEAN_FAILURE_STAGE_INPUT_VALIDATION ||
	failure.reason != BREP_BOOLEAN_FAILURE_REASON_INVALID_INPUT)
    {
	bu_log("invalid geometry did not return invalid-input status\n");
	return 6;
    }

    if (ON_Boolean(&output, first.get(), second.get(), BOOLEAN_DIFF,
	    &failure) != BREP_BOOLEAN_SUCCESS ||
	failure.stage != BREP_BOOLEAN_FAILURE_STAGE_NONE ||
	failure.reason != BREP_BOOLEAN_FAILURE_REASON_NONE)
    {
	bu_log("successful Boolean retained a failure classification\n");
	return 8;
    }

    const int statuses[] = {
	BREP_BOOLEAN_FAILURE,
	BREP_BOOLEAN_INVALID_INPUT,
	BREP_BOOLEAN_INDETERMINATE,
	BREP_BOOLEAN_BUDGET_EXCEEDED,
	BREP_BOOLEAN_UNSUPPORTED,
	BREP_BOOLEAN_INTERNAL_ERROR,
	BREP_BOOLEAN_SUCCESS,
	BREP_BOOLEAN_EMPTY
    };
    for (size_t status_index = 0;
	 status_index < sizeof(statuses) / sizeof(statuses[0]); ++status_index)
    {
	const char *description = ON_BooleanStatusDescription(
	    statuses[status_index]);
	if (description == NULL || description[0] == '\0') {
	    bu_log("Boolean status %d has no description\n",
		statuses[status_index]);
	    return 7;
	}
    }

    for (int stage = BREP_BOOLEAN_FAILURE_STAGE_NONE;
	 stage <= BREP_BOOLEAN_FAILURE_STAGE_INTERNAL; ++stage)
    {
	const char *description = ON_BooleanFailureStageDescription(
	    static_cast<enum brep_boolean_failure_stage>(stage));
	if (description == NULL || description[0] == '\0') {
	    bu_log("Boolean failure stage %d has no description\n", stage);
	    return 9;
	}
    }
    for (int reason = BREP_BOOLEAN_FAILURE_REASON_NONE;
	 reason <= BREP_BOOLEAN_FAILURE_REASON_SSI_EDGE_PARTITION; ++reason)
    {
	const char *description = ON_BooleanFailureReasonDescription(
	    static_cast<enum brep_boolean_failure_reason>(reason));
	if (description == NULL || description[0] == '\0') {
	    bu_log("Boolean failure reason %d has no description\n", reason);
	    return 10;
	}
    }

    return 0;
}
