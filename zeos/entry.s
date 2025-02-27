# 0 "entry.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "entry.S"




# 1 "include/asm.h" 1
# 6 "entry.S" 2
# 1 "include/segment.h" 1
# 7 "entry.S" 2
# 1 "include/errno.h" 1
# 8 "entry.S" 2
# 72 "entry.S"
.globl keyboard_handler; .type keyboard_handler, @function; .align 0; keyboard_handler:
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es
      movb $0x20, %al; outb %al, $0x20;
      call keyboard_routine
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl clock_handler; .type clock_handler, @function; .align 0; clock_handler:
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es
      movb $0x20, %al; outb %al, $0x20;
      call clock_routine
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl my_page_fault_handler; .type my_page_fault_handler, @function; .align 0; my_page_fault_handler:
      call my_page_fault_routine

.globl system_call_handler; .type system_call_handler, @function; .align 0; system_call_handler:
      # Guarda el context.
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es

      # Comprova si syscall es valida (0 > Syscall < MAX_SYSCALL)
      cmpl $0, %eax
      jl is_error
      cmpl $MAX_SYSCALL, %eax
      jg is_error

      #S'executa si no hi ha error
      call *sys_call_table(,%eax, 0x04);
      jmp end

is_error:
      #Posa el codi correponent a %eax
      movl $-38, %eax
end:
      movl %eax, 0x18(%ebp) # guardar valor de retorno en la pila para no machacarlo
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;
      iret

.globl syscall_handler_sysenter; .type syscall_handler_sysenter, @function; .align 0; syscall_handler_sysenter:
      #Guarda la info de usuario para que la pila quede igual que con el método de int 0x80
      push $0x2B
      push %EBP
      pushfl
      push $0x23
      push 4(%EBP)

      #Guarda el contexto
      pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es

      #Comprueba que sea syscall válida (en %eax)
      cmpl $0, %EAX
      jl sysenter_err
      cmpl $MAX_SYSCALL, %EAX
      jg sysenter_err

      #Ejecuta la syscall correspondiente
      call *sys_call_table(, %EAX, 0x04)
      jmp sysenter_fin

sysenter_err:
      #Se ejecuta solo en caso de error
      movl $-38, %EAX

sysenter_fin:
      #Restaura contexto
      movl %EAX, 0x18(%ESP)
      popl %edx; popl %ecx; popl %ebx; popl %esi; popl %edi; popl %ebp; popl %eax; popl %ds; popl %es; popl %fs; popl %gs;

      #Pone en %edx la return address del stack
      movl (%ESP), %EDX

      #Pone en %ecx el valor del %ebp del usuario
      movl 12(%ESP), %ECX

      #Activa interrupciones y vuelve a modo usuario
      sti
      sysexit

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
