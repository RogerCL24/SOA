# 0 "wrappers.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "wrappers.S"
# 1 "include/asm.h" 1
# 2 "wrappers.S" 2

#Guarda los registros que se pueden estar usando antes de haber llamado a la funcion





#Restaura los registros que se pueden estar usando antes de haber llamado a la funcion





.globl write; .type write, @function; .align 0; write:
    # Implementacion con int 0x80.

    # Guardar pila usuario
    pushl %ebp
 mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # pushl %ebx; pushl %esi; pushl %edi; --> Solo hace falta guardar ebx
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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

    # Restaurar registros antes de salir
    # popl %edi; popl %esi; popl %ebx;
    popl %ebx

 popl %ebp
 ret

.globl gettime; .type gettime, @function; .align 0; gettime:
    #implementación con int 0x80

    # Guardar pila usuario
    pushl %ebp
 mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # pushl %ebx; pushl %esi; pushl %edi;

    #Codigo system call en %eax
    movl $10, %eax

    # Llamada al sistema con int 0x80 --> Guarda return en %eax
    int $0x80

    #Comparamos el return de la syscall (Si negativo = error)
    cmpl $0, %eax
 jge gtime_no_error

 # Se ejcuta si hay error
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

gtime_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno (%eax contiene el return)

    # Restaurar registros antes de salir
    # popl %edi; popl %esi; popl %ebx;

 popl %ebp
 ret

.globl fast_gettime; .type fast_gettime, @function; .align 0; fast_gettime:

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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

fast_gt_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

 popl %ebp
 ret

.globl fast_write; .type fast_write, @function; .align 0; fast_write:
    # Implementacion con sysenter.

    # Guardar pila usuario
    pushl %ebp
 mov %esp,%ebp

    # Guardar registros que se podrian usar, ebx se usa para guardar la i si hay bucle así que es necesario
    # pushl %ebx; pushl %esi; pushl %edi;
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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

fast_wr_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno

    # Restaurar registros antes de salir
    # popl %edi; popl %esi; popl %ebx;
    popl %ebx

 popl %ebp
 ret

.globl getpid; .type getpid, @function; .align 0; getpid:
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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

getpid_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
 popl %ebp
 ret


.globl fork; .type fork, @function; .align 0; fork:
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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

fork_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
 popl %ebp
 ret


.globl exit; .type exit, @function; .align 0; exit:
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


.globl block; .type block, @function; .align 0; block:
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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

block_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
 popl %ebp
 ret


.globl unblock; .type unblock, @function; .align 0; unblock:
    pushl %ebp
    movl %esp, %ebp


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
 negl %eax # Para obtener codigo error en positivo
 movl %eax, errno # Pone el error en errno
 movl $-1, %eax

unblock_no_error:
    # Se ejecuta si no hay error o cuando el error se ha guardado en errno
 popl %ebp
 ret
