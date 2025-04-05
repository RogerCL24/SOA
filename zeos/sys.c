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
extern struct list_head freequeue, readyqueue;

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
	/*
	 * Primero de todo comprobamos si hay algun PCB libre en freequeue
	 * , de lo contrario, devolvemos error de no memory
	 */
	if(list_empty(&freequeue)) return -ENOMEM;
	
	/*
	 * Si hay PCBs libres, reservamos 1 para el nuevo proceso creado
	 * y lo borramos de freequeue
	 */
	struct list_head *lh = list_first(&freequeue);
	list_del(lh);

	/*
	 * Copiamos el PCB del padre al hijo.
	 * Primero obtenemos "child", el task_union del PCB del hijo
	 * Segundo, copiamos "current()" a "child", sus PCBs
	 */
	union task_union *child = (union task_union*) list_head_to_task_struct(lh);
	copy_data(current(), child, sizeof(union task_union));

	/*
	 * Cada proceso ha de tener su directorio de paginas.
	 * Asignamos uno nuevo al hijo.
	 */
	allocate_DIR((struct task_struct*) child);


	/* 
	 * Numero de paginas de data + stack ha reservar para el hijo
	 */
	int npages[NUM_PAG_DATA];

	for(int i = 0; i < NUM_PAG_DATA; ++i) {
		/*
		 * Reservamos una pagina fisica nueva.  
		 * alloc_frame devuelve el numero de pagina asignada
		 */ 
		npages[i] = alloc_frame(); 	 	
						
		if (npages[i] < 0) {		
			/*
			 * Si no hay memoria disponible alloc_frame devuelve valor negativo. 
			 * Si hubiesemos reservado 3 paginas y la 4 da error, debemos 
			 * liberar la 3 paginas anteriores para no dejar memoria reservada
			 * inutilmente.
			 *
			 * free_frame(@dirPag) hace el trabajo de liberar
			 * list_add_tail(list_head pcbField, list_head queue) devolvemos el
			 * PCB a la freequeue, el hijo no se creara porque no hemos podido
			 * reservar memoria
			 * EAGAIN error de memoria insuficiente para crear el hijo, pero
			 * si se intenta más tarde puede funcionar.
			 */			
			for (int j = 0; j < i; j++) free_frame(npages[j]); 
			list_add_tail(&child->task.list, &freequeue);
			return -EAGAIN;

		}
	}

	/*
	 * Ya tenemos memoria reservada para el hijo.
	 * Ahora tenemos que: 
	 * 1. Compartir las paginas de codigo entre padre e hijo
	 * 2. Compartir memoria del sistema (system code+data)
	 * 3. Asignar al hijo sus propias paginas de datos y stack
	 *
	 * Creamos/accedemos la tabla de paginas del hijo, esta tabla de paginas
	 * es accesible a través del campo "dis_pages_baseAddr" en el task_struct,
	 * por eso le pasamos "->task", que es el task_struct del union "child".
	 * Y tambien accedemos a la tabla de paginas del padre, para que el hijo herede
	 * parte de sus memoria (la antes mencionada)
	 */
	page_table_entry *child_PT = get_PT(&child->task);
	page_table_entry *parent_PT = get_PT(current());

	/*
	 * 2. Compartir memoria del sistema 
	 *
	 * Asignamos al hijo "child_PT" las mismas paginas que tiene el padre de kernel,
	 * ya que todos los procesos comparten el Kernel.
	 *
	 * get_frame() obtiene la pagina fisica que usa el padre
	 * set_ss_pag() pone la pagina fisica al Page Table del hijo   
	 */
	for (int i = 0; i  < NUM_PAG_KERNEL; i++) {
		set_ss_pag(child_PT, i, get_frame(parent_PT, i));
	}
	
	/*
	 * 1. Compartir las paginas de codigo entre padre e hijo
	 *
	 * Como el codigo es de solo lectura, compartimos las paginas del padre al hijo
	 * y no las copiamos.
	 */
	for (int i = 0; i < NUM_PAG_CODE; i++) {
		set_ss_pag(child_PT, PAG_LOG_INIT_CODE + i, get_frame(parent_PT, PAG_LOG_INIT_CODE+i));
	}
	
	/* 
	 * 3. Asignar al hijo sus propias paginas de datos y stack
	 *
	 * Ahora las paginas logicas de datos + stack estan mapeadas a las 
	 * paginas fisicas que hemos reservado antes, "npages[]" 
	 */
	for (int i = 0; i < NUM_PAG_DATA; ++i) {
		set_ss_pag(child_PT, PAG_LOG_INIT_DATA + i, npages[i]);
	}

	/*
	 * Falta copiar la data+stack del padre al hijo.
	 * Para ello hacemos que el padre pueda ver temporalmente la memoria del hijo.
	 *
	 * set_ss_pag(TP, PAG_LOG, PAG_FIS) asigna 'temporalmente' las paginas del hijo a la
	 * TP del padre
	 * copy_data() copia los datos. 
	 * - Cada pagina mide 4KB (2¹² bytes)
	 * - "i" representa el numero de pagina
	 * - Necesitamos una @ de memoria, hacemos shift a la izquierda (multiplica)
	 * - si "i" = 1 -> @real: 0x00001000  
	 * del_ss_pag() borra la pagina del hijo de la TP del padre, por eso 'temporalmente' 
	 */
	int FREE_SPACE = NUM_PAG_DATA + NUM_PAG_CODE;
	int TOTAL_SPACE = NUM_PAG_KERNEL + NUM_PAG_DATA;

	for (int i = NUM_PAG_KERNEL; i < TOTAL_SPACE; i++) {
		set_ss_pag(parent_PT, i + FREE_SPACE, get_frame(child_PT, i));
		copy_data((void *) (i << 12), (void*)((i + FREE_SPACE) << 12), PAGE_SIZE);
		del_ss_pag(parent_PT, i + FREE_SPACE);
	}
	
	/*
	 * Forzamos un flush de TLB del padre, nos aseguramos de que no pueda acceder a la
	 * memoria del hijo
	 */
	set_cr3(get_DIR(current()));
	
	/*
	 * PID unico asignado al hijo
	 */
	child->task.PID = ++globalpid;

	/*
	 * Preparamos la pila del hijo para "task_switch()"
	 *
	 * Cuando se cambie de tarea el sistema tiene que poder restaurar el estado del hijo
	 * correctamente y que el hijo pueda seguir ejecutandose como 'antes' (ya sabemos que
	 * el hijo acaba de spawnear y no se estaba ejecutando pero "task_switch()" siempre 
	 * cree que los procesos se estaban ejecutando anteriormente y trabaja con esa premisa
	 * , asi que, hay que simular la previa ejecucion del hijo)
	 *
	 * Para eso:
	 * 1. EBP debe tener algun valor, igual que en idle, ponemos 0. 
	 * 2. Hay que poner un @ de retorno en su pila, ret_from_fork.
	 * 3. kernel_esp apuntara al inicio de la pila, donde esta esperando" task_switch()". 
	 *
	 * Para verlo más claro, mirar Fig. 25, pág 54, de Zeos.pdf
	 */

	/*
	 * Pila que vera "task_switch":
	 * ________
	 * ebp (0) 
	 * ________ 0x13
	 * @ret_from_fork
	 * ________ 0x12
	 * @handler  
	 * ________ 0x11
	 * CTX SW (11 regs)
	 * ________ 0x05
	 * CTX HW (5 regs)
	 * ________ 0x00
	 *
	 * En -0x13 pondremos el valor del EBP y en -0x12 el valor de @ret_from_fork
	 */


	// 1.
	((unsigned long *)KERNEL_ESP(child))[-0x13] = (unsigned long) 0;

	// 2.
	((unsigned long *)KERNEL_ESP(child))[-0x12] = (unsigned long)&ret_from_fork;
	
	// 3.
	child->task.kernel_esp = &((unsigned long *)KERNEL_ESP(child))[-0x13];

	/*
	 * Por ultimo, añadimos child a readyqueue para que pueda ser ejecutado por la CPU
	 * y devolvemos el PID del hijo
	 */
	list_add_tail(&(child->task.list), &readyqueue);
	return child->task.PID;
}

void sys_exit()
{  
	struct task_struct *ts = current();
	page_table_entry *PT = get_PT(ts);
	int TOTAL = PAG_LOG_INIT_DATA + NUM_PAG_DATA;
	for (int i = PAG_LOG_INIT_DATA; i < TOTAL; ++i) {
		free_frame(get_frame(PT, i));
		del_ss_pag(PT, i);
	}

	ts->PID = -1;
	ts->dir_pages_baseAddr = NULL;
	update_process_state_rr(ts, &freequeue);
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
