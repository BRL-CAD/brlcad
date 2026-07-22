/*                   S T E P W R A P P E R . H
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
/** @file step/STEPWrapper.h
 *
 * Class definition for C++ wrapper to NIST STEP parser/database
 * functions.
 *
 */

#ifndef CONV_STEP_STEPWRAPPER_H
#define CONV_STEP_STEPWRAPPER_H

#include "common.h"

#ifdef DEBUG
#define REPORT_ERROR(arg) std::cerr << __FILE__ << ":" << __LINE__ << ":" << arg << std::endl
#else
#define REPORT_ERROR(arg)
#endif

/* system headers */
#include <chrono>
#include <list>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/* interface headers */
#include <sdai.h>

#include <STEPattribute.h>
#include <STEPcomplex.h>
#include <STEPfile.h>

#include <BRLCADWrapper.h>

#include "ap_schema.h"
#include "STEPDocument.h"

/*
class SDAI_Application_instance;
class SDAI_Select;
class STEPcomplex;
*/
class CartesianPoint;
class SurfacePatch;
class STEPEntity;

/** Owns a fully materialized conversion graph after its SDAI batch is gone. */
class STEPDetachedEntityArena
{
private:
    std::map<int, STEPEntity *> objects;
    std::list<STEPEntity *> unmapped_objects;

    friend class STEPWrapper;

public:
    STEPDetachedEntityArena();
    ~STEPDetachedEntityArena();
    STEPDetachedEntityArena(const STEPDetachedEntityArena &) = delete;
    STEPDetachedEntityArena &operator=(const STEPDetachedEntityArena &) = delete;

    STEPEntity *FindObject(int id) const;
    void ResetOpenNURBSState();
};

namespace brlcad {
struct PullbackStatistics;
#ifdef HAVE_STEPCODE_LAZY
namespace step {
struct STEPLazyDiagnostic;
class STEPLazyBatch;
class STEPLazySession;
}
#endif
}

typedef std::list<CartesianPoint *> LIST_OF_POINTS;
typedef std::list<LIST_OF_POINTS *> LIST_OF_LIST_OF_POINTS;
typedef std::list<SurfacePatch *> LIST_OF_PATCHES;
typedef std::list<LIST_OF_PATCHES *> LIST_OF_LIST_OF_PATCHES;
typedef std::list<std::string> LIST_OF_STRINGS;
typedef std::list<SDAI_Application_instance *> LIST_OF_ENTITIES;
typedef std::list<SDAI_Select *> LIST_OF_SELECTS;
typedef std::map<std::string, STEPcomplex *> MAP_OF_SUPERTYPES;
typedef std::map<std::string, int> MAP_OF_PRODUCT_NAME_TO_ENTITY_ID;
typedef std::map<int, std::string> MAP_OF_ENTITY_ID_TO_PRODUCT_NAME;
typedef std::map<int, int> MAP_OF_ENTITY_ID_TO_PRODUCT_ID;
typedef std::vector<double> VECTOR_OF_REALS;
typedef std::list<int> LIST_OF_INTEGERS;
typedef std::list<double> LIST_OF_REALS;
typedef std::list<LIST_OF_REALS *> LIST_OF_LIST_OF_REALS;

#define STEP_LOADED 1
#define STEP_LOAD_ERROR 2

class STEPWrapper
{
private:
    struct GeometryExecutor;
    std::string stepfile;
    std::string dotgfile;
    InstMgr *instance_list;
    Registry  *registry;
    STEPfile  *sfile;
    BRLCADWrapper *dotg;
    bool verbose;
    std::map<int, STEPEntity *> entity_objects;
    std::list<STEPEntity *> unmapped_objects;
    brlcad::step::ImportOptions import_options;
    brlcad::step::Document document;
    brlcad::step::ImportStatistics statistics;
    std::vector<brlcad::step::Diagnostic> diagnostics;
    mutable std::mutex diagnostic_mutex;
    brlcad::step::ImportProgress progress_state;
    struct ActiveGeometryJobProgress {
	std::chrono::steady_clock::time_point started;
	int64_t root_entity_id = 0;
	int64_t current_entity_id = 0;
	std::string entity_type;
	std::string phase;
	int64_t item_entity_id = 0;
	uint64_t item_completed = 0;
	uint64_t item_total = 0;
	std::string item_label;
	uint64_t secondary_completed = 0;
	uint64_t secondary_total = 0;
	std::string secondary_label;
	std::string detail;
    };
    std::map<std::thread::id, ActiveGeometryJobProgress>
	active_geometry_job_progress;
    mutable std::mutex progress_mutex;
    mutable std::mutex telemetry_mutex;
    std::function<bool()> cancellation_callback;
    std::unique_ptr<GeometryExecutor> geometry_executor;
#ifdef HAVE_STEPCODE_LAZY
    std::unique_ptr<brlcad::step::STEPLazySession> lazy_session;
    std::unique_ptr<brlcad::step::STEPLazyBatch> lazy_batch;
    std::vector<std::unique_ptr<brlcad::step::STEPLazyBatch> > lazy_supplemental_batches;
    std::vector<uint64_t> lazy_instance_ids;
    std::vector<uint64_t> lazy_iteration_ids;
    bool lazy_filter_active = false;
#endif

