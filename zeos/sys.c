/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

extern int zeos_ticks;

extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct list_head blocked;

extern struct task_struct* idle_task;

#define LECTURA 0
#define ESCRIPTURA 1

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int ret_from_fork() {
  return 0;
}

int globalpid = 100;
int sys_fork()
{
  //Busca un PCB en la freequeue, si no hi ha disponible retorna error
  if(list_empty(&freequeue)) return -ENOMEM;

  //Si hi ha PCB lliures reserva un PCB pel nou proces y es borra de la freequeue
  struct list_head *lh = list_first(&freequeue);
	list_del(lh);

  //Copiem el PCB del pare al fill
  union task_union *child = (union task_union*) list_head_to_task_struct(lh);   //Aixo es el task_union del PCB del fill
  copy_data(current(), child, sizeof(union task_union));                        //Copiem current() al fill

  //Assignem al procés fill el seu propi directori de pàgines
  allocate_DIR((struct task_struct*) child);

  //Reservem les pàgines de data del fill
  int npages[NUM_PAG_DATA];

	for(int frame = 0; frame < NUM_PAG_DATA; ++frame) {
		//Reservem una pàgina física nova -> El return es el número de pàgina
		npages[frame] = alloc_frame(); 	 	
		
    //Entrem al if si no tenim prous pàgines 
		if (npages[frame] < 0) {		 

      //Si haguessim reservat pàgines però mentre ho feiem ens quedem sense espai cal alliberar aquestes pàgines 
			for (int j = 0; j < frame; j++) free_frame(npages[j]); 

      //Retornem el PCB a la freequeue perquè no es creara el nou proces
			list_add_tail(&child->task.list, &freequeue);

			return -EAGAIN;
		}
	}
  //Ara aqui hem reservat totes les pàgines pel fill -> Fi comprovació possibles errors

  /*
    Ara tenim les pàgines reservades hem de: 
    1. Compartir les pàgines físiques de codi de pare i fill
    2. Compartir la memoria del sistema (kernel)
    3. Assignar al fill les seves pàgines de data i stack
  */

  //Aconseguim les dos taules de pàgines
  page_table_entry *child_PT = get_PT(&child->task);
	page_table_entry *parent_PT = get_PT(current());

  //1. Compartir les pàgines físiques de codi de pare i fill
  for (int page = 0; page < NUM_PAG_CODE; page++) {
    //No cal copiar perquè es comparteixen aixi que fem set del get
		set_ss_pag(child_PT, PAG_LOG_INIT_CODE + page, get_frame(parent_PT, PAG_LOG_INIT_CODE+page));
	}
  
  //2. Compartir la memoria del sistema (kernel)
  for (int page = 0; page  < NUM_PAG_KERNEL; page++) {
    //Tots processos comparteixen pàgines físiques kernel
		set_ss_pag(child_PT, page, get_frame(parent_PT, page));
	}

  //3. Assignar al fill les seves pàgines de dat i stack 
  for (int page = 0; page < NUM_PAG_DATA; ++page) {
    //Ara no fem un set del get sino que com volem que sigin noves utilitzem les pàgines que haviem reservat abans (npages[])
		set_ss_pag(child_PT, PAG_LOG_INIT_DATA + page, npages[page]);
	}
  
  //Encara cal copiar les dades del pare (no es comparteixen però al principi són iguals)
  int TOTAL_SPACE = NUM_PAG_KERNEL + NUM_PAG_DATA + NUM_PAG_CODE;

  //Recorrem totes les pàgines de data -> 
	for (int page = 0; page < NUM_PAG_DATA; page++) {
    //Assigna temporalment el frame que té el fill a una nova pàgina del pare 
    //(perquè així ara el pare té accés als dos frames, el que copiara i on enganxarà)
    //i aquest frame on enganxarà com hem fet get del child correspon a les pàgines físiques del child
		set_ss_pag(parent_PT, page + TOTAL_SPACE, get_frame(child_PT, PAG_LOG_INIT_DATA + page));

    //Copia el contingut de la pàgina original del pare a la nova pàgina assignada al pare. -> (Data comença )
    copy_data((void *)((PAG_LOG_INIT_DATA + page) << 12), (void *)((TOTAL_SPACE + page) << 12), PAGE_SIZE);


    //Elimina la pàgina temporalment assignada al pare
		del_ss_pag(parent_PT, TOTAL_SPACE + page);
	}

  //Fem un flush del TLB per deslinkar el pare i el fill del tot
  set_cr3(get_DIR(current()));

  //Assignem el PID unic del fill
  child->task.PID = ++globalpid;
  child->task.quantum = 100;


  /*Ara hem de preparar la pila del procés fill per a que funcioni amb task_switch
    Alesgores cal que:
    1. ebp tingui algun valor (posem per exemple 0 com en idle)
    2. Posem una @RET en la pila -> Que sigui a la funció ret_from_fork
    3. kernel_esp ha d'apuntar al inici de la pila, on espera task_switch()

    Pila que es veurà a task_switch:

      | ebp                       |       0x13
      | @ret_from_fork            |       0x12
      | @handler                  |       0x11
      |                           |        
      |     ctx SW (11 regs)      |       0x05
      |                           |
      |     ctx SW (5 regs)       |
      |                           |       0x00
  */

  //Posem en -0x13 el valor de ebp
  ((unsigned long *)KERNEL_ESP(child))[-0x13] = (unsigned long) 0;

  //Posem en -0x12 el valor de @ret_from_fork
  ((unsigned long *)KERNEL_ESP(child))[-0x12] = (unsigned long) ret_from_fork;

  //Posem en kernel_esp del fill el top de la pila (0x13)
  child->task.kernel_esp =(unsigned long) &(child->stack[KERNEL_STACK_SIZE - 19]);

  //Inicialitzaem les coses de block
  child->task.pending_unblocks = 0;
  INIT_LIST_HEAD(&(child->task.children_blocked));
  INIT_LIST_HEAD(&(child->task.children_unblocked));
  INIT_LIST_HEAD(&(child->task.sibiling));
  child->task.parent = current();

  //Afegim el nou procés a la llista de fills del pare (current())
  list_add_tail(&(child->task.sibiling), &(current()->children_unblocked));

  //Afegim al child a la readyqueue i retornem el PID del fill
  list_add_tail(&(child->task.list), &readyqueue);
	return child->task.PID;
}

void sys_exit()
{  
  //Obtenim el pcb i la taula de pàgines
  struct task_struct *current_pcb = (struct task_struct *)current();
  page_table_entry *current_PT = get_PT(current_pcb);

  //Abans de borrar el proces hem de gestionar les cues que s'utilitzaen per block/unblock
    //Eliminem el proces current() (el que borrarem) de la llista del pare -> Recordar que sibiling son els nodes de la llista de fills dels pares
    if (current_pcb->parent != NULL) list_del(&(current_pcb->sibiling));

    //Movem tots els fills blocked de current a Idle
      
      //Pos es loop counter / tmp es temporal necesari / Despres tenim el head de la llista sobre el que s'itera
      //Per tant iterem sobre els unblocked i pos seria element[i]
      struct list_head *pos, *tmp;    
      list_for_each_safe(pos, tmp, &current_pcb->children_unblocked) {
        struct task_struct *child = list_entry(pos, struct task_struct, sibiling);
        list_del(pos);
        list_add_tail(&(child->sibiling), &(idle_task->children_unblocked));
        child->parent = idle_task;
      }

      list_for_each_safe(pos, tmp, &(current_pcb->children_blocked)) {
        struct task_struct *child =  list_entry(pos, struct task_struct, sibiling);
        list_del(pos);
        list_add_tail(&(child->sibiling), &(idle_task->children_blocked));
        child->parent = idle_task;
      }

  //Recorrem totes les pagines de data per esborrar els frames fisics i després esborrem les entrades de la TP
  for (int page = 0; page < NUM_PAG_DATA; ++page) {
    //Obtenim el frame corresponent de data
    free_frame(get_frame(current_PT, PAG_LOG_INIT_DATA + page));

    //Esborrem les entradesd la TP
    del_ss_pag(current_PT, PAG_LOG_INIT_DATA + page);
  }

  //Afegim el pcb a la freequeue (No cal treure de readyqueue pq el current() mai esta a la readyqueue)
  list_add_tail(&current_pcb->list, &freequeue);

  //Passem al següent procés
  sched_next_rr();

}

