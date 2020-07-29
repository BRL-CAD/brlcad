#!/bin/bash
if [ ! -e "cvs-fast-export" ]; then
        curl -o cvs-fast-export.tar.gz https://gitlab.com/esr/cvs-fast-export/-/archive/1.48/cvs-fast-export-1.48.tar.gz
	tar -xvf cvs-fast-export.tar.gz
	mv cvs-fast-export-1.48 cvs-fast-export
fi
cd cvs-fast-export && make cvs-fast-export && cd ..

# To run the conversion (need to use cvs-fast-export rather than cvsconvert
# for the actual conversion to support the authors file):
if [ ! -e "brlcad_cvs.tar.gz" ]; then
        curl -o brlcad_cvs.tar.gz https://brlcad.org/brlcad_cvs.tar.gz
fi


if [ ! -e "brlcad_cvs" ];
then
	rm -rf brlcad_cvs
	tar -xf brlcad_cvs.tar.gz
	cd brlcad_cvs/brlcad
	# Create Git repository without author map (to preserve original CVS names)
	echo "Running cvs-fast-export $PWD"
	find . | ../../cvs-fast-export/cvs-fast-export > ../../brlcad_cvs_git.fi
	cd ../..
	rm -rf brlcad_cvs_git
	mkdir brlcad_cvs_git
	cd brlcad_cvs_git
	git init
	cat ../brlcad_cvs_git.fi | git fast-import
	git checkout master
	# Find branches
	git branch|sed -e 's/*//'|sed -e 's/ *//' > ../branches.txt
	cd ..
fi

# Find commits on branches
rm -rf branches
mkdir branches
cd brlcad_cvs_git

while IFS="" read -r p || [ -n "$p" ]
do
  printf '%s\n' "$p"
  OFILE=$p
  git rev-list --first-parent $p > ../branches/$OFILE
done < ../branches.txt

mv ../branches/master ..
cd ..

# Find commits unique to branches (i.e. not on master)
rm -rf branches_uniq
mkdir branches_uniq
cd branches
for f in *;
do
	echo "Uniq: $f"
	grep -Fvx -f ../master $f > uniq.txt
	mv uniq.txt ../branches_uniq/$f
done
cd ..

# For each branch, walk the non-master commits looking for the
# newest commit timestamp on the branch.  Store that date
cd brlcad_cvs_git

declare -A datemap

for f in ../branches_uniq/*;
do
	branch="$(basename $f)"
	skip="master"
	if [ $branch != $skip ];
	then
		echo "Date check: $branch"
		newcommit=0
		while IFS="" read -r p || [ -n "$p" ]
		do
			CDATE=$(git log -n1 --pretty=format:"%ct" $p)
			if [ "$CDATE" -gt "$newcommit" ];
			then
				newcommit=$CDATE;
			fi;
		done < $f
		echo "$branch $newcommit"
		datemap[$branch]=$newcommit
	fi;
done

cd ..

# For all branches, if a branch has a newest commit date older
# than the current branch, remove that branches commits from the
# current branch
cd branches_uniq
for f in *;
do
	FDATE=${datemap[$f]}
	for g in *;
	do
		if [ $g != $f ];
		then
			GDATE=${datemap[$g]}
			if [ "$GDATE" -lt "$FDATE" ];
			then
				echo "Scrub $FDATE:$GDATE : $f/$g"
				grep -Fvx -f $g $f > uniq.txt
				mv uniq.txt $f
			fi;
		fi;
	done
done

# Couple special cases I can't seem to detect otherwise:
grep -Fvx -f bobWinPort bobWinPort-20051223-freeze > uniq.txt
mv uniq.txt bobWinPort-20051223-freeze
grep -Fvx -f brlcad_5_1_alpha_patch rel-5-1 > uniq.txt
mv uniq.txt rel-5-1

cd ../brlcad_cvs_git
for f in ../branches_uniq/*;
do
	branch="$(basename $f)"
	echo "Date check: $branch"
	oldcommit=9223372036854775807
	newcommit=0
	while IFS="" read -r p || [ -n "$p" ]
	do
		CDATE=$(git log -n1 --pretty=format:"%ct" $p)
		if [ "$CDATE" -lt "$oldcommit" ];
		then
			oldcommit=$CDATE;
		fi;
		if [ "$CDATE" -gt "$newcommit" ];
		then
			newcommit=$CDATE;
		fi;
	done < $f
	echo "$branch $newcommit:$oldcommit"
done

cd ..


# Write out information to map files, using as a key msg sha1 + date in seconds

# mv master branches_uniq/ - Note: shouldn't need master explicitly, it's assumed

rm -f key_branchmap sha1_branchmap key_authormap sha1_authormap
cd brlcad_cvs_git
for f in ../branches_uniq/*;
do
	branch="$(basename $f)"
	echo "Map write: $branch"
	while IFS="" read -r p || [ -n "$p" ]
	do
		MSGSHA1=$(git log -n1 --pretty=format:"%B" $p | sha1sum | head -c 40)
		CDATE=$(git log -n1 --pretty=format:"%ct" $p)
		AUTHORNAME=$(git log -n1 --pretty=format:"%an" $p)
		echo $p:$branch >> ../sha1_branchmap
		echo $p:$AUTHORNAME >> ../sha1_authormap
		echo $MSGSHA1$CDATE:$branch >> ../key_branchmap
		echo $MSGSHA1$CDATE:$AUTHORNAME>> ../key_authormap
	done < $f
done

cd ..


# NOTE: to generate a map between the above keys and sha1 values, run the
# domap.sh script from within the repository you wish to map to.

