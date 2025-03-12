#include <libc.h>
#include <io.h>
#include <errno.h>

char buff[24];

int pid;

int add(int par1,int par2) {
	return par1 + par2;
}

int addAsm(int par1, int par2);

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  //Test Exceptions   
    //int res = add(0x42, 0x666);
    //int resASM = addAsm(0x42, 0x666);
    //printk("Hello World");
    // char* p = 0;
    // *p = 'x';


  //Test 0 Write
    /*
      char buff2[4096];
      for (int i = 0; i < 4096; ++i) {
        buff2[i] = 'a'; 
      }
        
        if (fast_write(1, buff2, strlen(buff2)) < 0) perror();
    */


  //Test Write
  if (write(1, "El siguiente write dara error de canal:\n", 40) < 0) perror();
  if (write(2, "Hello World!\n", 13) < 0) perror();
  if (write(1, "FUNCIONA!\n", 10) < 0) perror();
  if (fast_write(1, "Tambien pasara lo mismo con fast_write:\n", 40) < 0) perror();
  if (fast_write(2, "Hello World!\n", 13) < 0) perror();
  if (fast_write(1, "FUNCIONA!\n", 10) < 0) perror();

  //Test gettime
  char buff[24];
  itoa(fast_gettime(), buff); 
  if(write(1, "Time since start: ", 18) < 0) perror();
  if(write(1, buff, strlen(buff)) < 0) perror();
  if(write(1, "\n", 1) < 0) perror();
  //!!!!!!!!!!!!!!!!Problema con write al hacer bucle --> Solucionado salvando registro %ebx en rutina write
  int i;
  for (i = 0; i < 100000; ++i) {
    if (i%10000 == 0) {
      itoa(fast_gettime(), buff); 
      if(write(1, "Time since start: ", 18) < 0) perror();
      if(write(1, buff, strlen(buff)) < 0) perror();
      if(write(1, "\n", 1) < 0) perror();
    }
  }
    

  while(1) { 
    
  }
}
