	.section	__TEXT,__text,regular,pure_instructions
	.macosx_version_min 14, 0
	.globl	_main
	.align	4, 0x90
_main:                                  ## @main
	.cfi_startproc
## BB#0:                                ## %entry
	pushq	%rbx
Ltmp0:
	.cfi_def_cfa_offset 16
	subq	$32, %rsp
Ltmp1:
	.cfi_def_cfa_offset 48
Ltmp2:
	.cfi_offset %rbx, -16
	leaq	(%rsp), %rbx
	movq	%rbx, %rdi
	callq	"___struct#String_init"
	movq	(%rax), %rcx
	movq	8(%rax), %rdx
	movq	16(%rax), %rsi
	movq	24(%rax), %rax
	movq	%rax, 24(%rsp)
	movq	%rsi, 16(%rsp)
	movq	%rdx, 8(%rsp)
	movq	%rcx, (%rsp)
	movq	%rbx, %rdi
	callq	*24(%rsp)
	addq	$32, %rsp
	popq	%rbx
	retq
	.cfi_endproc

	.globl	"___automatic_init#String"
	.align	4, 0x90
"___automatic_init#String":             ## @"__automatic_init#String"
	.cfi_startproc
## BB#0:                                ## %initialiser
	movq	$0, -32(%rsp)
	movq	$0, -24(%rsp)
	leaq	"___struct#String_init"(%rip), %rax
	movq	%rax, -16(%rsp)
	leaq	"___struct#String_length"(%rip), %rax
	movq	%rax, -8(%rsp)
	leaq	-32(%rsp), %rax
	retq
	.cfi_endproc

	.globl	"___struct#String_init"
	.align	4, 0x90
"___struct#String_init":                ## @"__struct#String_init"
	.cfi_startproc
## BB#0:                                ## %entry
	pushq	%rbx
Ltmp3:
	.cfi_def_cfa_offset 16
	subq	$16, %rsp
Ltmp4:
	.cfi_def_cfa_offset 32
Ltmp5:
	.cfi_offset %rbx, -16
	movq	%rdi, 8(%rsp)
	callq	"___automatic_init#String"
	movq	%rax, %rbx
	movq	%rbx, 8(%rsp)
	movl	$100, %edi
	callq	_malloc
	movq	%rax, 8(%rbx)
	movq	8(%rsp), %rax
	movq	$128, (%rax)
	movq	8(%rsp), %rax
	addq	$16, %rsp
	popq	%rbx
	retq
	.cfi_endproc

	.globl	"___struct#String_length"
	.align	4, 0x90
"___struct#String_length":              ## @"__struct#String_length"
	.cfi_startproc
## BB#0:                                ## %entry
	movq	%rdi, -8(%rsp)
	movq	(%rdi), %rax
	retq
	.cfi_endproc


.subsections_via_symbols
