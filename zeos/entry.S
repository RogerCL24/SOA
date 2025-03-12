/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include <errno.h>


/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %edx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %ebx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %ebx; \
      pushl %ecx; \
      pushl %edx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define RESTORE_ALL \
      popl %edx; \
      popl %ecx; \
      popl %ebx; \
      popl %esi; \
      popl %edi; \
      popl %ebp; \
      popl %eax; \
      popl %ds; \
      popl %es; \
      popl %fs; \
      popl %gs; 

#define EOI \
      movb $0x20, %al; \
      outb %al, $0x20; \

ENTRY(keyboard_handler)
      SAVE_ALL
      call keyboard_routine
      EOI
      RESTORE_ALL
      iret 

ENTRY(clock_handler)
      SAVE_ALL
      EOI
      call clock_routine
      RESTORE_ALL
      iret

ENTRY(my_page_fault_handler)
      call my_page_fault_routine

ENTRY(system_call_handler)
      # Guarda el context.
      SAVE_ALL

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
      movl $-ENOSYS, %eax 
end:
      movl %eax, 0x18(%esp) # guardar valor de retorno en la pila para no machacarlo
      RESTORE_ALL
      iret

ENTRY(syscall_handler_sysenter)
      #Guarda la info de usuario para que la pila quede igual que con el método de int 0x80
      push $__USER_DS
      push %EBP // User stack address
      pushfl
      push $__USER_CS
      push 4(%EBP) // User return address

      #Guarda el contexto
      SAVE_ALL

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
      movl $-ENOSYS, %EAX

sysenter_fin:
      #Restaura contexto
      movl %EAX, 0x18(%ESP)	
      RESTORE_ALL

      #Pone en %edx la return address del stack
      movl (%ESP), %EDX // Return address

      #Pone en %ecx el valor del %ebp del usuario
      movl 12(%ESP), %ECX // User stack address
      
      #Activa interrupciones y vuelve a modo usuario
      sti // Enable interrupts again
      sysexit

ENTRY(writeMSR)
      # Guardar pila usuario
      pushl %ebp
      mov %esp,%ebp

      #Guardamos parametros en registros %ecx y %eax --> pq wrmsr hace esto: 
            #"Write the value in EDX:EAX to MSR specified by ECX."" 
      mov 0x8(%ebp), %ecx     //Guarda en ecx el identificador del MSR ????????
      movl $0, %edx           //La parte alta del registro siempre 0
      mov 0xc(%ebp), %eax     //La parte baja es el segundo parametro 
      wrmsr

      #Restauramos pila y retornamos
      pop %ebp
      ret
