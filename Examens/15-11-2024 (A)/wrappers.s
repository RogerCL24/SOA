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
 push %ebp
 mov %esp, %ebp

 push %edx # Sysenter no preserva estos registros
 push %ecx

 movl $20, %eax # eax contiene el identificador de sys_getpid

 push $gp_return # fake dynamic link necesario para sysenter
 push %ebp
 mov %esp, %ebp

 sysenter

gp_return:
 pop %ebp # restablecemos los regs despues de sysenter
 add $4, %esp
 pop %ecx
 pop %edx

 cmp $0, %eax # comprobamos si la llamada a dado error
 jge gp_no_error

 neg %eax # si ha dado error, pid = -1
 mov %eax, errno # se niega para obtener el valor abosluto
 mov $-1, %eax

gp_no_error:
 pop %ebp # devolvemos il pid en eax
 ret

.globl fork; .type fork, @function; .align 0; fork:
 push %ebp
 mov %esp, %ebp

 push %edx # Sysenter no preserva estos registros
 push %ecx

 movl $2, %eax

 push $fork_return # fake dynamic link necesario para sysenter
 push %ebp
 mov %esp, %ebp

 sysenter

fork_return:
 pop %ebp # restablecemos los regs despues de sysenter
 add $4, %esp
 pop %ecx
 pop %edx

 cmp $0, %eax # comprobamos si la llamada a dado error
 jge fork_no_error

 neg %eax # si ha dado error, pid = -1
 mov %eax, errno # se niega para obtener el valor abosluto
 mov $-1, %eax

fork_no_error:
 pop %ebp # devolvemos el pid en eax
 ret

.globl exit; .type exit, @function; .align 0; exit:
 push %ebp
 movl %esp, %ebp

 push %edx
 push %ecx

 movl $1, %eax
 push $ret_exit
 push %ebp
 mov %esp, %ebp

 sysenter

ret_exit:
 pop %ebp
 add $4, %esp
 pop %ecx
 pop %edx

 cmp $0, %eax
 jge exit_no_error

 neg %eax
 mov %eax, errno
 mov $-1, %eax

exit_no_error:
 pop %ebp
 ret
