/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

#if 1
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked, timeout_queue;
struct list_head freequeue, readyqueue;
struct task_struct *idle_task;
//struct task_struct *init_task;  // Prova, switch_context
int ticks_qt = 0;	// Cuanto deberia valer?

void writeMSR(unsigned long msr, unsigned long valor);
void change_stack(unsigned long *current_addr, unsigned long new_kesp);

// Pág. 55
void task_switch(union task_union *new);

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

int get_quantum(struct task_struct *t) {
	return t->quantum;
}

void set_quantum(struct task_struct *t, int new_quantum) {
	t->quantum = new_quantum;
}


void init_idle (void)
{	
	// primer lh (task_struct) libre
	struct list_head *lh = list_first(&freequeue);

	// lh ya no esta libre
	list_del(lh);
	
	// Pág. 45: Conversió del list_head a task_struct
	struct task_struct *pcb = list_head_to_task_struct(lh);

	// Campo PID del PCB tiene valor 0 (es idle)
	pcb->PID = 0;
	set_quantum(pcb, 10);	// 10???i
	pcb->parent = NULL;
	INIT_LIST_HEAD(&pcb->child_list);
	pcb->pending_unblocks = 0;
	
	// Inicializamos la var dir_pages_baseAddr que indica la 
	// direccion base del page_directory del proceso
	allocate_DIR(pcb);
	
	// Necesitamos el task_union donde se encuentra el pcb de idle
	// para modificar su campo stack (system stack)
	union task_union *tu_idle = (union task_union*) pcb;

	// Hacemos store al inicio de la sys_stack, de idle, del codigo
	// que debera ejecutar cuando se haga el task_switch (@ret) 
	tu_idle -> stack[KERNEL_STACK_SIZE - 1] = (unsigned long) cpu_idle;
	
	// 0 (puede ser otro) sera el valor del ebp al deshacer el 
	// dynamic link (pop ebp)
	tu_idle -> stack[KERNEL_STACK_SIZE - 2] = (unsigned long) 0;
	
	// En el nuevo campo, kernel_esp, del PCB guardamos la posicion,
	// dentro de la stack, donde se encuentra el valor de ebp,
	// asi cuando se cargue (haga el task_switch) esp apuntara 
	// correctamente a la pila de idle   
	tu_idle -> task.kernel_esp = (unsigned long) &(tu_idle->stack[KERNEL_STACK_SIZE - 2]);

	// Se declara como global. La inicializamos con el pcb de idle, asi es mas
	// facil acceder al su pcb desde el codigo
	idle_task = pcb;

}

void init_task1(void)
{	
	// Igual que amb init_idle
	struct list_head *lh = list_first(&freequeue);

	list_del(lh);

	struct task_struct *pcb = list_head_to_task_struct(lh);

	pcb->PID = 1;
	set_quantum(pcb, 10);			// 10???
	ticks_qt = 10;
	pcb->parent = NULL;
	INIT_LIST_HEAD(&pcb->child_list);
	INIT_LIST_HEAD(&pcb->sibling);
	pcb->pending_unblocks = 0;

	allocate_DIR(pcb);

	/*
	 * Assignamos paginas fisicas para el codigo y data del espacio de direcciones del
	 * user, además añadimos a la tabla de paginas del proceso la traduccion de @log a 
	 * @fis de estas paginas assignadas.
	 */
	set_user_pages(pcb);
	
	/*
	 * Necesitamos actualizar TSS para que apunte a la pila de la nueva tarea.
	 * esp0 apunta al inicio de la pila de sistema del proceso init 	 
	 */
	tss.esp0 = KERNEL_ESP((union task_union *) pcb);

	/*
	 * 0x175 = SYSENTER_ESP_MSR, contiene la @ de la pila de sistema cuando se hace 
	 * un sysenter. Escribimos en MSR 0x175 tss.esp0, sera la pila que cargara la CPU
	 * al llamar a sysenter
	 */
	writeMSR(0x175, (int) tss.esp0);

	// Registro cr3 apunta al page_directory del proceso init, pasa a ser el 
	// page_directory actual
	set_cr3(pcb->dir_pages_baseAddr);
	
	//init_task = pcb;

}

