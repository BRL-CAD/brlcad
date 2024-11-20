#!/bin/bash

# This script is intended to make full backup copies of all
# Git repositories in the GitHub BRL-CAD project.  This includes
# all archival repositories and all dependencies, so it will be
# a large amount of data!

# This script is inspired by the following gist:
# https://gist.github.com/caniszczyk/3856584?permalink_comment_id=3711733#gistcomment-3711733
#
#
#
# Note that once all these repositories are copied, in order to
# do a completely local clone from them some git magic is needed.
# Still working out the best way to handle this, but the first command
# I've actually gotten to do a local submodule checkout with internet
# disabled is:
#
# git -c url."file:///home/user/backup/BRL-CAD/".insteadOf="https://github.com/BRL-CAD/" -c protocol.file.allow=always submodule update --init astyle/
#
# hints from:
# https://lists.archlinux.org/archives/list/arch-dev-public@lists.archlinux.org/thread/YYY6KN2BJH7KR722GF26SEWNXPLAANNQ/
# https://stackoverflow.com/a/51920532
#
# Presumably if we were to move to a new hosting solution, the above would work
# with URLs instead of file:// once we uploaded the repositories.  (The
# protocol.file.allow option would presumably not be needed in that case.  It
# is added here because from the local filesystem we get a fatal: transport
# 'file' not allowed error without it.)



# Make the url to the input github organization's repository page.  Note that we can only
# get up to 100 per page, so we need multiple requests.
CURR_PAGE=1
ALL_REPOS=""

# Most of the time curl should work, but if a dev hits a query limit for
# anonymous requests (can happen, ran into working on this) they can comment
# out the curl lines, uncomment the gh versions and authenticate. See
# https://docs.github.com/en/rest/using-the-rest-api/getting-started-with-the-rest-api

# Initialize list with 10 entries (curl):
ROOT_URL="https://api.github.com/orgs/BRL-CAD/repos?per_page=10&page="
ADD_REPOS=$(curl -s ${ROOT_URL}$CURR_PAGE | grep html_url | awk 'NR%2 == 0' | cut -d ':' -f 2-3 | tr -d '",');

# Initialize list with 10 entries (gh):
#API_ROOT_URL="orgs/BRL-CAD/repos?per_page=10&page="
#ADD_REPOS=$(gh api ${API_ROOT_URL}$CURR_PAGE | jq | grep html_url | awk 'NR%2 == 0' | cut -d ':' -f 2-3 | tr -d '",');

# Stash the seed repo set in ALL_REPOS
ALL_REPOS+=$ADD_REPOS

# As long as we are finding content, pull down any additional pages with
# repositories.  Keep going until we have everything
while [ ! -z "${ADD_REPOS}" ]
do
	((CURR_PAGE++))
	# Append repos (curl)
	ADD_REPOS=$(curl -s ${ROOT_URL}$CURR_PAGE | grep html_url | awk 'NR%2 == 0' | cut -d ':' -f 2-3 | tr -d '",');
	# Append repos (gh)
	#ADD_REPOS=$(gh api --method GET ${API_ROOT_URL}$CURR_PAGE | jq | grep html_url | awk 'NR%2 == 0' | cut -d ':' -f 2-3 | tr -d '",');
	ALL_REPOS+=$ADD_REPOS
done

# Report how many were found.
repo_cnt=0
for ORG_REPO in ${ALL_REPOS}; do
	repo_cnt=$((repo_cnt + 1))
done
echo "Found $repo_cnt repositories"

# Clone all the repositories, or update them if they've been cloned previously.
# Specify --mirror so all upstream data (tags, etc.) is also preserved
for ORG_REPO in ${ALL_REPOS}; do
	dirname=${ORG_REPO/https:\/\/github.com\/BRL-CAD\//}
	if [ -e ./${dirname}.git ]; then
		echo "Updating ${dirname}.git"
		# https://stackoverflow.com/a/6151419/2037687
		cd ${dirname}.git && git remote update --prune && cd ..
	else
		echo "Mirror cloning ${ORG_REPO}.git"
		git clone --mirror ${ORG_REPO}.git;
	fi
done

