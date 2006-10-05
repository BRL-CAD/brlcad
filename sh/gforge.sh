#!/bin/sh
#
# Retrieve stats from gforge
#

if test "x$PROJECT" = "x" ; then
    PROJECT="BRL-CAD"
fi
if test "x$HOST" = "x" ; then
    HOST=localhost
fi

psql -U forge -h $HOST forge << EOF | sed 's/None//g'
SELECT project_name AS project, summary, priority, percent_complete, user_name AS assigned_to
FROM groups g, project_group_list pgl, project_task pt, project_assigned_to pat, users u
WHERE g.group_name = '$PROJECT'
AND pgl.group_id = g.group_id
AND pgl.group_project_id = pt.group_project_id
AND pt.project_task_id = pat.project_task_id
AND pat.assigned_to_id = u.user_id
ORDER BY project_name, priority ASC, percent_complete DESC, summary
EOF

