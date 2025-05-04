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
#define MAX_STACK_SIZE 4096 

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


static int do_fork() 
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
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
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;
  uchild->task.priority=20;		// hereda la prioridad?
  uchild->task.TID=1;
  uchild->task.thread_count=1;
  uchild->task.main_thread = &uchild->task;	// main_thread es él mismo

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
  
  INIT_LIST_HEAD(&(uchild->task.my_threads));
  INIT_LIST_HEAD(&(uchild->task.sibling));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);

  uchild->task.pause_time = 0;
  
  int frame_screen_page = get_frame(parent_PT, (int)current()->screen_page >> 12);
  set_ss_pag(process_PT, PAG_LOG_INIT_DATA+2*NUM_PAG_DATA, frame_screen_page);
  uchild->task.screen_page = (void*)((PAG_LOG_INIT_DATA+2*NUM_PAG_DATA) << 12);

  
  return uchild->task.PID;
}

int sys_clone(int what, void *(*func)(void *), void *param, int stack_size)
{
	if (what != CLONE_PROCESS && what != CLONE_THREAD) return -EINVAL;

	if (what == CLONE_PROCESS) {
		return do_fork();
	}

	if (what == CLONE_THREAD) {
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
	
	int new_pag = alloc_frame();
	if (new_pag == -1) {
		list_add_tail(lhcurrent, &freequeue);
		return -EAGAIN;
	}

	struct task_struct *parent = current();
	union task_union *new_thread;

	new_thread = (union task_union*) list_head_to_task_struct(lhcurrent);
	copy_data(parent, new_thread, sizeof(union task_union));

	// Se supone el caso donde un thread secundario crea otro thread
	struct task_struct *main_thread = current()->main_thread;

	int th_count = main_thread->thread_count;
	new_thread->task.state = ST_READY;
	++new_thread->task.TID;
	new_thread->task.pause_time = 0;
	new_thread->task.priority = 20;			//hereda la prioridad??
	page_table_entry *process_PT = get_PT(&new_thread->task);
	INIT_LIST_HEAD(&(new_thread->task.sibling));
	INIT_LIST_HEAD(&(new_thread->task.my_threads));
	

	// El main_thread es el que controla los threads del proceso
	list_add_tail(&(new_thread->task.sibling), &(main_thread->my_threads));

	// Pila de usuario
	int us_st_pt_entry = PAG_LOG_INIT_DATA + NUM_PAG_DATA*2 + th_count;
	set_ss_pag(process_PT, us_st_pt_entry, new_pag);
	++main_thread->thread_count;

	unsigned int stack_top = (us_st_pt_entry << 12) + stack_size;
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
  int i;

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
 


  /* Free task_struct */
  list_add_tail(&(current()->list), &freequeue);
 

  current()->PID=-1;
  current()->TID=-1;
  current()->dir_pages_baseAddr=NULL;
  /* Restarts execution of the next process */
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
	set_ss_pag(tp, PAG_LOG_INIT_DATA+2*NUM_PAG_DATA, frame);

	current()->screen_page = (void*)((PAG_LOG_INIT_DATA+2*NUM_PAG_DATA)* PAGE_SIZE);
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
	union task_union *tu = (union task_union*) ts;
	// Si es el thread principal y aun quedan threads disponibles no puede ser eliminado
	if (ts->main_thread == ts && !list_empty(&ts->my_threads)) {
		return -1;
	}
	list_del(&ts->sibling);
	// Deberia restar el counter pero creo que se acabara eliminando del TCB

	unsigned long user_esp = tu->stack[KERNEL_STACK_SIZE - 1];  // reg esp
	unsigned int user_stack_page = user_esp >> 12;
	page_table_entry *pt = get_PT(ts);
	free_frame(get_frame(pt, user_stack_page));	// solo libera 1 pag :(
	del_ss_pag(pt, user_stack_page);


	ts->PID=-1;
	ts->TID=-1;
	update_process_state_rr(ts, &freequeue);
	sched_next_rr();

	return 0;


}
