/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * hilbert.c - Computes Hilbert curve coordinates from position and v.v.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Thu Feb  7 1991
 * Copyright (c) 1991, University of Michigan
 */
static char rcsid[] = "$Header$";

/*
 * Lots of tables to simplify calculations.  Notation: p#i means bit i
 * in byte p (high order bit first).
 * p_to_s:	Output s is a byte from input p such that
 * 		s#i = p#i xor p#(i-1)
 * s_to_p:	The inverse of the above.
 * p_to_J:	Output J is "principle position" of input p.  The
 * 		principle position is the last bit s.t.
 * 		p#J != p#(n-1) (or n-1 if all bits are equal).
 * bit:		bit[i] == (1 << (n - i))
 * circshift:	circshift[b][i] is a right circular shift of b by i
 * 		bits in n bits.
 * parity:	Parity[i] is 1 or 0 depending on the parity of i (1 is odd).
 * bitof:	bitof[b][i] is b#i.
 * nbits:	The value of n for which the above tables have been
 * 		calculated.
 */

typedef unsigned int byte;

static int nbits = 0;

static byte
    p_to_s[512],
    s_to_p[512],
    p_to_J[512],
    bit[9],
    circshift[512][9],
    parity[512],
    bitof[512][9];

/* Calculate the above tables when nbits changes. */
static void
calctables(n)
int n;
{
    register int i, b;
    int two_n = 1 << n;

    if ( nbits == n )
	return;

    nbits = n;
    /* bit array is easy. */
    for ( b = 0; b < n; b++ )
	bit[b] = 1 << (n - b - 1);

    /* Next, do bitof. */
    for ( i = 0; i < two_n; i++ )
	for ( b = 0; b < n; b++ )
	    bitof[i][b] = (i & bit[b]) ? 1 : 0;

    /* circshift is independent of the others. */
    for ( i = 0; i < two_n; i++ )
	for ( b = 0; b < n; b++ )
	    circshift[i][b] = (i >> (b)) |
		((i << (n - b)) & (two_n - 1));

    /* So is parity. */
    parity[0] = 0;
    for ( i = 1, b = 1; i < two_n; i++ )
    {
	/* Parity of i is opposite of the number you get when you
	 * knock the high order bit off of i.
	 */
	if ( i == b * 2 )
	    b *= 2;
	parity[i] = !parity[i - b];
    }

    /* Now do p_to_s, s_to_p, and p_to_J. */
    for ( i = 0; i < two_n; i++ )
    {
	int s;

	s = i & bit[0];
	for ( b = 1; b < n; b++ )
	    if ( bitof[i][b] ^ bitof[i][b-1] )
		s |= bit[b];
	p_to_s[i] = s;
	s_to_p[s] = i;

	p_to_J[i] = n - 1;
	for ( b = 0; b < n; b++ )
	    if ( bitof[i][b] != bitof[i][n-1] )
		p_to_J[i] = b;
    }
}

/*****************************************************************
 * TAG( hilbert_i2c )
 * 
 * Convert an index into a Hilbert curve to a set of coordinates.
 * Inputs:
 * 	n:	Number of coordinate axes.
 * 	m:	Number of bits per axis.
 * 	r:	The index, contains n*m bits (so n*m must be <= 32).
 * Outputs:
 * 	a:	The list of n coordinates, each with m bits.
 * Assumptions:
 * 	n*m < (sizeof r) * (bits_per_byte), n <= 8, m <= 9.
 * Algorithm:
 * 	From A. R. Butz, "Alternative Algorithm for Hilbert's
 * 		Space-Filling Curve", IEEE Trans. Comp., April, 1971,
 * 		pp 424-426.
 */