void inner_task_switch(union task_union *new_task) {
	
	/*
	 * Primero, se ha de guardar la nueva direccion de pila de sistema en TSS.esp0
	 * para que ahora apunte a la pila de sistema del nuevo proceso.
	 *
	 * Si no cambiamos tss.esp0, cuando el nuevo proceso haga una syscall, el kernel 
	 * podria intentar usar la pila del proceso anterior, corromperia la memoria.
	 */
	tss.esp0 = KERNEL_ESP((union task_union *) new_task);
	
	/*
	 * Necesario si entramos kernel mode con sysenter
	 */
	writeMSR(0x175, (int) tss.esp0);
	
	
	/*
	 * Segundo, cambiamos el espacio de direcciones del usuario. Actualizamos el
	 * registro cr3 (set_cr3) con la @ del page_directory del nuevo proceso (get_DIR).
	 * Implicitamente provoca flush de TLB.
	 * 
	 * Si no cambiamos cr3 el nuevo proceso intentaria acceder la memoria virtual del 
	 * proceso anterior, fallo de segmentacion.
	 */
	set_cr3(get_DIR(&(new_task->task)));
	
	change_stack(&(current()->kernel_esp), new_task->task.kernel_esp);

}


void init_sched()
{
	INIT_LIST_HEAD(&freequeue);
	INIT_LIST_HEAD(&readyqueue);
	INIT_LIST_HEAD(&blocked);
	for (int i = 0; i < NR_TASKS; i++) {
		list_add( &(task[i].task.list), &freequeue); // añadimos al head de freequeue 
							     // nuevas entradas con los PCBs
							     // de la tareas disponibles
	}	

}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}


void update_sched_data_rr(void) {

	--ticks_qt;
}

int needs_sched_rr() {
	
	// El quantum es 0 y la lista de ready NO esta vacia (hay procesos esperando)
	if ((ticks_qt == 0) && (!list_empty(&readyqueue))) return 1;
	// El quantum es 0, pero ready esta vacia, puede seguir ejecutandose
	if (ticks_qt == 0) ticks_qt = get_quantum(current());
	return 0;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue) {

	struct list_head *lh_tmp = &t->list;
	
	// Si el proceso tiene sus punteros apuntando a otros list_head significa
	// que esta en una lista, lo eliminamos de su cola actual
	if (!(lh_tmp->prev==NULL && lh_tmp->next == NULL)) {
		list_del(lh_tmp);
	}
	
	// Si dst_queue es null significa que es runnning y no se le añade a ninguna
	// cola. De lo contrario, cambiamos a la nueva cola
	if (dst_queue && current() != idle_task) list_add_tail(lh_tmp, dst_queue);
}

void sched_next_rr(void) {
	
	struct list_head *lh;
	struct task_struct *ts;

	/*
	 * Si aun hay procesos en ready t = first de readyqueue, sino
	 * t = idle	
	 */

	if (!list_empty(&readyqueue)) {
		lh = list_first(&readyqueue);
		list_del(lh);
		ts = list_head_to_task_struct(lh);
	}
	else ts = idle_task;
	
	ticks_qt = get_quantum(ts);

	if (current()->PID != ts->PID) task_switch((union task_union*) ts);
}

void schedule() {
	// Update del numero de ticks del proceso current
	update_sched_data_rr();
	
	// Devuelve 1 is necesario cambiar el proceso current
	if (needs_sched_rr()) {
		// Update del estado de current de running a ready
		update_process_state_rr(current(), &readyqueue);
		/*
		 * Selecciona el siguiente proceso a ejecutar, lo extrae de la readyqueue
	 	 * e invoca el switch_context
		 */
		sched_next_rr();
	}

}
