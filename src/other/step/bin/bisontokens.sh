: ' Make <foo>.tok.h from <foo>.h '
egrep '^#.*define' $1.h | egrep -v 'YY(LTYPE|ACCEPT|ABORT|ERROR)' | sed 's/^#define	\([^ ]*\)	[^ ]*$/	"\1",/' > $1.tok.h
