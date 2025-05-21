/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>


#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

//Struct dels semafors
struct Semaphore {
  int sem_id;     //id del semafor -> S'utilitza per saber si s'ha inicialitzat
  int counter;    //Contador de threads que poden conviure al semafor
  struct list_head queue;   //Cua de threads al semafor
};

#define NR_SEMAPHORES 20
//Struct qe ues un vector de semafors i el pid del propietari del vector
struct SemaphoreVector {
  int owner;    //owner del Vector
  struct Semaphore sem[NR_SEMAPHORES];
};

extern struct SemaphoreVector semaphores[NR_TASKS];

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list;	/* Task struct enqueuing */
  int register_esp;		/* position in the stack */
  enum state_t state;		/* State of the process */
  int total_quantum;		/* Total quantum of the process */
  struct stats p_stats;		/* Process stats */
  int pause_time;       //Temps que li queda a un procés blocked per desbloquejar
  void* screen_page;    //Punter a la direccio de la pagina compartida de la pantalla

  //Caracteristiques afegides per fer threads
  int TID;
  int next_TID;
  int nthreads;                       //Nombre de threads del procés
  int priority;
  void* first_stack_page;
  int user_stack_pages;               //Numero de pagines del stack
  struct task_struct* main_thread;    //Apunta al thread principal
  struct list_head threads_list;      //Lista amb els threads del procés
  struct list_head sibling;          //Enllaç del pare als fills (node per vincular a thread_list del main)

  struct SemaphoreVector *p_semaphores;   //Vector de semafors d'un proces
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void schedule(void);

struct task_struct * current();

void task_switch(union task_union*t);
void switch_stack(int * save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

void init_semaphores();

#endif  /* __SCHED_H__ */
