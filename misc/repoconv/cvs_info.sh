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
	grep -Fvx -f ../master $f > uniq.txt
	mv uniq.txt ../branches_uniq/$f
done
cd ..
mv master branches_uniq/


# Write out information to map files, using as a key msg sha1 + date in seconds
rm -f branchmap authormap
cd brlcad_cvs_git
for f in ../branches_uniq/*;
do
	branch="$(basename $f)"
	while IFS="" read -r p || [ -n "$p" ]
	do
		MSGSHA1=$(git log -n1 --pretty=format:"%B" $p | sha1sum | head -c 40)
		CDATE=$(git log -n1 --pretty=format:"%ct" $p)
		AUTHORNAME=$(git log -n1 --pretty=format:"%an" $p)
		echo $MSGSHA1$CDATE:$branch >> ../branchmap
		echo $MSGSHA1$CDATE:$AUTHORNAME>> ../authormap
	done < $f
done

cd ..
