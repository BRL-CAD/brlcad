
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

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
