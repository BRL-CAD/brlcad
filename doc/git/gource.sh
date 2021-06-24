#!/bin/sh
#                       G O U R C E . S H
# BRL-CAD
#
# Published in 2021 by the United States Government.
# This work is in the public domain.
#
#
###
# Note - for current gource git log formatting, see:
#
# gource --log-command git


STARTDATE=2021-01-01
STARTSHA1=$(git rev-list -n1 --before="$STARTDATE" main)
STARTAUTH=$(git log -n 1 --format='%an' $STARTSHA1)
STARTTIME=$(git log -n 1 --format='%at' $STARTSHA1)
echo "user:$STARTAUTH" > input.log
echo "$STARTTIME" >> input.log
git ls-tree -r $STARTSHA1| awk '{printf ":000000 %s 0000000000 %s A  %s\n", $1, substr($3, 1, 10), $4}' >> input.log
echo "" >> input.log
git log --pretty=format:user:%aN%n%ct --reverse --raw --encoding=UTF-8 --no-renames --no-show-signature --after=$STARTDATE >> input.log

g++ -o git2gource git2gource.cpp
git2gource input.log > gource.log
#gource gource.log

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
