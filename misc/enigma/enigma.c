/*
 *	"enigma.c" is in file cbw.tar from
 *	anonymous FTP host watmsg.waterloo.edu: pub/crypt/cbw.tar.Z
 *
 *	A one-rotor machine designed along the lines of Enigma
 *	but considerably trivialized.
 *
 *	A public-domain replacement for the UNIX "crypt" command.
 *
 *	Upgraded to function properly on 64-bit machines.
 *
 *      Upgraded to have inline "makekey", and to test for
 *	backwards-compatible operation of crypt(), rather than MD5 version.
 *      crypt() is often found in -lcrypt
 *
 *	Added backwards-compatibility for handling illegal salt chars.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MINUSKVAR "CrYpTkEy"

#define ECHO 010
#define ROTORSZ 256
#define MASK 0377
char	t1[ROTORSZ];
char	t2[ROTORSZ];
char	t3[ROTORSZ];
char	deck[ROTORSZ];
char	ibuf[13];
char	buf[13];

void	shuffle(char *);

void
puth( title, cp, len )
char	*title;
char	*cp;
int	len;
{
	fprintf( stderr, "%s = %s = ", title, cp);
	while( len-- > 0 )  {
		fprintf(stderr, "%2.2x ", (*cp++) & 0xFF );
	}
	fprintf(stderr,"\n");
}

/* Different versions of crypt(3) interpret illegal chars differently.
 * Emulate the behavior seen on 4.3BSD and earlier AT&T versions.
 * (Why did FreeBSD have to change this?)
 */
int
saltfix(c)
int c;
{
	if( (c >= '.' && c <= '9') || (c >= 'A' && c <= 'Z' )
		|| (c >= 'a' && c <= 'z') ) return c;

	fprintf(stderr, "WARNING: Character '%c' is illegal in first two bytes of salt.\nAttempting to map in backwards-compatible manner.\n", c);
	/* Heuristics to match older behavior with newer libraries */
	if( c <= '-' )  return ('z' - ( c - '-' ));
	if( c >= '{' )  return ((c - '{' ) + '.');
	if( c >= '[' && c <= '`' )  return ((c - '[' ) + 'U' );
 	if( c >= ':' && c <= '@' )  return ((c - ':' ) + '3' );

	fprintf(stderr, "ERROR: Character '%c' is illegal in password, aborting.\n", c);	
	exit(1);
	/* NOTREACHED */
}

void
setup(pw)
	char *pw;
{
	int ic, i, k, temp;
	unsigned random;
	long seed;
        char *r;
        char salt[3];

        /* Verify backwards-compatible operation of library routine crypt() */
        r = crypt("glorp", "gl");
        if( strncmp( r, "$1$gl$85n.KNI", 13 ) == 0 )  {
                fprintf(stderr, "enigma: crypt() library routine is using MD5 rather than DES.\n%s\n",
			"Incompatible encryption would occur, aborting.");
                exit(1);
        }
        if( strcmp( r, "gl4EsjmGvYQE." ) != 0 )  {
                fprintf(stderr, "enigma: malfunction in crypt() library routine, aborting.\n");
                exit(1);
        }

        /* Don't exec makekey, just invoke library routine directly */
	strncpy(ibuf, pw, 8);
	while (*pw)
		*pw++ = '\0';
	ibuf[8] = '\0';
	salt[0] = saltfix(ibuf[0]);
	salt[1] = saltfix(ibuf[1]);
	salt[2] = '\0';
        r = crypt( ibuf, salt );
        strncpy( buf, r, sizeof(buf) );

	/* First 2 bytes are echo of the salt.  Replace with original salt. */
	buf[0] = ibuf[0];
	buf[1] = ibuf[1];

	seed = 123;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;
	for(i=0;i<ROTORSZ;i++) {
		t1[i] = i;
		deck[i] = i;
	}
	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		if( sizeof(long) > 4 )  {
			/* Force seed to stay in 32-bit signed math */
			if( seed & 0x80000000 )
				seed = seed | (-1L & ~0xFFFFFFFFL);
			else
				seed &= 0x7FFFFFFF;
		}
		random = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (random&MASK)%(k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (random&MASK) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0;i<ROTORSZ;i++)
		t2[t1[i]&MASK] = i;
}

int
main(argc, argv)
	char *argv[];
{
	register int i, n1, n2, nr1, nr2;
	int secureflg = 0, kflag = 0;
	char *cp;

	if (argc > 1 && argv[1][0] == '-') {
		if (argv[1][1] == 's') {
			argc--;
			argv++;
			secureflg = 1;
		} else if (argv[1][1] == 'k') {
			argc--;
			argv++;
			kflag = 1;
		}
	}
	if (kflag) {
		if ((cp = getenv(MINUSKVAR)) == NULL) {
			fprintf(stderr, "%s not set\n", MINUSKVAR);
			exit(1);
		}
		setup(cp);
	} else if (argc != 2) {
		setup(getpass("Enter key:"));
	}
	else
		setup(argv[1]);
	n1 = 0;
	n2 = 0;
	nr2 = 0;

	while((i=getchar()) != -1) {
		if (secureflg) {
			nr1 = deck[n1]&MASK;
			nr2 = deck[nr1]&MASK;
		} else {
			nr1 = n1;
		}
		i = t2[(t3[(t1[(i+nr1)&MASK]+nr2)&MASK]-nr2)&MASK]-nr1;
		putchar(i);
		n1++;
		if(n1==ROTORSZ) {
			n1 = 0;
			n2++;
			if(n2==ROTORSZ) n2 = 0;
			if (secureflg) {
				shuffle(deck);
			} else {
				nr2 = n2;
			}
		}
	}

	return 0;
}

void
shuffle(deck)
	char deck[];
{
	int i, ic, k, temp;
	unsigned random;
	static long seed = 123;

	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		random = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (random&MASK)%(k+1);
		temp = deck[k];
		deck[k] = deck[ic];
		deck[ic] = temp;
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
