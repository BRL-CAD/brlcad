: ' Make y.tok.h from y.tab.h '
grep '^#.*define' y.tab.h | sed 's/^# define \([^ ]*\) [^ ]*$/	"\1",/' > y.tok.h
