
# Usage: pull_comp file component#(s)
# This awk program pulls the selected component number(s) from the fastgen
# file and prints the result on standard out.

nawk '
BEGIN {
	while( getline < ARGV[1] ){
		for( i=2; i<ARGC; i++ ){
			if( (substr($0,32,4)+0) == ARGV[i] )
				print $0
		}
	}
} ' $*