#define BUFF_SIZE 256
char buffer_sys[BUFF_SIZE];

int sys_write(int fd, char *buffer, int size) {
  //Comprova el canal.
  int ret = check_fd(fd, ESCRIPTURA);
  if (ret < 0) return ret;

  //Comprova buffer.
  if(buffer == NULL) return -EFAULT;  /* Bad address */

  //Comprova size.
  if (size < 0 || !access_ok(VERIFY_READ, buffer, size)) return -EINVAL;   /* Invalid argument */

  int toWrite = size;         //Bytes que falten per escriure.
  char *newPointer = buffer;  //Punter auxiliar per apuntar a la zona del buffer on comencarem a escriure en cada iteracio.

  //Si no cap tot string en el buffer va poc a poc.
  while (toWrite > BUFF_SIZE) {
    copy_from_user(newPointer, buffer_sys, BUFF_SIZE);
    int bytes_written = sys_write_console(newPointer, BUFF_SIZE);

    newPointer += bytes_written;
    toWrite -= bytes_written;
  }

  //Quan ja cap tot el que queda del string al buffer escriu fins al final del string.
  copy_from_user(newPointer, buffer_sys, toWrite);
  int bytes_written = sys_write_console(newPointer, toWrite);

  toWrite -= bytes_written;

  //Retorna el N de bytes escrits, en principi = size pero por si acaso.
  return size - toWrite;
}

int sys_gettime() {
  return zeos_ticks;
}

int sys_block() {
  struct task_struct* current_pcb = current();
  struct task_struct* parent_pcb = current_pcb->parent;

  //Si hi ha pending unblocks només es decrementa variable i return
  if (current()->pending_unblocks > 0) {
    current()->pending_unblocks--;
    return 0;
  }

  //Afegim el proces a la blocked queue si el pare es diferent de idle (null)
  if (parent_pcb != NULL) {
    list_del(&(current_pcb->sibiling));   //Borrem de la llista actual
    list_add_tail(&(current_pcb->sibiling), &(parent_pcb->children_blocked));   //Afegim a llista de blocked del pare
  }

  //Afegim el proces a la blocked queue i canviem de proces
  update_process_state_rr(current_pcb, &blocked);
  sched_next_rr();
  return 0;
 }

 int sys_unblock(int pid) {
  struct task_struct* parent_pcb = current();

  //Recorrem tots els fills del pare per trobar el que té PID = pid
  struct list_head* pos;
  list_for_each(pos, &(parent_pcb->children_blocked)) {
    struct task_struct* child = list_entry(pos, struct task_struct, sibiling);
    if (child->PID == pid) {
      list_del(&(child->sibiling));
      list_add_tail(&(child->sibiling), &(parent_pcb->children_unblocked));
      update_process_state_rr(child, &readyqueue);
      return 0;
    }
  }

  //Si no estaba en blocked alsehores augmentem el pendingUnblocks
  list_for_each(pos, &(parent_pcb->children_unblocked)) {
    struct task_struct* child = list_entry(pos, struct task_struct, sibiling);
    if (child->PID == pid) {
      child->pending_unblocks++;
      return 0;
    }
  }

  //Retornem -1 si no hem trobat el pid
  return -1;
 }