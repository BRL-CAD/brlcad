/*                 S T E P W R A P P E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/STEPWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */

#include "common.h"
#include <cctype>
#include <algorithm>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <deque>
#include <exception>

#include "brep/pullback.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

/* interface header */
#include "./STEPWrapper.h"
#include "STEPBudget.h"
#include "STEPBrepValidation.h"
#include "STEPImportInternal.h"
#include "StepSchemaRuntime.h"

/* implementation headers */
#include "LocalUnits.h"
#include "GlobalUncertaintyAssignedContext.h"
#include "STEPEntity.h"
#include "STEPString.h"
#ifdef HAVE_STEPCODE_LAZY
#  include "STEPLazySession.h"
#endif

namespace {

/* Detached geometry conversion is parallel, but a raw candidate and a safe
 * repair retry for one item must not change any other worker's policy. */
thread_local const STEPWrapper *thread_option_wrapper = NULL;
thread_local brlcad::step::ImportOptions thread_import_options;
thread_local bool thread_options_active = false;
thread_local bool thread_options_speculative = false;
thread_local const STEPWrapper *thread_inference_wrapper = NULL;
thread_local std::shared_ptr<STEPWrapper::CurveInferenceTransaction>
    thread_inference_transaction;
thread_local bool thread_curve_inference_enabled = false;
thread_local bool thread_whole_item_curve_inference_enabled = false;

/* Metadata and relationship passes commonly visit many roots whose dependency
 * closures overlap almost completely.  Keep a modest group resident so each
 * shared graph is not parsed once per root, while bounding the extra live
 * graph on files whose selected roots are independent. */
const size_t kLazyIterationBatchRoots = 16;

/* STEPcode's lazy index identifies a Part 21 complex instance as one
 * COMPLEX_ENTITY record.  The historical converter census instead counted
 * each explicitly encoded component keyword.  Recover just those outer
 * component names from the indexed source record without materializing the
 * instance or attempting to parse any attribute values. */
std::vector<std::string>
complex_entity_keywords(const std::string &record)
{
    std::vector<std::string> keywords;
    size_t position = record.find('=');
    if (position == std::string::npos) return keywords;
    ++position;
    while (position < record.size() &&
	    std::isspace(static_cast<unsigned char>(record[position])))
	++position;
    if (position >= record.size() || record[position] != '(') return keywords;

    int depth = 1;
    bool quoted = false;
    bool comment = false;
    for (++position; position < record.size() && depth > 0;) {
	if (comment) {
	    if (record[position] == '*' && position + 1 < record.size() &&
		    record[position + 1] == '/') {
		comment = false;
		position += 2;
	    } else {
		++position;
	    }
	    continue;
	}
	if (quoted) {
	    if (record[position] == '\'') {
		if (position + 1 < record.size() && record[position + 1] == '\'') {
		    position += 2;
		    continue;
		}
		quoted = false;
	    }
	    ++position;
	    continue;
	}
	if (record[position] == '/' && position + 1 < record.size() &&
		record[position + 1] == '*') {
	    comment = true;
	    position += 2;
	    continue;
	}
	if (record[position] == '\'') {
	    quoted = true;
	    ++position;
	    continue;
	}
	if (depth == 1 && (std::isalpha(static_cast<unsigned char>(record[position])) ||
		record[position] == '_')) {
	    const size_t begin = position++;
	    while (position < record.size() &&
		    (std::isalnum(static_cast<unsigned char>(record[position])) ||
		     record[position] == '_'))
		++position;
	    const size_t end = position;
	    while (position < record.size() &&
		    std::isspace(static_cast<unsigned char>(record[position])))
		++position;
	    if (position < record.size() && record[position] == '(')
		keywords.push_back(record.substr(begin, end - begin));
	    continue;
	}
	if (record[position] == '(')
	    ++depth;
	else if (record[position] == ')')
	    --depth;
	++position;
    }
    return keywords;
}

using namespace step_import_detail;

}


const char *
STEPWrapper::SchemaName() const
{
    return brlcad::step::CurrentStepSchemaRuntime().Name();
}


const EntityDescriptor *
STEPWrapper::SchemaEntity(const char *name) const
{
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) return lazy_session->Entity(name);
#endif
    return registry ? brlcad::step::CurrentStepSchemaRuntime().Entity(*registry, name) : NULL;
}


const TypeDescriptor *
STEPWrapper::SchemaType(const char *name) const
{
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) return lazy_session->Type(name);
#endif
    return registry ? brlcad::step::CurrentStepSchemaRuntime().Type(*registry, name) : NULL;
}


bool
STEPWrapper::IsSchemaEntity(const SDAI_Application_instance *instance,
    const char *name) const
{
    const EntityDescriptor *descriptor = SchemaEntity(name);
    return instance && descriptor && instance->IsA(descriptor);
}


STEPWrapper::RepairModeScope::RepairModeScope(const STEPWrapper *wrapper,
    brlcad::step::RepairMode repair, bool speculative)
    : previous_wrapper(thread_option_wrapper),
      previous_options(thread_import_options),
      previous_active(thread_options_active),
      previous_speculative(thread_options_speculative)
{
    thread_import_options = wrapper ? wrapper->ImportOptions() :
	brlcad::step::ImportOptions();
    thread_import_options.repair = repair;
    thread_option_wrapper = wrapper;
    thread_options_active = wrapper != NULL;
    thread_options_speculative = speculative;
}


STEPWrapper::RepairModeScope::~RepairModeScope()
{
    thread_option_wrapper = previous_wrapper;
    thread_import_options = previous_options;
    thread_options_active = previous_active;
    thread_options_speculative = previous_speculative;
}


STEPWrapper::CurveInferenceScope::CurveInferenceScope(
    const STEPWrapper *wrapper,
    const std::shared_ptr<CurveInferenceTransaction> &transaction,
    bool enable_inference, bool enable_whole_item_inference)
    : previous_wrapper(thread_inference_wrapper),
      previous_transaction(thread_inference_transaction),
      previous_inference_enabled(thread_curve_inference_enabled),
      previous_whole_item_inference_enabled(
	  thread_whole_item_curve_inference_enabled)
{
    thread_inference_wrapper = wrapper;
    thread_inference_transaction = transaction;
    thread_curve_inference_enabled = enable_inference;
    thread_whole_item_curve_inference_enabled =
	enable_inference && enable_whole_item_inference;
}


STEPWrapper::CurveInferenceScope::~CurveInferenceScope()
{
    thread_inference_wrapper = previous_wrapper;
    thread_inference_transaction = previous_transaction;
    thread_curve_inference_enabled = previous_inference_enabled;
    thread_whole_item_curve_inference_enabled =
	previous_whole_item_inference_enabled;
}


STEPWrapper::GeometryAttemptScope::GeometryAttemptScope(STEPWrapper *sw)
    : wrapper(sw), previous_wrapper(thread_inference_wrapper),
      previous_transaction(thread_inference_transaction),
      attempt_transaction(new CurveInferenceTransaction()),
      previous_inference_enabled(thread_curve_inference_enabled),
      previous_whole_item_inference_enabled(
	  thread_whole_item_curve_inference_enabled),
      committed(false)
{
    thread_inference_wrapper = wrapper;
    thread_inference_transaction = attempt_transaction;
    /* A fallback construction strategy must use the same inference policy as
     * its enclosing whole-object transaction. */
    thread_curve_inference_enabled = previous_wrapper == wrapper &&
	previous_inference_enabled;
    thread_whole_item_curve_inference_enabled = previous_wrapper == wrapper &&
	previous_whole_item_inference_enabled;
}


STEPWrapper::GeometryAttemptScope::~GeometryAttemptScope()
{
    thread_inference_wrapper = previous_wrapper;
    thread_inference_transaction = previous_transaction;
    thread_curve_inference_enabled = previous_inference_enabled;
    thread_whole_item_curve_inference_enabled =
	previous_whole_item_inference_enabled;

    if (!committed || !attempt_transaction)
	return;
    if (previous_wrapper != wrapper || !previous_transaction) {
	if (wrapper)
	    wrapper->CommitCurveInferenceTransaction(attempt_transaction);
	return;
    }

    std::vector<brlcad::step::Diagnostic> attempt_diagnostics;
    std::vector<brlcad::step::Diagnostic> attempt_repairs;
    std::map<std::string, brlcad::step::InferredCurve> attempt_curves;
    bool candidate_observed = false;
    bool inference_required = false;
    bool retry_blocked = false;
    {
	std::lock_guard<std::mutex> guard(attempt_transaction->mutex);
	attempt_diagnostics = attempt_transaction->diagnostics;
	attempt_repairs = attempt_transaction->repairs;
	attempt_curves = attempt_transaction->curves;
	candidate_observed = attempt_transaction->candidate_observed;
	inference_required = attempt_transaction->inference_required;
	retry_blocked = attempt_transaction->retry_blocked;
    }

    const auto merge_diagnostics = [](std::vector<brlcad::step::Diagnostic> &to,
	    const std::vector<brlcad::step::Diagnostic> &from) {
	for (std::vector<brlcad::step::Diagnostic>::const_iterator source =
		from.begin(); source != from.end(); ++source) {
	    std::vector<brlcad::step::Diagnostic>::iterator existing =
		std::find_if(to.begin(), to.end(), [&source](
		    const brlcad::step::Diagnostic &candidate) {
			return candidate.severity == source->severity &&
			    candidate.entity_id == source->entity_id &&
			    candidate.entity_type == source->entity_type &&
			    candidate.attribute == source->attribute &&
			    candidate.message == source->message;
		    });
	    if (existing == to.end())
		to.push_back(*source);
	    else
		existing->repeat_count += source->repeat_count;
	}
    };
    {
	std::lock_guard<std::mutex> guard(previous_transaction->mutex);
	merge_diagnostics(previous_transaction->diagnostics,
	    attempt_diagnostics);
	merge_diagnostics(previous_transaction->repairs, attempt_repairs);
	for (std::map<std::string, brlcad::step::InferredCurve>::const_iterator
		curve = attempt_curves.begin(); curve != attempt_curves.end();
		++curve) {
	    std::map<std::string, brlcad::step::InferredCurve>::iterator existing =
		previous_transaction->curves.find(curve->first);
	    if (existing == previous_transaction->curves.end()) {
		previous_transaction->curves.insert(*curve);
		continue;
	    }
	    existing->second.discrepancy_mm = std::max(
		existing->second.discrepancy_mm, curve->second.discrepancy_mm);
	    if (curve->second.safe_limit_mm > 0.0 &&
		    (!(existing->second.safe_limit_mm > 0.0) ||
		     curve->second.safe_limit_mm < existing->second.safe_limit_mm))
		existing->second.safe_limit_mm = curve->second.safe_limit_mm;
	    existing->second.inference_limit_mm = std::max(
		existing->second.inference_limit_mm,
		curve->second.inference_limit_mm);
	    if (!curve->second.detail.empty() &&
		    existing->second.detail.find(curve->second.detail) ==
			std::string::npos) {
		if (!existing->second.detail.empty())
		    existing->second.detail += "; ";
		existing->second.detail += curve->second.detail;
	    }
	}
	previous_transaction->candidate_observed =
	    previous_transaction->candidate_observed || candidate_observed;
	previous_transaction->inference_required =
	    previous_transaction->inference_required || inference_required;
	previous_transaction->retry_blocked =
	    previous_transaction->retry_blocked || retry_blocked;
    }
}


