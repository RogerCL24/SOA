# 0 "wrappers.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "wrappers.S"
# 1 "include/asm.h" 1
# 2 "wrappers.S" 2

.globl write; .type write, @function; .align 0; write:
    # Implementacion con int 0x80.

    # Guardar pila usuario
    pushl %ebp
 mov %esp,%ebp

    #Pasar parametros
    mov 0x08(%ebp), %ebx # fd -> ebx
 mov 0x0c(%ebp), %ecx # buffer -> ecx
 mov 0x10(%ebp), %edx # size -> edx

    #Codigo system call en %eax
    movl $4, %eax

    # Llamada al sistema con int 0x80
    int $0x80

    #Comparamos el return de la syscall
    cmpl $0, %eax
 jge wr_no_error

 # Se ejcuta si hay error
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl -1, %eax

wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
 popl %ebp
 ret
