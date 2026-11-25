/*          D B _ H I S T O R Y _ R E T E N T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef LIBRT_DB_HISTORY_RETENTION_H
#define LIBRT_DB_HISTORY_RETENTION_H

#include "db_history_db5.h"
#include "db_history_workspace.h"

namespace db_history {

struct RetainedWorkspace {
    Db5HistoryDraft archive;
    std::vector<ArtifactKey> expired;
    WorkspacePrunePreview preview;
};

// Preserve revision IDs and the selected model. Verify the candidate
// archive before returning it to the workspace for publication.
bool plan_retention(const Pack &source, const WorkspaceHead &head, const WorkspaceRetention &policy,
		    const std::set<Id> &pins, RetainedWorkspace &result, struct bu_vls *message);

} // namespace db_history

#endif /* LIBRT_DB_HISTORY_RETENTION_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