void
STEPWrapper::GeometryAttemptScope::Commit()
{
    committed = true;
}


const brlcad::step::ImportOptions &
STEPWrapper::ImportOptions() const
{
    return thread_options_active && thread_option_wrapper == this ?
	thread_import_options : import_options;
}


bool
STEPWrapper::CurveInferenceTrialEnabled() const
{
    return thread_inference_wrapper == this &&
	thread_inference_transaction.get() != NULL &&
	thread_curve_inference_enabled;
}


bool
STEPWrapper::WholeItemCurveInferenceTrialEnabled() const
{
    return CurveInferenceTrialEnabled() &&
	thread_whole_item_curve_inference_enabled;
}


void
STEPWrapper::NoteCurveInferenceCandidate()
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    thread_inference_transaction->candidate_observed = true;
}


bool
STEPWrapper::CurveInferenceCandidateObserved() const
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return false;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    return thread_inference_transaction->candidate_observed;
}


void
STEPWrapper::RequireCurveInferenceCandidate()
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    thread_inference_transaction->candidate_observed = true;
    thread_inference_transaction->inference_required = true;
}


bool
STEPWrapper::CurveInferenceRequired() const
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return false;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    return thread_inference_transaction->inference_required;
}


void
STEPWrapper::BlockCurveInferenceRetry()
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    thread_inference_transaction->retry_blocked = true;
}


bool
STEPWrapper::CurveInferenceRetryBlocked() const
{
    if (thread_inference_wrapper != this ||
	    !thread_inference_transaction)
	return false;
    std::lock_guard<std::mutex> guard(thread_inference_transaction->mutex);
    return thread_inference_transaction->retry_blocked;
}


void
STEPWrapper::configureImportBudgets()
{
    const bool automatic_scale = !(import_options.budget_scale > 0.0);
    const bool calibration_needed = automatic_scale &&
	(!import_options.item_budget_milliseconds ||
	 !import_options.stall_timeout_milliseconds);
    brlcad::step::BudgetCalibration calibration;
    if (calibration_needed)
	calibration = brlcad::step::CalibrateImportBudgets(
	    import_options.effective_jobs);

    statistics.budget_calibration_ran = calibration_needed;
    statistics.budget_calibration_valid = calibration.valid;
    statistics.budget_calibration_queries = calibration.scalar_queries;
    statistics.budget_calibration_microseconds =
	calibration.scalar_microseconds;
    statistics.budget_calibration_parallel_workers =
	calibration.parallel_workers;
    statistics.budget_calibration_scalar_queries_per_second =
	calibration.scalar_queries_per_second;
    statistics.budget_calibration_parallel_queries_per_second =
	calibration.parallel_queries_per_second;
    statistics.budget_calibration_parallel_cpu_queries_per_second =
	calibration.parallel_cpu_queries_per_second;

    import_options.effective_budget_scale = automatic_scale ?
	(calibration.valid ? calibration.scale : 1.0) :
	import_options.budget_scale;

    const auto scaled = [this](uint64_t reference) {
	const long double value = static_cast<long double>(reference) *
	    static_cast<long double>(import_options.effective_budget_scale);
	if (value >= static_cast<long double>(UINT64_MAX)) return UINT64_MAX;
	return std::max<uint64_t>(1, static_cast<uint64_t>(value + 0.5L));
    };
    import_options.effective_item_budget_milliseconds =
	import_options.disable_item_budgets ? 0 :
	(import_options.item_budget_milliseconds ?
	 import_options.item_budget_milliseconds :
	 scaled(kMaximumExactPullbackMilliseconds));
    import_options.effective_stall_timeout_milliseconds =
	import_options.stall_timeout_milliseconds ?
	import_options.stall_timeout_milliseconds :
	scaled(kMaximumExactPullbackMilliseconds);
}

struct STEPWrapper::GeometryExecutor {
    struct Group {
	Group(const std::function<void(size_t)> &caller,
	    const std::function<void(size_t)> &helper, size_t task_count,
	    uint64_t remaining_milliseconds, uint64_t creation_order)
	    : caller_task(caller), helper_task(helper), count(task_count),
	      order(creation_order)
	{
	    deadline = remaining_milliseconds == UINT64_MAX ?
		std::chrono::steady_clock::time_point::max() :
		std::chrono::steady_clock::now() +
		std::chrono::milliseconds(remaining_milliseconds);
	}

	bool Claim(size_t &index)
	{
	    std::lock_guard<std::mutex> guard(lock);
	    if (next >= count) return false;
	    index = next++;
	    ++running;
	    return true;
	}

	bool HasUnclaimed()
	{
	    std::lock_guard<std::mutex> guard(lock);
	    return next < count;
	}

	void Finish(std::exception_ptr failure = std::exception_ptr())
	{
	    {
		std::lock_guard<std::mutex> guard(lock);
		if (failure && !exception) exception = failure;
		if (running) --running;
	    }
	    finished.notify_all();
	}

	void Wait()
	{
	    std::unique_lock<std::mutex> guard(lock);
	    finished.wait(guard, [this]() { return next >= count && running == 0; });
	    if (exception) std::rethrow_exception(exception);
	}

	std::function<void(size_t)> caller_task;
	std::function<void(size_t)> helper_task;
	std::chrono::steady_clock::time_point deadline;
	size_t count = 0;
	size_t next = 0;
	size_t running = 0;
	std::exception_ptr exception;
	uint64_t order = 0;
	std::mutex lock;
	std::condition_variable finished;
    };

    GeometryExecutor(STEPWrapper *source, unsigned int maximum_concurrency)
	: wrapper(source), capacity(std::max(1U, maximum_concurrency))
    {
	for (unsigned int i = 1; i < capacity; ++i)
	    helpers.emplace_back(&GeometryExecutor::RunHelper, this);
    }

    ~GeometryExecutor()
    {
	Shutdown();
    }

    void Shutdown()
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    stopping = true;
	}
	changed.notify_all();
	for (std::thread &helper : helpers) {
	    if (helper.joinable()) helper.join();
	}
	helpers.clear();
	if (wrapper) wrapper->SetGeometryHelpersActive(0);
    }

    void WorkerStarted(bool exclusive_pullback)
    {
	std::unique_lock<std::mutex> guard(lock);
	if (exclusive_pullback) ++exclusive_waiters;
	/* Admit roots freely while no nested work exists.  As soon as any active
	 * root publishes face/edge tasks, stop replacing completed root workers;
	 * their slots transfer to the helper pool until the nested queue drains.
	 * This keeps all CPUs useful without starting more wall-clock item
	 * deadlines than the machine can service. */
	changed.wait(guard, [this, exclusive_pullback]() {
	    RemoveCompletedGroupsLocked();
	    if (stopping) return true;
	    if (exclusive_pullback)
		return !exclusive_root_active && top_level_workers == 0 &&
		    active_helpers == 0 && groups.empty();
	    return !exclusive_root_active && exclusive_waiters == 0 &&
		groups.empty() && top_level_workers + active_helpers < capacity;
	});
	if (exclusive_pullback && exclusive_waiters) --exclusive_waiters;
	if (!stopping) {
	    ++top_level_workers;
	    if (exclusive_pullback) exclusive_root_active = true;
	}
	guard.unlock();
	changed.notify_all();
    }

    void WorkerFinished(bool exclusive_pullback)
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    if (top_level_workers) --top_level_workers;
	    if (exclusive_pullback) exclusive_root_active = false;
	}
	changed.notify_all();
    }

    void ParallelFor(size_t count, const std::function<void(size_t)> &caller_task,
	const std::function<void(size_t)> &helper_task,
	uint64_t remaining_milliseconds)
    {
	if (count < 2 || capacity < 2) {
	    for (size_t index = 0; index < count; ++index) caller_task(index);
	    return;
	}
	std::shared_ptr<Group> group;
	{
	    std::lock_guard<std::mutex> guard(lock);
	    group.reset(new Group(caller_task, helper_task, count,
		remaining_milliseconds, next_group_order++));
	    groups.push_back(group);
	}
	changed.notify_all();
	size_t index = 0;
	while (group->Claim(index)) {
	    try {
		group->caller_task(index);
		group->Finish();
	    } catch (...) {
		group->Finish(std::current_exception());
	    }
	}
	group->Wait();
	changed.notify_all();
    }

    unsigned int AvailableHelperCapacity()
    {
	std::lock_guard<std::mutex> guard(lock);
	const unsigned int occupied = top_level_workers + active_helpers;
	return occupied < capacity ? capacity - occupied : 0;
    }

