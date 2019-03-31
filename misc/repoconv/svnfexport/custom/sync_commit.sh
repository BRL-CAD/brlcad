#!/bin/sh
svn co -q file://$1/brlcad/branches/$2@$3 stable-$3
cd stable-$3
cp ../terra.dsp ./db/terra.dsp
find . -name \.svn | xargs rm -rf
../checkout_dercs.sh
find . -type f -executable > ../exec.txt
find . -type f ! -executable > ../noexec.txt
sed -i 's/\.\///' ../exec.txt
sed -i 's/\.\///' ../noexec.txt
cat ../exec.txt |xargs git hash-object > ../exec_hashes.txt
cat ../noexec.txt |xargs git hash-object > ../noexec_hashes.txt
echo "deleteall" > ../commit-$3.fi
#https://www.linuxquestions.org/questions/showthread.php?p=1682282#post1682282
awk  '
BEGIN {
        while ( getline < "../exec_hashes.txt" > 0 )
        {
                f1_counter++
                f1[f1_counter] = $1
        }
}
{
        print "M 100755", f1[NR], "\""$1"\""
} ' ../exec.txt >> ../commit-$3.fi
awk  '
BEGIN {
        while ( getline < "../noexec_hashes.txt" > 0 )
        {
                f1_counter++
                f1[f1_counter] = $1
        }
}
{
        print "M 100644", f1[NR], "\""$1"\""
} ' ../noexec.txt >> ../commit-$3.fi
rm ../exec.txt
rm ../noexec.txt
rm ../exec_hashes.txt
rm ../noexec_hashes.txt
cd ..
#rm -rf stable-$3
