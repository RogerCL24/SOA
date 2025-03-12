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

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
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
