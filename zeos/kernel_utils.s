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

.globl writeMSR; .type writeMSR, @function; .align 0; writeMSR:
      # Guardar pila usuario
      pushl %ebp
      mov %esp,%ebp

      #Guardamos parametros en registros %ecx y %eax --> pq wrmsr hace esto:
            #"Write the value in EDX:EAX to MSR specified by ECX."" 
      mov 0x8(%ebp), %ecx
      movl $0, %edx
      mov 0xc(%ebp), %eax
      wrmsr

      #Restauramos pila y retornamos
      pop %ebp
      ret

.globl task_switch; .type task_switch, @function; .align 0; task_switch:
 push %ebp
 mov %esp, %ebp

    #Guardem registres que es modifiquen
 push %esi
 push %edi
 push %ebx

    #Crida a inner_task_switch
 pushl 8(%ebp)
 call inner_task_switch
 addl $4, %esp

    #Restaurem Registres
 pop %ebx
 pop %edi
 pop %esi

 mov %ebp,%esp
 pop %ebp

 ret

.globl change_stack; .type change_stack, @function; .align 0; change_stack:
    #Com estem al final de inner_taskswitch volem tornar on sigui que hagi cridat a task switch (anteriorment)
    #No guardem la pila perquè precissament volem canviar de pila

    #Estructura Pila
        # | @ret a iner_task_switch | <-%esp
        # | current()->k_esp | 4 esp
        # | new->task->k_esp | 8 esp
        # | ebp (task_switch) | 12 esp --> Aqui ja estem en la nova pila
        # | @ret a task_switch | 16 esp

    #Guardem el current ebp
 movl 4(%esp), %eax
 movl %ebp, (%eax)

    #Posem %ebp en el valor del esp del nou procés (canvi pila)
 movl 8(%esp), %esp
    #Pop ebp per estructura pila
 popl %ebp
    #ret al que cridés a inner_task_switch (normalment task_switch)
 ret
