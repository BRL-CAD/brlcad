--
--	S Q R T . S
--
--	Adapted from the C SQRT routine for efficiency
--	Joseph C. Pistritto
--	US Army Ballistic Research Laboratory
--	(JCP@BRL)
--
--	Started with the assembly language output of the C compiler and
--	improvised from there
--
--	Using this, you take 48% of the time as the stock Gould UTX 1.1
--	routine.  (written in C)
--
.fartext
.fardata	2
SQRT:.ascii	"sqrt\0"
.fartext
.fardata	2
DOMAIN:.ascii	"sqrt\72 DOMAIN error\12\0"
--
--	Some useful constants
--
.data
.align	3
ZERO:	.word	0x0,0x0
.align	3
ONE:	.word	0x41100000,0x0
.align	3
HALF:	.word	0x40800000,0x0
.align	3
BIGNUM:	.word	0x7fffffff,0xffffffe0
--
--	code
--
.fartext
	.fartext
LL0:	.bf
	.align	2
	.globl	_sqrt
_sqrt:	.using	b1,_sqrt
	subea	LA1,b2
	movw	r1,[b2]
	movd	8w+LA1[b2],r0		-- x
--
--	exception tests, check for x <= 0
--
	cmpl	ZERO,r0
	jgt	DOSQRT			-- x > 0, do SQRT
--
	cmpl	ZERO,r0
	jne	ERR
	jmp	cret1			-- return 0 if x == 0
--
--	domain error has occurred (x < 0)
--	post an exception
--
ERR:	movw	#1,r0
	movw	r0,12w+LOC1[b2]
	movw	#SQRT,r1
	movw	r1,13w+LOC1[b2]
	movl	8w+LA1[b2],r0
	movl	r0,14w+LOC1[b2]
	movea	12w+LOC1[b2],r1
	movw	r1,8w[b2]
	func	#0x1,_matherr
	movw	r0,r0
	jne	L56
	movl	ZERO,r0
	movl	r0,18w+LOC1[b2]
	movw	#0x21,r0
	movw	r0,_errno
L56:	movd	18w+LOC1[b2],r0
	jmp	cret1
--
--	DOSQRT - part which performs the SQRT function
--
DOSQRT:
--
--	FREXP simulation:
--	Takes number in R0,
--	returns fraction in R6,R7, exponent in R5
--	Note: this can't handle zero or negative numbers!
--
--	Uses only R5,R6,R7
--
--	extract exponent into r5
--
	movd	r0,r6			-- copy x into r6,r7
	movw	r6,r5
	asrw	#0x18,r5		-- shift to low order, mul by 4
	subw	#0x40,r5		-- subtract bias * 4 (0x40 << 2)
	lslw	#2,r5			-- multiply by 4
--
--	now shift mantissa up one bit at a time, subtracting from exponent,
--	until mantissa becomes > 0.5 (to simulate a machine with binary exp)
--
L10:	btstw	#0x00800000,r6		-- is it > 0.5?
	jbt	L20			-- yes
--
	lslw	#1,r6			-- shift mantissa by one bit
	subw	#1,r5			-- decrement exponent
	jmp	L10
--
--	have mantissa, fix exponent and recover sign
--
L20:	andw	#0x00FFFFFF,r6		-- throw away existing sign & exp
	orw	#0x40000000,r6		-- stick in exponent of 0
--
--	end FREXP, R5 = iexp, R6,R7 = y, R0,R1 = x
--
	btstw	#0x00000001,r5		-- if( iexp % 2 )
	jbf	L30			-- {
	subw	#1,r5			--	iexp--;
	addd	r6,r6			--	y += y;
--					-- }
L30:	movd	ONE,r2			-- 1.0 -> r2,r3
	addd	r2,r6			-- y = y + 1;
	asrw	#1,r5			-- iexp/2
	subw	#1,r5			--   - 1