private:
    void RemoveCompletedGroupsLocked()
    {
	for (std::deque<std::shared_ptr<Group> >::iterator group = groups.begin();
		group != groups.end();) {
	    if (!*group || !(*group)->HasUnclaimed())
		group = groups.erase(group);
	    else
		++group;
	}
    }

    void RunHelper()
    {
	for (;;) {
	    std::shared_ptr<Group> group;
	    size_t index = 0;
	    uint64_t active_snapshot = 0;
	    {
		std::unique_lock<std::mutex> guard(lock);
		for (;;) {
		    RemoveCompletedGroupsLocked();
		    if (stopping) return;
		    if (!groups.empty() &&
			    top_level_workers + active_helpers < capacity)
			break;
		    changed.wait(guard);
		}
		/* Give helpers to the job with the least remaining calibrated work
		 * first.  Creation order keeps equal and unlimited budgets
		 * deterministic. */
		std::deque<std::shared_ptr<Group> >::iterator selected =
		    std::min_element(groups.begin(), groups.end(),
			[](const std::shared_ptr<Group> &left,
			   const std::shared_ptr<Group> &right) {
			    if (left->deadline != right->deadline)
				return left->deadline < right->deadline;
			    return left->order < right->order;
			});
		group = *selected;
		groups.erase(selected);
		if (!group || !group->Claim(index)) continue;
		if (group->HasUnclaimed()) groups.push_back(group);
		++active_helpers;
		active_snapshot = active_helpers;
	    }
	    if (wrapper) wrapper->SetGeometryHelpersActive(active_snapshot);
	    std::exception_ptr failure;
	    try {
		group->helper_task(index);
	    } catch (...) {
		failure = std::current_exception();
	    }
	    group->Finish(failure);
	    {
		std::lock_guard<std::mutex> guard(lock);
		if (active_helpers) --active_helpers;
		active_snapshot = active_helpers;
	    }
	    if (wrapper) wrapper->SetGeometryHelpersActive(active_snapshot);
	    changed.notify_all();
	}
    }

    STEPWrapper *wrapper = NULL;
    unsigned int capacity = 1;
    std::mutex lock;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Group> > groups;
    std::vector<std::thread> helpers;
    unsigned int top_level_workers = 0;
    unsigned int active_helpers = 0;
	unsigned int exclusive_waiters = 0;
	bool exclusive_root_active = false;
	uint64_t next_group_order = 0;
    bool stopping = false;
};


static bool
geometry_helper_cancelled(void *context)
{
    STEPWrapper *wrapper = static_cast<STEPWrapper *>(context);
    return wrapper && wrapper->CancellationRequested();
}

STEPWrapper::STEPWrapper()
    : registry(NULL), sfile(NULL), dotg(NULL), verbose(false)
{
    int ownsInstanceMemory = 1;
    instance_list = new InstMgr(ownsInstanceMemory);
}


STEPWrapper::~STEPWrapper()
{
    StopGeometryExecutor();
    ClearEntityCache();
#ifdef HAVE_STEPCODE_LAZY
    releaseLazyBatches();
    lazy_session.reset();
#endif
    delete sfile;
    delete instance_list;
    delete registry;
    dotg = NULL;
}


void
STEPWrapper::ConfigureGeometryExecutor(unsigned int concurrency)
{
    StopGeometryExecutor();
    if (concurrency > 1)
	geometry_executor.reset(new GeometryExecutor(this, concurrency));
}


void
STEPWrapper::StopGeometryExecutor()
{
    if (!geometry_executor) return;
    geometry_executor->Shutdown();
    geometry_executor.reset();
}


void
STEPWrapper::GeometryWorkerStarted(int64_t entity_id,
    const std::string &entity_type, bool exclusive_pullback)
{
    if (geometry_executor) geometry_executor->WorkerStarted(exclusive_pullback);
    std::lock_guard<std::mutex> guard(progress_mutex);
    ActiveGeometryJobProgress &job =
	active_geometry_job_progress[std::this_thread::get_id()];
    job = ActiveGeometryJobProgress();
    job.started = std::chrono::steady_clock::now();
    job.root_entity_id = entity_id;
    job.current_entity_id = entity_id;
    job.entity_type = entity_type;
    job.phase = "starting detached exact geometry";
}


void
STEPWrapper::GeometryWorkerFinished(bool exclusive_pullback)
{
    {
	std::lock_guard<std::mutex> guard(progress_mutex);
	active_geometry_job_progress.erase(std::this_thread::get_id());
    }
    if (geometry_executor) geometry_executor->WorkerFinished(exclusive_pullback);
}


void
STEPWrapper::ParallelForGeometry(size_t count,
    const std::function<void(size_t)> &task)
{
    if (!geometry_executor || count < 2) {
	for (size_t index = 0; index < count; ++index) task(index);
	return;
    }
    const uint64_t remaining = brlcad::PullbackWorkRemainingMilliseconds();
    const brlcad::PullbackWorkBudgetHandle work_budget =
	brlcad::CurrentPullbackWorkBudget();
    const double length = LocalUnits::length;
    const double planeangle = LocalUnits::planeangle;
    const double solidangle = LocalUnits::solidangle;
    const double tolerance = LocalUnits::tolerance;
    const double representation_tolerance =
	LocalUnits::representation_tolerance;
    const brlcad::step::RepairMode repair = ImportOptions().repair;
    const bool speculative = thread_options_active &&
	thread_option_wrapper == this && thread_options_speculative;
    const std::shared_ptr<CurveInferenceTransaction> inference_transaction =
	thread_inference_wrapper == this ? thread_inference_transaction :
	std::shared_ptr<CurveInferenceTransaction>();
    const bool curve_inference_enabled = thread_inference_wrapper == this &&
	thread_curve_inference_enabled;
    const bool whole_item_curve_inference_enabled =
	thread_inference_wrapper == this &&
	thread_whole_item_curve_inference_enabled;
    const std::function<void(size_t)> helper_task = [this, task, work_budget,
	length, planeangle, solidangle, tolerance, representation_tolerance, repair,
	speculative, inference_transaction, curve_inference_enabled,
	whole_item_curve_inference_enabled](size_t index) {
	RepairModeScope repair_scope(this, repair, speculative);
	CurveInferenceScope inference_scope(this, inference_transaction,
	    curve_inference_enabled, whole_item_curve_inference_enabled);
	LocalUnits::length = length;
	LocalUnits::planeangle = planeangle;
	LocalUnits::solidangle = solidangle;
	LocalUnits::tolerance = tolerance;
	LocalUnits::representation_tolerance = representation_tolerance;
	brlcad::SetPullbackWorkLimit(geometry_helper_cancelled, this,
	    work_budget,
	    import_options.effective_stall_timeout_milliseconds);
	try {
	    task(index);
	    const bool deadline_expired =
		brlcad::PullbackWorkDeadlineExpired();
	    const bool stalled = brlcad::PullbackWorkStalled();
	    brlcad::ClearPullbackWorkLimit();
	    if (deadline_expired || stalled)
		throw PullbackWorkStopped(deadline_expired, stalled);
	} catch (...) {
	    brlcad::ClearPullbackWorkLimit();
	    throw;
	}
    };
    try {
	geometry_executor->ParallelFor(count, task, helper_task, remaining);
    } catch (const PullbackWorkStopped &stopped) {
	brlcad::PropagatePullbackWorkStop(stopped.deadline_expired,
	    stopped.stalled);
    }
    /* Helper completion is forward progress for the waiting parent.  This
     * refreshes its stall heartbeat without clearing a propagated reason. */
    brlcad::PullbackWorkProgress(count);
}


unsigned int
STEPWrapper::AvailableGeometryHelperCapacity()
{
    return geometry_executor ? geometry_executor->AvailableHelperCapacity() : 0;
}


STEPDetachedEntityArena::STEPDetachedEntityArena() = default;


STEPDetachedEntityArena::~STEPDetachedEntityArena()
{
    for (std::map<int, STEPEntity *>::iterator object = objects.begin();
	 object != objects.end(); ++object)
	delete object->second;
    for (std::list<STEPEntity *>::iterator object = unmapped_objects.begin();
	 object != unmapped_objects.end(); ++object)
	delete *object;
}


STEPEntity *
STEPDetachedEntityArena::FindObject(int id) const
{
    std::map<int, STEPEntity *>::const_iterator found = objects.find(id);
    return found == objects.end() ? NULL : found->second;
}


void
STEPDetachedEntityArena::ResetOpenNURBSState()
{
    for (std::map<int, STEPEntity *>::iterator object = objects.begin();
	 object != objects.end(); ++object) {
	if (object->second)
	    object->second->ResetONState();
    }
    for (std::list<STEPEntity *>::iterator object = unmapped_objects.begin();
	 object != unmapped_objects.end(); ++object) {
	if (*object)
	    (*object)->ResetONState();
    }
}


int
STEPWrapper::InstanceCount() const
{
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session)
	return static_cast<int>(lazy_filter_active ? lazy_iteration_ids.size() : lazy_instance_ids.size());
#endif
    return instance_list ? instance_list->InstanceCount() : 0;
}


SDAI_Application_instance *
STEPWrapper::InstanceAt(int index)
{
    if (index < 0 || index >= InstanceCount()) return NULL;
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	const std::vector<uint64_t> &ids = lazy_filter_active ? lazy_iteration_ids : lazy_instance_ids;
	if (lazy_filter_active) {
	    const size_t position = static_cast<size_t>(index);
	    if (!lazy_batch || position < lazy_iteration_batch_begin ||
		    position >= lazy_iteration_batch_end) {
		releaseLazyBatches();
		const size_t begin = position -
		    position % kLazyIterationBatchRoots;
		const size_t end = std::min(ids.size(),
		    begin + kLazyIterationBatchRoots);
		const std::vector<uint64_t> roots(ids.begin() + begin,
		    ids.begin() + end);
		lazy_batch.reset(new brlcad::step::STEPLazyBatch(
		    lazy_session->LoadBatch(roots)));
		if (!lazy_batch->Valid())
		    return NULL;
		lazy_iteration_batch_begin = begin;
		lazy_iteration_batch_end = end;
	    }
	    return lazy_batch->Get(ids[position]);
	}
	return activateLazyRoot(ids[static_cast<size_t>(index)]);
    }
