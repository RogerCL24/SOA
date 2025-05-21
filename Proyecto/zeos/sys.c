/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

#define CLONE_PROCESS 0
#define CLONE_THREAD 1

#define MAX_PRIORITY 40
#define MAX_STACK_SIZE 65536 
#define USED_REGION NUM_PAG_CODE + NUM_PAG_DATA + NUM_PAG_KERNEL

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return current()->PID;
}

int global_PID=1000;

int ret_from_fork()
{
  return 0;
}

int find_free_stack_region(page_table_entry *PT, int from, int pages_needed, struct task_struct *main_thread) {

  struct list_head* thread_list = &main_thread->threads_list;
  for (int i = from; i < TOTAL_PAGES; ++i) {
    // primera pagina libre
   if (PT[i].entry == 0) {
     int new_start = i;
     int new_end = i + pages_needed;
     struct list_head *pos;
     int conflict = 0;
     // Comprobar si choca con alguna user_stack de algun thread
     list_for_each(pos, thread_list) {
       struct task_struct *thr = list_head_to_task_struct(pos);
       /*
        * _______________ thr_start
        *
        *  USER_STACK
        * _______________ thr_end
        *
        */
       int thr_start = ((unsigned int)thr->first_stack_page) >> 12;
       int thr_end = thr_start + thr->user_stack_pages;
 
       if (!(new_end <= thr_start || new_start >= thr_end)) {
         conflict = 1;
         i = thr_end - 1;
         break;
       }
 
 
     }
     // Pila del main_thread
     if (!conflict && main_thread->first_stack_page != NULL) {
                   int main_thr_start = ((unsigned int)main_thread->first_stack_page) >> 12;
                   int main_thr_end = main_thr_start + main_thread->user_stack_pages;
                   if (!(new_end <= main_thr_start || new_start >= main_thr_end)) {
                         conflict = 1;
                         i = main_thr_end - 1;
                   }
               }
 
     if (!conflict) return i;
   }
 
  }
  return -1;
 
 }


int do_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  struct task_struct *parent = current();
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());

  //Busquem quina es la primera entrada lliure que fara d'auxiliar
  int temp_entry = find_free_stack_region(parent_PT, USED_REGION + 1, NUM_PAG_DATA, parent->main_thread);
  if (temp_entry == -1) return -EFAULT;

  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  
  int init = NUM_PAG_KERNEL + NUM_PAG_CODE;
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+temp_entry, get_frame(process_PT, pag+init));
    copy_data((void*)((pag+init)<<12), (void*)((pag+temp_entry)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+temp_entry);
  }

  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  //Assignem les noves dades al task_struct del fill
  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;
  uchild->task.priority=20;		// hereda la prioridad?
  uchild->task.TID=1;
  uchild->task.next_TID = 2;	
  uchild->task.main_thread = &uchild->task;	// main_thread es él mismo
  uchild->task.user_stack_pages = 0;
  uchild->task.first_stack_page = NULL;
  INIT_LIST_HEAD(&(uchild->task.threads_list));
  INIT_LIST_HEAD(&(uchild->task.sibling));

  //Copiem la pila del thread
  if (parent->first_stack_page != NULL && parent->user_stack_pages > 0) {
    // Comprovem que hi hagi espai
    int new_stack_base = find_free_stack_region(process_PT, USED_REGION + 1, parent->user_stack_pages, &uchild->task);
    if (new_stack_base < 0) {
      //Tornem a posar les pagines en lliures
      for (int i = 0; i < NUM_PAG_DATA; ++i) {
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
      }
      //Afegim el task_struct a la freequeue i retornem error
      list_add_tail(lhcurrent, &freequeue);
      return -EAGAIN;
    }

    //si tenim prou espai anem reservant nous frames (utilitzaem en el pare l'espai auxiliar reservat abans)
    for (int i = 0; i < parent->user_stack_pages; ++i) {
      int new_frame = alloc_frame();
      if (new_frame != -1) {
        //Assignem el frame al fill i a l'auxiliar del pare
        set_ss_pag(process_PT, new_stack_base + i, new_frame);
        set_ss_pag(parent_PT, temp_entry + i, new_frame); // temporal en padre para copiar
        copy_data((void *)(((unsigned)parent->first_stack_page) + (i << 12)), (void *)((temp_entry + i) << 12), PAGE_SIZE);
        del_ss_pag(parent_PT, temp_entry + i);
      } 
      //Si no tenim prous frames físics desfem les assignacions i reserves i retornem error
      else {
        for (int j = 0; j < i; ++j) {
          del_ss_pag(process_PT, new_stack_base + j);
          free_frame(get_frame(process_PT, new_stack_base + j));
        }
        for (int k = 0; k < NUM_PAG_DATA; ++k) {
          del_ss_pag(process_PT, PAG_LOG_INIT_DATA + k);
          free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + k));
        }
        //Tornem a afegir el pcb a la freequeue
        list_add_tail(lhcurrent, &freequeue);
        return -EAGAIN;
      }
    }

    //Netejem la TLB per desassociar el pare del fill
    set_cr3(get_DIR(parent));

    //Assignem les noves dades del task_struct
    uchild->task.first_stack_page = (void *)(new_stack_base << 12);
    uchild->task.user_stack_pages = parent->user_stack_pages;
    unsigned int parent_esp = ((unsigned long *)KERNEL_ESP((union task_union*) parent))[-0x02];
    unsigned int offset = parent_esp - (unsigned int)(parent->first_stack_page);
    unsigned int new_esp = ((unsigned int)(uchild->task.first_stack_page)) + offset;
    ((unsigned long *)KERNEL_ESP(uchild))[-0x02] = new_esp;
  }

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);

  uchild->task.pause_time = 0;

  int frame_screen_page = get_frame(parent_PT, (int)current()->screen_page / PAGE_SIZE);
  set_ss_pag(process_PT, USED_REGION, frame_screen_page);
  uchild->task.screen_page = (void*)((USED_REGION) * PAGE_SIZE);

  //Semafors
  for (int i = 0; i < NR_TASKS; ++i) {
    if (semaphores[i].owner == -1) {
      semaphores[i].owner = uchild->task.PID;
      uchild->task.p_semaphores = &(semaphores[i]);
    }
  }
  
  return uchild->task.PID;
}

