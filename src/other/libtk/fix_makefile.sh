#!/bin/sh

cat Makefile

#sed -e s@lib/tk\$\(VERSION\)@tk\$\(VERSION\)@ Makefile |\
#	 sed -e s@\(prefix\)/include@\(prefix\)/include/brlcad@ |\
#	 sed -e "s@\$(\C\C)  \$(WISH_OBJS)@\$(\C\C) $LDFLAGS \$(WISH_OBJS)@" |\
#	 sed -e "s@\${\C\C}  \$(TKTEST_OBJS)@\${\C\C} $LDFLAGS \$(TKTEST_OBJS)@" |\
#         sed -e "s@wish\$(VERSION)@wish@"
