# For goulds...
#	use /bin/cc and /usr/include/brlcad
# For sgis...
#	use /usr/bin/cc, /usr/brlcad/include, and the -lmpc option
# CC = /bin/cc
CC = /usr/bin/cc
# CFLAGS = -I/usr/include/brlcad -DDFLT_SHELL=\"/bin/tbsh\"
CFLAGS = -I/usr/brlcad/include -DDFLT_SHELL=\"/bin/tbsh\"
OBJS = nirt.o command.o conversion.o if.o interact.o dist_def.o str_manip.o parse_fmt.o
LIBS = /usr/brlcad/lib/librt.a

nirt:nirt.o command.o conversion.o if.o interact.o dist_def.o str_manip.o parse_fmt.o
#	$(CC) $(OBJS) $(LIBS) -o $@ -lm		# This is for vim.brl.mil
	$(CC) $(OBJS) $(LIBS) -o $@ -lmpc -lm	# This is for war.brl.mil

lint: ; /usr/bin/lint *c
