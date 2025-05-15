#include <asm.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $SYSENTER_RETURN
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(SYSENTER_RETURN)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

/* int write(int fd, char *buffer, int size) */
ENTRY(write)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $4, %eax
	movl 0x8(%ebp), %ebx;	//fd
	movl 0xC(%ebp), %ecx;	//buffer
	movl 0x10(%ebp), %edx;	//size
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

/* Common code for negative return */
nok:
	neg %eax
	mov %eax, errno
	mov $-1, %eax
	popl %ebp
	ret

/* int gettime() */
ENTRY(gettime)
	pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int getpid() */
ENTRY(getpid)
	pushl %ebp
	movl %esp, %ebp
	movl $20, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	xorl %ebx, %ebx			//A sys_clone le pasas de primer parametro CLONE_PROCESS (0)
	movl $2, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* void exit() */
ENTRY(exit)
	pushl %ebp
	movl %esp, %ebp
	movl $1, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int yield() */
ENTRY(yield)
	pushl %ebp
	movl %esp, %ebp
	movl $13, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int get_stats(int pid, struct stats *st) */
ENTRY(get_stats)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $35, %eax
	movl 0x8(%ebp), %ebx;	//pid
	movl 0xC(%ebp), %ecx;	//st
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

ENTRY(GetKeyboardState)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $5, %eax
	movl 0x8(%ebp), %ebx;	//keyboard
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

ENTRY(pause)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $6, %eax
	movl 0x8(%ebp), %ebx;	//miliseconds
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

ENTRY(StartScreen)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $7, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

ENTRY(clone)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx		// Save EBX, ESI and EDI if modified
	pushl %esi
	movl $2 ,%eax
	movl 0x8(%ebp), %ebx		//what
	movl 0xC(%ebp), %ecx		//param
	movl 0x10(%ebp), %edx		//stack_size
	movl 0x14(%ebp), %esi
	call syscall_sysenter
	popl %esi
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

ENTRY(SetPriority)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 		//priority
	movl $8, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

ENTRY(pthread_exit)
	pushl %ebp
	movl %esp, %ebp
	movl $9, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret


ENTRY(sem_init)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 		//value
	movl $14, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

ENTRY(sem_wait)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 		//sem_id
	movl $15, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

ENTRY(sem_post)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 		//sem_id
	movl $16, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

ENTRY(sem_destroy)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 		//sem_id
	movl $17, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret