/*
 * Complex Number and FFT Library
 *
 *  12 Oct 84
 */

typedef struct {
	double	re;	/* Real Part */
	double	im;	/* Imaginary Part */
} COMPLEX;

extern double cos(), sin();

/***** TEST ROUTINES *****/
COMPLEX data[64];

main()
{
	int	i;

	for (i = 0; i < 64; i++) {
		data[i].re = sin((double)2.0*3.1415926535*i/64.0);
		data[i].re += 3*cos((double)2.0*3.1415926535*i/32.0);
		data[i].im = (double)0.0;
	}

	printf("Original Data:\n\n");
	display(data, 64);

	fft(data, 64);
	printf("\n\nTransformed Data:\n\n");
	display(data, 64);

	invfft(data, 64);
	printf("\n\nInversed Data:\n\n");
	display(data, 64);
}

display(dat, num)
COMPLEX dat[]; int num;
{
	int	i;

	for (i = 0; i < num; i++) {
		printf("%3d : ", i);
		printf("%f, %f\n", dat[i].re, dat[i].im);
	}
}
