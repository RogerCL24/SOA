#include <asm.h>

#Guarda los registros que se pueden estar usando antes de haber llamado a la funcion
#define SAVE_REGISTERS \
    pushl %ebx;  \
    pushl %esi;  \
    pushl %edi;

#Restaura los registros que se pueden estar usando antes de haber llamado a la funcion
#define RESTORE_REGISTERS \
    popl %edi;   \
    popl %esi;   \
    popl %ebx;

ENTRY(write)
    # Implementacion con int 0x80.

    # Guardar pila usuario
    pushl %ebp
	mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # SAVE_REGISTERS --> Solo hace falta guardar ebx
    pushl %ebx;

    #Pasar parametros
    mov 0x10(%ebp), %ebx # size -> ebx
	mov 0x0c(%ebp), %ecx # buffer -> ecx
	mov 0x08(%ebp), %edx # fd -> edx

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
	movl $-1, %eax

wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

    # Restaurar registros antes de salir
    # RESTORE_REGISTERS
    popl %ebx

	popl %ebp
	ret

ENTRY(gettime)
    #implementación con int 0x80

    # Guardar pila usuario
    pushl %ebp
	mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # SAVE_REGISTERS  //No hace falta aqui, no hay parametros

    #Codigo system call en %eax
    movl $10, %eax

    # Llamada al sistema con int 0x80 --> Guarda return en %eax
    int $0x80

    #Comparamos el return de la syscall (Si negativo = error)
    cmpl $0, %eax
	jge gtime_no_error

	# Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

gtime_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno (%eax contiene el return)

    # Restaurar registros antes de salir
    # RESTORE_REGISTERS     //NO hay parametros

	popl %ebp
	ret

ENTRY(fast_gettime)

    pushl %ebp
	mov %esp,%ebp

    #Codigo system call en %eax
    movl $10, %eax

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Guardar la return address en stack (loque se hará despues del sysenter)
    pushl $gettime_return 
    
    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
	sysenter

gettime_return:
    # Eliminamos data de stack 
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge fast_gt_no_error

	# Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

fast_gt_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

	popl %ebp
	ret

ENTRY(fast_write)
    # Implementacion con sysenter.

    # Guardar pila usuario
    pushl %ebp
	mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # SAVE_REGISTERS
    pushl %ebx

    #Pasar parametros
    mov 0x10(%ebp), %ebx # size -> ebx
	mov 0x0c(%ebp), %ecx # buffer -> ecx
	mov 0x08(%ebp), %edx # fd -> edx

    #Codigo system call en %eax
    movl $4, %eax

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Guardar la return address en stack (loque se hará despues del sysenter)
    pushl $write_return 
    
    # Se hace fake dinamic link
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
	jge fast_wr_no_error

	# Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

fast_wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

    # Restaurar registros antes de salir
    # RESTORE_REGISTERS
    popl %ebx

	popl %ebp
	ret

ENTRY(getpid)
    pushl %ebp
    movl %esp, %ebp

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Ponemos codigo syscall
    movl $20, %eax

    pushl $getpid_return

    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

getpid_return: 
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge getpid_no_error

	# Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

getpid_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret


ENTRY(fork)
    pushl %ebp
    movl %esp, %ebp

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Ponemos codigo syscall
    movl $2, %eax

    pushl $fork_return

    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

fork_return:
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge fork_no_error

    # Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

fork_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret


ENTRY(exit)
    pushl %ebp
    movl %esp, %ebp

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Ponemos codigo syscall
    movl $1, %eax

    pushl $exit_return

    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

exit_return:
    popl %ebp
    addl $4, %esp

    popl %edx
    popl %ecx
    
    # No hi ha possible error
	popl %ebp
	ret


ENTRY(block)
    pushl %ebp
    movl %esp, %ebp

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Ponemos codigo syscall
    movl $21, %eax

    pushl $block_return

    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

block_return:
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge block_no_error

    # Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

block_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret 


ENTRY(unblock)
    pushl %ebp
    movl %esp, %ebp

    //Guardemel parametre a edx
    mov 0x08(%ebp),%edx

    # Guardar %ecx y %edx en user stack
    pushl %ecx
    pushl %edx

    #Ponemos codigo syscall
    movl $22, %eax

    pushl $unblock_return

    # Se hace fake dinamic link
    pushl %ebp
    mov %esp, %ebp

    #Entrar al sistema
    sysenter

unblock_return:
    popl %ebp
    addl $4, %esp
    popl %edx
    popl %ecx

    #Comparamos el return de la syscall
    cmpl $0, %eax
	jge unblock_no_error

    # Se ejcuta si hay error
	negl %eax           # Para obtener codigo error en positivo
	movl %eax, errno    # Pone el error en errno
	movl $-1, %eax

unblock_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
	popl %ebp
	ret 