
#include <asm.h>
#include <segment.h>
#include <errno.h>

ENTRY(task_switch)
	push %ebp
	mov %esp, %ebp

	push %esi
	push %edi
	push %ebx

	pushl 0(%ebp)
	call inner_task_switch
	addl $4, %esp

	pop %ebx
	pop %edi
	pop %esi
	
	mov %ebp,%esp
	pop %ebp
	
	ret

ENTRY(change_stack)
	mov 4(%esp), %eax
	mov %ebp, 0(%eax)

	mov 8(%esp), %esp
	pop %ebp
	ret	

	
