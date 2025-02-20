#include <asm.h>

ENTRY(write)
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
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl -1, %eax

wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret


/*
ENTRY(write)
    # Implementacion con sysenter.

    # Guardar pila usuario
    pushl %ebp
	mov %esp,%ebp

    #Pasar parametros
    mov 0x08(%ebp), %ebx # fd -> ebx
	mov 0x0c(%ebp), %ecx # buffer -> ecx
	mov 0x10(%ebp), %edx # size -> edx

    #Codigo system call en %eax
    movl $4, %eax

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    # Se hace fake dinamic link
    pushl $write_return 
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

write_return:
    # Eliminamos data de stack 
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge wr_no_error

	# Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl -1, %eax

wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret
*/