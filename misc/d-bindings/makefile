# Note:  This makefile is posted on the D Wiki in the Cookbook section at:
#
#   http://wiki.dlang.org/Using_C_libraries_for_a_D_program
#
# Corrections should be posted there.

# use the dmd compiler
DMD = /usr/bin/dmd

BDIR    := /usr/brlcad/rel-7.25.0
LIBDIR  := $(BDIR)/lib
INCDIR  := $(BDIR)/include/brlcad
INCDIR2 := $(BDIR)/include

BINDINGS = bu.d

SRC = test_bu.d $(BINDINGS)

OBJ = $(SRC:.d=.o)

PROG = test-bu

all: $(PROG)

$(PROG): $(OBJ)
	$(DMD) $(OBJ) -L$(LIBDIR) -lbu

%.o: %.d
	$(DMD) -c $<

clean:
	-rm $(PROG) *.o
