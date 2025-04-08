# 0 "kernel_utils.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "kernel_utils.S"

# 1 "include/asm.h" 1
# 3 "kernel_utils.S" 2
# 1 "include/segment.h" 1
# 4 "kernel_utils.S" 2
# 1 "include/errno.h" 1
# 5 "kernel_utils.S" 2

.globl task_switch; .type task_switch, @function; .align 0; task_switch:
 push %ebp
 mov %esp, %ebp

 push %esi
 push %edi
 push %ebx

 pushl 8(%ebp)
 call inner_task_switch
 addl $4, %esp

 pop %ebx
 pop %edi
 pop %esi

 mov %ebp,%esp
 pop %ebp

 ret

.globl change_stack; .type change_stack, @function; .align 0; change_stack:
 mov 4(%esp), %eax
 mov %ebp, 0(%eax)

 mov 8(%esp), %esp
 pop %ebp
 ret
