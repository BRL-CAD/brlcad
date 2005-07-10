#!/usr/bin/perl
###################################
# Usage rename.pl FROM TO "*.ext"
###################################

$command = "find ./ -name \"$ARGV[2]\"";
print "$command\n";
@list = qx($command);

foreach $file (@list) {
  chop $file;
  print "converting: $file\n";

  $command = "sed s/$ARGV[0]/$ARGV[1]/g $file > /tmp/foo.txt";
  system($command);

  $command = "rm -f $file";
  system($command);

  $command = "mv /tmp/foo.txt $file";
  system($command);
}
