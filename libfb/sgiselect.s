|sgiselect( fd, r, w, time)
|{
|	static int exosfd;
|
|	if( exosfd == 0 )  {
|		exosfd = open( "/dev/EXOS/admin", 2 );
|		if( exosfd < 0 )  {
|			exosfd = 0;
|			return(-1);
|		}
|	}
|	ioctl( ?? );
|}
	.data
	.text
	.globl	_sgiselect
_sgiselect:
	linkw	a6,#-56
	tstl	exosfd
	bnes	isopen
	pea	0x2:w
	pea	.L17
	jsr	_open
	addql	#8,a7
	movl	d0,exosfd
	cmpl	#0xFFFFFFFF,d0
	bnes	isopen
	clrl	exosfd
	moveq	#-1,d0
	bras	out
isopen:
	movl	a6@(8),a6@(-24)		| fd
	movl	a6@(12),d0
	movl	d0,a6@(-16)		| r
	movl	a6@(16),d0
	movl	d0,a6@(-20)		| w
	movl	a6@(20),a6@(-12)	| time
	lea	a6@(-4),a0
	movl	a0,d0
	movl	d0,a6@(-8)
	pea	a6@(-56)
	pea	0x3B:w
	movl	exosfd,a7@-
	jsr	_ioctl
	addw	#0xC,a7
	movl	a6@(-4),d0
out:
	unlk	a6
	rts

	.data
exosfd:
	.long	0x0
.L17:
	.ascii	"/dev/EXOS/admin"
	.byte	0x0

