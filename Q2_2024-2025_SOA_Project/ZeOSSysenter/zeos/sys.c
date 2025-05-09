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

extern char keys[128];
extern struct list_head blocked;
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

 struct list_head* thread_list = &main_thread->my_threads;
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


static int do_fork() 
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
  copy_data((union task_union*)parent, uchild, sizeof(union task_union));
  
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
  page_table_entry *parent_PT = get_PT(parent);
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
  set_cr3(get_DIR(parent));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;
  uchild->task.priority=20;		// hereda la prioridad?
  uchild->task.TID=1;
  uchild->task.next_TID = 2;	
  uchild->task.main_thread = &uchild->task;	// main_thread es él mismo
  uchild->task.user_stack_pages = 0;
  uchild->task.first_stack_page = NULL;
  INIT_LIST_HEAD(&(uchild->task.my_threads));
  INIT_LIST_HEAD(&(uchild->task.sibling));

  // Copiamos la pila del thread (si existe)
  if (parent->first_stack_page != NULL && parent->user_stack_pages > 0) {
        int new_stack_base = find_free_stack_region(process_PT, USED_REGION + 1, parent->user_stack_pages, &uchild->task);
	// No hay espacio
        if (new_stack_base < 0) {
	      for (int i = 0; i < NUM_PAG_DATA; ++i) {
        	del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
        	free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
    	     }
    	     list_add_tail(lhcurrent, &freequeue);
    	     return -EAGAIN;
	}

        for (int i = 0; i < parent->user_stack_pages; ++i) {
            int new_frame = alloc_frame();
            if (new_frame != -1) {
		 set_ss_pag(process_PT, new_stack_base + i, new_frame);
            	set_ss_pag(parent_PT, temp_entry + i, new_frame); // temporal en padre para copiar
            	copy_data((void *)(((unsigned)parent->first_stack_page) + (i << 12)),
                      (void *)((temp_entry + i) << 12), PAGE_SIZE);
            	del_ss_pag(parent_PT, temp_entry + i);
	    } 
	    else {
		for (int j = 0; j < i; ++j) {
        		del_ss_pag(process_PT, new_stack_base + j);
        		free_frame(get_frame(process_PT, new_stack_base + j));
    		}
               for (int k = 0; k < NUM_PAG_DATA; ++k) {
                	del_ss_pag(process_PT, PAG_LOG_INIT_DATA + k);
                	free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + k));
               }
               list_add_tail(lhcurrent, &freequeue);
               return -EAGAIN;
	    }
        }
	set_cr3(get_DIR(parent));
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
  
  int frame_screen_page = get_frame(parent_PT, (int)current()->screen_page >> 12);
  set_ss_pag(process_PT, USED_REGION, frame_screen_page);
  uchild->task.screen_page = (void*)((USED_REGION) << 12); //No hace falta en verdad, ya se copia

  
  return uchild->task.PID;
}

int sys_clone(int what, void *(*func)(void *), void *param, int stack_size)
{
	if (what != CLONE_PROCESS && what != CLONE_THREAD) return -EINVAL;

	if (what == CLONE_PROCESS) {
		return do_fork();
	}
	else if (what == CLONE_THREAD) {
		if (!func) return -EINVAL;

		if (stack_size <= 0 || stack_size > MAX_STACK_SIZE) 
			return -EINVAL;
		// Verificarmos que el puntero a codigo es accesible
		if (!access_ok(VERIFY_READ, func, sizeof(void (*)(void*))))
			return -EFAULT;
		if (param && !access_ok(VERIFY_READ, param, sizeof(void*)))
			return -EFAULT;
	}

	if (list_empty(&freequeue)) return -ENOMEM;

	struct list_head *lhcurrent = NULL;

	lhcurrent = list_first(&freequeue);
	list_del(lhcurrent);
	
	union task_union *new_thread;
	new_thread = (union task_union*) list_head_to_task_struct(lhcurrent);
	copy_data(current(), new_thread, sizeof(union task_union));

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


	new_thread->task.state = ST_READY;
	new_thread->task.TID = main_thread->next_TID++;
	new_thread->task.pause_time = 0;
	new_thread->task.priority = 20;			//hereda la prioridad??
	new_thread->task.first_stack_page = (void *) (first_st_pg << 12);
	new_thread->task.user_stack_pages = pages_needed;
	INIT_LIST_HEAD(&(new_thread->task.sibling));
	INIT_LIST_HEAD(&(new_thread->task.my_threads));
	

	// El main_thread es el que controla los threads del proceso
	list_add_tail(&(new_thread->task.sibling), &(main_thread->my_threads));

	// Pila de usuario
	unsigned int stack_top = (first_st_pg << 12) + stack_size;
	unsigned int* user_esp = (unsigned int *)stack_top;

	user_esp -= sizeof(Byte);
	*(user_esp) = (unsigned int)param;
	user_esp -= sizeof(Byte);
	*(user_esp) = 0;

	// Contexto hardware
	((unsigned long *) KERNEL_ESP(new_thread))[-0x02] = (unsigned long) user_esp;	// esp
	((unsigned long *) KERNEL_ESP(new_thread))[-0x05] = (unsigned long) func;	// eip 

	new_thread->task.register_esp = (unsigned long) &(new_thread->stack[KERNEL_STACK_SIZE - 0x12]); 

	list_add_tail(&new_thread->task.list, &readyqueue);
	return 0;

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

  // Libera todos los stacks de threads del proceso
  list_for_each_safe(pos, n, &(main_proc->my_threads)) {
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
  }

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

int sys_GetKeyboardState(char *keyboard) {

	if (!access_ok(VERIFY_WRITE,keyboard, 128)) return -EFAULT;
	
	copy_to_user(keys, keyboard, 128);
	
	return 0;
}

int sys_pause(int miliseconds) {
	
	if (miliseconds < 0) return -EINVAL;

	current()->pause_time = miliseconds*0.018;
	update_process_state_rr(current(), &blocked);
	sched_next_rr();

	return 0;
}

void* sys_StartScreen() {
	
	if(current()->screen_page != (void*)-1) {
		return current()->screen_page;
	}

	int frame = alloc_frame();
	if (frame < 0) return (void*) -EAGAIN;

	page_table_entry* tp = get_PT(current());
	set_ss_pag(tp, USED_REGION, frame);

	current()->screen_page = (void*)((USED_REGION)* PAGE_SIZE);
	return (void*)(current()->screen_page);
}

int sys_SetPriority(int priority) {

	if (priority < 0 || priority > MAX_PRIORITY)
		return -EINVAL;

	current()->priority = priority;
	return 0;
}

int sys_pthread_exit() {
	struct task_struct *ts = current();
	// Si es el thread principal y aun quedan threads disponibles no puede ser eliminado
	if (ts->main_thread == ts && !list_empty(&ts->my_threads)) {
		return -1;
	}
	list_del(&ts->sibling);

	page_table_entry *pt = get_PT(ts);
	unsigned int first_addr = (unsigned int) ts->first_stack_page;
	int first_page = first_addr >> 12;
	int last_page = first_page + ts->user_stack_pages - 1;

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
