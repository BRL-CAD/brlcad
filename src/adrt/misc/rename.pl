#!/usr/bin/perl


@list= qx(ls *.cxx *.h);
foreach $file (@list) {
  chop $file;
  print $file;
  system("sed s/Point3d/ADM_3/g $file > $file.bak");
  system("mv -f $file.bak $file");
} #eof
