#!/bin/sh
svn co -q file://$1/brlcad/trunk@$2 trunk-$2
cd trunk-$2
cp ../terra.dsp ./db/terra.dsp
find . -name \.svn | xargs rm -rf
../checkout_dercs.sh
find . -type f -executable > ../exec.txt
find . -type f ! -executable > ../noexec.txt
sed -i 's/\.\///' ../exec.txt
sed -i 's/\.\///' ../noexec.txt
cat ../exec.txt |xargs git hash-object > ../exec_hashes.txt
cat ../noexec.txt |xargs git hash-object > ../noexec_hashes.txt
echo "deleteall" > ../custom/$2-tree.fi
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
} ' ../exec.txt >> ../custom/$2-tree.fi
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
} ' ../noexec.txt >> ../custom/$2-tree.fi
rm ../exec.txt
rm ../noexec.txt
rm ../exec_hashes.txt
rm ../noexec_hashes.txt
cd ..

# fix sha1 on objects where git and svn disagree
# misc/archlinux/brlcad.install
sed -i -e 's/2db7d92d08d6b81c9bfac0b5cc232f45b867e0a5/b491df545130756e59e586db082e8be15f8c154b/g' custom/$2-tree.fi

# doc/docbook/articles/en/images/projection_shader_fig05.png
sed -i -e 's/b67a1bdac086671244186372498369656e15b482/674bc5bec1631eb373d026e3c35f02580d33d6d9/g' custom/$2-tree.fi

# doc/docbook/resources/standard/xsl/extensions/xalan27.jar
sed -i -e 's/8749b32c1b86d101019e7f7b3c3a085b4ea64597/cb090114149cda7052c417e8c2ba557b635b647e/g' custom/$2-tree.fi
sed -i -e 's/76170136a4416561bf7a6ee58fc6d79e9cd6ed86/7f06fdb34becd3fed03b2e9ddf43fdb8da5376cc/g' custom/$2-tree.fi

# doc/docbook/resources/standard/xsl/extensions/saxon65.jar
sed -i -e 's/06d5271f3e9ec1ca4411865b26a3e1ac1b64b1e8/744df1a5cb1124b64cee08086f6316c89616c1ae/g' custom/$2-tree.fi
sed -i -e 's/2f7de475e6bbd22df7305fce1f6750bec8016ba2/5bad294f6af0d436036d00d2666d2a3309ca8cdb/g' custom/$2-tree.fi

# regress/gcv/fastgen/fastgen_box.fast4
sed -i -e 's/3785040dbb9252dcf56f1d3edcfedc8859141339/33ddd71cf9d6b856d94d7bd3c0600f24c1db7120/g' custom/$2-tree.fi