void
hilbert_i2c( n, m, r, a)
int n;
int m;
long int r;
int a[];
{
    byte rho[9], rh, J, sigma, tau,
    	 sigmaT, tauT, tauT1 = 0, omega, omega1 = 0, alpha[9];
    register int i, b;
    int Jsum;

    /* Initialize bit twiddle tables. */
    calctables(n);

    /* Distribute bits from r into rho. */
    for ( i = m - 1; i >= 0; i-- )
    {
	rho[i] = r & ((1 << n) - 1);
	r >>= n;
    }

    /* Loop over bytes. */
    Jsum = 0;
    for ( i = 0; i < m; i++ )
    {
	rh = rho[i];
	/* J[i] is principle position of rho[i]. */
	J = p_to_J[rh];

	/* sigma[i] is derived from rho[i] by exclusive-oring adjacent bits. */
	sigma = p_to_s[rh];

	/* tau[i] complements low bit of sigma[i], and bit at J[i] if
	 * necessary to make even parity.
	 */
	tau = sigma ^ 1;
	if ( parity[tau] )
	    tau ^= bit[J];
	
	/* sigmaT[i] is circular shift of sigma[i] by sum of J[0..i-1] */
	/* tauT[i] is same circular shift of tau[i]. */
	if ( Jsum > 0 )
	{
	    sigmaT = circshift[sigma][Jsum];
	    tauT = circshift[tau][Jsum];
	}
	else
	{
	    sigmaT = sigma;
	    tauT = tau;
	}

	Jsum += J;
	if ( Jsum >= n )
	    Jsum -= n;

	/* omega[i] is xor of omega[i-1] and tauT[i-1]. */
	if ( i == 0 )
	    omega = 0;
	else
	    omega = omega1 ^ tauT1;
	omega1 = omega;
	tauT1 = tauT;

	/* alpha[i] is xor of omega[i] and sigmaT[i] */
	alpha[i] = omega ^ sigmaT;
    }

    /* Build coordinates by taking bits from alphas. */
    for ( b = 0; b < n; b++ )
    {
	register int ab, bt;
	ab = 0;
	bt = bit[b];
	/* Unroll the loop that stuffs bits into ab.
	 * The result is shifted left by 9-m bits.
	 */
	switch( m )
	{
	case 9:	if ( alpha[8] & bt) ab |= 0x01;
	case 8:	if ( alpha[7] & bt) ab |= 0x02;
	case 7:	if ( alpha[6] & bt) ab |= 0x04;
	case 6:	if ( alpha[5] & bt) ab |= 0x08;
	case 5:	if ( alpha[4] & bt) ab |= 0x10;
	case 4:	if ( alpha[3] & bt) ab |= 0x20;
	case 3:	if ( alpha[2] & bt) ab |= 0x40;
	case 2:	if ( alpha[1] & bt) ab |= 0x80;
	case 1:	if ( alpha[0] & bt) ab |= 0x100;
	}
	a[b] = ab >> (9 - m);
    }
}

/*****************************************************************
 * TAG( hilbert_c2i )
 * 
 * Convert coordinates of a point on a Hilbert curve to its index.
 * Inputs:
 * 	n:	Number of coordinates.
 * 	m:	Number of bits/coordinate.
 * 	a:	Array of n m-bit coordinates.
 * Outputs:
 * 	r:	Output index value.  n*m bits.
 * Assumptions:
 * 	n*m <= 32, n <= 8, m <= 9.
 * Algorithm:
 * 	Invert the above.
 */