    void printEntity(SDAI_Application_instance *se, int level);
    void printEntityAggregate(STEPaggregate *sa, int level);
    const char *getBaseType(int type);
    double deriveTolerance();
    void collectEntityCounts();
    InstMgrBase *referenceManager() const;
#ifdef HAVE_STEPCODE_LAZY
    SDAI_Application_instance *activateLazyRoot(uint64_t id);
    void releaseLazyBatches();
    void recordLazyDiagnostic(const brlcad::step::STEPLazyDiagnostic &source);
    void synchronizeLazyDiagnostics();
#endif

public:
    STEPWrapper();
    virtual ~STEPWrapper();

    bool convert(BRLCADWrapper *dotg);

    void SetImportOptions(const brlcad::step::ImportOptions &options) { import_options = options; }
    const brlcad::step::ImportOptions &ImportOptions() const { return import_options; }
    const brlcad::step::Document &Document() const { return document; }
    brlcad::step::Document &Document() { return document; }
    const brlcad::step::ImportStatistics &Statistics() const { return statistics; }
    brlcad::step::ImportStatistics &Statistics() { return statistics; }
    const std::vector<brlcad::step::Diagnostic> &Diagnostics() const { return diagnostics; }
    void RecordDiagnostic(brlcad::step::DiagnosticSeverity severity, int64_t entity_id,
	const std::string &entity_type, const std::string &attribute, const std::string &message);
    void RecordRepair(int64_t entity_id, const std::string &entity_type,
	const std::string &attribute, const std::string &message);
    /** Preserve an entity-specific geometry failure for structured reports.
     * This does not update geometry_skipped; callers own the coverage count. */
    void RecordSkippedItem(int64_t entity_id, const std::string &entity_type,
	const std::string &reason);
    void SetCancellationCallback(const std::function<bool()> &callback) { cancellation_callback = callback; }
    bool CancellationRequested() const { return cancellation_callback && cancellation_callback(); }
    void SetProgress(const std::string &phase, uint64_t completed = 0,
	uint64_t total = 0, int64_t current_entity_id = 0,
	uint64_t secondary_completed = 0,
	const std::string &secondary_label = std::string(),
	const std::string &detail = std::string());
    /** Update the active worker/entity detail without discarding the outer
     * completed/total item count. */
    void SetProgressDetail(const std::string &phase, int64_t current_entity_id,
	uint64_t secondary_completed = 0, uint64_t secondary_total = 0,
	const std::string &secondary_label = std::string(),
	const std::string &detail = std::string());
    /** Publish a consistent snapshot of the bounded detached-geometry
     * scheduler.  A zero capacity clears scheduler-specific telemetry. */
    void SetGeometrySchedulerProgress(uint64_t queued, uint64_t active,
	uint64_t ready, uint64_t spooled, uint64_t finished,
	uint64_t materializing, uint64_t in_flight,
	uint64_t runnable_capacity, uint64_t ready_bytes,
	uint64_t ready_byte_budget);
    void SetGeometryOverallProgress(uint64_t processed, uint64_t total);
    /** Configure a persistent helper pool shared by all exact-geometry jobs.
     * Top-level solid jobs count against the same capacity, so nested edge
     * work can only occupy otherwise-idle -j slots. */
    void ConfigureGeometryExecutor(unsigned int concurrency);
    void StopGeometryExecutor();
    void GeometryWorkerStarted(int64_t entity_id,
        const std::string &entity_type, bool exclusive_pullback = false);
    void GeometryWorkerFinished(bool exclusive_pullback = false);
    void ParallelForGeometry(size_t count,
	const std::function<void(size_t)> &task);
    /** Snapshot helper slots not currently occupied by top-level solids or
     * nested geometry work.  Large face batching uses this to avoid retaining
     * standalone face BREPs when the top-level queue already saturates -j. */
    unsigned int AvailableGeometryHelperCapacity();
    void SetGeometryHelpersActive(uint64_t active);
    brlcad::step::ImportProgress Progress() const;
    void RecordStageTiming(const std::string &stage, int64_t entity_id,
	const std::string &entity_type, uint64_t elapsed_us,
	uint64_t faces = 0, uint64_t edges = 0, uint64_t trims = 0);
    void RecordPullbackStatistics(const brlcad::PullbackStatistics &source);
    /** Return true for an unfiltered entity or a requested representation
     * item root, recording requested roots encountered during conversion. */
    bool ShouldConvertEntity(int64_t entity_id);

