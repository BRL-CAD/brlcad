CFLAGS=-O
LDFLAGS=-i

IKLIB=../libik/libik.o

PRODUCT=pp-ik pc-ik ikclear ikgrid rle-ik ik-rle ikcmap ikzoom

all:	$(PRODUCT)

rle-ik: rle-ik.o $(IKLIB)
	$(CC) $(LDFLAGS) rle-ik.o $(IKLIB) -o rle-ik

ik-rle: ik-rle.o $(IKLIB)
	$(CC) $(LDFLAGS) ik-rle.o $(IKLIB) -o ik-rle

pp-ik: pp-ik.o $(IKLIB)
	$(CC) $(LDFLAGS) pp-ik.o $(IKLIB) -o pp-ik

pc-ik: pc-ik.o $(IKLIB)
	$(CC) $(LDFLAGS) pc-ik.o $(IKLIB) -o pc-ik

ikclear: ikclear.o $(IKLIB)
	$(CC) $(LDFLAGS) ikclear.o $(IKLIB) -o ikclear

ikzoom:	ikzoom.o $(IKLIB)
	$(CC) $(LDFLAGS) ikzoom.o $(IKLIB) -o ikzoom

ikgrid: ikgrid.o $(IKLIB)
	$(CC) $(LDFLAGS) ikgrid.o $(IKLIB) -o ikgrid

ikcmap: ikcmap.o $(IKLIB)
	$(CC) $(LDFLAGS) ikcmap.o $(IKLIB) -o ikcmap

install: $(PRODUCT)
	cp $(PRODUCT) /usr/brl/bin/.

clean:
	rm $(PRODUCT) *.o *~