--
--	invoke inline LDEXP - code stolen from LDEXP, without the packaging
--	args are EXP in r5, y in r6,r7
--
--	uses r2,r3 as scratch
--	Create a floating point number that represents 2 ^ exp
--	and then multiply by the incoming fraction
--
	movw	r5,r2
	andw	#0x00000003,r5		-- r5 = exp % 4
	asrw	#2,r2			-- r2 = exp / 4
	addw	#0x41,r2		-- r2 = exp / 4 + 1 + bias
--
	cmpw	#0x00,r2		-- if exp < 0, go away...
	jlt	SMALL
	cmpw	#0x7F,r2		-- if exp > 127, go away...
	jgt	HUGE

	lslw	#0x18,r2
	andw	#0x7F000000,r2		-- mask all other bits
--
--	now take the leftover exponent (in r5), and translate into
--	mantissa value of 1/16, 1/8, 1/4 or 1/2 for values of 0,1,2,3
--
	movw	#0x00100000,r3		-- starting mantissa = 1/16
	cmpw	#0x00,r5
	jeq	COMB			-- goto combine number
--
	lslw	#1,r3			-- now 1/8
	cmpw	#0x01,r5
	jeq	COMB
--
	lslw	#1,r3			-- now 1/4
	cmpw	#0x02,r5
	jeq	COMB
--
	lslw	#1,r3			-- now 1/2
--
COMB:	orw	r3,r2
	movw	#0x0,r3			-- r2,r3 is now a floating pt #
--
	muld	r2,r6			-- multiply by original #
	jbf	AFTER			-- ldexp complete, no overflow
--
--	Overflow - determine if over or under flow by condition of CC4
--	(eq/ne bit)
--
OVFLO:	jeq	HUGE			-- CC4 set means overflow = toobig
--
--	Exponent too small, return a little number
--
SMALL:	movd	ZERO,r6
	jmp	AFTER
--
--	Exponent is too big, return a large number
--
HUGE:	movd	BIGNUM,r6
--
--	Newtonian iteration unrolled, 5 iterations
--
--	Iteration One
--
AFTER:	movd	r0,r2			-- x -> temp location (t)
	divd	r6,r2			-- t = x/y
	addd	r6,r2			-- t = (y + x/y)
	movl	r2,r6			-- y = t
	movd	HALF,r2
	muld	r2,r6			-- y = 0.5 * (y + x/y)
--
--	Iteration Two
--
	movd	r0,r2			-- x -> temp location (t)
	divd	r6,r2			-- t = x/y
	addd	r6,r2			-- t = (y + x/y)
	movl	r2,r6			-- y = t
	movd	HALF,r2
	muld	r2,r6			-- y = 0.5 * (y + x/y)
--
--	Iteration Three
--
	movd	r0,r2			-- x -> temp location (t)
	divd	r6,r2			-- t = x/y
	addd	r6,r2			-- t = (y + x/y)
	movl	r2,r6			-- y = t
	movd	HALF,r2
	muld	r2,r6			-- y = 0.5 * (y + x/y)
--
--	Iteration Four
--
	movd	r0,r2			-- x -> temp location (t)
	divd	r6,r2			-- t = x/y
	addd	r6,r2			-- t = (y + x/y)
	movl	r2,r6			-- y = t
	movd	HALF,r2
	muld	r2,r6			-- y = 0.5 * (y + x/y)
--
--	decided to return here - if you must have more accuracy,
--	delete the next two instructions and use the fifth iteration.
--	I haven't found any place where it makes a difference however.
--
	movd	r6,r0
	jmp	cret
--
--	Iteration Five
--
	movd	r0,r2			-- x -> temp location (t)
	divd	r6,r2			-- t = x/y
	addd	r6,r2			-- t = (y + x/y)
	movl	r2,r6			-- y = t
	movd	HALF,r2
	muld	r2,r6			-- y = 0.5 * (y + x/y)
--
	movd	r6,r0
	jmp	cret
	.ef	2
	.set	LOC1,4w
	.set	LA1,32w
.data
	.text
