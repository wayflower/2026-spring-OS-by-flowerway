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
	.string	"init: pipe failed\n"
	.align	3
.LC2:
	.string	"init: fork failed\n"
	.align	3
.LC3:
	.string	"hello_world"
	.align	3
.LC4:
	.string	"init: exec sh failed\n"
	.align	3
.LC5:
	.string	"This is my output: %s\n"
	.align	3
.LC6:
	.string	"init: wait returned an error\n"
	.section	.text.startup,"ax",@progbits
	.align	1
	.globl	main
	.type	main, @function
main:
	addi	sp,sp,-48
	li	a2,0
	li	a1,1
	li	a0,2
	sd	ra,40(sp)
	sd	s0,32(sp)
	sd	s1,24(sp)
	call	dev@plt
	li	a0,0
	call	dup@plt
	li	a0,0
	call	dup@plt
	lla	a0,.LC0
	call	printf@plt
	addi	a0,sp,8
	call	pipe@plt
	li	a5,-1
	bne	a0,a5,.L2
	lla	a0,.LC1
.L10:
	call	printf@plt
	li	a0,1
	call	exit@plt
.L2:
	call	fork@plt
	mv	s0,a0
	bge	a0,zero,.L3
	lla	a0,.LC2
	j	.L10
.L3:
	bne	a0,zero,.L4
	lw	a0,8(sp)
	call	close@plt
	lw	a0,12(sp)
	li	a1,1
	call	dup2@plt
	lw	a0,12(sp)
	call	close@plt
	lla	a1,.LANCHOR0
	lla	a0,.LC3
	call	exec@plt
	lla	a0,.LC4
	j	.L10
.L4:
	lw	a0,12(sp)
	call	close@plt
	lw	a0,8(sp)
	li	a2,1023
	lla	a1,.LANCHOR0+8
	call	read@plt
	mv	s1,a0
	lw	a0,8(sp)
	call	close@plt
	ble	s1,zero,.L5
	lla	a5,.LANCHOR0
	add	a5,a5,s1
	sb	zero,8(a5)
.L5:
	lla	a1,.LANCHOR0+8
	lla	a0,.LC5
	call	printf@plt
.L7:
	li	a0,0
	call	wait@plt
	beq	s0,a0,.L6
	bge	a0,zero,.L7
	lla	a0,.LC6
	j	.L10
.L6:
	call	shutdown@plt
	ld	ra,40(sp)
	ld	s0,32(sp)
	ld	s1,24(sp)
	li	a0,0
	addi	sp,sp,48
	jr	ra
	.size	main, .-main
	.globl	test_outputs
	.globl	argv
	.bss
	.align	3
	.set	.LANCHOR0,. + 0
	.type	argv, @object
	.size	argv, 8
argv:
	.zero	8
	.type	test_outputs, @object
	.size	test_outputs, 1024
test_outputs:
	.zero	1024
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