    STEPEntity *FindObject(int id) const;
    void AddObject(STEPEntity *object);
    void ClearEntityCache();
    std::unique_ptr<STEPDetachedEntityArena> DetachEntityCache();
    /** Release materialized STEP entities after conversion data is detached. */
    void ReleaseSourceData();
    void ResetOpenNURBSState();
    int InstanceCount() const;
    SDAI_Application_instance *InstanceAt(int index);
    void SetInstanceTypes(const std::vector<std::string> &types,
	const std::vector<uint64_t> &excluded_ids = std::vector<uint64_t>());
    /** Restrict lazy iteration to an explicit, file-order-stable ID set.
     * Eager sessions retain their existing full-instance iteration. */
    void SetInstanceIds(const std::vector<uint64_t> &ids);
    void ResetInstanceTypes();

    /** Read-only access to the lazy Part 21 index.  These calls never
     * materialize an SDAI instance and return empty results for eager
     * sessions and schemas without lazy support. */
    bool HasLazyIndex() const;
    std::vector<uint64_t> LazyInstancesByType(const std::string &type) const;
    std::string LazyTypeName(uint64_t id) const;
    std::string LazySourceRecord(uint64_t id) const;
    std::vector<uint64_t> LazyForwardReferences(uint64_t id) const;
    std::vector<uint64_t> LazyReverseReferences(uint64_t id) const;

    std::map<int,int> entity_status;
    char *summary_log_file;
    int dry_run;

    SDAI_Application_instance *getEntity(int STEPid);
    SDAI_Application_instance *getEntity(int STEPid, const char *name);
    SDAI_Application_instance *getEntity(SDAI_Application_instance *, const char *name);
    std::string getLogicalString(Logical v);
    std::string getBooleanString(Boolean v);

    // helper functions based on STEP id
    STEPattribute *getAttribute(int STEPid, const char *name);
    LIST_OF_STRINGS *getAttributes(int STEPid);
    Boolean getBooleanAttribute(int STEPid, const char *name);
    SDAI_Application_instance *getEntityAttribute(int STEPid, const char *name);
    int getEnumAttribute(int STEPid, const char *name);
    int getIntegerAttribute(int STEPid, const char *name);
    Logical getLogicalAttribute(int STEPid, const char *name);
    double getRealAttribute(int STEPid, const char *name);
    LIST_OF_ENTITIES *getListOfEntities(int STEPid, const char *name);
    LIST_OF_LIST_OF_POINTS *getListOfListOfPoints(int STEPid, const char *attrName);
    MAP_OF_SUPERTYPES *getMapOfSuperTypes(int STEPid);
    void getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m);
    SDAI_Application_instance *getSuperType(int STEPid, const char *name);
    std::string getStringAttribute(int STEPid, const char *name);

    // helper functions based on entity instance pointer
    STEPattribute *getAttribute(SDAI_Application_instance *sse, const char *name);
    LIST_OF_STRINGS *getAttributes(SDAI_Application_instance *sse);
    Boolean getBooleanAttribute(SDAI_Application_instance *sse, const char *name);
    SDAI_Application_instance *getEntityAttribute(SDAI_Application_instance *sse, const char *name);
    SDAI_Select *getSelectAttribute(SDAI_Application_instance *sse, const char *name);
    int getEnumAttribute(SDAI_Application_instance *sse, const char *name);
    int getIntegerAttribute(SDAI_Application_instance *sse, const char *name);
    Logical getLogicalAttribute(SDAI_Application_instance *sse, const char *name);
    double getRealAttribute(SDAI_Application_instance *sse, const char *name);
    LIST_OF_ENTITIES *getListOfEntities(SDAI_Application_instance *sse, const char *name);
    LIST_OF_SELECTS *getListOfSelects(SDAI_Application_instance *sse, const char *name);
    LIST_OF_LIST_OF_PATCHES *getListOfListOfPatches(SDAI_Application_instance *sse, const char *attrName);
    LIST_OF_LIST_OF_POINTS *getListOfListOfPoints(SDAI_Application_instance *sse, const char *attrName);
    MAP_OF_SUPERTYPES *getMapOfSuperTypes(SDAI_Application_instance *sse);
    void getSuperTypes(SDAI_Application_instance *sse, MAP_OF_SUPERTYPES &m);
    SDAI_Application_instance *getSuperType(SDAI_Application_instance *sse, const char *name);
    std::string getStringAttribute(SDAI_Application_instance *sse, const char *name);

    bool load(std::string &step_file);
    LIST_OF_PATCHES *parseListOfPatchEntities(const char *in);
    LIST_OF_REALS *parseListOfReals(const char *in);
    LIST_OF_POINTS *parseListOfPointEntities(const char *in);
    void printLoadStatistics();

    bool Verbose() const
    {
	return verbose;
    }

    const std::string &SourceFile() const { return stepfile; }
    const char *SchemaName() const
    {
#if defined(AP214e3)
	return "AP214";
#elif defined(AP203e2)
	return "AP203e2";
#elif defined(AP242)
	return "AP242";
#else
	return "AP203";
#endif
    }

    void Verbose(bool value)
    {
	this->verbose = value;
    }
};

#endif /* CONV_STEP_STEPWRAPPER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
