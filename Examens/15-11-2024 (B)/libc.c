/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

#include <errno.h>

int errno = 0;

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}

void perror() {
  write(1, "perror: ", 8);
  switch (errno)
  {
    case EBADF:
      write(1, "Bad file number\n", 16);
      break;

    case EACCES:
      write(1, "Permission denied\n", 18);
      break;
    
    case ENOSYS:
      write(1, "Syscall not implemented\n", 24);
      break;

    case EFAULT:
      write(1, "Bad address\n", 12);
      break;
    
    case EINVAL:
      write(1, "Invalid argument\n", 17);
      break;

    case 0:
    write(1, "No known errors\n", 16);
    break;
    
    default:
      char buff[4];
      itoa(errno, buff);
      write(1, "Message for error ", 18);
      write(1, buff, strlen(buff));
      write(1, " not found\n", 11);
      break;
  }
}
