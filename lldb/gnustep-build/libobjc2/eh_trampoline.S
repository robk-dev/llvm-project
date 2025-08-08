	.file	"eh_trampoline.cc"
	.text
	.hidden	_Z13eh_trampolinev              # -- Begin function _Z13eh_trampolinev
	.globl	_Z13eh_trampolinev
	.p2align	4
	.type	_Z13eh_trampolinev,@function
_Z13eh_trampolinev:                     # @_Z13eh_trampolinev
.Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, DW.ref.test_eh_personality
	.cfi_lsda 27, .Lexception0
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
.Ltmp0:
	callq	_Z9cxx_throwv@PLT
.Ltmp1:
	jmp	.LBB0_1
.LBB0_1:                                # %invoke.cont
	leaq	-1(%rbp), %rdi
	callq	_ZZ13eh_trampolinevEN1XD2Ev
	xorl	%eax, %eax
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.LBB0_2:                                # %lpad
	.cfi_def_cfa %rbp, 16
.Ltmp2:
	movq	%rax, %rcx
	movl	%edx, %eax
	movq	%rcx, -16(%rbp)
	movl	%eax, -20(%rbp)
	leaq	-1(%rbp), %rdi
	callq	_ZZ13eh_trampolinevEN1XD2Ev
# %bb.3:                                # %eh.resume
	movq	-16(%rbp), %rdi
	callq	_Unwind_Resume@PLT
.Lfunc_end0:
	.size	_Z13eh_trampolinev, .Lfunc_end0-_Z13eh_trampolinev
	.cfi_endproc
	.section	.gcc_except_table,"a",@progbits
	.p2align	2, 0x0
GCC_except_table0:
.Lexception0:
	.byte	255                             # @LPStart Encoding = omit
	.byte	255                             # @TType Encoding = omit
	.byte	1                               # Call site Encoding = uleb128
	.uleb128 .Lcst_end0-.Lcst_begin0
.Lcst_begin0:
	.uleb128 .Ltmp0-.Lfunc_begin0           # >> Call Site 1 <<
	.uleb128 .Ltmp1-.Ltmp0                  #   Call between .Ltmp0 and .Ltmp1
	.uleb128 .Ltmp2-.Lfunc_begin0           #     jumps to .Ltmp2
	.byte	0                               #   On action: cleanup
	.uleb128 .Ltmp1-.Lfunc_begin0           # >> Call Site 2 <<
	.uleb128 .Lfunc_end0-.Ltmp1             #   Call between .Ltmp1 and .Lfunc_end0
	.byte	0                               #     has no landing pad
	.byte	0                               #   On action: cleanup
.Lcst_end0:
	.p2align	2, 0x0
                                        # -- End function
	.text
	.p2align	4                               # -- Begin function _ZZ13eh_trampolinevEN1XD2Ev
	.type	_ZZ13eh_trampolinevEN1XD2Ev,@function
_ZZ13eh_trampolinevEN1XD2Ev:            # @_ZZ13eh_trampolinevEN1XD2Ev
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	_ZZ13eh_trampolinevEN1XD2Ev, .Lfunc_end1-_ZZ13eh_trampolinevEN1XD2Ev
	.cfi_endproc
                                        # -- End function
	.hidden	DW.ref.test_eh_personality
	.weak	DW.ref.test_eh_personality
	.section	.data.DW.ref.test_eh_personality,"awG",@progbits,DW.ref.test_eh_personality,comdat
	.p2align	3, 0x0
	.type	DW.ref.test_eh_personality,@object
	.size	DW.ref.test_eh_personality, 8
DW.ref.test_eh_personality:
	.quad	test_eh_personality
	.ident	"clang version 20.1.8 (https://github.com/robk-dev/llvm-project.git bbf91aa53798c9e24933040e73c336b57262af58)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym _Z9cxx_throwv
	.addrsig_sym test_eh_personality
	.addrsig_sym _Unwind_Resume