int pthreadCreate(void *(*func)(void *), void *param, int stack_size) {
  //Comprovació parametres
  if (!func) return -EINVAL;
  if (stack_size < 0) return -EINVAL;
  if (!access_ok(VERIFY_READ, func, sizeof(void (*)(void*)))) return -EFAULT;
	if (param && !access_ok(VERIFY_READ, param, sizeof(void*))) return -EFAULT;

  //Buscamos un TCB para el thread
  if (list_empty(&freequeue)) return -ENOMEM;
  struct list_head *lhcurrent = NULL;
  lhcurrent=list_first(&freequeue);
  list_del(lhcurrent);
  
  //Reservamos una pagina para el TCB
  int new_pag = alloc_frame();
	if (new_pag == -1) {
		list_add_tail(lhcurrent, &freequeue);
		return -EAGAIN;
	}

  
  //Copiamos task_struct del padre
	union task_union *new_thread;
	new_thread = (union task_union*) list_head_to_task_struct(lhcurrent);
	copy_data(current(), new_thread, sizeof(union task_union));

  //Reservamos el user stack
  struct task_struct *main_thread = current()->main_thread;
  page_table_entry *process_PT = get_PT(&new_thread->task);
	int pages_needed = (stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
	int first_st_pg = find_free_stack_region(process_PT, USED_REGION+1, pages_needed, main_thread);
	if (first_st_pg < 0) {
		list_add_tail(lhcurrent, &freequeue);
		return -ENOMEM;
	}

	for (int pag = 0; pag < pages_needed; pag++) {
		int new_pag = alloc_frame();
		if (new_pag != -1)
			set_ss_pag(process_PT, first_st_pg+pag, new_pag);
		else {
			for (int i = 0; i < pag; i++) {
			   free_frame(get_frame(process_PT, first_st_pg+i));
			   del_ss_pag(process_PT, first_st_pg+i);
			}
			list_add_tail(lhcurrent, &freequeue);
			return -EAGAIN;
		}
	}

  //Inicialitzem dades task_Struct (amb les del main thread)
  new_thread->task.state = ST_READY;
	new_thread->task.TID = main_thread->next_TID++;
	new_thread->task.pause_time = 0;
	new_thread->task.priority = 20;			//hereda la prioridad??
	new_thread->task.first_stack_page = (void *) (first_st_pg << 12);
	new_thread->task.user_stack_pages = pages_needed;
	INIT_LIST_HEAD(&(new_thread->task.sibling));
	INIT_LIST_HEAD(&(new_thread->task.threads_list));

  //Afegin el thread actual a la llista del main
  list_add_tail(&(new_thread->task.sibling), &(main_thread->threads_list));

  //Assignem la pila d'usuari
  unsigned int stack_top = (first_st_pg << 12) + stack_size;
	unsigned int* user_esp = (unsigned int *)stack_top;

  user_esp -= sizeof(Byte);
	*(user_esp) = (unsigned int)param;
	user_esp -= sizeof(Byte);
	*(user_esp) = 0;

  //Modifiquem el context hardware per coherencia
  ((unsigned long *) KERNEL_ESP(new_thread))[-0x02] = (unsigned long) user_esp;	// esp
	((unsigned long *) KERNEL_ESP(new_thread))[-0x05] = (unsigned long) func;	// eip 
  new_thread->task.register_esp = (unsigned long) &(new_thread->stack[KERNEL_STACK_SIZE - 0x12]); 

  //Afegim a la readyqueue i retornem
	list_add_tail(&new_thread->task.list, &readyqueue);
	return 0;

}

int sys_clone(int what, void *(*func)(void *), void *param, int stack_size)
{
  if (what == CLONE_PROCESS) return do_fork();

  else if (what == CLONE_THREAD) return pthreadCreate(func, param, stack_size);

  return -EINVAL; 
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
char localbuffer [TAM_BUFFER];
int bytes_left;
int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	bytes_left = nbytes;
	while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	}
	if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	}
	return (nbytes-bytes_left);
}


extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

void sys_exit()
{  
  struct task_struct *p = current();
  struct list_head *pos, *n;

  struct task_struct *main_proc = p->main_thread; 

  // Allibera tots els stacks dels diferents threads del procés
  list_for_each_safe(pos, n, &(main_proc->threads_list)) {
    struct task_struct *t = list_entry(pos, struct task_struct, sibling);

    if (t->first_stack_page != NULL && t->user_stack_pages > 0) {
      for (int i = 0; i < t->user_stack_pages; ++i) {
        unsigned int logical = ((unsigned int)t->first_stack_page >> 12) + i;
        free_frame(get_frame(get_PT(t), logical));
        del_ss_pag(get_PT(t), logical);
      }
    }

    list_del(&(t->sibling)); 
    update_process_state_rr(t, &freequeue);
    t->PID = -1;
    t->TID = -1;
    t->dir_pages_baseAddr = NULL;

    //LLiberem el vector de semafors
    t->p_semaphores->owner = -1;
    for (int i = 0; i < NR_SEMAPHORES; ++i) {
      t->p_semaphores->sem[i].sem_id = -1;
      t->p_semaphores->sem[i].counter = 0;
    }
    t->p_semaphores = NULL;
  }

  //Alliberem la data del procés
  page_table_entry *process_PT = get_PT(main_proc);
  for (int i = 0; i < NUM_PAG_DATA; i++) {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
  }

  // main_thread, no incluido en my_threads
  if (main_proc->first_stack_page != NULL && main_proc->user_stack_pages > 0) {
    for (int i = 0; i < main_proc->user_stack_pages; ++i) {
      unsigned int logical = ((unsigned int)main_proc->first_stack_page >> 12) + i;
      free_frame(get_frame(process_PT, logical));
      del_ss_pag(process_PT, logical);
    }
  }
  
  main_proc->PID = -1;
  main_proc->TID = -1;
  main_proc->dir_pages_baseAddr = NULL;

  main_proc->p_semaphores->owner = -1;
    for (int i = 0; i < NR_SEMAPHORES; ++i) {
      main_proc->p_semaphores->sem[i].sem_id = -1;
      main_proc->p_semaphores->sem[i].counter = 0;
    }
    main_proc->p_semaphores = NULL;

  //Posem el proces main a la freequeue i programem el seguent procés
  update_process_state_rr(main_proc, &freequeue);
  sched_next_rr();
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}

extern char keys[128];
int sys_GetKeyboardState(char* keyboard) {
  //Comprovem que sigui una direcció accessible
  if (!access_ok(VERIFY_WRITE, keyboard, 128)) return -EFAULT; 

  //Copiem el vector de sistema al vector d'usuari
  copy_to_user(keys, keyboard, 128);

  return 0;
}

extern struct list_head blocked;
int sys_pause (int miliseconds) {
  //Comprobem que el temps sigui > 0
  if (miliseconds < 0) return -EINVAL;

  //if (current()->screen_page != (void*)-1) dumpScreen();

  current()->pause_time = miliseconds*0.018;
  update_process_state_rr(current(), &blocked);
  sched_next_rr();

  return 0;
}


void* sys_StartScreen() {
  //Comprovem que no s'hagi fet ja StartScreen, si ja s'ha fet només retornem la direcció
  if (current()->screen_page != (void*)-1) {
    return current()->screen_page;
  }

  //Busquem nou frame (si no hi ha cap error)
  int frame = alloc_frame();
  if (frame < 0) return (void*)-EAGAIN;

  //Associem frame a dir lògica
  page_table_entry* tp = get_PT(current());
  set_ss_pag(tp, USED_REGION, frame);

  //Guardem al task_struct la direccio de la pantalla
  current()->screen_page = (void*)((USED_REGION) * PAGE_SIZE);

  //Retornem la direccio equivalent a l'inici de la pagina reservada
  return (void*)(current()->screen_page);
}

