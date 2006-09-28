#!/bin/sh
#
# Retrieve stats from gforge
#
psql -U forge -h $HOST forge << EOF
select project_name,summary,priority,percent_complete from 
project_group_list l,project_task t where l.group_id=10 and 
l.group_project_id=t.group_project_id
order by project_name,priority asc,percent_complete desc;
\q
EOF
