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

extern struct list_head blocked;

struct list_head freequeue;
struct list_head readyqueue;

struct task_struct *idle_task;
struct task_struct *init_task;

int ticks_current;

void writeMSR(unsigned long msr, unsigned long valor);
void task_switch(union task_union *new);
void change_stack(unsigned long *current_addr, unsigned long new_kesp);

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
		//TEST task_switch y exit (para ver si se ejecuta idle al no haber procesos)
		printk("IDLE\n");
		
		;
	}
}

void init_idle (void)
{
	//Obtenim primer element lliure de la freequeue
	struct list_head *lh = list_first(&freequeue);

	//Eliminem de la freequeue el element
	list_del(lh);

	//Convertim el list_head en taskstruct
	struct task_struct *pcb = list_head_to_task_struct(lh);

	//Posem el PID en 0 i el quantum en 10 p ex
	pcb->PID = 0;	
	set_quantum(pcb, 10);

	//Inicialitzem les variables per block i unblock
	pcb->parent = NULL;
	INIT_LIST_HEAD(&(pcb->children_blocked));
	INIT_LIST_HEAD(&(pcb->children_unblocked));
	pcb->pending_unblocks = 0;

	//Inicialitzaem camp dir_pages_baseAaddr (direcció del pageDir)
	allocate_DIR(pcb);

	//Aqui hem de modificar el stack d'acord a la context switch routine
		//Primer cal obtenir el task_union per poder modificar el stack
		union task_union *tu_idle = (union task_union*) pcb; 

		//Posem adress de la rutina cpu_idle al principi de la pila (es el que s'executara al fer el task_switch)
		tu_idle->stack[KERNEL_STACK_SIZE - 1] = (unsigned long) cpu_idle;

		//Posem en seguent posicio stack el valor de ebp al desfer el link (pot ser 0)
		tu_idle->stack[KERNEL_STACK_SIZE -2] = 0;

		//Guardem al nou camp kernel_esp guardem la posicio dins la pila on es troba el valor d'ebp.
		//així al fer task_switch esp apuntarà a la pila d'idle
		tu_idle->task.kernel_esp = (unsigned long) &(tu_idle->stack[KERNEL_STACK_SIZE -2]);

		//Declarem com a global el pcb de idle per poder accedir fàcilment des del codi
		idle_task = pcb;
}

void init_task1(void)
{
	//Igual proces que en init_idle per arribar a assignar PID
	struct list_head *lh = list_first(&freequeue);
	list_del(lh);
	struct task_struct *pcb = list_head_to_task_struct(lh);


	pcb->PID = 1;
	set_quantum(pcb, 10);
	ticks_current = 10;	

	//Inicialitzem les variables per block i unblock
	pcb->parent = NULL;
	INIT_LIST_HEAD(&(pcb->children_blocked));
	INIT_LIST_HEAD(&(pcb->children_unblocked));
	INIT_LIST_HEAD(&(pcb->sibiling));
	pcb->pending_unblocks = 0;

	//Inicialitzaem camp dir_pages_baseAaddr (direcció del pageDir)
	allocate_DIR(pcb);

	//Assignem pagines físiques al espai d'usuari i afegim a taual de pagines la traduccio @log - @fis
	set_user_pages(pcb);

	//Actualitzem tss per que apunti a la pila de la new_task.
	tss.esp0 = KERNEL_ESP((union task_union*) pcb);

	//Actualitzame també el registre MSR 0x175 (per q contingui @ de la pila de sistema)
	writeMSR(0x175, (int) tss.esp0);

	//Actualitzem registre cr3 per a que apunti al pageDir de init 
	set_cr3(pcb->dir_pages_baseAddr);

	//Declarem com a global el pcb de idle per poder accedir fàcilment des del codi
	init_task = pcb;
}

void init_sched()
{
	//Inicialitzem freequeue
	INIT_LIST_HEAD(&freequeue);
	INIT_LIST_HEAD(&readyqueue);
	INIT_LIST_HEAD(&blocked);
	//Recorre vector de tasks per afegir tots en freeequeue
	for (int i = 0; i < NR_TASKS; ++i) 
		list_add (&(task[i].task.list), &freequeue);


	//Inicialitzem la readyqueue
	INIT_LIST_HEAD(&readyqueue);
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

void inner_task_switch(union task_union *new) {
	//Actualitzem pointer a la nova pila (tss.esp0 -> int0x80 / MSR 0x175 -> sysenter)
	tss.esp0 = KERNEL_ESP((union task_union *) new);
	writeMSR(0x175, (int) tss.esp0);

	//Canviem el user space modificant cr3 (q apunti a page directory de new)
	set_cr3(get_DIR(&(new->task)));
		//set_cr3(new->task.dir_pages_baseAddr);  --> Es lo mateix

	change_stack(&current()->kernel_esp, new->task.kernel_esp);
}


//COSAS DE SCHEDULING

//Retorna el valor del quantum
int get_quantum (struct task_struct *t) {
	return t->quantum;
}

//Posa en el camp quantum del proces t el quantum new_quantum
void set_quantum (struct task_struct *t, int new_quantum) {
	t->quantum = new_quantum;
}


//Redueix el numero de ticks del quantum del proces actual
void update_sched_data_rr(void)
{
	ticks_current--;
}

//Funció per decidir si cal canviar de procés (ret = 1 si canvi 0 otherwise)
int needs_sched_rr (void) {
	//S'ha acabat el quantum i la llista de ready no és buida
	if (ticks_current <= 0 && !list_empty(&readyqueue)) return 1;

	//No es canvia de proces
	else return 0;
}

void update_process_state_rr (struct task_struct *t, struct list_head *dst_queue) {

	//Si el proces passara a current nomes borrem de la cua en la q esta
	if (dst_queue == NULL) {
		list_del(&(t->list));
	}

	//Si el proces que volem modificar es el current() no esta a cap cua per tant nomes cal afegir
	else if (current() == t) list_add_tail(&(t->list), dst_queue);

	else {
		list_del(&(t->list));
		list_add_tail(&(t->list), dst_queue);
	}
}

void sched_next_rr () {
	struct list_head *lh_new;
	struct task_struct *ts_new;

	//Si la cua no està buida canviem al primer a la llista
	if (!list_empty(&readyqueue)) {
		//Obtenim el primer node de la llista 
		lh_new = list_first(&readyqueue);
		list_del(lh_new);

		//Obtenim el task struct del primer node
		ts_new = list_head_to_task_struct(lh_new);
		
	}
	//Si la cua està buida canviem a idle
	else ts_new = idle_task;
	
		//ts -> state = ST_RUN;
	ticks_current = get_quantum(ts_new);

	if (current()->PID != ts_new->PID) task_switch((union task_union*) ts_new);
}

void schedule() {
	// Actualitza el nombre de ticks del proces current
	update_sched_data_rr();
	
	// Comprova si cal canviar de procés (s'ha acabat quantum )
	if (needs_sched_rr()) {
		// Fa update del current (posar el current a la ready_queue)
		update_process_state_rr(current(), &readyqueue);
		
		//selecciona el seguent proces a executar i fa el task switch al final
		sched_next_rr();
	}
}