#endif
    return instance_list ? instance_list->GetSTEPentity(index) : NULL;
}


void
STEPWrapper::SetInstanceTypes(const std::vector<std::string> &types,
    const std::vector<uint64_t> &excluded_ids)
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session) return;
    releaseLazyBatches();
    std::set<uint64_t> selected;
    const std::set<uint64_t> excluded(excluded_ids.begin(), excluded_ids.end());
    for (std::vector<std::string>::const_iterator type = types.begin(); type != types.end(); ++type) {
	const std::vector<uint64_t> ids = lazy_session->InstancesByType(*type);
	selected.insert(ids.begin(), ids.end());
    }
    lazy_iteration_ids.clear();
    lazy_iteration_ids.reserve(selected.size());
    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	 id != lazy_instance_ids.end(); ++id) {
	if (selected.find(*id) != selected.end() && excluded.find(*id) == excluded.end())
	    lazy_iteration_ids.push_back(*id);
    }
    lazy_filter_active = true;
#else
    (void)types;
    (void)excluded_ids;
#endif
}


void
STEPWrapper::SetInstanceIds(const std::vector<uint64_t> &ids)
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session) return;
    releaseLazyBatches();
    const std::set<uint64_t> selected(ids.begin(), ids.end());
    lazy_iteration_ids.clear();
    lazy_iteration_ids.reserve(selected.size());
    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	 id != lazy_instance_ids.end(); ++id) {
	if (selected.find(*id) != selected.end())
	    lazy_iteration_ids.push_back(*id);
    }
    lazy_filter_active = true;
#else
    (void)ids;
#endif
}


void
STEPWrapper::ResetInstanceTypes()
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session) return;
    releaseLazyBatches();
    lazy_iteration_ids.clear();
    lazy_filter_active = false;
#endif
}


bool
STEPWrapper::HasLazyIndex() const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session.get() != NULL;
#else
    return false;
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyInstancesByType(const std::string &type) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->InstancesByType(type) : std::vector<uint64_t>();
#else
    (void)type;
    return std::vector<uint64_t>();
#endif
}


bool
STEPWrapper::LazyIsSchemaEntity(uint64_t id, const char *name) const
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session || !name) return false;
    const EntityDescriptor *base = SchemaEntity(name);
    if (!base) return false;
    const std::string type = lazy_session->TypeName(id);
    const EntityDescriptor *actual = type.empty() ? NULL : SchemaEntity(type.c_str());
    if (actual && (actual == base || actual->IsA(base))) return true;

    /* A Part 21 complex instance has no single file keyword which names its
     * generated C++ entity.  Test each explicitly encoded component against
     * the same public schema-descriptor hierarchy instead. */
    if (type.empty() || type == "COMPLEX_ENTITY") {
	const std::vector<std::string> components =
	    complex_entity_keywords(lazy_session->SourceRecord(id));
	for (std::vector<std::string>::const_iterator component = components.begin();
	     component != components.end(); ++component) {
	    actual = SchemaEntity(component->c_str());
	    if (actual && actual->IsA(base)) return true;
	}
    }
#else
    (void)id;
    (void)name;
#endif
    return false;
}


std::vector<uint64_t>
STEPWrapper::LazyInstancesBySchemaType(const std::string &type) const
{
    std::vector<uint64_t> result;
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session || !SchemaEntity(type.c_str())) return result;
    const EntityDescriptor *base = SchemaEntity(type.c_str());

    /* collectEntityCounts has already identified every distinct ordinary
     * Part 21 keyword.  Resolve schema inheritance once per keyword, then use
     * STEPcode's exact-type index instead of repeating a full instance scan
     * for each requested base type.  Complex instances need their component
     * descriptors checked individually, so retain their IDs during the same
     * inventory pass. */
    for (std::map<std::string, uint64_t>::const_iterator entry =
	    document.entity_counts.begin(); entry != document.entity_counts.end();
	    ++entry) {
	const EntityDescriptor *actual = SchemaEntity(entry->first.c_str());
	if (!actual || (actual != base && !actual->IsA(base))) continue;
	const std::vector<uint64_t> ids = lazy_session->InstancesByType(entry->first);
	result.insert(result.end(), ids.begin(), ids.end());
    }
    for (std::vector<uint64_t>::const_iterator id =
	    lazy_complex_instance_ids.begin(); id != lazy_complex_instance_ids.end(); ++id)
	if (LazyIsSchemaEntity(*id, type.c_str())) result.push_back(*id);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
#else
    (void)type;
#endif
    return result;
}


std::string
STEPWrapper::LazyTypeName(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->TypeName(id) : std::string();
#else
    (void)id;
    return std::string();
#endif
}

std::vector<std::string>
STEPWrapper::LazyComponentTypes(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->ComponentTypes(id) :
	std::vector<std::string>();
#else
    (void)id;
    return std::vector<std::string>();
#endif
}


