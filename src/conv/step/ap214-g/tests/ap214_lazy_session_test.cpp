/*          A P 2 1 4 _ L A Z Y _ S E S S I O N _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPLazySession.h"

#include <clstepcore/instmgr.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void
expect(bool condition, const std::string &message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

} // namespace

int
main(int argc, char **argv)
{
    if (argc != 4) {
	std::cerr << "usage: ap214_lazy_session_test input.stp diagnostics.stp legacy-contexts.stp\n";
	return 2;
    }

    brlcad::step::STEPLazySession session;
    uint64_t progress_events = 0;
    session.SetProgressCallback([&progress_events](const brlcad::step::STEPLazyProgress &) {
	++progress_events;
    });
    expect(session.Open(argv[1]), "lazy AP214 scan succeeds");

    const brlcad::step::STEPLazyStatistics scanned = session.Statistics();
    expect(scanned.instances_scanned > 0, "scan statistics count instances");
    expect(scanned.instances_loaded == 0, "scan does not materialize DATA instances");
    expect(scanned.data_sections == 1, "fixture contains one DATA section");
    expect(progress_events > 0, "scan progress callback is invoked");
    expect(session.AllInstances().size() == scanned.instances_scanned,
	"all-instance cursor covers the complete lazy index");

    const std::vector<uint64_t> products = session.InstancesByType("product");
    const std::vector<uint64_t> solids = session.InstancesByType("MANIFOLD_SOLID_BREP");
    expect(products.size() == 2, "type index finds both fixture products");
    expect(solids.size() == 1, "type index finds the fixture solid");
    if (!products.empty()) {
	const std::vector<uint64_t> references = session.ForwardReferences(products[0]);
	expect(!references.empty(), "forward references are indexed without materialization");

	{
	    brlcad::step::STEPLazyBatch first = session.LoadBatch(products[0]);
	    expect(first.Valid(), "first product dependency batch is valid");
	    expect(first.Get(products[0]) != NULL, "batch exposes its materialized root");
	    const brlcad::step::STEPLazyStatistics first_stats = session.Statistics();
	    expect(first_stats.instances_loaded == first.Instances().size(),
		"loaded count matches one batch closure");
	    expect(first_stats.resident_source_bytes > 0,
		"materialized batch reports its resident source-record bytes");
	    expect(first_stats.source_bytes_high_water >= first_stats.resident_source_bytes,
		"source-byte high-water mark covers the current cache footprint");
	    InstMgrBase *reference_manager = session.ReferenceManager();
	    MgrNodeBase *root_node = reference_manager ?
		reference_manager->FindFileId(static_cast<int>(products[0])) : NULL;
	    expect(root_node && root_node->GetSTEPentity() == first.Get(products[0]),
		"SDAI reference adapter resolves a batch-owned cache hit");
	    {
		brlcad::step::STEPLazyBatch second = session.LoadBatch(products[0]);
		expect(second.Valid(), "overlapping dependency batch is valid");
		first.Release();
		expect(session.Statistics().instances_loaded == second.Instances().size(),
		    "releasing one batch preserves dependencies pinned by another");
	    }
	}
	expect(session.Statistics().instances_loaded == 0,
	    "releasing all product batches returns to the scan-only cache state");
	expect(session.Statistics().resident_source_bytes == 0,
	    "releasing all product batches clears resident source-record bytes");
    }

    if (!solids.empty()) {
	for (int cycle = 0; cycle < 3; ++cycle) {
	    {
		brlcad::step::STEPLazyBatch batch = session.LoadBatch(solids[0]);
		expect(batch.Valid() && batch.Get(solids[0]) != NULL,
		    "solid dependency batch materializes on repeated cycle");
	    }
	    expect(session.Statistics().instances_loaded == 0,
		"solid dependency batch evicts cleanly after release");
	}
    }

    const brlcad::step::STEPLazyStatistics final_stats = session.Statistics();
    expect(final_stats.active_batches == 0, "no lazy batches remain active");
    expect(final_stats.instances_pinned == 0, "no lazy instances remain pinned");
    expect(final_stats.evictions > 0, "batch release records cache evictions");
    expect(final_stats.resident_source_bytes == 0,
	"no materialized source-record bytes remain resident");
    expect(final_stats.source_bytes_high_water > 0,
	"cache statistics retain a nonzero source-byte high-water mark");

    brlcad::step::STEPLazySession diagnostic_session;
    uint64_t diagnostic_events = 0;
    diagnostic_session.SetDiagnosticCallback(
	[&diagnostic_events](const brlcad::step::STEPLazyDiagnostic &) {
	    ++diagnostic_events;
	});
    expect(diagnostic_session.Open(argv[2]), "malformed-reference fixture is indexed");
    const std::vector<brlcad::step::STEPLazyDiagnostic> &diagnostics =
	diagnostic_session.Diagnostics();
    expect(diagnostic_events == 1, "duplicate diagnostic callbacks remain bounded");
    expect(diagnostics.size() == 1, "duplicate diagnostics retain one stored record");
    if (!diagnostics.empty()) {
	expect(diagnostics[0].entity_id == 1 && diagnostics[0].file_offset > 0,
	    "structured diagnostic retains entity and source offset");
	expect(diagnostics[0].occurrences == 2,
	    "bounded diagnostic retains its exact repetition count");
    }

    brlcad::step::STEPLazySession legacy_context_session;
    expect(legacy_context_session.Open(argv[3]), "legacy AP203 context names in AP214 are indexed");
    {
	brlcad::step::STEPLazyBatch contexts =
	    legacy_context_session.LoadBatch(std::vector<uint64_t>{2, 3});
	expect(contexts.Valid(), "legacy context dependency batch is valid");
	expect(contexts.Get(2) != NULL, "DESIGN_CONTEXT materializes as its AP214 parent");
	expect(contexts.Get(3) != NULL, "MECHANICAL_CONTEXT materializes as its AP214 parent");
    }
    expect(legacy_context_session.Diagnostics().empty(),
	"legacy context aliases do not produce materialization errors");

    brlcad::step::STEPLazySession cancelled_session;
    cancelled_session.SetCancellationCallback([]() { return true; });
    expect(cancelled_session.Open(argv[1]),
	"cancelled scan leaves an observable partial index");
    const brlcad::step::STEPLazyStatistics cancelled_stats =
	cancelled_session.Statistics();
    expect(cancelled_stats.cancelled, "cancellation callback stops the lazy scan");
    expect(cancelled_stats.instances_loaded == 0 &&
	cancelled_stats.instances_pinned == 0 &&
	cancelled_stats.active_batches == 0,
	"cancelled scan retains no materialized or pinned instances");

    if (failures)
	std::cerr << failures << " AP214 lazy session test(s) failed\n";
    return failures ? 1 : 0;
}
