#include <inc/mmu.h>
#include <inc/memlayout.h>

// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.
	//
	// Explanation:
	//   We must prepare the trap-time stack for our eventual return to
	//   re-execute the instruction that faulted.
	//   Unfortunately, we can't return directly from the exception stack:
	//   We can't call 'jmp', since that requires that we load the address
	//   into a register, and all registers must have their trap-time
	//   values after the return.
	//   We can't call 'ret' from the exception stack either, since if we
	//   did, %esp would have the wrong value.
	//   So instead, we push the trap-time %eip onto the *trap-time* stack!
	//   Below we'll switch to that stack and call 'ret', which will
	//   restore %eip to its pre-fault value.
	//
	//   In the case of a recursive fault on the exception stack,
	//   note that the word we're pushing now will fit in the
	//   blank word that the kernel reserved for us.
	//
	// Throughout the remaining code, think carefully about what
	// registers are available for intermediate calculations.  You
	// may find that you have to rearrange your code in non-obvious
	// ways as registers become unavailable as scratch space.
	//
	// LAB 4: Your code here.
	
	//	trap-time esp  //48(%esp)
	//	trap-time eflags
	//	trap-time eip  //40(%esp)
	//	utf_regs.reg_eax
	//	utf_regs.reg_ecx
	//	utf_regs.reg_edx
	//	utf_regs.reg_ebx
	//	utf_regs.reg_oesp
	//	utf_regs.reg_ebp
	//	utf_regs.reg_esi
	//	utf_regs.reg_edi
	//	utf_err (error code)
	//	utf_fault_va            <-- %esp
	
	movl 0x30(%esp), %eax //save trap-time %esp into %eax
	subl $4, %eax //reserve a word on the trap-time stack
	movl %eax, 0x30(%esp) //put back adjusted trap-time %esp so that ret pops correctly
	movl 0x28(%esp), %ebx //save trap-time %eip into %ebx
	movl %ebx, (%eax) //load trap-time %eip onto trap-time stack
	
	
	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.
	
	//	trap-time esp
	//	trap-time eflags
	//	trap-time eip
	//	utf_regs.reg_eax
	//	utf_regs.reg_ecx
	//	utf_regs.reg_edx
	//	utf_regs.reg_ebx
	//	utf_regs.reg_oesp
	//	utf_regs.reg_ebp
	//	utf_regs.reg_esi
	//	utf_regs.reg_edi
	//	utf_err (error code)
	//	utf_fault_va            <-- %esp
	
	addl $8, %esp //pop fault_va and utf_err(error code)
	popal
	
	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.
	
	//	trap-time esp
	//	trap-time eflags
	//	trap-time eip <-- %esp
	
	addl $4, %esp //skip eip
	popfl
	
	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
	
	popl %esp
	
	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
	
	ret //ret will pop already from trap-time stack