std::string
STEPWrapper::LazySourceRecord(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->SourceRecord(id) : std::string();
#else
    (void)id;
    return std::string();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyForwardReferences(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->ForwardReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyReverseReferences(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->ReverseReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


InstMgrBase *
STEPWrapper::referenceManager() const
{
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) return lazy_session->ReferenceManager();
#endif
    return instance_list;
}


static bool
diagnostic_less(const brlcad::step::Diagnostic &left,
    const brlcad::step::Diagnostic &right)
{
    if (left.entity_id != right.entity_id)
	return left.entity_id < right.entity_id;
    if (left.severity != right.severity)
	return static_cast<int>(left.severity) < static_cast<int>(right.severity);
    if (left.entity_type != right.entity_type)
	return left.entity_type < right.entity_type;
    if (left.attribute != right.attribute)
	return left.attribute < right.attribute;
    return left.message < right.message;
}


#ifdef HAVE_STEPCODE_LAZY
static void
update_lazy_statistics(brlcad::step::ImportStatistics &statistics,
    const brlcad::step::STEPLazyStatistics &cache)
{
    statistics.lazy_indexed_instances = cache.instances_scanned;
    statistics.lazy_current_loaded_instances = cache.instances_loaded;
    statistics.lazy_loaded_instances = std::max(statistics.lazy_loaded_instances,
	cache.cache_high_water);
    statistics.lazy_pinned_instances = cache.instances_pinned;
    statistics.lazy_cache_hits = cache.cache_hits;
    statistics.lazy_cache_misses = cache.cache_misses;
    statistics.lazy_materializations = cache.materializations;
    statistics.lazy_evictions = cache.evictions;
    statistics.lazy_active_batches = cache.active_batches;
    statistics.lazy_data_sections = cache.data_sections;
    statistics.lazy_cache_bytes = cache.resident_source_bytes;
    statistics.lazy_cache_byte_high_water = std::max(
	statistics.lazy_cache_byte_high_water, cache.source_bytes_high_water);
    statistics.lazy_cache_bytes_available = true;
}

void
STEPWrapper::recordLazyDiagnostic(const brlcad::step::STEPLazyDiagnostic &source)
{
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    brlcad::step::DiagnosticSeverity severity = brlcad::step::DiagnosticSeverity::Information;
    if (source.severity == 1) severity = brlcad::step::DiagnosticSeverity::Warning;
    if (source.severity == 2) severity = brlcad::step::DiagnosticSeverity::Error;
    if (source.severity >= 3) severity = brlcad::step::DiagnosticSeverity::Fatal;
    for (std::vector<brlcad::step::Diagnostic>::iterator diagnostic = diagnostics.begin();
	 diagnostic != diagnostics.end(); ++diagnostic) {
	if (diagnostic->severity == severity && diagnostic->entity_id ==
		static_cast<int64_t>(source.entity_id) &&
	    diagnostic->entity_type == source.entity_type &&
	    diagnostic->attribute == source.attribute &&
	    diagnostic->message == source.message) {
	    diagnostic->file_offset = source.file_offset;
	    diagnostic->line = source.line;
	    diagnostic->repeat_count = std::max(diagnostic->repeat_count,
		source.occurrences);
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.entity_id = static_cast<int64_t>(source.entity_id);
    diagnostic.entity_type = source.entity_type;
    diagnostic.file_offset = source.file_offset;
    diagnostic.line = source.line;
    diagnostic.attribute = source.attribute;
    diagnostic.message = source.message;
    diagnostic.repeat_count = source.occurrences;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}

void
STEPWrapper::synchronizeLazyDiagnostics()
{
    if (!lazy_session) return;
    const std::vector<brlcad::step::STEPLazyDiagnostic> &source =
	lazy_session->Diagnostics();
    for (std::vector<brlcad::step::STEPLazyDiagnostic>::const_iterator diagnostic =
	     source.begin(); diagnostic != source.end(); ++diagnostic)
	recordLazyDiagnostic(*diagnostic);
}

void
STEPWrapper::releaseLazyBatches()
{
    if (!lazy_session) return;
    ClearEntityCache();
    lazy_supplemental_batches.clear();
    lazy_batch.reset();
    lazy_iteration_batch_begin = 0;
    lazy_iteration_batch_end = 0;
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
}


SDAI_Application_instance *
STEPWrapper::activateLazyRoot(uint64_t id)
{
    releaseLazyBatches();
    lazy_batch.reset(new brlcad::step::STEPLazyBatch(lazy_session->LoadBatch(id)));
    if (!lazy_batch->Valid()) return NULL;
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
    update_lazy_statistics(statistics, cache);
    return lazy_batch->Get(id);
}
#endif


STEPEntity *
STEPWrapper::FindObject(int id) const
{
    std::map<int, STEPEntity *>::const_iterator found = entity_objects.find(id);
    return found == entity_objects.end() ? NULL : found->second;
}


void
STEPWrapper::AddObject(STEPEntity *object)
{
    if (!object) return;
    if (object->STEPid() > 0)
	entity_objects[object->STEPid()] = object;
    else
	unmapped_objects.push_back(object);
}


void
STEPWrapper::ClearEntityCache()
{
    for (std::map<int, STEPEntity *>::iterator i = entity_objects.begin(); i != entity_objects.end(); ++i)
	delete i->second;
    entity_objects.clear();
    for (std::list<STEPEntity *>::iterator i = unmapped_objects.begin(); i != unmapped_objects.end(); ++i)
	delete *i;
    unmapped_objects.clear();
}


std::unique_ptr<STEPDetachedEntityArena>
STEPWrapper::DetachEntityCache()
{
    std::unique_ptr<STEPDetachedEntityArena> arena(new STEPDetachedEntityArena());
    arena->objects.swap(entity_objects);
    arena->unmapped_objects.swap(unmapped_objects);
    return arena;
}


void
STEPWrapper::ReleaseSourceData()
{
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	releaseLazyBatches();
	return;
    }
#endif
    ClearEntityCache();
}


bool
STEPWrapper::ShouldConvertEntity(int64_t entity_id)
{
    if (import_options.selected_entity_ids.empty()) return true;
    if (import_options.selected_entity_ids.find(entity_id) ==
	import_options.selected_entity_ids.end()) return false;
    statistics.selected_entity_ids_encountered.insert(entity_id);
    return true;
}


void
STEPWrapper::ResetOpenNURBSState()
{
    for (std::map<int, STEPEntity *>::iterator i = entity_objects.begin();
	 i != entity_objects.end(); ++i) {
	if (i->second)
	    i->second->ResetONState();
    }
    for (std::list<STEPEntity *>::iterator i = unmapped_objects.begin();
	 i != unmapped_objects.end(); ++i) {
	if (*i)
	    (*i)->ResetONState();
    }
}


void
STEPWrapper::SetProgress(const std::string &phase, uint64_t completed,
    uint64_t total, int64_t current_entity_id, uint64_t secondary_completed,
    const std::string &secondary_label, const std::string &detail)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.phase = phase;
    progress_state.completed = completed;
    progress_state.total = total;
    progress_state.current_entity_id = current_entity_id;
    progress_state.secondary_completed = secondary_completed;
    progress_state.secondary_total = 0;
    progress_state.secondary_label = secondary_label;
    progress_state.detail = detail;
}


void
STEPWrapper::SetProgressDetail(const std::string &phase,
    int64_t current_entity_id, uint64_t secondary_completed,
    uint64_t secondary_total, const std::string &secondary_label,
    const std::string &detail)
{
    brlcad::PullbackWorkProgress();
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.phase = phase;
    progress_state.current_entity_id = current_entity_id;
    progress_state.secondary_completed = secondary_completed;
    progress_state.secondary_total = secondary_total;
    progress_state.secondary_label = secondary_label;
    progress_state.detail = detail;
    std::map<std::thread::id, ActiveGeometryJobProgress>::iterator active =
	active_geometry_job_progress.find(std::this_thread::get_id());
    if (active != active_geometry_job_progress.end()) {
	active->second.phase = phase;
	/* A face counter is the stable outer item context.  Nested pullback,
	 * seam, and loop calls may update the operation detail without erasing
	 * which face out of the complete solid is being processed. */
	if (secondary_label == "faces" && secondary_total) {
	    active->second.item_entity_id = current_entity_id;
	    active->second.item_completed = secondary_completed;
	    active->second.item_total = secondary_total;
	    active->second.item_label = secondary_label;
	} else {
	    if (current_entity_id > 0)
		active->second.current_entity_id = current_entity_id;
	    active->second.secondary_completed = secondary_completed;
	    active->second.secondary_total = secondary_total;
	    active->second.secondary_label = secondary_label;
	}
	active->second.detail = detail;
    }
}


void
STEPWrapper::SetGeometrySchedulerProgress(uint64_t queued, uint64_t active,
    uint64_t ready, uint64_t spooled, uint64_t finished,
    uint64_t materializing, uint64_t in_flight,
    uint64_t runnable_capacity, uint64_t ready_bytes,
    uint64_t ready_byte_budget)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_jobs_queued = queued;
    progress_state.geometry_workers_active = active;
    progress_state.geometry_jobs_ready = ready;
    progress_state.geometry_jobs_spooled = spooled;
    progress_state.geometry_jobs_finished = finished;
    progress_state.geometry_jobs_materializing = materializing;
    progress_state.geometry_jobs_in_flight = in_flight;
    progress_state.geometry_runnable_capacity = runnable_capacity;
    progress_state.geometry_ready_bytes = ready_bytes;
    progress_state.geometry_ready_byte_budget = ready_byte_budget;
}


void
STEPWrapper::SetGeometryOverallProgress(uint64_t processed, uint64_t total)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_items_processed = std::max(
	progress_state.geometry_items_processed, processed);
    progress_state.geometry_items_total = std::max(
	progress_state.geometry_items_total, total);
}


void
STEPWrapper::SetGeometryHelpersActive(uint64_t active)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_helpers_active = active;
}


brlcad::step::ImportProgress
STEPWrapper::Progress() const
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    brlcad::step::ImportProgress snapshot = progress_state;
    if (active_geometry_job_progress.empty())
	return snapshot;

    std::map<std::thread::id, ActiveGeometryJobProgress>::const_iterator oldest =
	active_geometry_job_progress.begin();
    for (std::map<std::thread::id, ActiveGeometryJobProgress>::const_iterator job =
	    active_geometry_job_progress.begin();
	    job != active_geometry_job_progress.end(); ++job) {
	if (job->second.started < oldest->second.started)
	    oldest = job;
    }
    const ActiveGeometryJobProgress &job = oldest->second;
    snapshot.phase = job.phase;
    snapshot.current_entity_id = job.root_entity_id;
    snapshot.geometry_root_entity_id = job.root_entity_id;
    snapshot.geometry_item_entity_id = job.item_entity_id;
    snapshot.geometry_item_completed = job.item_completed;
    snapshot.geometry_item_total = job.item_total;
    snapshot.geometry_item_label = job.item_label;
    snapshot.geometry_subentity_id = job.current_entity_id;
    snapshot.secondary_completed = job.secondary_completed;
    snapshot.secondary_total = job.secondary_total;
    snapshot.secondary_label = job.secondary_label;
    std::ostringstream detail;
    if (!job.entity_type.empty())
	detail << job.entity_type;
    if (job.current_entity_id > 0 &&
	    job.current_entity_id != job.root_entity_id)
	detail << (detail.tellp() > 0 ? " " : "") << "subentity=#"
	    << job.current_entity_id;
    if (!job.detail.empty() && job.detail != job.entity_type)
	detail << (detail.tellp() > 0 ? " " : "") << job.detail;
    const uint64_t active_seconds = static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::seconds>(
	    std::chrono::steady_clock::now() - job.started).count());
    detail << (detail.tellp() > 0 ? " " : "")
	<< "oldest-active=" << active_seconds << 's';
    snapshot.detail = detail.str();
    return snapshot;
}


void
STEPWrapper::RecordStageTiming(const std::string &stage, int64_t entity_id,
    const std::string &entity_type, uint64_t elapsed_us, uint64_t faces,
    uint64_t edges, uint64_t trims)
{
    if (stage.empty()) return;
    std::lock_guard<std::mutex> guard(telemetry_mutex);
    brlcad::step::StageTiming &timing = statistics.stage_timings[stage];
    ++timing.calls;
    timing.total_us += elapsed_us;
    if (elapsed_us > timing.maximum_us) {
	timing.maximum_us = elapsed_us;
	timing.maximum_entity_id = entity_id;
    }
    if (elapsed_us < kSlowStageTimingMicroseconds) return;
    if (statistics.slow_item_timings.size() >= kMaximumSlowItemTimings) {
	++statistics.slow_item_timings_omitted;
	return;
    }
    brlcad::step::ItemTiming item;
    item.entity_id = entity_id;
    item.entity_type = entity_type;
    item.stage = stage;
    item.elapsed_us = elapsed_us;
    item.faces = faces;
    item.edges = edges;
    item.trims = trims;
    statistics.slow_item_timings.push_back(item);
}


void
STEPWrapper::RecordPullbackStatistics(const brlcad::PullbackStatistics &source)
{
    std::lock_guard<std::mutex> guard(telemetry_mutex);
    statistics.pullback_closest_point_queries += source.closest_point_queries;
    statistics.pullback_surfaces_prepared += source.surfaces_prepared;
    statistics.pullback_surface_cache_hits += source.surface_cache_hits;
    statistics.pullback_span_boxes_built += source.span_boxes_built;
    statistics.pullback_span_boxes_tested += source.span_boxes_tested;
    statistics.pullback_primary_search_successes +=
	source.primary_search_successes;
    statistics.pullback_continuity_seed_searches +=
	source.continuity_seed_searches;
    statistics.pullback_continuity_seed_successes +=
	source.continuity_seed_successes;
    statistics.pullback_continuity_seed_failures +=
	source.continuity_seed_failures;
    statistics.pullback_continuity_seed_finite_candidates +=
	source.continuity_seed_finite_candidates;
    statistics.pullback_continuity_seed_iterations +=
	source.continuity_seed_iterations;
    statistics.pullback_continuity_seed_line_searches +=
	source.continuity_seed_line_searches;
    statistics.pullback_maximum_continuity_seed_iterations = std::max(
	statistics.pullback_maximum_continuity_seed_iterations,
	source.maximum_continuity_seed_iterations);
    statistics.pullback_maximum_continuity_seed_line_searches = std::max(
	statistics.pullback_maximum_continuity_seed_line_searches,
	source.maximum_continuity_seed_line_searches);
    statistics.pullback_multiseed_fallbacks += source.multiseed_fallbacks;
    statistics.pullback_multiseed_successes += source.multiseed_successes;
    statistics.pullback_multiseed_failures += source.multiseed_failures;
    statistics.pullback_fallback_calls_with_finite_primary +=
	source.fallback_calls_with_finite_primary;
    statistics.pullback_fallback_samples_evaluated +=
	source.fallback_samples_evaluated;
    statistics.pullback_fallback_seed_refinements +=
	source.fallback_seed_refinements;
    statistics.pullback_fallback_refinement_improvements +=
	source.fallback_refinement_improvements;
    statistics.pullback_fallback_late_seed_improvements +=
	source.fallback_late_seed_improvements;
    statistics.pullback_maximum_winning_seed_index = std::max(
	statistics.pullback_maximum_winning_seed_index,
	source.maximum_winning_seed_index);
    statistics.pullback_subdivision_nodes += source.subdivision_nodes;
    statistics.pullback_maximum_subdivision_nodes = std::max(
	statistics.pullback_maximum_subdivision_nodes,
	source.maximum_subdivision_nodes);
    statistics.pullback_preparation_us += source.preparation_us;
    statistics.pullback_primary_search_us += source.primary_search_us;
    statistics.pullback_continuity_seed_us += source.continuity_seed_us;
    statistics.pullback_multiseed_us += source.multiseed_us;
    statistics.pullback_fallback_primary_improvement_total +=
	source.fallback_primary_improvement_total;
    statistics.pullback_fallback_primary_improvement_maximum = std::max(
	statistics.pullback_fallback_primary_improvement_maximum,
	source.fallback_primary_improvement_maximum);
    statistics.pullback_fallback_refinement_improvement_total +=
	source.fallback_refinement_improvement_total;
    statistics.pullback_fallback_refinement_improvement_maximum = std::max(
	statistics.pullback_fallback_refinement_improvement_maximum,
	source.fallback_refinement_improvement_maximum);
}