int sys_SetPriority(int priority) {
  //Comprovem que la prioritat sigui vàlida
	if (priority < 0 || priority > MAX_PRIORITY) return -EINVAL;

  //Assignem la prioritat pertinent
	current()->priority = priority;
	return 0;
}

int sys_pthread_exit() {
	struct task_struct *ts = current();
	// Si es el thread principal y aun quedan threads disponibles no puede ser eliminado
	if (ts->main_thread == ts && !list_empty(&ts->threads_list)) {
		return -1;
	}
	list_del(&ts->sibling);

  //Guardem l'inici i final de la pila del thread
	page_table_entry *pt = get_PT(ts);
	unsigned int first_addr = (unsigned int) ts->first_stack_page;
	int first_page = first_addr >> 12;
	int last_page = first_page + ts->user_stack_pages - 1;

  //Alliberem la pila d'usuari del thread
	for (int page = first_page; page <= last_page; ++page) {
    free_frame(get_frame(pt, page));
    del_ss_pag(pt, page);
	}

	ts->PID=-1;
	ts->TID=-1;
	update_process_state_rr(ts, &freequeue);
	sched_next_rr();

	return 0;
}

//Semafors
struct SemaphoreVector semaphores[NR_TASKS];

int sys_sem_init(int count) {
  //Comprovem que count sigui > 0
  if (count < 0) return -EINVAL;

  struct task_struct* c = current();
  for (int i = 0; i < NR_SEMAPHORES; ++i) {
    //si es cumpleix agafem aquest com a semàfor a inicialitzar
    if (c->p_semaphores->sem[i].sem_id < 0) {
      c->p_semaphores->sem[i].sem_id = i;
      c->p_semaphores->sem[i].counter = count;
      INIT_LIST_HEAD(&(c->p_semaphores->sem[i].queue));
      return i;
    }
  }
  //no tenim prou espai pel semafor
  return -ENAVAIL;
}

int sys_sem_wait(int sem_id) {
  //Comprovem que sigui un id vàlid
  if (sem_id < 0 || sem_id > NR_SEMAPHORES) return -EINVAL;

  struct task_struct* c = current();
  //Comprovem que el id > 0 (vol dir que ha estat inicialitzat)
  struct Semaphore *s = &(c->p_semaphores->sem[sem_id]);
  if (s->sem_id < 0) return -EACCES;

  if (s->counter <= 0) {
    list_add_tail(&(current()->list), &(s->queue));
    sched_next_rr();
  }
  else --(s->counter);

  return 0;
}

int sys_sem_post(int sem_id) {
  //Comprovem que sigui un id vàlid
  if (sem_id < 0 || sem_id > NR_SEMAPHORES) return -EINVAL;

  struct task_struct* c = current();
  //Comprovem que el id > 0 (vol dir que ha estat inicialitzat)
  struct Semaphore *s = &(c->p_semaphores->sem[sem_id]);
  if (s->sem_id < 0) return -EACCES;

  //Si la cua esta buida incrementem el contador -> Ara podem tenir un proces més concurrentment
  if (list_empty(&(s->queue))) ++(s->counter);
  //Si la cua no es buida cal eliminar el primer element de la cua del semafor i passarlo a ready
  else {
    struct list_head *lh_unlocked = list_first(&(s->queue));
    list_del(lh_unlocked);
    struct task_struct *ts_unlocked = list_head_to_task_struct(lh_unlocked);
    update_process_state_rr(ts_unlocked, &readyqueue);
  }
  return 0;
}

int sys_sem_destroy(int sem_id) {
  //Comprovem que sigui un id vàlid
  if (sem_id < 0 || sem_id > NR_SEMAPHORES) return -EINVAL;

  struct task_struct* c = current();
  //Comprovem que el id > 0 (vol dir que ha estat inicialitzat)
  struct Semaphore *s = &(c->p_semaphores->sem[sem_id]);
  if (s->sem_id < 0) return -EACCES;

  //Si la cua del semafor no esta buida, posem tots els threads de la cua en ready
  if (!(list_empty(&(s->queue)))) {
    struct list_head *pos, *tmp;    
    list_for_each_safe(pos, tmp, &(s->queue)) {
      struct task_struct *t= list_entry(pos, struct task_struct, list);
      list_del(&(t->list));
      update_process_state_rr(t, &readyqueue);
    }
  }

  //Reiniciem les dades del semàfor per invalidar-lo
  s->sem_id = -1;
  s->counter = 0;

  return 0;
}