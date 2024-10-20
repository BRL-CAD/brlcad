#!/bin/bash

# This script is intended to make full backup copies of all
# Git repositories in the GitHub BRL-CAD project.  This includes
# all archival repositories and all dependencies, so it will be
# a large amount of data!

# https://gist.github.com/caniszczyk/3856584?permalink_comment_id=3711733#gistcomment-3711733

# Make the url to the input github organization's repository page.
ORG_URL="https://api.github.com/orgs/BRL-CAD/repos?per_page=200";

# List of all repositories of that organization (separated by newline-eol).
ALL_REPOS=$(curl -s ${ORG_URL} | grep html_url | awk 'NR%2 == 0' | cut -d ':' -f 2-3 | tr -d '",');

# Clone all the repositories.  Specify --mirror so all upstream data
# (tags, etc.) is also preserved
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