void
STEPWrapper::RecordDiagnostic(brlcad::step::DiagnosticSeverity severity, int64_t entity_id,
    const std::string &entity_type, const std::string &attribute, const std::string &message)
{
    if (thread_options_active && thread_option_wrapper == this &&
	    thread_options_speculative)
	return;
    if (thread_inference_wrapper == this && thread_inference_transaction) {
	std::lock_guard<std::mutex> transaction_guard(
	    thread_inference_transaction->mutex);
	for (std::vector<brlcad::step::Diagnostic>::iterator i =
		thread_inference_transaction->diagnostics.begin();
		i != thread_inference_transaction->diagnostics.end(); ++i) {
	    if (i->severity == severity && i->entity_id == entity_id &&
		    i->entity_type == entity_type && i->attribute == attribute &&
		    i->message == message) {
		++i->repeat_count;
		return;
	    }
	}
	brlcad::step::Diagnostic diagnostic;
	diagnostic.severity = severity;
	diagnostic.entity_id = entity_id;
	diagnostic.entity_type = entity_type;
	diagnostic.attribute = attribute;
	diagnostic.message = message;
	thread_inference_transaction->diagnostics.push_back(diagnostic);
	return;
    }
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    for (std::vector<brlcad::step::Diagnostic>::iterator i = diagnostics.begin(); i != diagnostics.end(); ++i) {
	if (i->severity == severity && i->entity_id == entity_id && i->entity_type == entity_type &&
	    i->attribute == attribute && i->message == message) {
	    ++i->repeat_count;
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.entity_id = entity_id;
    diagnostic.entity_type = entity_type;
    diagnostic.attribute = attribute;
    diagnostic.message = message;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}


void
STEPWrapper::RecordRepair(int64_t entity_id, const std::string &entity_type,
    const std::string &attribute, const std::string &message)
{
    if (thread_options_active && thread_option_wrapper == this &&
	    thread_options_speculative)
	return;
    if (thread_inference_wrapper == this && thread_inference_transaction) {
	std::lock_guard<std::mutex> transaction_guard(
	    thread_inference_transaction->mutex);
	for (std::vector<brlcad::step::Diagnostic>::iterator i =
		thread_inference_transaction->repairs.begin();
		i != thread_inference_transaction->repairs.end(); ++i) {
	    if (i->entity_id == entity_id && i->entity_type == entity_type &&
		    i->attribute == attribute && i->message == message) {
		++i->repeat_count;
		return;
	    }
	}
	brlcad::step::Diagnostic repair;
	repair.severity = brlcad::step::DiagnosticSeverity::Information;
	repair.entity_id = entity_id;
	repair.entity_type = entity_type;
	repair.attribute = attribute;
	repair.message = message;
	thread_inference_transaction->repairs.push_back(repair);
	return;
    }
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    ++statistics.repairs;
    for (std::vector<brlcad::step::Diagnostic>::iterator i = diagnostics.begin();
	 i != diagnostics.end(); ++i) {
	if (i->severity == brlcad::step::DiagnosticSeverity::Information &&
	    i->entity_id == entity_id && i->entity_type == entity_type &&
	    i->attribute == attribute && i->message == message) {
	    ++i->repeat_count;
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = brlcad::step::DiagnosticSeverity::Information;
    diagnostic.entity_id = entity_id;
    diagnostic.entity_type = entity_type;
    diagnostic.attribute = attribute;
    diagnostic.message = message;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}


void
STEPWrapper::RecordInferredCurve(int64_t edge_entity_id,
    const std::string &kind, double discrepancy_mm, double safe_limit_mm,
    double inference_limit_mm, double declared_tolerance_mm,
    const std::string &detail)
{
    if (edge_entity_id <= 0 || kind.empty())
	return;
    if (thread_inference_wrapper == this && thread_inference_transaction) {
	std::lock_guard<std::mutex> transaction_guard(
	    thread_inference_transaction->mutex);
	const std::string key = std::to_string(edge_entity_id) + "\n" + kind;
	std::map<std::string, brlcad::step::InferredCurve>::iterator existing =
	    thread_inference_transaction->curves.find(key);
	if (existing == thread_inference_transaction->curves.end()) {
	    brlcad::step::InferredCurve inference;
	    inference.edge_entity_id = edge_entity_id;
	    inference.kind = kind;
	    inference.discrepancy_mm = discrepancy_mm;
	    inference.safe_limit_mm = safe_limit_mm;
	    inference.inference_limit_mm = inference_limit_mm;
	    inference.declared_tolerance_mm = declared_tolerance_mm;
	    inference.detail = detail;
	    thread_inference_transaction->curves[key] = inference;
	    return;
	}
	existing->second.discrepancy_mm = std::max(
	    existing->second.discrepancy_mm, discrepancy_mm);
	if (safe_limit_mm > 0.0 &&
		(!(existing->second.safe_limit_mm > 0.0) ||
		 safe_limit_mm < existing->second.safe_limit_mm))
	    existing->second.safe_limit_mm = safe_limit_mm;
	if (inference_limit_mm > existing->second.inference_limit_mm)
	    existing->second.inference_limit_mm = inference_limit_mm;
	if (!existing->second.detail.empty() &&
		existing->second.detail.find(detail) == std::string::npos)
	    existing->second.detail += "; " + detail;
	else if (existing->second.detail.empty())
	    existing->second.detail = detail;
	return;
    }
    if (thread_options_active && thread_option_wrapper == this &&
	    thread_options_speculative)
	return;
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    const std::string key = std::to_string(edge_entity_id) + "\n" + kind;
    std::map<std::string, brlcad::step::InferredCurve>::iterator existing =
	inferred_curves.find(key);
    if (existing == inferred_curves.end()) {
	brlcad::step::InferredCurve inference;
	inference.edge_entity_id = edge_entity_id;
	inference.kind = kind;
	inference.discrepancy_mm = discrepancy_mm;
	inference.safe_limit_mm = safe_limit_mm;
	inference.inference_limit_mm = inference_limit_mm;
	inference.declared_tolerance_mm = declared_tolerance_mm;
	inference.detail = detail;
	inferred_curves[key] = inference;
	++statistics.inferred_curves;
	return;
    }
    existing->second.discrepancy_mm = std::max(
	existing->second.discrepancy_mm, discrepancy_mm);
    if (safe_limit_mm > 0.0 &&
	    (!(existing->second.safe_limit_mm > 0.0) ||
	     safe_limit_mm < existing->second.safe_limit_mm))
	existing->second.safe_limit_mm = safe_limit_mm;
    if (inference_limit_mm > existing->second.inference_limit_mm)
	existing->second.inference_limit_mm = inference_limit_mm;
    if (!existing->second.detail.empty() &&
	existing->second.detail.find(detail) == std::string::npos)
	existing->second.detail += "; " + detail;
    else if (existing->second.detail.empty())
	existing->second.detail = detail;
}


void
STEPWrapper::CommitCurveInferenceTransaction(
    const std::shared_ptr<CurveInferenceTransaction> &transaction)
{
    if (!transaction)
	return;
    std::vector<brlcad::step::InferredCurve> accepted;
    std::vector<brlcad::step::Diagnostic> accepted_diagnostics;
    std::vector<brlcad::step::Diagnostic> accepted_repairs;
    {
	std::lock_guard<std::mutex> guard(transaction->mutex);
	accepted_diagnostics = transaction->diagnostics;
	accepted_repairs = transaction->repairs;
	for (std::map<std::string, brlcad::step::InferredCurve>::const_iterator i =
		transaction->curves.begin(); i != transaction->curves.end(); ++i)
	    accepted.push_back(i->second);
    }

    /* A construction-time error on an inferred edge describes why the
     * ordinary candidate was rejected, not the disposition of the accepted
     * transaction.  Its measured contradiction is retained by the inference
     * warning and provenance below; do not leave a resolved error in an
     * otherwise complete report. */
    {
	std::set<int64_t> resolved_edges;
	for (std::vector<brlcad::step::InferredCurve>::const_iterator inference =
		accepted.begin(); inference != accepted.end(); ++inference)
	    resolved_edges.insert(inference->edge_entity_id);
	std::lock_guard<std::mutex> guard(diagnostic_mutex);
	diagnostics.erase(std::remove_if(diagnostics.begin(), diagnostics.end(),
	    [&resolved_edges](const brlcad::step::Diagnostic &diagnostic) {
		return diagnostic.severity ==
			brlcad::step::DiagnosticSeverity::Error &&
		    resolved_edges.find(diagnostic.entity_id) !=
			resolved_edges.end() &&
		    (diagnostic.attribute == "edge_geometry" ||
		     diagnostic.attribute == "edge_list" ||
		     diagnostic.attribute == "trim_pcurve");
	    }), diagnostics.end());
    }
    for (std::vector<brlcad::step::Diagnostic>::const_iterator diagnostic =
	    accepted_diagnostics.begin();
	    diagnostic != accepted_diagnostics.end(); ++diagnostic)
	for (uint64_t repeat = 0; repeat < diagnostic->repeat_count; ++repeat)
	    RecordDiagnostic(diagnostic->severity, diagnostic->entity_id,
		diagnostic->entity_type, diagnostic->attribute,
		diagnostic->message);
    for (std::vector<brlcad::step::Diagnostic>::const_iterator repair =
	    accepted_repairs.begin(); repair != accepted_repairs.end(); ++repair)
	for (uint64_t repeat = 0; repeat < repair->repeat_count; ++repeat)
	    RecordRepair(repair->entity_id, repair->entity_type,
		repair->attribute, repair->message);
    for (std::vector<brlcad::step::InferredCurve>::const_iterator inference =
	    accepted.begin(); inference != accepted.end(); ++inference) {
	RecordInferredCurve(inference->edge_entity_id, inference->kind,
	    inference->discrepancy_mm, inference->safe_limit_mm,
	    inference->inference_limit_mm,
	    inference->declared_tolerance_mm, inference->detail);
	std::ostringstream warning;
	warning << "accepted permissive " << inference->kind
	    << " inference only after the rebuilt complete BREP passed validation"
	    << "; source discrepancy=" << inference->discrepancy_mm
	    << " mm, safe limit=" << inference->safe_limit_mm
	    << " mm, inference limit=" << inference->inference_limit_mm << " mm";
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    inference->edge_entity_id, "EDGE_CURVE", "geometry_inference",
	    warning.str());
    }
}


std::vector<brlcad::step::InferredCurve>
STEPWrapper::InferredCurves() const
{
    std::vector<brlcad::step::InferredCurve> result;
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    for (std::map<std::string, brlcad::step::InferredCurve>::const_iterator i =
	    inferred_curves.begin(); i != inferred_curves.end(); ++i)
	result.push_back(i->second);
    return result;
}


std::vector<brlcad::step::InferredCurve>
STEPWrapper::InferredCurvesForEdges(
    const std::set<int64_t> &edge_entity_ids) const
{
    std::vector<brlcad::step::InferredCurve> result;
    if (edge_entity_ids.empty())
	return result;
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    for (std::map<std::string, brlcad::step::InferredCurve>::const_iterator i =
	    inferred_curves.begin(); i != inferred_curves.end(); ++i) {
	if (edge_entity_ids.find(i->second.edge_entity_id) !=
		edge_entity_ids.end())
	    result.push_back(i->second);
    }
    return result;
}


bool
STEPWrapper::HasInferredCurve(int64_t edge_entity_id,
    const std::string &kind) const
{
    if (edge_entity_id <= 0 || kind.empty())
	return false;
    if (thread_inference_wrapper == this && thread_inference_transaction) {
	std::lock_guard<std::mutex> transaction_guard(
	    thread_inference_transaction->mutex);
	const std::string transaction_key = std::to_string(edge_entity_id) +
	    "\n" + kind;
	/* An ordinary job must not inherit an inference accepted by an earlier
	 * geometry item which happens to reuse this STEP edge.  During a rebuild,
	 * only provenance created by this transaction can authorize the wider
	 * association. */
	return thread_curve_inference_enabled &&
	    thread_inference_transaction->curves.find(transaction_key) !=
		thread_inference_transaction->curves.end();
    }
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    const std::string key = std::to_string(edge_entity_id) + "\n" + kind;
    return inferred_curves.find(key) != inferred_curves.end();
}


void
STEPWrapper::collectEntityCounts()
{
    document.entity_counts.clear();
    document.unsupported_counts.clear();
    document.entity_counts_complete = true;
#ifdef HAVE_STEPCODE_LAZY
    lazy_complex_instance_ids.clear();
#endif
    statistics.input_instances = static_cast<uint64_t>(InstanceCount());
    const auto record_type = [this](std::string type) {
	std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
	    return static_cast<char>(std::toupper(c));
	});
	++document.entity_counts[type];
	if (type.find("ANNOTATION") != std::string::npos || type.find("CALLOUT") != std::string::npos ||
	    type.find("CAMERA") != std::string::npos || type.find("DRAUGHTING") != std::string::npos ||
	    type.find("KINEMATIC") != std::string::npos || type.find("LIGHT_SOURCE") != std::string::npos ||
	    type.find("SECURITY_CLASSIFICATION") != std::string::npos || type.find("APPROVAL") != std::string::npos ||
	    type.find("DOCUMENT") != std::string::npos || type.find("DATUM") != std::string::npos ||
	    type.find("SYMBOL") != std::string::npos || type == "COMPOSITE_TEXT" ||
	    type.compare(0, 5, "TEXT_") == 0 || type.find("TOLERANCE") != std::string::npos)
	    ++document.unsupported_counts[type];
    };
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	uint64_t completed = 0;
	for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	     id != lazy_instance_ids.end(); ++id) {
	    std::string type = lazy_session->TypeName(*id);
	    if (type.empty() || type == "COMPLEX_ENTITY") {
		lazy_complex_instance_ids.push_back(*id);
		const std::vector<std::string> components =
		    complex_entity_keywords(lazy_session->SourceRecord(*id));
		if (components.empty()) {
		    record_type("COMPLEX_ENTITY");
		} else {
		    for (std::vector<std::string>::const_iterator component =
			    components.begin(); component != components.end(); ++component)
			record_type(*component);
		}
	    } else {
		record_type(type);
	    }
	    ++completed;
	    if ((completed & 0x3fff) == 0)
		SetProgress("counting STEP entity types", completed,
		    statistics.input_instances, static_cast<int64_t>(*id));
	}
	SetProgress("counting STEP entity types", completed,
	    statistics.input_instances);
	return;
    }
