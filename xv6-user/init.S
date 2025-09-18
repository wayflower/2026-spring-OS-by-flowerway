	.file	"init.c"
	.option pic
	.option norelax
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"init: starting sh\n"
	.align	3
.LC1:
	.string	"init: fork failed\n"
	.align	3
.LC2:
	.string	"getpid"
	.align	3
.LC3:
	.string	"init: exec sh failed\n"
	.align	3
.LC4:
	.string	"init: wait returned an error\n"
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	addi	sp,sp,-16
	li	a2,0
	li	a1,1
	li	a0,2
	sd	ra,8(sp)
	sd	s0,0(sp)
	call	dev@plt
	li	a0,0
	call	dup@plt
	li	a0,0
	call	dup@plt
	lla	a0,.LC0
	call	printf@plt
	call	fork@plt
	bge	a0,zero,.L2
	lla	a0,.LC1
.L8:
	call	printf@plt
	li	a0,1
	call	exit@plt
.L2:
	mv	s0,a0
	bne	a0,zero,.L3
	lla	a1,.LANCHOR0
	lla	a0,.LC2
	call	exec@plt
	lla	a0,.LC3
	j	.L8
.L3:
	li	a0,0
	call	wait@plt
	beq	s0,a0,.L4
	bge	a0,zero,.L3
	lla	a0,.LC4
	j	.L8
.L4:
	call	shutdown@plt
	ld	ra,8(sp)
	ld	s0,0(sp)
	li	a0,0
	addi	sp,sp,16
	jr	ra
	.size	main, .-main
	.globl	argv
	.section	.rodata.str1.8
	.align	3
.LC5:
	.string	"sh"
	.section	.data.rel.local,"aw"
	.align	3
	.set	.LANCHOR0,. + 0
	.type	argv, @object
	.size	argv, 16
argv:
	.dword	.LC5
	.dword	0
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
