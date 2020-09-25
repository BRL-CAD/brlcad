#!/bin/bash

# To use this script, place the prior brlcad_git conversion
# in the directory git_old, and the new conversion in git_new.
# It will process both repositories to build a mapping of old
# sha1 identifiers for tags in git_old to the new ones in
# git_new, and print a series of sed commands which can be
# applied to the tags fast import file to update it for the
# git_new repository

domap() {
	cd git_old
	git checkout tags/$1 2>/dev/null 1>/dev/null
	V1=$(git log --format="%H" -n 1 2>/dev/null)
	MSG=$(git show --quiet --format="%s" 2>/dev/null)
	DATE=$(git show --quiet --format="%ct" 2>/dev/null)
	rm -rf *
	git reset --hard 2>/dev/null 1>/dev/null
	cd ../git_new
	SHA1=$(git log --all --grep="$MSG\$" --grep="$DATE" --format="%H" 2>/dev/null)
	cd ..
	# If the old sha1 values in the fast import file don't
	# correspond to those in git_old, we have to print them
	# out and manually update the file
	echo $1:
	echo $SHA1
	# If git_old matches the fast import file, we can generate
	# sed lines and automatically replace the old values with
	# the new.
	#echo "sed -i 's/$V1/$SHA1/g' r29886_tags.fi"
}

declare -a tags=(
CMD
Original
ansi-20040316-freeze
autoconf-20031202
autoconf-20031203
bobWinPort-20051223-freeze
ctj-4-5-post
ctj-4-5-pre
hartley-6-0-post
help
itcl3-2
libpng_1_0_2
merge-to-head-20051223
offsite-5-3-pre
opensource-pre
postmerge-autoconf
premerge-20040315-windows
premerge-20040404-ansi
premerge-20051223-bobWinPort
premerge-autoconf
rel-2-0
rel-5-0
rel-5-0-beta
rel-5-0beta
rel-5-1
rel-5-2
rel-5-3
rel-6-0
rel-6-0-1
rel-6-1-DP
rel-7-0
rel-7-0-1
rel-7-0-2
rel-7-0-4
rel-7-10-0
rel-7-10-2
rel-7-2-0
rel-7-2-2
rel-7-2-4
rel-7-2-6
rel-7-4-0
rel-7-6-0
rel-7-6-4
rel-7-6-6
rel-7-8-0
rel-7-8-2
rel-7-8-4
release-7-0
stable-branch
tcl8-3
temp_tag
tk8-3
windows-20040315-freeze
zlib_1_0_4
)

# Clean up any odd state a prior run may have left
cd git_old
git reset --hard 2>/dev/null 1>/dev/null
git checkout master 2>/dev/null 1>/dev/null
cd ../git_new
git reset --hard 2>/dev/null 1>/dev/null
git checkout master 2>/dev/null 1>/dev/null
cd ..

for i in "${tags[@]}"
do
	domap $i
done