#endif
    for (int i = 0; i < InstanceCount(); ++i) {
	SDAI_Application_instance *instance = InstanceAt(i);
	if (!instance || !instance->EntityName()) continue;
	record_type(instance->EntityName());
	if ((i & 0x3fff) == 0)
	    SetProgress("counting STEP entity types", static_cast<uint64_t>(i),
		statistics.input_instances, instance->STEPfile_id);
    }
	SetProgress("counting STEP entity types", statistics.input_instances,
	    statistics.input_instances);
}


double
STEPWrapper::deriveTolerance()
{
    if (import_options.absolute_tolerance_mm > 0.0)
	return import_options.absolute_tolerance_mm;

    double result = 0.0;
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	const std::vector<uint64_t> uncertainty_ids =
	    lazy_session->InstancesByType("GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT");
	for (std::vector<uint64_t>::const_iterator id = uncertainty_ids.begin();
	     id != uncertainty_ids.end(); ++id) {
	    SDAI_Application_instance *instance = activateLazyRoot(*id);
	    if (!instance) continue;
	    GlobalUncertaintyAssignedContext context(this, instance->STEPfile_id);
	    if (!context.Load(this, instance)) continue;
	    double candidate = context.GetLengthUncertainty();
	    if (candidate > 0.0 && (result <= 0.0 || candidate < result)) result = candidate;
	}
	releaseLazyBatches();
    } else
#endif
    for (int i = 0; i < InstanceCount(); ++i) {
	SDAI_Application_instance *instance = InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !IsSchemaEntity(instance, "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT")) continue;
	/* A representation context is commonly a complex entity that is also a
	 * geometric and unit context.  Factory selection intentionally chooses the
	 * geometric view for conversion, so materialize this uncertainty view
	 * explicitly instead of depending on complex-supertype dispatch order. */
	GlobalUncertaintyAssignedContext context(this, instance->STEPfile_id);
	if (!context.Load(this, instance)) continue;
	double candidate = context.GetLengthUncertainty();
	if (candidate > 0.0 && (result <= 0.0 || candidate < result)) result = candidate;
    }
    ClearEntityCache();

    // Until geometry has been detached there is no reliable model bounding box.
    // Use a millimetre-space numerical floor and retain the file's larger stated
    // uncertainty verbatim.
    const double numerical_floor = 1.0e-9;
    if (result <= 0.0) return 1.0e-6;
    return result > numerical_floor ? result : numerical_floor;
}

bool
STEPWrapper::BeginGeometryResult(int64_t entity_id)
{
    if (entity_id <= 0)
	return true;
    /* Result reporting is serialized with diagnostics, which are updated by
     * the same finalization path immediately after this claim succeeds. */
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    return geometry_results_recorded.insert(entity_id).second;
}





namespace {

int
coverage_precedence(brlcad::step::RepresentationCoverageStatus status)
{
    using brlcad::step::RepresentationCoverageStatus;
    switch (status) {
	case RepresentationCoverageStatus::Malformed: return 6;
	case RepresentationCoverageStatus::Unsupported: return 5;
	case RepresentationCoverageStatus::Skipped: return 4;
	case RepresentationCoverageStatus::PreservedInvalid: return 3;
	case RepresentationCoverageStatus::Filtered: return 2;
	case RepresentationCoverageStatus::Handled: return 1;
	case RepresentationCoverageStatus::IntentionallyNonGeometric: return 1;
	default: return 0;
    }
}

bool
coverage_omitted(brlcad::step::RepresentationCoverageStatus status)
{
    using brlcad::step::RepresentationCoverageStatus;
    return status == RepresentationCoverageStatus::Skipped ||
	status == RepresentationCoverageStatus::Malformed ||
	status == RepresentationCoverageStatus::Unsupported;
}

}


