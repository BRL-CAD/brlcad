#	'Optimized'  sqrt()  routine  with  inline  frexp,  heavy
#		register   usage,   and    comments    describing
#		correspondence to libm version of sqrt().
#	Most of the speedup is in frexp() (thanks to  Mike  Muuss
#		at BRL for this).  Improvement	varies	for  this,
#		contributing  lots  more  when	large	or   small
#		arguments are used. The hand-optimizations  I  did
#		contribute abother  10-15%  at	most.  I  estimate
#		overall speedup roughly 2:1 over library  version,
#		depending on application.
#	As a final speedup, I removed the fifth Newton	iteration
#		step.  I could not find a case where  it  made	a
#		difference but I could be wrong.
#
#	Jon Leech (jpl284@cit-vax)	8/24/84
#
	.data
	.comm	_errno,4
	.align	2
# Repeatedly used constants
_Zero:	.double   0d0.0e+00
_Half:	.double   0d5.0e-01
_One:	.double   0d1.0e+00
_Two:	.double   0d2.0e+00
_Long:	.double   0d1.0737418240e+09
	.text
#	Calling sequence of this routine:
#	(Yes, I know doubles aren't normally put into registers!)
#	double sqrt(arg)
#	register double arg;
#	{ register double x, temp;
#	  register int exp;
#	}
#	Register usage		Description
#	(r3,r4)  : 1/2		Constant value 0.5, in a register
#	(r5,r6)  : arg		Argument to take square root of
#	r7	 : exp		Exponent of argument
#	(r8,r9)  : temp 	Temporary in Newton iteration
#	(r10,r11): x		Fraction of argument
LL0:	.align	1
	.globl	_sqrt
# For profiling purposes, uncomment next 3 lines
	 .data
	 .align  2
L15:	 .long	 0			# Profiling buffer
	.text
	.set	L19,0xFF8		# Routine uses r3-r11
_sqrt:.word  L19
#	 movab	 L15,r0 		 # For profiling, uncomment these 2
#	 jsb	 mcount 		 # .
	movd	4(ap),r5		# (r5,r6) = arg
	movd	_Half,r3		# (r3,r4) = 0.5
	cmpd	r5,_Zero		# if (arg <= Zero) {
	jgtr	L23
	cmpd	r5,_Zero		#	if (arg < Zero)
	jgeq	L24
	movl	$33,_errno		#		errno = EDOM;
L24:	movd	_Zero,r0		#	return Zero;
	ret				# }
# Inline frexp()
L23:	movd	r5,r10			# x gets arg
	extzv	$7,$8,r10,r7		# exp gets exponent
	jeql	L26			# if exponent zero, we're done
	subl2	$128,r7 		# bias exponent (excess 128)
	insv	$128,$7,$8,r10		# remove exponent from x
# End of inline frexp()
L26:	jbr	L25			# while (x < Half) {
L2000001:muld2	_Two,r10		#	x *= Two;
	decl	r7			#	exp--;
L25:	cmpd	r10,r3			# }
	jlss	L2000001
	jlbc	r7,L27			# if (exp & 1) {
	muld2	_Two,r10		#	x *= Two;
	decl	r7			#	exp--; }
L27:	addd3	r10,_One,r0		# temp = (One + x)
	muld3	r0,r3,r8		#	 * Half;
	jbr	L28			# while (exp > 60) {
L2000003:muld2	_Long,r8		#	temp *= Long;
	subl2	$60,r7			#	exp -= 60;
L28:	cmpl	r7,$60			# }
	jleq	L30
	jbr	L2000003		# while (exp < -60) {
L2000005:divd2	_Long,r8		#	temp /= Long;
	addl2	$60,r7			#	exp += 60;
L30:	cmpl	r7,$-60 		# }
	jlss	L2000005
	tstl	r7			# if (exp >= 0)
	jlss	L32
	divl3	$2,r7,r0
	ashl	r0,$1,r0
	cvtld	r0,r0
	muld2	r0,r8			#	temp *= 1L << (exp/2);
	jbr	L33			# else
L32:	mnegl	r7,r0
	divl2	$2,r0
	ashl	r0,$1,r0
	cvtld	r0,r0
	divd2	r0,r8			#	temp /= 1L << (-exp/2)
# Unroll Newton iteration into a loop
# First iteration
L33:	divd3	r8,r5,r0		# (r0,r1) = arg/temp
	addd2	r8,r0			# (r0,r1) = temp + arg/temp
	muld3	r0,r3,r8		# temp = Half * (temp + arg/temp)
# Second iteration
	divd3	r8,r5,r0		# (r0,r1) = arg/temp
	addd2	r8,r0			# (r0,r1) = temp + arg/temp
	muld3	r0,r3,r8		# temp = Half * (temp + arg/temp)
# Third iteration
	divd3	r8,r5,r0		# (r0,r1) = arg/temp
	addd2	r8,r0			# (r0,r1) = temp + arg/temp
	muld3	r0,r3,r8		# temp = Half * (temp + arg/temp)
# Fourth iteration
	divd3	r8,r5,r0		# (r0,r1) = arg/temp
	addd2	r8,r0			# (r0,r1) = temp + arg/temp
	muld3	r0,r3,r8		# temp = Half * (temp + arg/temp)
# Fifth iteration seems not to be needed
# If it's a problem, replace it.
#	 divd3	 r8,r5,r0		 # (r0,r1) = arg/temp
#	 addd2	 r8,r0			 # (r0,r1) = temp + arg/temp
#	 muld3	 r0,r3,r8		 # temp = Half * (temp + arg/temp)
# End of Newton iteration
	movd	r8,r0			# return temp;
	ret				# end of procedure
