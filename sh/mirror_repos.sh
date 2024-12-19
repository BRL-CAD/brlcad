#!/bin/bash

# This script is intended to make full backup copies of all
# Git repositories in the GitHub BRL-CAD project.  This includes
# all archival repositories and all dependencies, so it will be
# a large amount of data!

# This script is inspired by the following gist:
# https://gist.github.com/caniszczyk/3856584?permalink_comment_id=3711733#gistcomment-3711733


# Note that once all these repositories are copied, in order to do a completely
# local clone from them some git magic is needed.  Thanks to hints from the
# following sites, we have prepared instructions on how to do this below:
#
# https://lists.archlinux.org/archives/list/arch-dev-public@lists.archlinux.org/thread/YYY6KN2BJH7KR722GF26SEWNXPLAANNQ/
# https://stackoverflow.com/a/51920532
#
#
# Once a full mirror has been downloaded, as a quick test one can clone bext
# and use command line options to verify the repos are behaving as expected.
# (If github.com is still up and you have an active internet connection, you'll
# probably want to turn networking off for this test to avoid git transparently
# pulling from github.com by accident if you make a mistake with the settings.)
#
#
# Let's say the local mirror of BRL-CAD's git repositories is located in:
#
# /home/user/github/BRL-CAD/
#
# As a first step, we clone the local bext repository into /home/user/github/bext:
#
# github$ git clone /home/user/github/BRL-CAD/bext.git
# Cloning into 'bext'...
# done.
# github$ cd bext
# bext (main)$
#
# From there, we can update submodules using the local copies rather than the
# github.com versions by specifying config values on the command line.  To pick
# a small example to start with, we will try initializing the astyle submodule.
# As a first cut, let's try using insteadOf to remap the URL from github.com to
# our local filesystem mirror:
#
# bext (main)$ git -c url."file:///home/user/github/".insteadOf="https://github.com/" submodule update --init astyle/astyle
# Cloning into '/home/user/github/bext/astyle/astyle'...
# fatal: transport 'file' not allowed
# fatal: clone of 'https://github.com/BRL-CAD/astyle' into submodule path '/home/user/github/bext/astyle/astyle' failed
# Failed to clone 'astyle/astyle'. Retry scheduled
# Cloning into '/home/user/github/bext/astyle/astyle'...
# fatal: transport 'file' not allowed
# fatal: clone of 'https://github.com/BRL-CAD/astyle' into submodule path '/home/user/github/bext/astyle/astyle' failed
# Failed to clone 'astyle/astyle' a second time, aborting
#
# Whoops!  Well, we got an error, but not because of the URL remapping itself -
# rather, the file:// protocol is disabled by default.  Thus, git tried to do
# the local clone from the right location but failed.  To allow the operation
# to succeed, we need to add a second configure setting that instructs git to
# allow file:// URLs:
#
# bext (main)$ git -c url."file:///home/user/github/".insteadOf="https://github.com/" -c protocol.file.allow=always submodule update --init astyle/astyle
# Cloning into '/home/user/github/bext/astyle/astyle'...
# Submodule path 'astyle/astyle': checked out '35f559a2f62bba26ad49b064762458cdc74e4ccd'
# bext (main)$
#
# This time, the astyle submodule was populated successfully.   If you wish to
# be thorough and have the disk space you can do a recursive submodule init for
# all bext submodules to verify everything expected is present.
#
# So, in summary, the two key options for git URL redirection to local
# repositories are:
#
# Instruct git to remap the github.com URL to our local copy:
#
#	-c url."file:///home/user/github/".insteadOf="https://github.com/"
#
# Allow the file:// protocol:
#
#	-c protocol.file.allow=always


# The above works for one-off situations, but what if there is a need to
# permanently remap github.com to some other location? (Say, for example, the
# BRL-CAD project has changed hosting solutions but you need a checkout of an
# older release.) In that situation it is possible to define global config
# options to avoid the need to explicitly specify the above options.
#
# Unless we're actually working with local repos, we don't want to globally
# enable the file:// protocol.  So to set up our .gitconfig for this purpose,
# we use the includeIf features to conditionally include a second, purpose
# specific config file just when we're working with the BRL-CAD repositories.
# The .gitconfig toplevel directives look like:
#
# [includeIf "hasconfig:remote.*.url:https://github.com/BRL-CAD/**"]
#         path = .githubbrlcad
# [includeIf "gitdir:/home/user/github/BRL-CAD/"]
#         path = .githubbrlcad
#
# and the contents of .githubbrlcad (stored in the same directory as the
# global .gitconfig file) are:
#
# [protocol "file"]
#         allow = always
# [url "file:///home/user/github/BRL-CAD/"]
#         insteadOf = https://github.com/BRL-CAD
#
# When these configurations are in place, if we start again with a clean
# checkout of the uninitialized bext repository the following commands will
# "just work" referencing the local mirror copies:
#
# github$ git clone /home/user/github/BRL-CAD/bext.git
# Cloning into 'bext'...
# done.
# github$ cd bext/
# bext (main)$ git submodule update --init astyle/astyle
# Cloning into '/home/user/github/bext/astyle/astyle'...
# Submodule path 'astyle/astyle': checked out '35f559a2f62bba26ad49b064762458cdc74e4ccd'
# bext (main)$
#
# Note, however, that there is one significant drawback to doing this if
# github.com/BRL-CAD is still active - the remapping will result in git not
# being able to pull from https://github.com/BRL-CAD while the setting is
# active. The global includeIf for the url MUST be disabled before git will be
# able to clone or update from the original URLs again.  To check the current
# settings, you can use the following commands from the bext repository.  If
# they return nothing the overrides are not active, otherwise they will print
# the settings currently in use:
#
# git config --get protocol.file.allow
# git config --get url."file:///home/user/github/BRL-CAD/".insteadOf
#
#
# If for whatever reason you want to work with the local repository copies on
# an ad-hoc basis, the command line -c options are the least invasive approach.
# One way to make using them easier is to define aliases in your .gitconfig
# file.  Defining (for example) a bsubmodule alias that would encapsulate the
# above two -c options and calling submodule looks like this:
#
# [alias]
#        bsubmodule = -c url."file:///home/user/github/".insteadOf="https://github.com/" -c protocol.file.allow=always submodule
#


#******************************************************************************
#                    Start of main script logic
#******************************************************************************

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


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