void
STEPWrapper::RecordRepresentationItemCoverage(int64_t entity_id,
    brlcad::step::RepresentationCoverageStatus status,
    const std::string &reason)
{
    for (std::map<int64_t, brlcad::step::RepresentationCoverage>::iterator coverage =
	    document.representation_coverage.begin();
	 coverage != document.representation_coverage.end(); ++coverage) {
	for (std::vector<brlcad::step::RepresentationItemCoverage>::iterator item =
		coverage->second.items.begin(); item != coverage->second.items.end(); ++item) {
	    if (item->entity_id != entity_id) continue;
	    if (item->status == brlcad::step::RepresentationCoverageStatus::Unclassified ||
		    coverage_precedence(status) > coverage_precedence(item->status)) {
		item->status = status;
		item->reason = reason;
	    } else if (item->reason.empty() && !reason.empty()) {
		item->reason = reason;
	    }
	}
    }
}


void
STEPWrapper::FinalizeRepresentationCoverage()
{
    using brlcad::step::RepresentationCoverageStatus;
    const std::set<int64_t> &selected = import_options.selected_entity_ids;
    for (std::map<int64_t, brlcad::step::RepresentationCoverage>::iterator entry =
	    document.representation_coverage.begin();
	 entry != document.representation_coverage.end(); ++entry) {
	brlcad::step::RepresentationCoverage &coverage = entry->second;
	coverage.handled_items = 0;
	coverage.preserved_invalid_items = 0;
	coverage.filtered_items = 0;
	coverage.intentionally_non_geometric_items = 0;
	coverage.omitted_items = 0;

	for (std::vector<brlcad::step::RepresentationItemCoverage>::iterator item =
		coverage.items.begin(); item != coverage.items.end(); ++item) {
	    if (item->status == RepresentationCoverageStatus::Unclassified) {
		if (!selected.empty() && selected.find(item->entity_id) == selected.end()) {
		    item->status = RepresentationCoverageStatus::Skipped;
		    item->reason = "outside the explicitly requested entity selection";
		} else {
		    item->status = RepresentationCoverageStatus::Unsupported;
		    item->reason = "no importer is registered for this product-bound "
			"representation item type";
		    ++statistics.geometry_attempted;
		    ++statistics.geometry_skipped;
		    RecordSkippedItem(item->entity_id, item->type, item->reason);
		    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
			item->entity_id, item->type, "representation.items", item->reason);
		}
	    }
	    if (item->status == RepresentationCoverageStatus::Handled)
		++coverage.handled_items;
	    else if (item->status == RepresentationCoverageStatus::PreservedInvalid)
		++coverage.preserved_invalid_items;
	    else if (item->status == RepresentationCoverageStatus::Filtered)
		++coverage.filtered_items;
	    else if (item->status == RepresentationCoverageStatus::IntentionallyNonGeometric)
		++coverage.intentionally_non_geometric_items;
	    else if (coverage_omitted(item->status))
		++coverage.omitted_items;
	}

	if (coverage.status == RepresentationCoverageStatus::Malformed) {
	    /* Preserve a representation-level structural error identified during
	     * zero-copy graph construction, even if some items were recoverable. */
	} else if (coverage.omitted_items) {
	    coverage.status = RepresentationCoverageStatus::Skipped;
	    for (std::vector<brlcad::step::RepresentationItemCoverage>::const_iterator item =
		    coverage.items.begin(); item != coverage.items.end(); ++item) {
		if (!coverage_omitted(item->status)) continue;
		coverage.status = item->status;
		coverage.reason = std::to_string(coverage.omitted_items) + " of " +
		    std::to_string(coverage.items.size()) +
		    " representation items omitted; #" +
		    std::to_string(item->entity_id) + " " + item->reason;
		break;
	    }
	} else if (coverage.preserved_invalid_items) {
	    coverage.status = RepresentationCoverageStatus::PreservedInvalid;
	    coverage.reason = std::to_string(coverage.preserved_invalid_items) +
		" of " + std::to_string(coverage.items.size()) +
		" representation items were preserved as unresolved invalid imports";
	} else if (coverage.filtered_items) {
	    coverage.status = RepresentationCoverageStatus::Filtered;
	    coverage.reason = std::to_string(coverage.filtered_items) + " of " +
		std::to_string(coverage.items.size()) +
		" representation items were excluded by explicit import policy";
	} else if (coverage.handled_items) {
	    coverage.status = RepresentationCoverageStatus::Handled;
	    coverage.reason = "all product geometry items were converted";
	} else {
	    coverage.status = RepresentationCoverageStatus::IntentionallyNonGeometric;
	    coverage.reason = coverage.items.empty() ?
		"representation contains no representation items" :
		"representation contains only placement or other non-geometric items";
	}
    }

    bool has_input_error = false;
    for (std::vector<brlcad::step::Diagnostic>::const_iterator diagnostic =
	    diagnostics.begin(); diagnostic != diagnostics.end(); ++diagnostic) {
	if (diagnostic->severity == brlcad::step::DiagnosticSeverity::Error ||
		diagnostic->severity == brlcad::step::DiagnosticSeverity::Fatal) {
	    has_input_error = true;
	    break;
	}
    }
    if (statistics.output_failures ||
	(import_options.strict &&
	 (statistics.geometry_skipped || statistics.invalid_breps_written ||
	  statistics.properties_invalid || statistics.pmi_invalid_records)))
	statistics.outcome = "failed";
    else if (!statistics.geometry_attempted && has_input_error)
	statistics.outcome = "failed";
    else if (!statistics.geometry_attempted)
	statistics.outcome = "empty";
    else if (!statistics.geometry_written && statistics.geometry_skipped)
	statistics.outcome = "failed";
    else if (statistics.geometry_skipped || statistics.invalid_breps_written ||
	statistics.properties_invalid || statistics.pmi_invalid_records)
	statistics.outcome = "partial";
    else
	statistics.outcome = "complete";
}


void
STEPWrapper::RecordSkippedItem(int64_t entity_id, const std::string &entity_type,
    const std::string &reason)
{
    std::lock_guard<std::mutex> lock(diagnostic_mutex);
    for (std::vector<brlcad::step::SkippedItem>::const_iterator item =
	    statistics.skipped_items.begin(); item !=
	    statistics.skipped_items.end(); ++item)
	if (item->entity_id == entity_id && item->entity_type == entity_type &&
		item->reason == reason)
	    return;
    if (statistics.skipped_items.size() >= kMaximumReportedSkippedItems) {
	++statistics.skipped_items_omitted;
	return;
    }
    brlcad::step::SkippedItem item;
    item.entity_id = entity_id;
    item.entity_type = entity_type;
    item.reason = reason;
    statistics.skipped_items.push_back(item);
}


bool
STEPWrapper::load(std::string &step_file)
{
    SetProgress("reading and indexing STEP file");
#ifdef HAVE_STEPCODE_LAZY
    stepfile = step_file;
    lazy_session.reset(new brlcad::step::STEPLazySession(
	brlcad::step::CurrentStepSchemaRuntime()));
    lazy_session->SetProgressCallback([this](const brlcad::step::STEPLazyProgress &progress) {
	SetProgress("reading and indexing STEP file", progress.offset,
	    progress.file_size, 0, progress.instances_scanned, "instances");
    });
    lazy_session->SetCancellationCallback([this]() {
	return CancellationRequested() || brlcad::PullbackWorkCancelled();
    });
    lazy_session->SetDiagnosticCallback([this](const brlcad::step::STEPLazyDiagnostic &source) {
	recordLazyDiagnostic(source);
    });
    if (!lazy_session->Open(stepfile)) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), "STEPcode could not lazily index the exchange file");
	lazy_session.reset();
	return false;
    }
    lazy_instance_ids = lazy_session->AllInstances();
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
    statistics.input_instances = cache.instances_scanned;
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
    if (cache.cancelled || lazy_instance_ids.size() != cache.instances_scanned) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), cache.cancelled ? "STEP lazy scan was cancelled" :
	    "STEP lazy index did not retain every DATA instance");
	return false;
    }
    SetProgress("STEP index complete", statistics.input_instances,
	statistics.input_instances, 0, statistics.input_instances, "instances");
    const std::chrono::steady_clock::time_point inventory_started =
	std::chrono::steady_clock::now();
    collectEntityCounts();
    statistics.inventory_time_us = static_cast<int64_t>(
	std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - inventory_started).count());
    SetProgress("STEP inventory complete", statistics.input_instances,
	statistics.input_instances, 0, document.entity_counts.size(), "entity types");
    return true;
#else
    registry = new Registry(brlcad::step::CurrentStepSchemaRuntime().Initializer());
    sfile = new STEPfile(*registry, *instance_list);

    stepfile = step_file;
    try {
	/* load STEP file */
	Severity severity = sfile->ReadExchangeFile(stepfile.c_str());
	statistics.input_instances = static_cast<uint64_t>(instance_list->InstanceCount());
	if (severity < SEVERITY_WARNING) {
	    std::string message = sfile->Error().UserMsg();
	    if (message.empty()) message = sfile->Error().DetailMsg();
	    if (message.empty()) message = "STEPcode rejected the exchange file";
	    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
		std::string(), message);
	    return false;
	}
	if (severity == SEVERITY_WARNING) {
	    std::string message = sfile->Error().UserMsg();
	    if (message.empty()) message = "STEPcode reported recoverable input errors";
	    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0, "PART21_FILE",
		std::string(), message);
	}

    } catch (std::exception &e) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), e.what());
	return false;
    }
    SetProgress("STEP read complete", statistics.input_instances,
	statistics.input_instances, 0, statistics.input_instances, "instances");
    const std::chrono::steady_clock::time_point inventory_started =
	std::chrono::steady_clock::now();
    collectEntityCounts();
    statistics.inventory_time_us = static_cast<int64_t>(
	std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - inventory_started).count());
    SetProgress("STEP inventory complete", statistics.input_instances,
	statistics.input_instances, 0, document.entity_counts.size(), "entity types");
    return true;
#endif

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
