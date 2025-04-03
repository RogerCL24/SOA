
#include <asm.h>
#include <segment.h>
#include <errno.h>

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

ENTRY(task_switch)
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

ENTRY(change_stack)
    #Com estem al final de inner_taskswitch volem tornar on sigui que hagi cridat a task switch (anteriorment)
    #No guardem la pila perquè precissament volem canviar de pila

    #Estructura Pila
        #   |   @ret a iner_task_switch     |       <-%esp
        #   |   current()->k_esp            |       4 esp
        #   |   new->task->k_esp            |       8 esp
        #   |   ebp (task_switch)           |       12 esp   --> Aqui ja estem en la nova pila
        #   |   @ret a task_switch          |       16 esp

    #Guardem el current ebp  
	movl 4(%esp), %eax
	movl %ebp, (%eax)

    #Posem %ebp en el valor del esp del nou procés (canvi pila)
	movl 8(%esp), %esp
    #Pop ebp per estructura pila
	popl %ebp
    #ret al que cridés a inner_task_switch (normalment task_switch)
	ret	

	