void
hilbert_c2i( n, m, a, r)
int n;
int m;
int a[];
long int *r;
{
    byte rho[9], J, sigma, tau,
    	 sigmaT, tauT, tauT1 = 0, omega, omega1 = 0, alpha[9];
    register int i, b;
    int Jsum;
    long int rl;

    calctables(n);

    /* Unpack the coordinates into alpha[i]. */
    /* First, zero out the alphas. */
    switch ( m ) {
    case 9: alpha[8] = 0;
    case 8: alpha[7] = 0;
    case 7: alpha[6] = 0;
    case 6: alpha[5] = 0;
    case 5: alpha[4] = 0;
    case 4: alpha[3] = 0;
    case 3: alpha[2] = 0;
    case 2: alpha[1] = 0;
    case 1: alpha[0] = 0;
    }

    /* The loop that unpacks bits of a[b] into alpha[i] has been unrolled.
     * The high-order bit of a[b] has to go into alpha[0], so we pre-shift
     * a[b] so that its high-order bit is always in the 0x100 position.
     */
    for ( b = 0; b < n; b++ )
    {
	register int bt = bit[b], t = a[b] << (9 - m);

	switch (m)
	{
	case 9: if ( t & 0x01 ) alpha[8] |= bt;
	case 8: if ( t & 0x02 ) alpha[7] |= bt;
	case 7: if ( t & 0x04 ) alpha[6] |= bt;
	case 6: if ( t & 0x08 ) alpha[5] |= bt;
	case 5: if ( t & 0x10 ) alpha[4] |= bt;
	case 4: if ( t & 0x20 ) alpha[3] |= bt;
	case 3: if ( t & 0x40 ) alpha[2] |= bt;
	case 2: if ( t & 0x80 ) alpha[1] |= bt;
	case 1: if ( t & 0x100 ) alpha[0] |= bt;
	}
    }

    Jsum = 0;
    for ( i = 0; i < m; i++ )
    {
	/* Compute omega[i] = omega[i-1] xor tauT[i-1]. */
	if ( i == 0 )
	    omega = 0;
	else
	    omega = omega1 ^ tauT1;

	sigmaT = alpha[i] ^ omega;
	/* sigma[i] is the left circular shift of sigmaT[i]. */
	if ( Jsum != 0 )
	    sigma = circshift[sigmaT][n - Jsum];
	else
	    sigma = sigmaT;

	rho[i] = s_to_p[sigma];

	/* Now we can get the principle position. */
	J = p_to_J[rho[i]];

	/* And compute tau[i] and tauT[i]. */
	/* tau[i] complements low bit of sigma[i], and bit at J[i] if
	 * necessary to make even parity.
	 */
	tau = sigma ^ 1;
	if ( parity[tau] )
	    tau ^= bit[J];

	/* tauT[i] is right circular shift of tau[i]. */
	if ( Jsum != 0 )
	    tauT = circshift[tau][Jsum];
	else
	    tauT = tau;
	Jsum += J;
	if ( Jsum >= n )
	    Jsum -= n;

	/* Carry forth the "i-1" values. */
	tauT1 = tauT;
	omega1 = omega;
    }

    /* Pack rho values into r. */
    rl = 0;
    for ( i = 0; i < m; i++ )
	rl = (rl << n) | rho[i];
    *r = rl;
}


#ifdef test
#include <stdio.h>

main()
{
    int a[9], n, m, i;
    long int r, r1;

    for (;;)
    {
	printf( "Enter n, m: " );
	scanf( "%d", &n );
	if ( n == 0 )
	    break;
	scanf( "%d", &m );
	while ( (i = getchar()) != '\n' && i != EOF )
	    ;
	if ( i == EOF )
	    break;
	for ( r = 0; r < 1 << (n*m); r++ )
	{
	    hilbert_i2c( n, m, r, a );
	    if ( n*m <= 6 )
	    {
		printf( "a = " );
		for ( i = 0; i < n; i++ )
		    printf( "0x%0*x ", (m+3)/4, a[i] );
	    }
	    hilbert_c2i( n, m, a, &r1 );
	    if ( n*m <= 6 )
		printf( "r = 0x%0*x\n", (n*m+3)/4, r1 );
	    else if ( r != r1 )
		printf( "r = 0x%0*x; r1 = 0x%0*x\n", (n*m+3)/4, r,
			(n*m+3)/4, r1 );
	}
    }
}

p1t( tbl, n )
byte tbl[];
int n;
{
    int i;

    for ( i = 0; i < n; i++ )
	printf( "%02x ", tbl[i] );
    putchar( '\n' );
}

p2t( tbl, n, m )
byte tbl[][9];
int n;
{
    int i;

    for ( i = 0; i < n; i++ )
    {
	printf( "%3d: ", i );
	p1t( tbl[i], m );
    }
}


#endif

